#include "utils.h"
#include "download_and_parse_TLE_functions.h"
#include "coordinates_translation_functions.h"

// #include "SGP4_library/SGP4.h" // Allready included in utils.h


// The following command compiles and executes the code
// gcc -o main main.c download_and_parse_TLE_functions.c coordinates_translation_functions.c -lcurl -L./SGP4_library -lSGP4 -lm && ./main

// Helper functions
void trim_whitespace(char *str) {
    // Trim leading whitespace
    char *start = str;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != str) memmove(str, start, strlen(start) + 1);

    // Trim trailing whitespace
    char *end = str + strlen(str) - 1;
    while (end >= str && isspace((unsigned char)*end)) *end-- = '\0';
}

// Define the observer's location (Sant Vicenç dels Horts, Catalonia, Spain)
GeodeticPosition observer = {
    .latitude_deg = 41.393552,
    .longitude_deg = 2.009787,
    .altitude_km = 0.022 // 22 meters
};


int main() {
    // The new URL for Starlink TLEs
    const char* url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=starlink&FORMAT=tle";

    // 1) Download TLE data
    // printf("Downloading Starlink TLE data from %s...\n", url);
    char* tle_data_string = download_tles(url);
    if (!tle_data_string) {
        fprintf(stderr, "Failed to download TLE data. Exiting.\n");
        return 1;
    }

    // 2) Parse the downloaded TLE data
    ownTLE* satellites = NULL;
    int num_satellites = parse_tle_string(tle_data_string, &satellites);
    free(tle_data_string);

    if (num_satellites <= 0) {
        printf("No satellites to process.\n");
    } else {
        // Convert observer's geodetic position to ECEF once
        double observer_ecef[3];
        geodetic_to_ecef(&observer, &observer_ecef[0], &observer_ecef[1], &observer_ecef[2]);

        // Define a list to store satellite passes
        Pass* passes = NULL;
        int pass_count = 0;

        // 3) For each satellite, propagate its position and check for visibility
        for(int i = 0; i < num_satellites; ++i) {
            
            // Set operation mode ('a' = AFSPC, 'i' = improved)
            char opsmode = 'i';

            // 3.1) Initialize SGP4
            if (!sgp4init(opsmode, &satellites[i].tle.rec)) {
                fprintf(stderr, "SGP4 initialization failed.\n");
                free(satellites);
                return 1;
            }

            time_t current_time_unix = time(NULL);
            bool is_in_pass = false;
            time_t pass_start_unix = 0;
            double max_elevation_in_pass = 0.0;

            // 3.2) Simulate for the next 30 minutes in 15-second intervals
            for (double t_min = 0; t_min < 30; t_min += 0.25) { // 30 minutes, 0.25 minute step
                double tsince = t_min; // minutes since epoch

                double sat_pos_eci[3], sat_vel_eci[3];

                double current_julian_date = unix_to_julian(current_time_unix + (time_t)(t_min * 60));

                int error_code = sgp4(&satellites[i].tle.rec, tsince, sat_pos_eci, sat_vel_eci);
                if (error_code >= 0) {
                    double sat_pos_ecef[3];
                    eci_to_ecef(sat_pos_eci, current_julian_date, sat_pos_ecef);
                    
                    double azimuth, elevation, range;
                    ecef_to_topo(observer_ecef, sat_pos_ecef, &observer, &azimuth, &elevation, &range);

                    if (elevation > 0) {
                        if (!is_in_pass) {
                            is_in_pass = true;
                            pass_start_unix = current_time_unix + (time_t)(t_min * 60);
                            max_elevation_in_pass = elevation;
                        } else {
                            if (elevation > max_elevation_in_pass) {
                                max_elevation_in_pass = elevation;
                            }
                        }
                    } else {
                        if (is_in_pass) {
                            is_in_pass = false;
                            time_t pass_end_unix = current_time_unix + (time_t)(t_min * 60);

                            passes = realloc(passes, (pass_count + 1) * sizeof(Pass));
                            if (passes == NULL) {
                                fprintf(stderr, "Memory reallocation failed. Exiting.\n");
                                free(satellites);
                                return 1;
                            }

                            strcpy(passes[pass_count].name, satellites[i].name);
                            trim_whitespace(passes[pass_count].name);

                            struct tm *start_tm = localtime(&pass_start_unix);
                            strftime(passes[pass_count].start_time, sizeof(passes[pass_count].start_time) - 1, "%Y-%m-%d %H:%M:%S", start_tm);

                            struct tm *end_tm = localtime(&pass_end_unix);
                            strftime(passes[pass_count].end_time, sizeof(passes[pass_count].end_time) - 1, "%Y-%m-%d %H:%M:%S", end_tm);
                            passes[pass_count].max_elevation = max_elevation_in_pass;
                            
                            pass_count++;
                        }
                    }

                    // Debug output for each time step
                    //printf("Time: %f min, Azimuth: %f deg, Elevation: %f deg, Range: %f km, ", tsince, azimuth, elevation, range);
                    //printf("Is in pass: %s\n", is_in_pass ? "Yes" : "No");
                } else {
                    printf("SGP4 propagation error at time %f min.\n", tsince);
                    free(satellites);
                    return 1;
                }
            }
        
        }

        // Print all detected passes
        //printf("Detected %d passes:\n", pass_count);
        pass_count = 10; // For testing, limit to first 10 passes
        for (int j = 0; j < pass_count; j++) {
            printf("Satellite: %s, Start: %s, End: %s, Max Elevation: %.2f deg\n",
                   passes[j].name, passes[j].start_time, passes[j].end_time, passes[j].max_elevation);
        }   
        
    }

    // Clean up
    free(satellites);
    
    
    return 0;
}