#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <curl/curl.h>

// Link self created libraries
#include "SGP4_library/libSGP4.h"

#include "utils.h"
#include "download_and_parse_TLE_functions.h"


// This program requires the SGP4 library and the cURL library.
// For example, on a Debian-based system, you might install them with:
// sudo apt-get install libcurl4-openssl-dev libsgp4-dev
//
// Then, compile the program with a command like this:
// gcc -o skyplot_generator skyplot_data_generator.c -lcurl -lsgp4 -lm

// gcc -o main main.c download_and_parse_TLE_functions.c SGP4.c TLE.c -lcurl -lm && ./main


int main() {
    // The new URL for Starlink TLEs
    const char* url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=starlink&FORMAT=tle";

    printf("Downloading Starlink TLE data from %s...\n", url);
    char* tle_data_string = download_tles(url);
    if (!tle_data_string) {
        fprintf(stderr, "Failed to download TLE data. Exiting.\n");
        return 1;
    }

    TLE* satellites = NULL;
    int num_satellites = parse_tle_string(tle_data_string, &satellites);
    free(tle_data_string);

    printf("Successfully parsed %d Starlink TLEs.\n", num_satellites);
    printf("--------------------------------------------------\n");

    
    
    return 0;
}