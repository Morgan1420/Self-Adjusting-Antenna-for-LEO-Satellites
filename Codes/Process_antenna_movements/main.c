
// --- CONFIGURATION ---
#include <unistd.h>
#include <limits.h>

#include "utils.h"
#include "download_and_parse_TLE_functions.h"
#include "coordinates_translation_functions.h"
#include "arduino_communications.h"

// Compile with:
// gcc -o main main.c download_and_parse_TLE_functions.c coordinates_translation_functions.c arduino_communications.c -lcurl -L./../SGP4_library -lSGP4 -lm

// The command to send to the Arduino. 'ON' will tell it to turn on, 'OFF' to turn off.
#define COMMAND_ON "ON\n"
#define COMMAND_OFF "OFF\n"

// URLs for different constellations
const char *url_starlink = "https://celestrak.org/NORAD/elements/gp.php?GROUP=starlink&FORMAT=tle";
const char *url_orbcomm = "https://celestrak.org/NORAD/elements/gp.php?GROUP=orbcomm&FORMAT=tle";
const char *url_iridium = "https://celestrak.org/NORAD/elements/gp.php?GROUP=iridium&FORMAT=tle";

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
    .altitude_km = 0.022
};

int calculateOrbit(char* satellite_name, char* constellation_name, char ***arduinoCommands_ptr){
    const char *url = NULL;

    // 1. Find correct URL based on constellation
    switch (constellation_name[0]) {
        case 'S':
        case 's':
            url = url_starlink;
            break;
        case 'O':
        case 'o':
            url = url_orbcomm;
            break;
        case 'I':
        case 'i':
            url = url_iridium;
            break;
        default:
            fprintf(stderr, "Unknown constellation name: %s\n", constellation_name);
            return 1;
    }

    // 2. Download TLE data
    char* tle_data_string = download_tles(url);
    if (!tle_data_string) {
        fprintf(stderr, "Failed to download TLE data.\n");
        return 1;
    }

    // 3. Parse TLE data
    ownTLE* tles = NULL;
    int tle_count = parse_tle_string(tle_data_string, &tles);
    free(tle_data_string); // Free the downloaded string after parsing

    if (tle_count <= 0) {
        fprintf(stderr, "No TLEs parsed from source.\n");
        free(tles);
        return 1;
    }

    // Trim parsed TLE names (remove trailing newlines/spaces) to ensure strcmp matches
    for (int i = 0; i < tle_count; ++i) {
        if (tles[i].name) trim_whitespace(tles[i].name);
    }

    // Prepare a trimmed copy of the queried satellite name
    char sat_name_clean[128];
    strncpy(sat_name_clean, satellite_name, sizeof(sat_name_clean) - 1);
    sat_name_clean[sizeof(sat_name_clean) - 1] = '\0';
    trim_whitespace(sat_name_clean);

    // 4. Locate the specific satellite (case-sensitive match as TLE names usually are)
    ownTLE *selected_tle = NULL;
    for (int i = 0; i < tle_count; ++i) {
        if (tles[i].name && strcmp(tles[i].name, sat_name_clean) == 0) {
            selected_tle = &tles[i];
            break;
        }
    }

    // 5. Check if the satellite was found
    if (!selected_tle) {
        fprintf(stderr, "Satellite %s not found in TLE data.\n", satellite_name);
        free(tles);
        return 1;
    }

    // 6. Set operation mode and initialize SGP4
    char opsmode = 'i'; // improved
    if (!sgp4init(opsmode, &selected_tle->tle.rec)) {
        fprintf(stderr, "SGP4 initialization failed for %s.\n", satellite_name);
        free(tles);
        return 1;
    }

    // 7. Prepare observer position (local site)
    double observer_ecef[3];
    geodetic_to_ecef(&observer, &observer_ecef[0], &observer_ecef[1], &observer_ecef[2]);

    // 8. Propagate for the next 30 minutes at 15-second (0.25 minute) steps and print results
    time_t current_time_unix = time(NULL);
    // Prepare command list (pairs: command, timestamp). We'll return a NULL-terminated char** via arduinoCommands_ptr
    char **cmds = NULL;
    int cmd_count = 0;

    for (double t_min = 0.0; t_min < 30.0; t_min += 0.25) {
        double tsince = t_min; // minutes since epoch

        double sat_pos_eci[3], sat_vel_eci[3];

        // 8.1. Propagate satellite position
        int error_code = sgp4(&selected_tle->tle.rec, tsince, sat_pos_eci, sat_vel_eci);
        if (error_code < 0) {
            fprintf(stderr, "SGP4 propagation error (code %d) at tsince=%.3f min.\n", error_code, tsince);
            free(tles);
            return 1;
        }

        // 8.2.Convert ECI to ECEF for the given epoch/time
        time_t step_time_unix = current_time_unix + (time_t)(t_min * 60.0);
        double jd = unix_to_julian(step_time_unix);
        double sat_pos_ecef[3];
        eci_to_ecef(sat_pos_eci, jd, sat_pos_ecef);

    // 8.3. Convert ECEF to topocentric (azimuth, elevation, range)
    // Note: ecef_to_topo already returns azimuth and elevation in degrees.
    double azimuth_deg = 0.0, elevation_deg = 0.0, range_km = 0.0;
    ecef_to_topo(observer_ecef, sat_pos_ecef, &observer, &azimuth_deg, &elevation_deg, &range_km);

        // Print timestamp (ISO-like), azimuth (deg), elevation (deg) and range (km)
        char timebuf[64];
        struct tm *tm = localtime(&step_time_unix);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm);

        printf("%s, Az: %.2f deg, El: %.2f deg, Range: %.3f km\n", timebuf, azimuth_deg, elevation_deg, range_km);

        // Only generate Arduino commands for visible positions (elevation > 0)
        if (elevation_deg <= 0.0) {
            continue;
        }

        // Round azimuth/elevation to integer degrees
        int az_int = (int)round(azimuth_deg);
        // Normalize az_int to 0..359
        az_int = (az_int % 360 + 360) % 360;
        int el_int = (int)round(elevation_deg);
        if (el_int < 0) el_int = 0;
        if (el_int > 99) el_int = 99; // two digits for elevation in the protocol

        static int prev_az = -9999;
        static int prev_el = -9999;
        static int first = 1;

        char cmdbuf[64];

        if (first) {
            // Initial absolute position command: AXXXEXX (XXX az, XX el)
            snprintf(cmdbuf, sizeof(cmdbuf), "A%03dE%02d", az_int, el_int);

            // push command
            cmds = realloc(cmds, (cmd_count + 1) * sizeof(char*));
            cmds[cmd_count++] = strdup(cmdbuf);
            // push timestamp
            cmds = realloc(cmds, (cmd_count + 1) * sizeof(char*));
            cmds[cmd_count++] = strdup(timebuf);

            prev_az = az_int;
            prev_el = el_int;
            first = 0;
        } else {
            // Azimuth difference; handle wrap-around for minimal rotation
            int delta_az = az_int - prev_az;
            if (delta_az > 180) delta_az -= 360;
            if (delta_az < -180) delta_az += 360;

            if (delta_az != 0) {
                int abs_az = abs(delta_az);
                if (abs_az > 999) abs_az = 999;
                if (delta_az > 0) {
                    // Move Azimuth Right
                    snprintf(cmdbuf, sizeof(cmdbuf), "MAR%03d", abs_az);
                } else {
                    // Move Azimuth Left
                    snprintf(cmdbuf, sizeof(cmdbuf), "MAL%03d", abs_az);
                }
                cmds = realloc(cmds, (cmd_count + 1) * sizeof(char*));
                cmds[cmd_count++] = strdup(cmdbuf);
                cmds = realloc(cmds, (cmd_count + 1) * sizeof(char*));
                cmds[cmd_count++] = strdup(timebuf);
                prev_az = az_int;
            }

            int delta_el = el_int - prev_el;
            if (delta_el != 0) {
                int abs_el = abs(delta_el);
                if (abs_el > 99) abs_el = 99;
                if (delta_el > 0) {
                    snprintf(cmdbuf, sizeof(cmdbuf), "MEU%02d", abs_el);
                } else {
                    snprintf(cmdbuf, sizeof(cmdbuf), "MED%02d", abs_el);
                }
                cmds = realloc(cmds, (cmd_count + 1) * sizeof(char*));
                cmds[cmd_count++] = strdup(cmdbuf);
                cmds = realloc(cmds, (cmd_count + 1) * sizeof(char*));
                cmds[cmd_count++] = strdup(timebuf);
                prev_el = el_int;
            }
        }
    }

    // NULL-terminate the list so caller can detect its length
    cmds = realloc(cmds, (cmd_count + 1) * sizeof(char*));
    cmds[cmd_count] = NULL;

    // Return the pointer via the provided out-parameter
    if (arduinoCommands_ptr) {
        *arduinoCommands_ptr = cmds;
    } else {
        // Caller didn't provide a place to store commands; free them
        for (int i = 0; i < cmd_count; ++i) free(cmds[i]);
        free(cmds);
    }

    free(tles);
    return 0;
}

int main(int argc, char *argv[]) {
    
    // 0. Prepare variables
    // An array that will hold the commands to be sent to the Arduino
    // Array format [[command1, timestamp1], [command2, timestamp2], ...]
    // The first command looks like: AXXXEXX (where XXX is azimuth and XX is elevation) always with leading zeros.
    // The following commands can look like one of the following: 
    // - MALXXX (Move Azimuth Left XXX degrees)
    // - MARXXX (Move Azimuth Right XXX degrees)
    // - MEUXX (Move Elevation Up XX degrees)
    // - MEDXX (Move Elevation Down XX degrees)
    char **arduinoCommands = NULL;

    // 0. Read arguments
    if (argc != 3) {
        // Expecting two arguments: satellite name and constellation name
        fprintf(stderr, "Usage: %s <satellite_name> <constellation_name>\n", argv[0]);
        return 1;
    }
    // Read satellite and constellation names
    char *satellite_name = argv[1]; // SV-name must be like: "STARLINK-1234" 
    char *constellation_name = argv[2]; // Can be: "S" (for Starlink), "O" (for Orbcomm) or "I" (for Iridium)
    // Basic validation
    if (satellite_name == NULL) {
        fprintf(stderr, "Satellite name is NULL\n");
        return 1;
    }
    if (constellation_name == NULL) {
        fprintf(stderr, "Constellation name is NULL\n");
        return 1;
    }

    // 1. Propagate satellite orbit and get azimuth/elevation over time
    if (calculateOrbit(satellite_name, constellation_name, &arduinoCommands) != 0) {
        fprintf(stderr, "Failed to calculate orbit for satellite %s in constellation %s\n", satellite_name, constellation_name);
        return 1;
    }

    // Pritn all generated commands for debugging
    printf("Generated Arduino Commands:\n");
    for (int i = 0; arduinoCommands && arduinoCommands[i] != NULL; i += 2) {
        printf("Time: %s, Command: %s\n", arduinoCommands[i + 1], arduinoCommands[i]);
    }   
    
/*
    // 2) Obrir comunicació sèrie amb l'Arduino
    // Find arduino port
    char arduino_port[PATH_MAX] = {0};
    if (find_arduino_port(arduino_port, sizeof(arduino_port)) != 0) {
        // fallback to common device if auto-detection failed
        const char *fallback = "/dev/ttyACM0";
        fprintf(stderr, "Warning: automatic Arduino detection failed, trying fallback %s\n", fallback);
        strncpy(arduino_port, fallback, sizeof(arduino_port) - 1);
        arduino_port[sizeof(arduino_port) - 1] = '\0';
    }

    int fd = open_and_configure_port(arduino_port, B9600);

    // Pause briefly to ensure the port is ready for data transmission
    sleep(2); 

    if (fd == -1) {
        fprintf(stderr, "Failed to connect to the Arduino. Check port name and permissions.\n");
        return 1;
    }

    // Write the command to the serial port
    const char *option = argv[1]; // 'ON' to turn on, 'OFF' to turn off
    ssize_t n;
    if (strcmp(option, "ON") == 0) {
        n = write(fd, COMMAND_ON, strlen(COMMAND_ON));
    } else if (strcmp(option, "OFF") == 0) {
        n = write(fd, COMMAND_OFF, strlen(COMMAND_OFF));
    } else {
        fprintf(stderr, "Invalid command. Use 'ON' to turn on or 'OFF' to turn off.\n");
        close(fd);
        return 1;
    }

    if (n < 0) {
        perror("Error writing to serial port");
        close(fd);
        return 1;
    }

    printf("Sent command '%s' successfully. Bytes written: %ld\n", option, n);

    // Give the Arduino time to process the command
    usleep(50000); // Wait 50ms

    // Close the file descriptor
    close(fd);


    // 2) Calibrar l'antena perquè estigui mirant a azimuth 0º i elevació 0º

    // 3) Per cada timeStep (1 segon, 0.5 segons...) moure els motors 

    // 4) Finalitzar el programa quan el SV ja no sigui visible.


    // NOTA 1: Cal veure si per fer la captura amb la SDR (USRP) cal executar cap altre codi (si es en paral·lel es podria fer servir MPI o OpenMP)
    // NOTA 2: Fer un sistema de referència de forma que el rang de moviment de l'antena sigui -360 ←→ +360, per evitar que els cables s'enrrollin entre ells 
    */
    return 0;
}