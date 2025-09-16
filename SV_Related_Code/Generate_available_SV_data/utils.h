
// Header guard
#ifndef UTILS_H
#define UTILS_H

// Minimum required libraries
#include <time.h>

// Constants for coordinate transformations
#define PI 3.14159265358979323846
#define DEGREES_TO_RADIANS (PI / 180.0)
#define MINUTES_PER_DAY 1440.0
#define SECONDS_PER_DAY 86400.0
#define EARTH_RADIUS_WGS84 6378.137 // km
#define EARTH_FLATTENING_WGS84 (1.0 / 298.257223563)
#define EARTH_OMEGA_RAD_PER_SEC 7.2921151467e-5 // rad/s

// Struct to hold the TLE data for a single satellite
typedef struct {
    char name[25];
    char line1[70];
    char line2[70];
    sat_t sgp4_sat; // SGP4 library's satellite data structure
} TLE;

// Struct to hold observer's geodetic location
typedef struct {
    double latitude_deg;
    double longitude_deg;
    double altitude_km;
} GeodeticPosition;

// Struct to store a single visibility pass
typedef struct {
    char name[25];
    time_t start_time;
} Pass;


#endif // UTILS_H