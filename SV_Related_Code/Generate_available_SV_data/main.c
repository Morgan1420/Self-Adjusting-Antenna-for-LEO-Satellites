#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <curl/curl.h>

// Link self created libraries
#include "SGP4_library/libSGP4.h"

#include "utils.h"
#include "download_and_parse_TLE_functions.h"


// The following command compiles and executes the code
// gcc -o main main.c download_and_parse_TLE_functions.c -lcurl -lm -L./SGP4_library -lSGP4 && ./main


int main() {
    // The new URL for Starlink TLEs
    const char* url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=starlink&FORMAT=tle";

    printf("Downloading Starlink TLE data from %s...\n", url);
    char* tle_data_string = download_tles(url);
    if (!tle_data_string) {
        fprintf(stderr, "Failed to download TLE data. Exiting.\n");
        return 1;
    }

    ownTLE* satellites = NULL;
    int num_satellites = parse_tle_string(tle_data_string, &satellites);
    free(tle_data_string);

    printf("Successfully parsed %d Starlink TLEs.\n", num_satellites);
    printf("--------------------------------------------------\n");

    
    
    return 0;
}