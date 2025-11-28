#include "osgb2glb.h"

#include <vector>

#include <osg/Material>
#include <osg/PagedLOD>
#include <osgDB/ReadFile>
#include <osgDB/ConvertUTF>
#include <osgUtil/Optimizer>
#include <osgUtil/SmoothingVisitor>
#include <Eigen/Eigen>

#include "GeoTransform.h"


// #define STB_IMAGE_IMPLEMENTATION
// #define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"
#include "stb_image.h"
#include "stb_image_write.h"

using namespace std;

#define LOG_E(fmt,...) \
			char buf[512];\
			sprintf(buf,fmt,##__VA_ARGS__);\
			printf("%s\n", buf);


struct MeshInfo
{
    string name;
    std::vector<double> min;
    std::vector<double> max;
};

void write_buf(void* context, void* data, int len) {
    std::vector<char> *buf = (std::vector<char>*)context;
    buf->insert(buf->end(), (char*)data, (char*)data + len);
}

template<class T>
void alignment_buffer(std::vector<T>& buf) {
    while (buf.size() % 4 != 0) {
        buf.push_back(0x00);
    }
}

template<class T>
void put_val(std::vector<unsigned char>& buf, T val) {
    buf.insert(buf.end(), (unsigned char*)&val, (unsigned char*)&val + sizeof(T));
}

template<class T>
void put_val(std::string& buf, T val) {
    buf.append((unsigned char*)&val, (unsigned char*)&val + sizeof(T));
}

struct OsgBuildState
{
    tinygltf::Buffer* buffer;
    tinygltf::Model* model;
    osg::Vec3f point_max;
    osg::Vec3f point_min;
    int draw_array_first;
    int draw_array_count;
};

tinygltf::Material make_color_material_osgb(double r, double g, double b) {
    tinygltf::Material material;
    material.name = "default";
    tinygltf::Parameter baseColorFactor;
    baseColorFactor.number_array = { r, g, b, 1.0 };
    material.values["baseColorFactor"] = baseColorFactor;

    tinygltf::Parameter metallicFactor;
    metallicFactor.number_value = 0;
    material.values["metallicFactor"] = metallicFactor;
    tinygltf::Parameter roughnessFactor;
    roughnessFactor.number_value = 1;
    material.values["roughnessFactor"] = roughnessFactor;
    //
    return material;
}

class InfoVisitor : public osg::NodeVisitor
{
    std::string path;
public:
    InfoVisitor(std::string _path, bool loadAllType = false)
    :osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
    , path(_path), is_pagedlod(loadAllType), is_loadAllType(loadAllType)
    {}

    ~InfoVisitor() {
    }

    void apply(osg::Geometry& geometry){
        if (geometry.getVertexArray() == nullptr
            || geometry.getVertexArray()->getDataSize() == 0U
            || geometry.getNumPrimitiveSets() == 0U)
            return;

        if (is_pagedlod)
            geometry_array.push_back(&geometry);
        else
            other_geometry_array.push_back(&geometry);

        if (GeoTransform::pOgrCT)
        {
            osg::Vec3Array *vertexArr = (osg::Vec3Array *)geometry.getVertexArray();
            OGRCoordinateTransformation *poCT = GeoTransform::pOgrCT;

            /** 1. We obtain the bound of this tile */
            glm::dvec3 Min = glm::dvec3(DBL_MAX);
            glm::dvec3 Max = glm::dvec3(-DBL_MAX);
            for (int VertexIndex = 0; VertexIndex < vertexArr->size(); VertexIndex++)
            {
                osg::Vec3d Vertex = vertexArr->at(VertexIndex);
                glm::dvec3 vertex = glm::dvec3(Vertex.x(), Vertex.y(), Vertex.z());
                Min = glm::min(vertex, Min);
                Max = glm::max(vertex, Max);
            }

            /**
             * 2. We correct the eight points of the bounding box.
             * The point will be transformed from projected coordinate system
             * which is given by the original osgb tileset to geographic coordinate system,
             * and then transformed to Cesium ECEF coordinate system,
             * at last we transform the point from ECEF to the ENU of the origin.
             * We do this to correct the coordinate offset that
             * can occur when the tile is located far from the origin.
             */
            auto Correction = [&](glm::dvec3 Point) {
                glm::dvec3 cartographic = Point + glm::dvec3(GeoTransform::OriginX, GeoTransform::OriginY, GeoTransform::OriginZ);
                poCT->Transform(1, &cartographic.x, &cartographic.y, &cartographic.z);
                glm::dvec3 ecef = GeoTransform::CartographicToEcef(cartographic.x, cartographic.y, cartographic.z);
                glm::dvec3 enu = GeoTransform::EcefToEnuMatrix * glm::dvec4(ecef, 1);
                return enu;
            };
            vector<glm::dvec4> OriginalPoints(8);
            vector<glm::dvec4> CorrectedPoints(8);
            OriginalPoints[0] = glm::dvec4(Min.x, Min.y, Min.z, 1);
            OriginalPoints[1] = glm::dvec4(Max.x, Min.y, Min.z, 1);
            OriginalPoints[2] = glm::dvec4(Min.x, Max.y, Min.z, 1);
            OriginalPoints[3] = glm::dvec4(Min.x, Min.y, Max.z, 1);
            OriginalPoints[4] = glm::dvec4(Max.x, Max.y, Min.z, 1);
            OriginalPoints[5] = glm::dvec4(Min.x, Max.y, Max.z, 1);
            OriginalPoints[6] = glm::dvec4(Max.x, Min.y, Max.z, 1);
            OriginalPoints[7] = glm::dvec4(Max.x, Max.y, Max.z, 1);
            for (int i = 0; i < 8; i++)
                CorrectedPoints[i] = glm::dvec4(Correction(OriginalPoints[i]), 1);

            /**
             * 3. We use the least squares method to calculate the transformation matrix
             * that transforms the original box to the corrected box.
            */
            Eigen::MatrixXd A, B;
            A.resize(8, 4);
            B.resize(8, 4);
            for (int row = 0; row < 8; row++)
            {
                A.row(row) << OriginalPoints[row].x, OriginalPoints[row].y, OriginalPoints[row].z, 1;
            }
            for (int row = 0; row < 8; row++)
            {
                B.row(row) << CorrectedPoints[row].x, CorrectedPoints[row].y, CorrectedPoints[row].z, 1;
            }
            Eigen::BDCSVD<Eigen::MatrixXd> SVD(A, Eigen::ComputeThinU | Eigen::ComputeThinV);
            Eigen::MatrixXd X = SVD.solve(B);

            /*
             * 4. At last we apply the matrix to all the points of the tile to correct the offset.
            */
            glm::dmat4 Transform = glm::dmat4(
                X(0, 0), X(0, 1), X(0, 2), X(0, 3),
                X(1, 0), X(1, 1), X(1, 2), X(1, 3),
                X(2, 0), X(2, 1), X(2, 2), X(2, 3),
                X(3, 0), X(3, 1), X(3, 2), X(3, 3));

            for (int VertexIndex = 0; VertexIndex < vertexArr->size(); VertexIndex++)
            {
                osg::Vec3d Vertex = vertexArr->at(VertexIndex);
                glm::dvec4 v = Transform * glm::dvec4(Vertex.x(), Vertex.y(), Vertex.z(), 1);
                Vertex = osg::Vec3d(v.x, v.y, v.z);
                vertexArr->at(VertexIndex) = Vertex;
            }
        }
        if (auto ss = geometry.getStateSet() ) {
            osg::Texture* tex = dynamic_cast<osg::Texture*>(ss->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
            if (tex) {
                if (is_pagedlod)
                    texture_array.insert(tex);
                else
                    other_texture_array.insert(tex);
                texture_map[&geometry] = tex;
            }
        }
    }

    void apply(osg::PagedLOD& node) {
        //std::string path = node.getDatabasePath();
        int n = node.getNumFileNames();
        for (size_t i = 1; i < n; i++)
        {
            std::string file_name = path + "/" + node.getFileName(i);
            sub_node_names.push_back(file_name);
        }
        if (!is_loadAllType) is_pagedlod = true;
        traverse(node);
        if (!is_loadAllType) is_pagedlod = false;
    }

public:
    // Storing PagedLOD Geometry
    std::vector<osg::Geometry*> geometry_array;
    std::set<osg::Texture*> texture_array;
    std::map<osg::Geometry*, osg::Texture*> texture_map;
    std::vector<std::string> sub_node_names;
    bool is_loadAllType; // true: Store all geometry to geometry_array, false: Store by type
    bool is_pagedlod;
    // Storing Other Geometry
    std::vector<osg::Geometry*> other_geometry_array;
    std::set<osg::Texture*> other_texture_array;
};

void expand_bbox2d(osg::Vec2f& point_max, osg::Vec2f& point_min, osg::Vec2f point)
{
    point_max.x() = std::max(point.x(), point_max.x());
    point_min.x() = std::min(point.x(), point_min.x());
    point_max.y() = std::max(point.y(), point_max.y());
    point_min.y() = std::min(point.y(), point_min.y());
}

void expand_bbox3d(osg::Vec3f& point_max, osg::Vec3f& point_min, osg::Vec3f point)
{
    point_max.x() = std::max(point.x(), point_max.x());
    point_min.x() = std::min(point.x(), point_min.x());
    point_max.y() = std::max(point.y(), point_max.y());
    point_min.y() = std::min(point.y(), point_min.y());
    point_max.z() = std::max(point.z(), point_max.z());
    point_min.z() = std::min(point.z(), point_min.z());
}

void
write_vec2_array(osg::Vec2Array* v2f, OsgBuildState* osgState)
{
    int vec_start = 0;
    int vec_end   = v2f->size();
    if (osgState->draw_array_first >= 0)
    {
        vec_start = osgState->draw_array_first;
        vec_end   = osgState->draw_array_count + vec_start;
    }
    osg::Vec2f point_max(-1e38, -1e38);
    osg::Vec2f point_min(1e38, 1e38);
    unsigned buffer_start = osgState->buffer->data.size();
    for (int vidx = vec_start; vidx < vec_end; vidx++)
    {
        osg::Vec2f point = v2f->at(vidx);
        put_val(osgState->buffer->data, point.x());
        put_val(osgState->buffer->data, point.y());
        expand_bbox2d(point_max, point_min, point);
    }
    alignment_buffer(osgState->buffer->data);

    tinygltf::Accessor acc;
    acc.bufferView = osgState->model->bufferViews.size();
    acc.count = vec_end - vec_start;
    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    acc.type = TINYGLTF_TYPE_VEC2;
    acc.maxValues = {point_max.x(), point_max.y()};
    acc.minValues = {point_min.x(), point_min.y()};
    osgState->model->accessors.push_back(acc);

    tinygltf::BufferView bfv;
    bfv.buffer = 0;
    bfv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    bfv.byteOffset = buffer_start;
    bfv.byteLength = osgState->buffer->data.size() - buffer_start;
    osgState->model->bufferViews.push_back(bfv);
}


void
write_vec3_array(osg::Vec3Array* v3f, OsgBuildState* osgState, osg::Vec3f& point_max, osg::Vec3f& point_min)
{
    int vec_start = 0;
    int vec_end   = v3f->size();
    if (osgState->draw_array_first >= 0)
    {
        vec_start = osgState->draw_array_first;
        vec_end   = osgState->draw_array_count + vec_start;
    }
    unsigned buffer_start = osgState->buffer->data.size();
    for (int vidx = vec_start; vidx < vec_end; vidx++)
    {
        osg::Vec3f point = v3f->at(vidx);
        put_val(osgState->buffer->data, point.x());
        put_val(osgState->buffer->data, point.y());
        put_val(osgState->buffer->data, point.z());
        expand_bbox3d(point_max, point_min, point);
    }
    alignment_buffer(osgState->buffer->data);

    tinygltf::Accessor acc;
    acc.bufferView = osgState->model->bufferViews.size();
    acc.count = vec_end - vec_start;
    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    acc.type = TINYGLTF_TYPE_VEC3;
    acc.maxValues = {point_max.x(), point_max.y(), point_max.z()};
    acc.minValues = {point_min.x(), point_min.y(), point_min.z()};
    osgState->model->accessors.push_back(acc);

    tinygltf::BufferView bfv;
    bfv.buffer = 0;
    bfv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    bfv.byteOffset = buffer_start;
    bfv.byteLength = osgState->buffer->data.size() - buffer_start;
    osgState->model->bufferViews.push_back(bfv);
}


bool write_file(const char* filename, const char* buf, unsigned long buf_len)
	{
		// return false;
		std::ofstream ofs(filename, std::ios::binary);
		if (!ofs.is_open()) {
			return false;
		}
		ofs.write(buf, buf_len);
		if (!ofs.good()) {
			ofs.close();
			return false;
		}
		ofs.close();

		std::cout << "write_file" << std::endl;
		return true;
	}

std::string get_parent(std::string str) {
    auto p0 = str.find_last_of("/\\");
    if (p0 != std::string::npos)
        return str.substr(0, p0);
    else
        return "";
}

std::string osg_string ( const char* path ) {
    #ifdef WIN32
        std::string root_path =
        osgDB::convertStringFromUTF8toCurrentCodePage(path);
    #else
        std::string root_path = (path);
    #endif // WIN32
    return root_path;
}



template<class T> void
write_osg_indecis(T* drawElements, OsgBuildState* osgState, int componentType)
{
    unsigned max_index = 0;
    unsigned min_index = 1 << 30;
    unsigned buffer_start = osgState->buffer->data.size();

    unsigned IndNum = drawElements->getNumIndices();
    for (unsigned m = 0; m < IndNum; m++)
    {
        auto idx = drawElements->at(m);
        put_val(osgState->buffer->data, idx);
        if (idx > max_index) max_index = idx;
        if (idx < min_index) min_index = idx;
    }
    alignment_buffer(osgState->buffer->data);

    tinygltf::Accessor acc;
    acc.bufferView = osgState->model->bufferViews.size();
    acc.type = TINYGLTF_TYPE_SCALAR;
    acc.componentType = componentType;
    acc.count = IndNum;
    acc.maxValues = { (double)max_index };
    acc.minValues = { (double)min_index };
    osgState->model->accessors.push_back(acc);

    tinygltf::BufferView bfv;
    bfv.buffer = 0;
    bfv.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
    bfv.byteOffset = buffer_start;
    bfv.byteLength = osgState->buffer->data.size() - buffer_start;
    osgState->model->bufferViews.push_back(bfv);
}

struct PrimitiveState
{
    int vertexAccessor;
    int normalAccessor;
    int textcdAccessor;
};

void
write_element_array_primitive(osg::Geometry* g, osg::PrimitiveSet* ps, OsgBuildState* osgState, PrimitiveState* pmtState)
{
    tinygltf::Primitive primits;
    // indecis
    primits.indices = osgState->model->accessors.size();
    // reset draw_array state
    osgState->draw_array_first = -1;
    osg::PrimitiveSet::Type t = ps->getType();
    switch (t)
    {
        case(osg::PrimitiveSet::DrawElementsUBytePrimitiveType):
        {
            const osg::DrawElementsUByte* drawElements = static_cast<const osg::DrawElementsUByte*>(ps);
            write_osg_indecis(drawElements, osgState, TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE);
            break;
        }
        case(osg::PrimitiveSet::DrawElementsUShortPrimitiveType):
        {
            const osg::DrawElementsUShort* drawElements = static_cast<const osg::DrawElementsUShort*>(ps);
            write_osg_indecis(drawElements, osgState, TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
            break;
        }
        case(osg::PrimitiveSet::DrawElementsUIntPrimitiveType):
        {
            const osg::DrawElementsUInt* drawElements = static_cast<const osg::DrawElementsUInt*>(ps);
            write_osg_indecis(drawElements, osgState, TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT);
            break;
        }
        case osg::PrimitiveSet::DrawArraysPrimitiveType:
        {
            primits.indices = -1;
            osg::DrawArrays* da = dynamic_cast<osg::DrawArrays*>(ps);
            osgState->draw_array_first = da->getFirst();
            osgState->draw_array_count = da->getCount();
            break;
        }
        default:
        {
            LOG_E("unsupport osg::PrimitiveSet::Type [%d]", t);
            exit(1);
            break;
        }
    }
    // vertex: full vertex and part indecis
    if (pmtState->vertexAccessor > -1 && osgState->draw_array_first == -1)
    {
        primits.attributes["POSITION"] = pmtState->vertexAccessor;
    }
    else
    {
        osg::Vec3f point_max(-1e38, -1e38, -1e38);
        osg::Vec3f point_min(1e38, 1e38, 1e38);
        osg::Vec3Array* vertexArr = (osg::Vec3Array*)g->getVertexArray();
        primits.attributes["POSITION"] = osgState->model->accessors.size();
        // reuse vertex accessor if multi indecis
        if (pmtState->vertexAccessor == -1 && osgState->draw_array_first == -1)
        {
            pmtState->vertexAccessor = osgState->model->accessors.size();
        }
        write_vec3_array(vertexArr, osgState, point_max, point_min);
        // merge mesh bbox
        if (point_min.x() <= point_max.x() && point_min.y() <= point_max.y() && point_min.z() <= point_max.z())
        {
            expand_bbox3d(osgState->point_max, osgState->point_min, point_max);
            expand_bbox3d(osgState->point_max, osgState->point_min, point_min);
        }
    }
    // normal
    osg::Vec3Array* normalArr = (osg::Vec3Array*)g->getNormalArray();
    if (normalArr)
    {
        if (pmtState->normalAccessor > -1 && osgState->draw_array_first == -1)
        {
            primits.attributes["NORMAL"] = pmtState->normalAccessor;
        }
        else
        {
            osg::Vec3f point_max(-1e38, -1e38, -1e38);
            osg::Vec3f point_min(1e38, 1e38, 1e38);
            primits.attributes["NORMAL"] = osgState->model->accessors.size();
            // reuse vertex accessor if multi indecis
            if (pmtState->normalAccessor == -1 && osgState->draw_array_first == -1)
            {
                pmtState->normalAccessor = osgState->model->accessors.size();
            }
            write_vec3_array(normalArr, osgState, point_max, point_min);
        }
    }
    // textcoord
    osg::Vec2Array* texArr = (osg::Vec2Array*)g->getTexCoordArray(0);
    if (texArr)
    {
        if (pmtState->textcdAccessor > -1 && osgState->draw_array_first == -1)
        {
            primits.attributes["TEXCOORD_0"] = pmtState->textcdAccessor;
        }
        else
        {
            primits.attributes["TEXCOORD_0"] = osgState->model->accessors.size();
            // reuse textcoord accessor if multi indecis
            if (pmtState->textcdAccessor == -1 && osgState->draw_array_first == -1)
            {
                pmtState->textcdAccessor = osgState->model->accessors.size();
            }
            write_vec2_array(texArr, osgState);
        }
    }
    // material
    primits.material = -1;

    switch (ps->getMode())
    {
    case GL_TRIANGLES:
        primits.mode = TINYGLTF_MODE_TRIANGLES;
        break;
    case GL_TRIANGLE_STRIP:
        primits.mode = TINYGLTF_MODE_TRIANGLE_STRIP;
        break;
    case GL_TRIANGLE_FAN:
        primits.mode = TINYGLTF_MODE_TRIANGLE_FAN;
        break;
    default:
        LOG_E("Unsupport Primitive Mode: %d", (int)ps->getMode());
        exit(1);
        break;
    }
    osgState->model->meshes.back().primitives.push_back(primits);
}

void write_osgGeometry(osg::Geometry* g, OsgBuildState* osgState)
{
    osg::PrimitiveSet::Type t = g->getPrimitiveSet(0)->getType();
    PrimitiveState pmtState = {-1, -1, -1};
    for (unsigned int k = 0; k < g->getNumPrimitiveSets(); k++)
    {
        osg::PrimitiveSet* ps = g->getPrimitiveSet(k);
        if (t != ps->getType())
        {
            LOG_E("PrimitiveSets type are NOT same in osgb");
            exit(1);
        }
        write_element_array_primitive(g, ps, osgState, &pmtState);
    }
}

bool osgb2glb(const char* in, const char* out) {
    bool b_pbr_texture = true;
    MeshInfo mesh_info;
    std::string glb_buf;
    std::string path = osg_string(in);
    int node_type = -1;

    vector<string> fileNames = { path };
    std::string parent_path = get_parent(path);
    osg::ref_ptr<osg::Node> root = osgDB::readNodeFiles(fileNames);
    if (!root.valid()) {
        return false;
    }
    InfoVisitor infoVisitor(parent_path, node_type == -1);
    root->accept(infoVisitor);
    if (node_type == 2 || infoVisitor.geometry_array.empty()) {
        infoVisitor.geometry_array = infoVisitor.other_geometry_array;
        infoVisitor.texture_array = infoVisitor.other_texture_array;
    }
    if (infoVisitor.geometry_array.empty())
        return false;

    osgUtil::SmoothingVisitor sv;
    root->accept(sv);

    tinygltf::TinyGLTF gltf;
    tinygltf::Model model;
    tinygltf::Buffer buffer;

    osg::Vec3f point_max, point_min;
    OsgBuildState osgState = {
        &buffer, &model, osg::Vec3f(-1e38,-1e38,-1e38), osg::Vec3f(1e38,1e38,1e38), -1, -1
    };
    // mesh
    model.meshes.resize(1);
    int primitive_idx = 0;
    for (auto g : infoVisitor.geometry_array)
    {
        if (!g->getVertexArray() || g->getVertexArray()->getDataSize() == 0)
            continue;

        write_osgGeometry(g, &osgState);
        // update primitive material index
        if (infoVisitor.texture_array.size())
        {
            for (unsigned int k = 0; k < g->getNumPrimitiveSets(); k++)
            {
                auto tex = infoVisitor.texture_map[g];
                // if hava texture
                if (tex)
                {
                    for (auto texture : infoVisitor.texture_array)
                    {
                        model.meshes[0].primitives[primitive_idx].material++;
                        if (tex == texture)
                            break;
                    }
                }
                primitive_idx++;
            }
        }
    }
    // empty geometry or empty vertex-array
    if (model.meshes[0].primitives.empty())
        return false;

    mesh_info.min = {
        osgState.point_min.x(),
        osgState.point_min.y(),
        osgState.point_min.z()
    };
    mesh_info.max = {
        osgState.point_max.x(),
        osgState.point_max.y(),
        osgState.point_max.z()
    };
    // image
    {
        for (auto tex : infoVisitor.texture_array)
        {
            unsigned buffer_start = buffer.data.size();
            std::vector<unsigned char> jpeg_buf;
            int width, height;
            if (tex) {
                if (tex->getNumImages() > 0) {
                    osg::Image* img = tex->getImage(0);
                    if (img) {
                        width = img->s();
                        height = img->t();

                        const GLenum format = img->getPixelFormat();
                        const char* rgb = (const char*)(img->data());
                        uint32_t rowStep = img->getRowStepInBytes();
                        uint32_t rowSize = img->getRowSizeInBytes();
                        switch (format)
                        {
                        case GL_RGBA:
                            jpeg_buf.resize(width * height * 3);
                            for (int i = 0; i < height; i++)
                            {
                                for (int j = 0; j < width; j++)
                                {
                                    jpeg_buf[i * width * 3 + j * 3] = rgb[i * width * 4 + j * 4];
                                    jpeg_buf[i * width * 3 + j * 3 + 1] = rgb[i * width * 4 + j * 4 + 1];
                                    jpeg_buf[i * width * 3 + j * 3 + 2] = rgb[i * width * 4 + j * 4 + 2];
                                }
                            }
                            break;
                        case GL_BGRA:
                            jpeg_buf.resize(width * height * 3);
                            for (int i = 0; i < height; i++)
                            {
                                for (int j = 0; j < width; j++)
                                {
                                    jpeg_buf[i * width * 3 + j * 3] = rgb[i * width * 4 + j * 4 + 2];
                                    jpeg_buf[i * width * 3 + j * 3 + 1] = rgb[i * width * 4 + j * 4 + 1];
                                    jpeg_buf[i * width * 3 + j * 3 + 2] = rgb[i * width * 4 + j * 4];
                                }
                            }
                            break;
                        case GL_RGB:
                            for (int i = 0; i < height; i++)
                            {
                                for (int j = 0; j < rowSize; j++)
                                {
                                    jpeg_buf.push_back(rgb[rowStep * i + j]);
                                }
                            }
                            break;
                        default:
                            break;
                        }
                    }
                }
            }
            if (!jpeg_buf.empty()) {
                int buf_size = buffer.data.size();
                buffer.data.reserve(buffer.data.size() + width * height * 3);
                stbi_write_jpg_to_func(write_buf, &buffer.data, width, height, 3, jpeg_buf.data(), 80);
            }
            else {
                std::vector<char> v_data(256 * 256 * 3, 255);
                width = height = 256;
                stbi_write_jpg_to_func(write_buf, &buffer.data, width, height, 3, v_data.data(), 80);
            }
            tinygltf::Image image;
            image.mimeType = "image/jpeg";
            image.bufferView = model.bufferViews.size();
            model.images.push_back(image);
            tinygltf::BufferView bfv;
            bfv.buffer = 0;
            bfv.byteOffset = buffer_start;
            alignment_buffer(buffer.data);
            bfv.byteLength = buffer.data.size() - buffer_start;
            model.bufferViews.push_back(bfv);
        }
    }
    // node
    {
        tinygltf::Node node;
        node.mesh = 0;
        model.nodes.push_back(node);
    }
    // scene
    {
        tinygltf::Scene sence;
        sence.nodes.push_back(0);
        model.scenes = { sence };
        model.defaultScene = 0;
    }
    // sample
    {
        tinygltf::Sampler sample;
        sample.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
        sample.minFilter = TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR;
        sample.wrapS = TINYGLTF_TEXTURE_WRAP_REPEAT;
        sample.wrapT = TINYGLTF_TEXTURE_WRAP_REPEAT;
        model.samplers = { sample };
    }
    // use KHR_materials_unlit
    model.extensionsRequired = { "KHR_materials_unlit" };
    model.extensionsUsed = { "KHR_materials_unlit" };
    for (int i = 0 ; i < infoVisitor.texture_array.size(); i++)
    {
        tinygltf::Material mat = make_color_material_osgb(1.0, 1.0, 1.0);
        
        tinygltf::Value::Object unlitExtension;
        mat.extensions["KHR_materials_unlit"] = tinygltf::Value(unlitExtension);

        mat.pbrMetallicRoughness.metallicFactor = 1.0;
        mat.pbrMetallicRoughness.roughnessFactor = 1.0;
        mat.pbrMetallicRoughness.baseColorTexture.index = 0;

        model.materials.push_back(mat);
    }

    // finish buffer
    model.buffers.push_back(std::move(buffer));
    // texture
    {
        int texture_index = 0;
        for (auto tex : infoVisitor.texture_array)
        {
            tinygltf::Texture texture;
            texture.source = texture_index++;
            texture.sampler = 0;
            model.textures.push_back(texture);
        }
    }
    model.asset.version = "2.5";
    model.asset.generator = "haige";

    bool res = gltf.WriteGltfSceneToFile(&model, out, false, true, true, true);

    return res;
}
