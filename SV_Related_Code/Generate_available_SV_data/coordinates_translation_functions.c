#include "utils.h"
#include "coordinates_translation_functions.h"

// Function to convert geodetic coordinates to ECEF (Earth-Centered, Earth-Fixed)
void geodetic_to_ecef(const GeodeticPosition* geo, double* ecef_x, double* ecef_y, double* ecef_z) {
    double lat_rad = geo->latitude_deg * DEGREES_TO_RADIANS;
    double lon_rad = geo->longitude_deg * DEGREES_TO_RADIANS;
    double N = EARTH_RADIUS_WGS84 / sqrt(1 - (2 * EARTH_FLATTENING_WGS84 - EARTH_FLATTENING_WGS84 * EARTH_FLATTENING_WGS84) * sin(lat_rad) * sin(lat_rad));

    *ecef_x = (N + geo->altitude_km) * cos(lat_rad) * cos(lon_rad);
    *ecef_y = (N + geo->altitude_km) * cos(lat_rad) * sin(lon_rad);
    *ecef_z = (N * (1 - (2 * EARTH_FLATTENING_WGS84 - EARTH_FLATTENING_WGS84 * EARTH_FLATTENING_WGS84)) + geo->altitude_km) * sin(lat_rad);
}

// Function to convert ECI (Earth-Centered Inertial) to ECEF coordinates
void eci_to_ecef(const double* eci_pos, double julian_date, double* ecef_pos) {
    // Julian Date to Greenwich Sidereal Time (GST)
    double jd = julian_date;
    double MJD_J2000 = jd - 2451545.0; // Modified Julian Date J2000
    double sidereal_time = 280.46061837 + 360.98564736629 * MJD_J2000;
    double lon_rad = fmod(sidereal_time, 360.0) * DEGREES_TO_RADIANS;
    
    // Rotation matrix for ECI to ECEF
    double cos_lon = cos(lon_rad);
    double sin_lon = sin(lon_rad);

    ecef_pos[0] = eci_pos[0] * cos_lon + eci_pos[1] * sin_lon;
    ecef_pos[1] = -eci_pos[0] * sin_lon + eci_pos[1] * cos_lon;
    ecef_pos[2] = eci_pos[2];
}

// Function to convert ECEF to topocentric coordinates (azimuth, elevation, range)
void ecef_to_topo(const double* observer_ecef, const double* satellite_ecef, const GeodeticPosition* observer_geo, double* azimuth, double* elevation, double* range) {
    double rx = satellite_ecef[0] - observer_ecef[0];
    double ry = satellite_ecef[1] - observer_ecef[1];
    double rz = satellite_ecef[2] - observer_ecef[2];

    double lat_rad = observer_geo->latitude_deg * DEGREES_TO_RADIANS;
    double lon_rad = observer_geo->longitude_deg * DEGREES_TO_RADIANS;

    double topoS[3];
    topoS[0] = -sin(lat_rad) * cos(lon_rad) * rx - sin(lat_rad) * sin(lon_rad) * ry + cos(lat_rad) * rz;
    topoS[1] = -sin(lon_rad) * rx + cos(lon_rad) * ry;
    topoS[2] = cos(lat_rad) * cos(lon_rad) * rx + cos(lat_rad) * sin(lon_rad) * ry + sin(lat_rad) * rz;

    *range = sqrt(topoS[0]*topoS[0] + topoS[1]*topoS[1] + topoS[2]*topoS[2]);
    *azimuth = atan2(topoS[1], topoS[0]);
    *elevation = asin(topoS[2] / *range);

    *azimuth /= DEGREES_TO_RADIANS;
    *elevation /= DEGREES_TO_RADIANS;
    if (*azimuth < 0) *azimuth += 360.0;
}

// Helper function to convert a Unix timestamp to a Julian date
double unix_to_julian(time_t unix_time) {
    return (double)unix_time / SECONDS_PER_DAY + 2440587.5;
}