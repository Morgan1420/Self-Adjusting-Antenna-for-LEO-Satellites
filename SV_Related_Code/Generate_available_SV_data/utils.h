
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

// Struct to hold the TLE data for a single satellite
typedef struct {
    char name[25];
    char line1[70];
    char line2[70];
} ownTLE;

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