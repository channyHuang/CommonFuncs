#include "Gis.h"

#include <cmath>
#include <vector>
#include <algorithm>

#include <GeographicLib/LocalCartesian.hpp>
#include <GeographicLib/Geocentric.hpp>

namespace Haige {
    // 角度转弧度
    constexpr double deg2rad(double deg) {
        return deg * M_PI / 180.0;
    }

    // 从经纬高计算 ECEF 坐标（高精度）
    std::array<double, 3> gpsToEcef(double lon_deg, double lat_deg, double alt) {
        // 转换为弧度
        double lon_rad = deg2rad(lon_deg);
        double lat_rad = deg2rad(lat_deg);
        
        // 预计算三角函数
        double sin_lat = sin(lat_rad);
        double cos_lat = cos(lat_rad);
        double sin_lon = sin(lon_rad);
        double cos_lon = cos(lon_rad);
        
        double sin2_lat = sin_lat * sin_lat;
        double sin4_lat = sin2_lat * sin2_lat;
        double sin6_lat = sin4_lat * sin2_lat;
        
        // 计算卯酉圈曲率半径 N（高精度）
        // N = a / sqrt(1 - e² * sin²(lat))
        double denom = 1.0 - WGS84::e2 * sin2_lat;
        
        // 更高精度的计算，避免精度损失
        if (denom < 1e-12) denom = 1e-12;
        double sqrt_denom = sqrt(denom);
        double N = WGS84::a / sqrt_denom;
        
        // 更高精度的 N 计算（可选，用于极点附近）
        // 使用级数展开提高精度
        double N_high_precision = WGS84::a * (1.0 
            + WGS84::e2/2.0 * sin2_lat 
            + 3.0/8.0 * WGS84::e4 * sin4_lat
            + 5.0/16.0 * WGS84::e6 * sin6_lat);
        
        // 选择更高精度的值（在高纬度地区更准确）
        if (fabs(lat_deg) > 60.0) {
            N = N_high_precision;
        }
        
        // 计算 ECEF 坐标
        double N_plus_h = N + alt;
        double N_cos_lat = N_plus_h * cos_lat;
        
        std::array<double, 3> ecef;
        ecef[0] = N_cos_lat * cos_lon;                   // X
        ecef[1] = N_cos_lat * sin_lon;                   // Y
        
        // 计算 Z 时考虑椭球
        double N_ellipsoid = (WGS84::b * WGS84::b) / (WGS84::a * sqrt_denom);
        ecef[2] = (N_ellipsoid + alt) * sin_lat;         // Z
        
        return ecef;
    }

    // 计算 ENU 到 ECEF 的旋转矩阵
    std::array<double, 9> computeEnuToEcefRotation(double lon_rad, double lat_rad) {
        double sin_lat = sin(lat_rad);
        double cos_lat = cos(lat_rad);
        double sin_lon = sin(lon_rad);
        double cos_lon = cos(lon_rad);
        
        // ENU 基向量在 ECEF 中的表示
        // 东方向向量
        double ex = -sin_lon;
        double ey = cos_lon;
        double ez = 0.0;
        
        // 北方向向量
        double nx = -sin_lat * cos_lon;
        double ny = -sin_lat * sin_lon;
        double nz = cos_lat;
        
        // 天方向向量
        double ux = cos_lat * cos_lon;
        double uy = cos_lat * sin_lon;
        double uz = sin_lat;
        
        // 返回 3x3 旋转矩阵（行优先）
        return {ex, nx, ux,
                ey, ny, uy,
                ez, nz, uz};
    }

    void GISTranslate::initialize() {
        ecef = gpsToEcef(m_fLonDeg, m_fLatDeg, m_fAlt);
        double fLonRad = deg2rad(m_fLonDeg);
        double fLatRad = deg2rad(m_fLatDeg);
        rotation_matrix = computeEnuToEcefRotation(fLonRad, fLatRad);
        bInitlized = true;
    }

    Eigen::Vector3d GISTranslate::gps2enuModelCoord(double fLonDeg, double fLatDeg, double fAlt) {
        GeographicLib::LocalCartesian enu(m_fLatDeg, m_fLonDeg, m_fAlt);
        double x, y, z;
        enu.Forward(fLatDeg, fLonDeg, fAlt, x, y, z);
        return Eigen::Vector3d(x, y, z);
    }

    Eigen::Vector3d GISTranslate::enu2gpsModelCoord(double x, double y, double z) {
        GeographicLib::LocalCartesian enu(m_fLatDeg, m_fLonDeg, m_fAlt);
        double fLonDeg, fLatDeg, fAlt;
        enu.Reverse(x, y, z, fLatDeg, fLonDeg, fAlt);  
        return Eigen::Vector3d(fLonDeg, fLatDeg, fAlt);
    }




// 主函数：从 ENU 原点 GPS 计算 3D Tiles transform
std::array<double, 16> computeTransformFromEnuOrigin(
    double lon_deg,     // 经度 (度)
    double lat_deg,     // 纬度 (度) 
    double alt,         // 高度 (米)
    bool high_precision
) {
    // 1. 转换为弧度
    double lon_rad = deg2rad(lon_deg);
    double lat_rad = deg2rad(lat_deg);
    
    // 2. 计算 ECEF 坐标
    std::array<double, 3> ecef_origin;
    if (high_precision) {
        ecef_origin = gpsToEcef(lon_deg, lat_deg, alt);
    } else {
        // 快速近似（球体）
        double lon_rad = deg2rad(lon_deg);
        double lat_rad = deg2rad(lat_deg);
        double R = 6371000.0 + alt;  // 地球平均半径 + 高度
        
        ecef_origin[0] = R * cos(lat_rad) * cos(lon_rad);
        ecef_origin[1] = R * cos(lat_rad) * sin(lon_rad);
        ecef_origin[2] = R * sin(lat_rad);
    }
    
    // 3. 计算旋转矩阵
    auto rotation = computeEnuToEcefRotation(lon_rad, lat_rad);
    
    // 4. 构建 3D Tiles transform 矩阵（4x4，列主序）
    std::array<double, 16> transform;
    
    // 旋转部分 (3x3) - 注意：需要转置，因为computeEnuToEcefRotation返回行优先
    // 3D Tiles 使用列主序，所以我们的 rotation 矩阵的列应该对应 transform 的行
    // 实际上 rotation 矩阵已经是 ENU->ECEF，直接按列放入即可
    
    transform[0] = rotation[0];   // R[0][0] - 列0行0
    transform[1] = rotation[3];   // R[1][0] - 列0行1
    transform[2] = rotation[6];   // R[2][0] - 列0行2
    transform[3] = 0.0;
    
    transform[4] = rotation[1];   // R[0][1] - 列1行0
    transform[5] = rotation[4];   // R[1][1] - 列1行1
    transform[6] = rotation[7];   // R[2][1] - 列1行2
    transform[7] = 0.0;
    
    transform[8] = rotation[2];   // R[0][2] - 列2行0
    transform[9] = rotation[5];   // R[1][2] - 列2行1
    transform[10] = rotation[8];  // R[2][2] - 列2行2
    transform[11] = 0.0;
    
    // 平移部分
    transform[12] = ecef_origin[0];  // X
    transform[13] = ecef_origin[1];  // Y
    transform[14] = ecef_origin[2];  // Z
    transform[15] = 1.0;
    
    return transform;
}


}
