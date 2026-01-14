// --- CONFIGURATION ---
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>

#include "Satellite_data/utils.h"
#include "Satellite_data/download_and_parse_TLE_functions.h"
#include "Satellite_data/coordinates_translation_functions.h"
#include "Arduino_stuff/arduino_communications.h"
#include "AntennaMovementInstructions/command_handler_functions.h"
#include "AntennaMovementInstructions/JSON_related_functions.h"

// Compile with:
// NEED TO UPDATE COMMAND
// gcc -o process_antenna_movements process_anenna_movements.c Satellite_data/download_and_parse_TLE_functions.c Satellite_data/coordinates_translation_functions.c Arduino_stuff/arduino_communications.c AntennaMovementInstructions/command_handler_functions.c AntennaMovementInstructions/JSON_related_functions.c AntennaMovementInstructions/utils.c -lcurl -L./SGP4_library -lSGP4 -lm

// URLs for different constellations
const char *url_starlink = "https://celestrak.org/NORAD/elements/gp.php?GROUP=starlink&FORMAT=tle";
const char *url_orbcomm = "https://celestrak.org/NORAD/elements/gp.php?GROUP=orbcomm&FORMAT=tle";
const char *url_iridium = "https://celestrak.org/NORAD/elements/gp.php?GROUP=iridium&FORMAT=tle";
const char *url_globalstar = "https://celestrak.org/NORAD/elements/gp.php?GROUP=globalstar&FORMAT=tle";

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

// Posició (Sant Vicenç dels Horts, Catalonia, Spain)
GeodeticPosition observer = {
    .latitude_deg = 41.393552,
    .longitude_deg = 2.009787,
    .altitude_km = 0.022
};


// Aquesta funció calcula l'òrbita d'un satèl·lit i genera les comandes per a l'Arduino
int calculateOrbitAndGenerateCommands(char* satellite_name, char* constellation_name, char ***arduinoCommands_ptr){
    const char *url = NULL;

    // ------ 1. Trobar la URL correcta segons la constel·lació
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
        case 'G':
        case 'g':
            url = url_globalstar;
            break;
        default:
            fprintf(stderr, "Unknown constellation name: %s\n", constellation_name);
            return 1;
    }

    // ------ 2. Ens descarreguem les dades TLE
    char* tle_data_string = download_tles(url);
    if (!tle_data_string) {
        fprintf(stderr, "Failed to download TLE data.\n");
        return 1;
    }

    // ------ 3. Parsejem les dades TLE
    ownTLE* tles = NULL;
    int tle_count = parse_tle_string(tle_data_string, &tles);
    free(tle_data_string); // Alliberem la cadena descarregada després de parsejar

    if (tle_count <= 0) {
        fprintf(stderr, "No TLEs parsed from source.\n");
        free(tles);
        return 1;
    }

    // Ens assegurem que els noms dels TLE estan nets, sense espais innecessaris
    for (int i = 0; i < tle_count; ++i) {
        if (tles[i].name) trim_whitespace(tles[i].name);
    }
    char sat_name_clean[128];
    strncpy(sat_name_clean, satellite_name, sizeof(sat_name_clean) - 1);
    sat_name_clean[sizeof(sat_name_clean) - 1] = '\0';
    trim_whitespace(sat_name_clean);

    // ------ 4. Localitzem el satèl·lit específic 
    ownTLE *selected_tle = NULL;
    for (int i = 0; i < tle_count; ++i) {
        if (tles[i].name && strcmp(tles[i].name, sat_name_clean) == 0) {
            selected_tle = &tles[i];
            break;
        }
    }

    // ------ 5. Comprovem si s'ha trobat el satèl·lit
    if (!selected_tle) {
        fprintf(stderr, "Satellite %s not found in TLE data.\n", satellite_name);
        free(tles);
        return 1;
    }

    // ------ 6. Configurem el mode d'operació i inicialitzem SGP4
    char opsmode = 'i'; // improved
    if (!sgp4init(opsmode, &selected_tle->tle.rec)) {
        fprintf(stderr, "SGP4 initialization failed for %s.\n", satellite_name);
        free(tles);
        return 1;
    }

    // ------ 7. Preparem la posició de l'observador (lloc local)
    double observer_ecef[3];
    geodetic_to_ecef(&observer, &observer_ecef[0], &observer_ecef[1], &observer_ecef[2]);

    // ------ 8. Propaguem per als següents 30 minuts en passos de 5 segons (0.0833 minuts) i imprimim resultats
    time_t current_time_unix = time(NULL);
    
    // Create timebuf for current time (for first commands to execute immediately)
    char current_timebuf[64];
    struct tm *tm_current = localtime(&current_time_unix);
    strftime(current_timebuf, sizeof(current_timebuf), "%Y-%m-%d %H:%M:%S", tm_current);
    
    char **cmds = NULL; // Preparem la llista de comandes (parells: comanda, marca de temps).
    int cmd_count = 0;
    int capture_scheduled = 0;
    for (double t_min = 0.0; t_min < 30.0; t_min += 0.0833) {
        double tsince = t_min; // minutes since epoch

        double sat_pos_eci[3], sat_vel_eci[3];

        // 8.1. Propaguem l'òrbita amb SGP4
        int error_code = sgp4(&selected_tle->tle.rec, tsince, sat_pos_eci, sat_vel_eci);
        if (error_code < 0) {
            fprintf(stderr, "SGP4 propagation error (code %d) at tsince=%.3f min.\n", error_code, tsince);
            free(tles);
            return 1;
        }

        // 8.2. Convertim ECI a ECEF per a l'època/temps donat
        time_t step_time_unix = current_time_unix + (time_t)(t_min * 60.0);
        double jd = unix_to_julian(step_time_unix);
        double sat_pos_ecef[3];
        eci_to_ecef(sat_pos_eci, jd, sat_pos_ecef);

        // 8.3. Convertim ECEF a topocèntric (azimut, elevació, distància)
        // Nota: ecef_to_topo ja retorna azimut i elevació en graus.
        double azimuth_deg = 0.0, elevation_deg = 0.0, range_km = 0.0;
        ecef_to_topo(observer_ecef, sat_pos_ecef, &observer, &azimuth_deg, &elevation_deg, &range_km);

        // 8.4. Preparem la marca de temps per a la sortida
        char timebuf[64];
        struct tm *tm = localtime(&step_time_unix);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm);
        // printf("%s, Az: %.2f deg, El: %.2f deg, Range: %.3f km\n", timebuf, azimuth_deg, elevation_deg, range_km);

        // Només generem comandes Arduino per a posicions visibles (elevació > 0)
        if (elevation_deg <= 0.0) {
            continue;
        }

        // Schedule capture once when elevation crosses >= 30 deg
        if (!capture_scheduled && elevation_deg >= 30.0) {
            cmds = realloc(cmds, (cmd_count + 1) * sizeof(char*));
            cmds[cmd_count++] = strdup("C");
            cmds = realloc(cmds, (cmd_count + 1) * sizeof(char*));
            cmds[cmd_count++] = strdup(timebuf);
            capture_scheduled = 1;
        }

        // 8.5. Arodonim azimut/elevació a graus enters i generem les comandes corresponents
        int az_int = (int)round(azimuth_deg);
        az_int = (az_int % 360 + 360) % 360;
        int el_int = (int)round(elevation_deg);
        if (el_int < 0) el_int = 0;
        if (el_int > 99) el_int = 99;

        static int prev_az = -9999;
        static int prev_el = -9999;
        static int first = 1;

        char cmdbuf[64];

        if (first) {
            // Initial relative movement commands: assume starting position is (Az 0, El 90) after calibration
            // First command: Azimuth movement
            int delta_az = az_int; // Starting from 0 degrees after calibration
            if (delta_az > 180) delta_az -= 360;
            if (delta_az < -180) delta_az += 360;
            
            int abs_az = abs(delta_az);
            if (abs_az > 999) abs_az = 999;
            if (delta_az > 0) {
                snprintf(cmdbuf, sizeof(cmdbuf), "MAR%03d", abs_az);
            } else if (delta_az < 0) {
                snprintf(cmdbuf, sizeof(cmdbuf), "MAL%03d", abs_az);
            } else {
                snprintf(cmdbuf, sizeof(cmdbuf), "MAR000"); // No movement needed
            }
            cmds = realloc(cmds, (cmd_count + 1) * sizeof(char*));
            cmds[cmd_count++] = strdup(cmdbuf);
            cmds = realloc(cmds, (cmd_count + 1) * sizeof(char*));
            cmds[cmd_count++] = strdup(current_timebuf);

            // Second command: Elevation movement (from 90 degrees to target)
            int delta_el = el_int - 90; // Starting from 90 degrees after calibration
            int abs_el = abs(delta_el);
            if (abs_el > 99) abs_el = 99;
            if (delta_el > 0) {
                snprintf(cmdbuf, sizeof(cmdbuf), "MEU%03d", abs_el);
            } else if (delta_el < 0) {
                snprintf(cmdbuf, sizeof(cmdbuf), "MED%03d", abs_el);
            } else {
                snprintf(cmdbuf, sizeof(cmdbuf), "MEU000"); // No movement needed
            }
            cmds = realloc(cmds, (cmd_count + 1) * sizeof(char*));
            cmds[cmd_count++] = strdup(cmdbuf);
            cmds = realloc(cmds, (cmd_count + 1) * sizeof(char*));
            cmds[cmd_count++] = strdup(current_timebuf);

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
                    snprintf(cmdbuf, sizeof(cmdbuf), "MEU%03d", abs_el);
                } else {
                    snprintf(cmdbuf, sizeof(cmdbuf), "MED%03d", abs_el);
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
    // ------ 1. Llegim els arguments
    // Comprovem que hi hagi un nombre correcte d'arguments
    if (argc != 3) {
        // Esperem dos arguments: nom del satèl·lit i nom de la constel·lació
        fprintf(stderr, "Usage: %s <satellite_name> <constellation_name>\n", argv[0]);
        return 1;
    }
    // Llegim els noms del satèl·lit i de la constel·lació
    char *satellite_name = argv[1]; // SV-name: "STARLINK-1234" 
    char *constellation_name = argv[2]; // Poden ser: "S" (per Starlink), "O" (per Orbcomm), "I" (per Iridium) o "G" (per Globalstar)
    // Validem les entrades
    if (satellite_name == NULL) {
        fprintf(stderr, "Satellite name is NULL\n");
        return 1;
    }
    if (constellation_name == NULL) {
        fprintf(stderr, "Constellation name is NULL\n");
        return 1;
    }
    if (constellation_name[0] != 'S' && constellation_name[0] != 's' &&
        constellation_name[0] != 'O' && constellation_name[0] != 'o' &&
        constellation_name[0] != 'I' && constellation_name[0] != 'i' &&
        constellation_name[0] != 'G' && constellation_name[0] != 'g') {
        fprintf(stderr, "Invalid constellation name: %s (must be S, O, I, or G)\n", constellation_name);
        return 1;
    }

    // ------ 2. Preparem les variables
    // Array de comandes per a l'Arduino
    char **arduinoCommands = NULL;

    // ------ 3. Calibrem l'antenna
    // Aconseguim el "path" absolut del fitxer JSON
    char json_path[1024];
    get_absolute_json_path(argv[0], json_path, sizeof(json_path));

    // Path to capture script next to the executable
    char script_path[PATH_MAX];
    char exe_full[PATH_MAX];
    ssize_t exelen = readlink("/proc/self/exe", exe_full, sizeof(exe_full) - 1);
    if (exelen != -1) {
        exe_full[exelen] = '\0';
    } else {
        strncpy(exe_full, argv[0], sizeof(exe_full) - 1);
        exe_full[sizeof(exe_full) - 1] = '\0';
    }
    char *slash = strrchr(exe_full, '/');
    if (slash) {
        *slash = '\0';
        snprintf(script_path, sizeof(script_path), "%s/capture_files/_capture_script.py", exe_full);
    } else {
        snprintf(script_path, sizeof(script_path), "./capture_files/_capture_script.py");
    }

    // Llegim el fitxer JSON
    char *json = read_file_alloc(json_path);
    if (!json) { fprintf(stderr, "Failed to read JSON file '%s'\n", json_path); return 5; }

    // Busquem els valors actuals d'azimut i elevació
    int curAz = 0, curEl = 0;
    if (parse_json_int(json, "currentAz", &curAz) != 0) {
        fprintf(stderr, "Failed to parse currentAz from JSON, defaulting to 0\n");
        curAz = 0;
    }
    if (parse_json_int(json, "currentEl", &curEl) != 0) {
        fprintf(stderr, "Failed to parse currentEl from JSON, defaulting to 0\n");
        curEl = 0;
    }

    handle_calibrate_action(json_path, curAz, curEl);
    free(json);

    // ------ 4. Calculem l'òrbita i generem les comandes per a l'Arduino

    if (calculateOrbitAndGenerateCommands(satellite_name, constellation_name, &arduinoCommands) != 0) {
        fprintf(stderr, "Failed to calculate orbit for satellite %s in constellation %s\n", satellite_name, constellation_name);
        return 1;
    }
    // Mostrem les comandes generades per a la depuració
    printf("Generated Arduino Commands:\n");
    for (int i = 0; arduinoCommands && arduinoCommands[i] != NULL; i += 2) {
        printf("Time: %s, Command: %s\n", arduinoCommands[i + 1], arduinoCommands[i]);
    } 
    

    // ------ 5. Enviem les comandes a l'Arduino usant les funcions del command handler
    if (arduinoCommands) {
        for (int idx = 0; arduinoCommands[idx] != NULL; idx += 2) {
            // 5.1. Preparem la comanda i la marca de temps
            const char *cmd = arduinoCommands[idx];
            const char *ts = arduinoCommands[idx + 1];
            
            if (!cmd) continue; // safety check

            // 5.2. Esperem fins que la marca de temps coincideixi (o sigui més gran) que l'hora actual
            // Obtenim l'hora actual
            time_t now = time(NULL);
            struct tm *tm_now = localtime(&now);
            char now_str[64];
            strftime(now_str, sizeof(now_str), "%Y-%m-%d %H:%M:%S", tm_now);
            while (strcmp(now_str, ts) < 0) {
                // Ens esperem una mica abans de tornar a comprovar
                usleep(100000); // 100 ms
                now = time(NULL);
                tm_now = localtime(&now);
                strftime(now_str, sizeof(now_str), "%Y-%m-%d %H:%M:%S", tm_now);
            }
            // 5.3. Execute capture script or send movement
            if (strcmp(cmd, "C") == 0) {
                pid_t pid = fork();
                if (pid == 0) {
                    execlp("python3", "python3", script_path, "30", (char*)NULL);
                    _exit(127);
                } else if (pid < 0) {
                    fprintf(stderr, "Failed to start capture process\n");
                } else {
                    printf("Started capture at %s (pid %d)\n", now_str, (int)pid);
                }
            } else {
                char *json_updated = read_file_alloc(json_path);
                if (!json_updated) {
                    fprintf(stderr, "Failed to read updated JSON file '%s'\n", json_path);
                    continue;
                }
                int currentAz = 0, currentEl = 0;
                parse_json_int(json_updated, "currentAz", &currentAz);
                parse_json_int(json_updated, "currentEl", &currentEl);
                if (send_command_and_update_json(cmd, json_path, currentAz, currentEl) != 0) {
                    fprintf(stderr, "Failed to send command: %s\n", cmd);
                    free(json_updated);
                    break;
                }
                printf("Sent command: %s at %s (scheduled for %s)\n", cmd, now_str, ts);
                free(json_updated);
            }
        }

        // Free the command list
        for (int i = 0; arduinoCommands[i] != NULL; ++i) free(arduinoCommands[i]);
        free(arduinoCommands);
        arduinoCommands = NULL;
    }

    // ------ 6. Desfem tots els moviments 
    char *json2 = read_file_alloc(json_path);
    if (!json2) { fprintf(stderr, "Failed to read JSON file '%s'\n", json_path); return 5; }

    handle_origin_action(json2, json_path);
    free(json2);

    return 0;
}