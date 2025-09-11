
// Header guard
#ifndef UTILS_H
#define UTILS_H

// Struct to hold the TLE data for a single satellite
typedef struct {
    char name[25];
    char line1[70];
    char line2[70];
} TLE;

#endif // UTILS_H