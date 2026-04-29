#include "ply2osgb.h"

namespace HG {
    namespace ply {

bool Ply2Osgb::ply2osgb(const char* pFileIn, const char* pFileOut) {
    VertexData stVertexData;
    bool bReaded = stVertexData.readPlyFile(pFileIn);
    if (!bReaded) return false;

    osg::Node* pNode = pack2osgb(stVertexData);
    if (pNode == nullptr) return false;
    bool bWrited = osgDB::writeNodeFile(*pNode, pFileOut);
    return bWrited;
}

osg::Node* Ply2Osgb::pack2osgb(VertexData &stVertexData) {
    osg::ref_ptr<osg::Vec3Array> osgVertices = new osg::Vec3Array;
    osg::ref_ptr<osg::Vec3Array> osgNormals = new osg::Vec3Array;
    for (size_t i = 0; i < stVertexData.vVertices.size(); i += 3) {
        osgVertices->push_back(osg::Vec3(stVertexData.vVertices[i], stVertexData.vVertices[i + 1], stVertexData.vVertices[i + 2]));
        if (!stVertexData.vNormals.empty()) {

        }
    }

    // Create geometry node
    osg::Geode* geode = new osg::Geode;
    // for (int i = 0; i < stVertexData.vTexcoords.size(); ++i ) {
    //     osg::Geometry* geom  =  new osg::Geometry;
    //     // set the vertex array
    //     geom->setVertexArray(osgVertices.get());

    //     // Add the primitive set
    //     bool hasTriOrQuads = false;
    //     if (stVertexData.vTriangles[i].valid() && stVertexData.vTriangles[i]->size() > 0 )
    //     {
    //         geom->addPrimitiveSet(stVertexData.vTriangles[i].get());
    //         hasTriOrQuads = true;
    //     }

    //     if (_quads.valid() && _quads->size() > 0 )
    //     {
    //         geom->addPrimitiveSet(_quads.get());
    //         hasTriOrQuads = true;
    //     }
    //     // Print points if the file contains unsupported primitives
    //     // if(!hasTriOrQuads)
    //     //     geom->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, _vertices->size()));


    //     // Apply the colours to the model; at the moment this is a
    //     // kludge because we only use one kind and apply them all the
    //     // same way. Also, the priority order is completely arbitrary

    //     if(_colors.valid())
    //     {
    //         geom->setColorArray(_colors.get(), osg::Array::BIND_PER_VERTEX );
    //     }
    //     else if(_ambient.valid())
    //     {
    //         geom->setColorArray(_ambient.get(), osg::Array::BIND_PER_VERTEX );
    //     }
    //     else if(_diffuse.valid())
    //     {
    //         geom->setColorArray(_diffuse.get(), osg::Array::BIND_PER_VERTEX );
    //     }
    //     else if(_specular.valid())
    //     {
    //         geom->setColorArray(_specular.get(), osg::Array::BIND_PER_VERTEX );
    //     }
    //     else if (vTexcoords[i].valid())
    //     {
    //         geom->setTexCoordArray(i, vTexcoords[i].get());
    //     }

    //     // If the model has normals, add them to the geometry
    //     if(!osgNormals->empty())
    //     {
    //         geom->setNormalArray(osgNormals.get(), osg::Array::BIND_PER_VERTEX);
    //     }
    //     else
    //     {   // If not, use the smoothing visitor to generate them
    //         // (quads will be triangulated by the smoothing visitor)
    //         osgUtil::SmoothingVisitor::smooth((*geom), osg::PI/2);
    //     }

    //     // set flags true to activate the vertex buffer object of drawable
    //     geom->setUseVertexBufferObjects(true);

    //     osg::ref_ptr<osg::Image> image;
    //     if (!vTextureFiles.empty() && (image = osgDB::readRefImageFile(vTextureFiles[i])) != NULL)
    //     {
    //         osg::Texture2D *texture = new osg::Texture2D;
    //         texture->setImage(image.get());
    //         texture->setResizeNonPowerOfTwoHint(false);

    //         osg::TexEnv *texenv = new osg::TexEnv;
    //         texenv->setMode(osg::TexEnv::REPLACE);

    //         osg::StateSet *stateset = geom->getOrCreateStateSet();
    //         stateset->setTextureAttributeAndModes(i, texture, osg::StateAttribute::ON);
    //         stateset->setTextureAttribute(0, texenv);

    //         // osg::PolygonMode* polymode = new osg::PolygonMode;
    //         // polymode->setMode(osg::PolygonMode::FRONT_AND_BACK,osg::PolygonMode::LINE);
    //         // stateset->setAttributeAndModes(polymode,osg::StateAttribute::OVERRIDE|osg::StateAttribute::ON);
    //     }
    //     geode->addDrawable(geom);
    // }
        
    return geode;
}

    } // end of namespace ply
} // end of namespace HG
