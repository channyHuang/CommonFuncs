#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <array>
#include <unordered_map>

#include <Eigen/Eigen>

namespace Haige {

    std::array<double, 16> computeTransformFromEnuOrigin(
        double lon_deg,     // 经度 (度)
        double lat_deg,     // 纬度 (度) 
        double alt,         // 高度 (米)
        bool high_precision = true
    );

// WGS84 椭球参数
struct WGS84 {
    static constexpr double a = 6378137.0;               // 长半轴 (米)
    static constexpr double f = 1.0 / 298.257223563;     // 扁率
    static constexpr double b = a * (1.0 - f);           // 短半轴 (米)
    static constexpr double e2 = (a*a - b*b) / (a*a);    // 第一偏心率的平方
    static constexpr double e4 = e2 * e2;                // e^4
    static constexpr double e6 = e4 * e2;                // e^6
};

class GISTranslate {
public:
    GISTranslate(double fLon, double fLat, double fAlt) 
        : m_fLatDeg(fLat), m_fLonDeg(fLon), m_fAlt(fAlt) {
            initialize();
        }

    Eigen::Vector3d gps2enuModelCoord(double fLonDeg, double fLatDeg, double fAlt);
    Eigen::Vector3d enu2gpsModelCoord(double x, double y, double z);

    void initialize();

public:
    // enu (0, 0, 0) -> GPS
    double m_fLonDeg = 0;
    double m_fLatDeg = 0;
    double m_fAlt = 0;

    std::array<double, 3> ecef;
    std::array<double, 9> rotation_matrix; // ecef -> enu
    bool bInitlized = false;
};

}; // end of namespace Haige
