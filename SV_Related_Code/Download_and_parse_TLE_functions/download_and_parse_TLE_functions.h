// Header guard
#ifndef DOWNLOAD_AND_PARSE_TLE_FUNCTIONS_H
#define DOWNLOAD_AND_PARSE_TLE_FUNCTIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "../utils.h"

// Function prototypes
char* download_tles(const char* url);
int parse_tle_string(const char* data, TLE** tles);

// Define PI for calculations
#ifndef PI
#define PI 3.14159265358979323846
#endif

// Struct to store data received from the cURL callback
typedef struct {
    char *data;
    size_t size;
} MemoryStruct;


#endif // DOWNLOAD_AND_PARSE_TLE_FUNCTIONS_H