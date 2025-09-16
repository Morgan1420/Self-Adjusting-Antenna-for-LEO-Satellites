// Header guard
#ifndef DOWNLOAD_AND_PARSE_TLE_FUNCTIONS_H
#define DOWNLOAD_AND_PARSE_TLE_FUNCTIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

// Function prototypes
char* download_tles(const char* url);
int parse_tle_string(const char* data, ownTLE** tles);


// Struct to store data received from the cURL callback
typedef struct {
    char *data;
    size_t size;
} MemoryStruct;


#endif // DOWNLOAD_AND_PARSE_TLE_FUNCTIONS_H