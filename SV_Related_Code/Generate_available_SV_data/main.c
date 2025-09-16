#include "utils.h"
#include "download_and_parse_TLE_functions.h"

// #include "SGP4_library/SGP4.h" // Allready included in utils.h


// The following command compiles and executes the code
// gcc -o main main.c download_and_parse_TLE_functions.c -lcurl -L./SGP4_library -lSGP4 -lm && ./main


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

    if (num_satellites <= 0) {
        printf("No satellites to process.\n");
    } else {
        // For testing, we only process the first satellite
        num_satellites = 1;
        for(int i = 0; i < num_satellites; ++i) {
            // Debugging information
            printf("Satellite %d: %s\n", i + 1, satellites[i].name);
            printf("  Line 1: %s\n", satellites[i].tle.line1);
            printf("  Line 2: %s\n", satellites[i].tle.line2);
            printf("--------------------------------------------------\n");
            

            // Set operation mode ('a' = AFSPC, 'i' = improved)
            char opsmode = 'i';
            // Initialize SGP4
            if (!sgp4init(opsmode, &satellites[i].tle.rec)) {
                fprintf(stderr, "SGP4 initialization failed.\n");
                free(satellites);
                return 1;
            }

            printf("Beginning satellite propagation for the first satellite...\n");
            for (int i = 0; i < 60; ++i) { // 60 minutes, 1 minute step
                double tsince = i; // minutes since epoch
                double r[3], v[3];
                if (sgp4(&satellites[i].tle.rec, tsince, r, v)) {
                    printf("Time: %f min, Position: (%f, %f, %f), Velocity: (%f, %f, %f)\n", tsince, r[0], r[1], r[2], v[0], v[1], v[2]);
                } else {
                    printf("SGP4 propagation error at time %f min.\n", tsince);
                }
            }
        
        }
        
    }

    // Clean up
    free(satellites);
    
    
    return 0;
}