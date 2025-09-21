#include "utils.h"
#include "download_and_parse_TLE_functions.h"
#include "coordinates_translation_functions.h"

// #include "SGP4_library/SGP4.h" // Allready included in utils.h


// The following command compiles and executes the code
// gcc -o main main.c download_and_parse_TLE_functions.c -lcurl -L./SGP4_library -lSGP4 -lm && ./main

// --------------------------------------------------

// Define the observer's location (Sant Vicenç dels Horts, Catalonia, Spain)
GeodeticPosition observer = {
    .latitude_deg = 41.393552,
    .longitude_deg = 2.009787,
    .altitude_km = 0.022 // 22 meters
};

// --------------------------------------------------

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
        // Convert observer's geodetic position to ECEF once
        double observer_ecef[3];
        geodetic_to_ecef(&observer, &observer_ecef[0], &observer_ecef[1], &observer_ecef[2]);

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
            for (double t = 0; t < 30; t += 0.25) { // 30 minutes, 0.5 minute step
                double tsince = t; // minutes since epoch
                double sat_pos_eci[3], sat_vel_eci[3];
                if (sgp4(&satellites[i].tle.rec, tsince, sat_pos_eci, sat_vel_eci)) {
                    // Convert ECI to ECEF
                    double julian_date = unix_to_julian(satellites[i].tle.rec.jdsatepoch + (tsince / 1440.0)); // tsince in days
                    double sat_pos_ecef[3];
                    eci_to_ecef(sat_pos_eci, julian_date, sat_pos_ecef);

                    // Convert ECEF to topocentric coordinates
                    double azimuth, elevation, range;
                    ecef_to_topo(observer_ecef, sat_pos_ecef, &observer, &azimuth, &elevation, &range);

                    printf("Time: %f min, Azimuth: %f deg, Elevation: %f deg, Range: %f km\n", tsince, azimuth, elevation, range);
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