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


int main(int argc, char *argv[]) {
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

        // 4) Filter passes with the data sent as arguments
        if (argc == 6){ 
            int filter_exclude_allready_visible = atoi(argv[1]);
            int filter_exclude_DTC = atoi(argv[2]); 
            double filter_min_elevation = atof(argv[3]);
            int sort_by_option = atoi(argv[4]);
            double number_of_SV_to_display = atof(argv[5]);


            Pass* filtered_passes = NULL;
            int filtered_count = 0;
            time_t now = time(NULL);

            // 4.1) Apply filtering criteria for each detected pass
            for (int j = 0; j < pass_count; j++) {

                // See if the satellite is a DTC (Direct to Cell) satellite by checking its name
                char satellite_name[100];
                strcpy(satellite_name, passes[j].name);
                trim_whitespace(satellite_name);
                char *dtc_ptr = strrchr(satellite_name, '[');
                bool has_DTC = (dtc_ptr != NULL && strstr(dtc_ptr, "DTC") != NULL);

                // Convert pass start and end times back to unix time for comparison
                struct tm pass_start_tm;
                strptime(passes[j].start_time, "%Y-%m-%d %H:%M:%S", &pass_start_tm);
                time_t pass_start_unix = mktime(&pass_start_tm);

                struct tm pass_end_tm;
                strptime(passes[j].end_time, "%Y-%m-%d %H:%M:%S", &pass_end_tm);
                time_t pass_end_unix = mktime(&pass_end_tm);

                // Check if the pass is currently ongoing
                bool is_currently_visible = (now >= pass_start_unix && now <= pass_end_unix);

                // Apply filtering criteria
                if ((filter_exclude_allready_visible && is_currently_visible) ||
                    (filter_exclude_DTC && has_DTC) ||
                    (passes[j].max_elevation < filter_min_elevation)) {
                    continue; // Skip this pass
                }

                // If the pass passes all filters, add it to the filtered list
                filtered_passes = realloc(filtered_passes, (filtered_count + 1) * sizeof(Pass));
                if (filtered_passes == NULL) {
                    fprintf(stderr, "Memory reallocation failed during filtering. Exiting.\n");
                    free(satellites);
                    free(passes);
                    return 1;
                }
                filtered_passes[filtered_count++] = passes[j];
            }

            // 4.2) Sort the filtered passes based on the selected option
            switch (sort_by_option) {
                case 1: // Sort by elevation (highest first)
                    for (int m = 0; m < filtered_count - 1; m++) {
                        for (int n = m + 1; n < filtered_count; n++) {
                            if (filtered_passes[m].max_elevation < filtered_passes[n].max_elevation) {
                                Pass temp = filtered_passes[m];
                                filtered_passes[m] = filtered_passes[n];
                                filtered_passes[n] = temp;
                            }
                        }
                    }
                    break;
                case 2: // Sort by start time (earliest first)
                    for (int m = 0; m < filtered_count - 1; m++) {
                        for (int n = m + 1; n < filtered_count; n++) {
                            struct tm tm_m, tm_n;
                            strptime(filtered_passes[m].start_time, "%Y-%m-%d %H:%M:%S", &tm_m);
                            strptime(filtered_passes[n].start_time, "%Y-%m-%d %H:%M:%S", &tm_n);
                            time_t time_m = mktime(&tm_m);
                            time_t time_n = mktime(&tm_n);
                            if (time_m > time_n) {
                                Pass temp = filtered_passes[m];
                                filtered_passes[m] = filtered_passes[n];
                                filtered_passes[n] = temp;
                            }
                        }
                    }
                    break;
                case 3: // Sort by longer duration (longest first)
                    for (int m = 0; m < filtered_count - 1; m++) {
                        for (int n = m + 1; n < filtered_count; n++) {
                            struct tm start_tm_m, end_tm_m, start_tm_n, end_tm_n;
                            strptime(filtered_passes[m].start_time, "%Y-%m-%d %H:%M:%S", &start_tm_m);
                            strptime(filtered_passes[m].end_time, "%Y-%m-%d %H:%M:%S", &end_tm_m);
                            strptime(filtered_passes[n].start_time, "%Y-%m-%d %H:%M:%S", &start_tm_n);
                            strptime(filtered_passes[n].end_time, "%Y-%m-%d %H:%M:%S", &end_tm_n);
                            time_t duration_m = mktime(&end_tm_m) - mktime(&start_tm_m);
                            time_t duration_n = mktime(&end_tm_n) - mktime(&start_tm_n);
                            if (duration_m < duration_n) {
                                Pass temp = filtered_passes[m];
                                filtered_passes[m] = filtered_passes[n];
                                filtered_passes[n] = temp;
                            }
                        }
                    }
                    break;
            }

            // 4.3) Print the filtered and sorted passes, limited to the specified number
            int display_count = (number_of_SV_to_display > 0 && number_of_SV_to_display < filtered_count) ? (int)number_of_SV_to_display : filtered_count;
            for (int j = 0; j < display_count; j++) {
                printf("Satellite: %s, Start: %s, End: %s, Max Elevation: %.2f deg\n",
                       filtered_passes[j].name, filtered_passes[j].start_time, filtered_passes[j].end_time, filtered_passes[j].max_elevation);
            }

            // Clean up
            free(filtered_passes);

        }else if(argc == 0)
        {        
            // No filtering criteria provided, default behaviour (Show 10 passes unfiltered)
            pass_count = 10;
            for (int j = 0; j < pass_count; j++) {
                printf("Satellite: %s, Start: %s, End: %s, Max Elevation: %.2f deg\n",
                       passes[j].name, passes[j].start_time, passes[j].end_time, passes[j].max_elevation);
            }   

        }else{
            // Incorrect number of arguments, notify user and continue with default behaviour (Show 10 passes unfiltered)
            printf("Incorrect number of arguments. Expected 5 arguments for filtering criteria.\n");
            
            pass_count = 10; 
            for (int j = 0; j < pass_count; j++) {
                printf("Satellite: %s, Start: %s, End: %s, Max Elevation: %.2f deg\n",
                       passes[j].name, passes[j].start_time, passes[j].end_time, passes[j].max_elevation);
            } 
        }
        
        // Clean up
        free(passes);

    }

    // Clean up
    free(satellites);
    
    
    return 0;
}