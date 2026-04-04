#include "utils.h"
#include "coordinates_translation_functions.h"

// Function to convert geodetic coordinates to ECEF (Earth-Centered, Earth-Fixed)
void geodetic_to_ecef(const GeodeticPosition* geo, double* ecef_x, double* ecef_y, double* ecef_z) {
    double lat_rad = geo->latitude_deg * DEGREES_TO_RADIANS;
    double lon_rad = geo->longitude_deg * DEGREES_TO_RADIANS;
    
    // Radius of curvature in prime vertical (N)
    double N = EARTH_RADIUS_WGS84 / sqrt(1.0 - EARTH_ECCENTRICITY_SQUARED * sin(lat_rad) * sin(lat_rad));

    *ecef_x = (N + geo->altitude_km) * cos(lat_rad) * cos(lon_rad);
    *ecef_y = (N + geo->altitude_km) * cos(lat_rad) * sin(lon_rad);
    *ecef_z = (N * (1.0 - EARTH_ECCENTRICITY_SQUARED) + geo->altitude_km) * sin(lat_rad);
}

// Function to convert ECI (Earth-Centered Inertial) to ECEF coordinates
void eci_to_ecef(const double* eci_pos, double julian_date, double* ecef_pos) {
    // Julian Date to Greenwich Sidereal Time (GST) - GMST calculation per USNO standards
    double jd = julian_date;
    double T_UT1 = (jd - 2451545.0) / 36525.0; // Julian centuries from J2000.0
    double gmst_seconds = 67310.54841 + (876600.0 * 3600.0 + 8640184.812866) * T_UT1 
                         + 0.093104 * T_UT1 * T_UT1 - 6.2e-6 * T_UT1 * T_UT1 * T_UT1;
    double gmst_degrees = fmod(gmst_seconds / 240.0, 360.0);
    if (gmst_degrees < 0) gmst_degrees += 360.0;
    double lon_rad = gmst_degrees * DEGREES_TO_RADIANS;
    
    // Rotation matrix for ECI to ECEF (rotation about z-axis by GMST)
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

    // Transformation to NEZ (North-East-Zenith) frame
    double topoS[3];
    topoS[0] = -sin(lat_rad) * cos(lon_rad) * rx - sin(lat_rad) * sin(lon_rad) * ry + cos(lat_rad) * rz;  // North
    topoS[1] = -sin(lon_rad) * rx + cos(lon_rad) * ry;                                                      // East
    topoS[2] = cos(lat_rad) * cos(lon_rad) * rx + cos(lat_rad) * sin(lon_rad) * ry + sin(lat_rad) * rz;   // Zenith

    *range = sqrt(topoS[0]*topoS[0] + topoS[1]*topoS[1] + topoS[2]*topoS[2]);
    
    // Azimuth: measured from North (0 deg) through East (90 deg), South (180 deg), West (270 deg)
    // In this NEZ frame, we use atan2(East, North)
    *azimuth = atan2(topoS[1], topoS[0]);
    
    // Elevation: angle above horizon
    *elevation = asin(topoS[2] / *range);

    // Convert from radians to degrees
    *azimuth = *azimuth / DEGREES_TO_RADIANS;
    *elevation = *elevation / DEGREES_TO_RADIANS;
    
    // Ensure azimuth is in [0, 360) range
    if (*azimuth < 0) *azimuth += 360.0;
}

// Helper function to convert a Unix timestamp to a Julian date
double unix_to_julian(time_t unix_time) {
    return (double)unix_time / SECONDS_PER_DAY + 2440587.5;
}