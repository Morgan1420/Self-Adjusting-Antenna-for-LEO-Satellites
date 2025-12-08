
// Header guard
#ifndef UTILS_H
#define UTILS_H

// Minimum required libraries
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <curl/curl.h>
#include <ctype.h>


// Link self created libraries
#include "../SGP4_library/libSGP4.h"

// Constants for coordinate transformations
#define PI 3.14159265358979323846
#define DEGREES_TO_RADIANS (PI / 180.0)
#define MINUTES_PER_DAY 1440.0
#define SECONDS_PER_DAY 86400.0
#define EARTH_RADIUS_WGS84 6378.137 // km
#define EARTH_FLATTENING_WGS84 (1.0 / 298.257223563)


// Struct to hold the TLE data for a single satellite
typedef struct {
    char name[25];
    TLE tle; // Include the TLE struct from libSGP4.h
} ownTLE;

// Struct to hold observer's geodetic location
typedef struct {
    double latitude_deg;
    double longitude_deg;
    double altitude_km;
} GeodeticPosition;

// Struct to store a single visibility pass
typedef struct {
    char name[50];
    char start_time[50];
    char end_time[50];
    double max_elevation;
} Pass;


#endif // UTILS_H