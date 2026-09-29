#ifndef GEOUTILS_HPP
#define GEOUTILS_HPP

#include <cmath>
#include <algorithm>

namespace GeoUtils {
inline constexpr double pi = 3.14159265358979323846;

inline double distance(double lat1, double lon1, double lat2, double lon2) {
    constexpr double R = 111319.5;
    double dx = (lon1 - lon2) * std::cos((lat1 + lat2) * pi / 360.0);
    double dy = lat1 - lat2;
    return std::sqrt(dx * dx + dy * dy) * R;
}

inline double turnAngle(double lat1, double lon1,
                        double lat2, double lon2,
                        double lat3, double lon3) {
    double v1x = lon2 - lon1;
    double v1y = lat2 - lat1;
    double v2x = lon3 - lon2;
    double v2y = lat3 - lat2;

    double len1 = std::sqrt(v1x * v1x + v1y * v1y);
    double len2 = std::sqrt(v2x * v2x + v2y * v2y);

    if (len1 < 1e-10 || len2 < 1e-10) return 0.0;

    v1x /= len1; v1y /= len1;
    v2x /= len2; v2y /= len2;

    double dot = v1x * v2x + v1y * v2y;
    dot = std::max(-1.0, std::min(1.0, dot));

    return std::acos(dot) * 180.0 / pi;
}

// Convert air distance offset to lat/lon deltas
inline void offsetToLatLon(double start_lat, double distance_m, double angle_rad,
                           double& dlat, double& dlon) {
    constexpr double R = 111319.5;
    dlat = (distance_m * std::cos(angle_rad)) / R;
    dlon = (distance_m * std::sin(angle_rad)) / (R * std::cos(start_lat * pi / 180.0));
}

}

#endif
