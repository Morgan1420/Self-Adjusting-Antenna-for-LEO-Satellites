#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <termios.h>

#include "command_handler_functions.h"
#include "JSON_related_functions.h"
#include "utils.h"
#include "../Arduino_stuff/arduino_communications.h"
#include "../AntennaMovementInstructions/utils.h"

#define AZ_MIN -360
#define AZ_MAX 360
#define EL_MIN 0
#define EL_MAX 90

// Action handler: Origin/Undo all movements
int handle_origin_action(const char *json, const char *json_path) {
    char *inner = extract_last_movements_inner(json);
    if (!inner || strlen(inner) == 0) {
        fprintf(stderr, "No movement history to rewind.\n");
        return 0;
    }
    
    int inner_len = strlen(inner) / 12; // 6 is the length of each command
    printf("Rewinding %d movements to origin...\n", inner_len);
    while (inner_len > 0) {
        // Reread JSON to get updated movements
        char *tmp_json = read_file_alloc(json_path);

        // Undo last movement
        handle_back_action(tmp_json, json_path);
        free(tmp_json);

        inner_len--;
    }
    
    // Parse starting position from JSON
    json = read_file_alloc(json_path);
    int startAz = 0, startEl = 0;
    if (parse_json_position(json, &startAz, &startEl) != 0) {
        fprintf(stderr, "Failed to parse starting position from JSON.\n");
        return 1;
    }

    // Write JSON with calculated starting position and empty LastMovements
    FILE *out = fopen(json_path, "w");
    if (out) {
        fprintf(out, "{\n");
        fprintf(out, "  \"currentAz\": %d,\n", startAz);
        fprintf(out, "  \"currentEl\": %d,\n", startEl);
        fprintf(out, "  \"LastMovements\": [\n");
        fprintf(out, "  ]\n");
        fprintf(out, "}\n");
        fclose(out);
        fprintf(stderr, "Rewound to starting position (Az:%d El:%d). Array cleared.\n", startAz, startEl);
    }
}

// Acció B (Back 1): Desfer l'últim moviment
int handle_back_action(const char *json, const char *json_path) {
    // 1. Extraiem el vector de "LastMovements"
    char *inner = extract_last_movements_inner(json);
    if (!inner || strlen(inner) == 0) {
        fprintf(stderr, "No movement history to undo.\n");
        return 0;
    }
    
    // 2. Parsejem tots els moviments desats
    char **movements = NULL;
    int num_movements = 0;
    const char *p = inner;
    while (*p) {
        while (*p && (isspace((unsigned char)*p) || *p == ',')) ++p;
        if (!*p) break;

        if (*p == '"') {
            ++p;
            int i = 0;
            movements = realloc(movements, (num_movements + 1) * sizeof(char *));
            if (!movements) {
                fprintf(stderr, "Memory allocation failed while parsing movements.\n");
                free(inner);
                return 1;
            }
            movements[num_movements] = malloc(32);
            if (!movements[num_movements]) {
                fprintf(stderr, "Memory allocation failed for movement entry.\n");
                free(movements);
                free(inner);
                return 1;
            }
            while (*p && *p != '"' && i < 31) {
                movements[num_movements][i++] = *p;
                ++p;
            }
            if (*p == '"') ++p;
            movements[num_movements][i] = '\0';
            if (i > 0) num_movements++;
        } else {
            ++p;
        }
    }
    
    // Si no hi ha moviments a desfer, no fem res
    if (num_movements == 0) {
        fprintf(stderr, "No movement to undo.\n");
        free(inner);
        return 0;
    }
    
    // 3. Invertim i enviem l'últim moviment
    char inverted[32];
    invert_command(movements[num_movements - 1], inverted, sizeof(inverted));
    printf("Undoing last movement: %s\n", inverted);
    
    char port_path[256];
    if (find_arduino_port(port_path, sizeof(port_path)) != 0) {
        fprintf(stderr, "No Arduino serial device found.\n");
        free(inner);
        return 6;
    }
    
    int fd = open_and_configure_port(port_path, B9600);
    if (fd < 0) {
        fprintf(stderr, "Failed to open serial port %s\n", port_path);
        free(inner);
        return 7;
    }
    
    char reply[128] = {0};
    int rr = read_line_with_timeout(fd, reply, sizeof(reply), 30);
    if (rr > 0) {
        fprintf(stderr, "Arduino reply: %s\n", reply);
    }

    printf("Writing to Arduino: %s", inverted);
    ssize_t w = write(fd, inverted, strlen(inverted));
    if (w < 0) {
        fprintf(stderr, "Failed to write to serial port: %s\n", strerror(errno));
        close(fd);
        free(inner);
        return 8;
    }
    tcdrain(fd);

    reply[0] = '\0';
    rr = read_line_with_timeout(fd, reply, sizeof(reply), 30);
    if (rr > 0) {
        fprintf(stderr, "Arduino reply: %s\n", reply);
    }
    close(fd);

    // Remove last movement from array (dynamic buffer to avoid overflow)
    size_t cap = 256;
    size_t used = 0;
    char *entries = malloc(cap);
    if (!entries) {
        for (int i = 0; i < num_movements; ++i) free(movements[i]);
        free(movements);
        free(inner);
        return 9;
    }
    entries[0] = '\0';
    int has_entries = 0;
    for (int i = 0; i < num_movements - 1; i++) {
        size_t need = used + (has_entries ? 2 : 0) + 2 + strlen(movements[i]) + 1;
        if (need > cap) {
            while (cap < need) cap *= 2;
            char *tmp = realloc(entries, cap);
            if (!tmp) {
                free(entries);
                for (int k = 0; k < num_movements; ++k) free(movements[k]);
                free(movements);
                free(inner);
                return 9;
            }
            entries = tmp;
        }
        if (has_entries) { strcat(entries, ", "); used += 2; }
        strcat(entries, "\""); used += 1;
        strcat(entries, movements[i]); used += strlen(movements[i]);
        strcat(entries, "\""); used += 1;
        has_entries = 1;
    }

    // Extract step size and update position
    int step_size = 1;
    const char *last_cmd = movements[num_movements - 1];
    int cmd_len = strlen(last_cmd);
    if (cmd_len >= 4) {
        step_size = atoi(&last_cmd[3]);
        if (step_size == 0) step_size = 1;
    }

    // Parse current position from JSON
    int curAz = 0, curEl = 0;
    parse_json_int(json, "currentAz", &curAz);
    parse_json_int(json, "currentEl", &curEl);

    if (strncmp(movements[num_movements - 1], "MAL", 3) == 0) {
        curAz += step_size;
    } else if (strncmp(movements[num_movements - 1], "MAR", 3) == 0) {
        curAz -= step_size;
    } else if (strncmp(movements[num_movements - 1], "MEU", 3) == 0) {
        curEl -= step_size;
    } else if (strncmp(movements[num_movements - 1], "MED", 3) == 0) {
        curEl += step_size;
    }

    FILE *out = fopen(json_path, "w");
    if (out) {
        fprintf(out, "{\n");
        fprintf(out, "  \"currentAz\": %d,\n", curAz);
        fprintf(out, "  \"currentEl\": %d,\n", curEl);
        fprintf(out, "  \"LastMovements\": [\n");
        if (has_entries) {
            fprintf(out, "    %s\n", entries);
        }
        fprintf(out, "  ]\n");
        fprintf(out, "}\n");
        fclose(out);
    }

    // Free allocated movement entries
    for (int i = 0; i < num_movements; ++i) {
        free(movements[i]);
    }
    free(movements);
    free(entries);
    free(inner);
    return 0;
}

// Action handler: Test sequence
int handle_test_action(const char *json_path, int curAz, int curEl) {
    fprintf(stderr, "Starting test sequence...\n");
    
    // Move 360 degrees left
    fprintf(stderr, "Moving 360 degrees left...\n");
    for (int i = 0; i < 360; i++) {
        if (send_command_and_update_json("MAL001", json_path, curAz - (i + 1), curEl) != 0) {
            fprintf(stderr, "Test failed during left movement\n");
            return 10;
        }
    }
    curAz -= 360;
    
    // Move 90 degrees up
    fprintf(stderr, "Moving 90 degrees up...\n");
    for (int i = 0; i < 90; i++) {
        if (send_command_and_update_json("MEU001", json_path, curAz, curEl + (i + 1)) != 0) {
            fprintf(stderr, "Test failed during up movement\n");
            return 11;
        }
    }
    curEl += 90;
    
    // Move 360 degrees right
    fprintf(stderr, "Moving 360 degrees right...\n");
    for (int i = 0; i < 360; i++) {
        if (send_command_and_update_json("MAR001", json_path, curAz + (i + 1), curEl) != 0) {
            fprintf(stderr, "Test failed during right movement\n");
            return 12;
        }
    }
    curAz += 360;
    
    // Move 90 degrees down
    fprintf(stderr, "Moving 90 degrees down...\n");
    for (int i = 0; i < 90; i++) {
        if (send_command_and_update_json("MED001", json_path, curAz, curEl - (i + 1)) != 0) {
            fprintf(stderr, "Test failed during down movement\n");
            return 13;
        }
    }
    
    fprintf(stderr, "Test sequence completed successfully.\n");
    return 0;
}

// Action handler: Calibrate to 0,90
int handle_calibrate_action(const char *json_path, int curAz, int curEl) {
    (void)curAz;
    (void)curEl;
    fprintf(stderr, "Calibration (placeholder): setting Az=0, El=90 in JSON only.\n");

    // Read the JSON file
    char *json = read_file_alloc(json_path);
    if (!json) {
        fprintf(stderr, "Failed to read JSON file '%s'\n", json_path);
        return 14;
    }

    FILE *out = fopen(json_path, "w");
    if (!out) {
        fprintf(stderr, "Failed to open JSON for writing during calibration.\n");
        free(json);
        return 14;
    }

    // Read current azimuth and elevation from JSON
    int currentAz = 0, currentEl = 0;
    if (parse_json_position(json, &currentAz, &currentEl) == 0) {
        fprintf(stderr, "Current position before calibration: Az=%d, El=%d\n", currentAz, currentEl);
    }

    // Calculate required movements to reach Az=0, El=90
    int azimuth_correction = -currentAz;
    int elevation_correction = 90 - currentEl;

    // Create movement commands
    char az_command[32];
    char el_command[32];
    
    int abs_az = abs(azimuth_correction);
    if (abs_az > 999) abs_az = 999;
    int abs_el = abs(elevation_correction);
    if (abs_el > 99) abs_el = 99;
    
    if (azimuth_correction > 0) {
        snprintf(az_command, sizeof(az_command), "MAR%03d", abs_az);
    } else if (azimuth_correction < 0) {
        snprintf(az_command, sizeof(az_command), "MAL%03d", abs_az);
    } else {
        snprintf(az_command, sizeof(az_command), "MAR000");
    }
    
    if (elevation_correction > 0) {
        snprintf(el_command, sizeof(el_command), "MEU%03d", abs_el);
    } else if (elevation_correction < 0) {
        snprintf(el_command, sizeof(el_command), "MED%03d", abs_el);
    } else {
        snprintf(el_command, sizeof(el_command), "MEU000");
    }

    // Send commands to Arduino
    char action = '-';
    if(azimuth_correction < 0) action = 'L';
    else if(azimuth_correction > 0) action = 'R';

    if (handle_movement_action(action , azimuth_correction, json, json_path, currentAz, curEl)) {
        fprintf(stderr, "Calibration failed during azimuth movement\n");
        fclose(out);
        free(json);
        return 10;

    }

    currentAz = currentAz + azimuth_correction;
    action = '-';
    if(elevation_correction < 0) action = 'D';
    else if(elevation_correction > 0) action = 'U';

    if (handle_movement_action(action , elevation_correction, json, json_path, currentAz, currentEl)) {
        fprintf(stderr, "Calibration failed during elevation movement\n");
        fclose(out);
        free(json);
        return 11;
    }
    

    currentEl = currentEl + elevation_correction;
    
    fclose(out);
    free(json);

    fprintf(stderr, "Calibration completed: Az=0, El=90\n");
    return 0;
}

// Action handler: Force 0-90 position
int handle_force_action(const char *json_path, int curAz, int curEl) {
    fprintf(stderr, "Forcing position to Az=0, El=90...\n");
    
    
    fprintf(stderr, "Force 0-90 completed\n");
    
    // Write JSON with empty LastMovements
    FILE *out = fopen(json_path, "w");
    if (out) {
        fprintf(out, "{\n");
        fprintf(out, "  \"currentAz\": %d,\n", 0);
        fprintf(out, "  \"currentEl\": %d,\n", 90);
        fprintf(out, "  \"LastMovements\": [\n");
        fprintf(out, "  ]\n");
        fprintf(out, "}\n");
        fclose(out);
    }
    
    return 0;
}

// Action handler: Simple movements (L, R, U, D)
int handle_movement_action(char action, int step, const char *json, const char *json_path, int curAz, int curEl) {
    int newAz = curAz;
    int newEl = curEl;
    
    if (action == 'L') {
        newAz = curAz - step;
        if (newAz < AZ_MIN) newAz = AZ_MIN;
    } else if (action == 'R') {
        newAz = curAz + step;
        if (newAz > AZ_MAX) newAz = AZ_MAX;
    } else if (action == 'U') {
        newEl = curEl + step;
        if (newEl > EL_MAX) newEl = EL_MAX;
    } else if (action == 'D') {
        newEl = curEl - step;
        if (newEl < EL_MIN) newEl = EL_MIN;
    }

    if (newAz == curAz && newEl == curEl) {
        fprintf(stderr, "No movement needed (already at requested limit).\n");
        return 0;
    }

    // Build command
    char command[32];
    if (action == 'L') {
        snprintf(command, sizeof(command), "MAL%03d\n", step);
    } else if (action == 'R') {
        snprintf(command, sizeof(command), "MAR%03d\n", step);
    } else if (action == 'U') {
        snprintf(command, sizeof(command), "MEU%03d\n", step);
    } else {
        snprintf(command, sizeof(command), "MED%03d\n", step);
    }

    printf("Sending command: %s", command);

    // Find Arduino port
    char port_path[256];
    if (find_arduino_port(port_path, sizeof(port_path)) != 0) {
        fprintf(stderr, "No Arduino serial device found.\n");
        return 6;
    }

    int fd = open_and_configure_port(port_path, B9600);
    if (fd < 0) {
        fprintf(stderr, "Failed to open serial port %s\n", port_path);
        return 7;
    }

    char reply[128] = {0};
    int rr = read_line_with_timeout(fd, reply, sizeof(reply), 30);
    if (rr > 0) {
        fprintf(stderr, "Arduino reply: %s\n", reply);
    } else {
        fprintf(stderr, "No reply from Arduino (timeout or error).\n");
    }

    printf("Writing to Arduino on %s: %s (%d)", port_path, command, (int)strlen(command)); 
    ssize_t w = write(fd, command, strlen(command));
    if (w < 0) {
        fprintf(stderr, "Failed to write to serial port: %s\n", strerror(errno));
        close(fd);
        return 8;
    }
    tcdrain(fd);

    reply[0] = '\0';
    rr = read_line_with_timeout(fd, reply, sizeof(reply), 30);
    if (rr > 0) {
        fprintf(stderr, "Arduino reply: %s\n", reply);
    } else {
        fprintf(stderr, "No reply from Arduino (timeout or error).\n");
    }
    close(fd);

    // Update JSON with dynamic buffer to avoid stack overflow on large histories
    char *inner = extract_last_movements_inner(json);
    
    char cmd_no_nl[32];
    strncpy(cmd_no_nl, command, sizeof(cmd_no_nl)-1);
    cmd_no_nl[sizeof(cmd_no_nl)-1] = '\0';
    for (size_t i = 0; i < strlen(cmd_no_nl); ++i) {
        if (cmd_no_nl[i] == '\n' || cmd_no_nl[i] == '\r') { cmd_no_nl[i] = '\0'; break; }
    }
    char new_item[128];
    snprintf(new_item, sizeof(new_item), "\"%s\"", cmd_no_nl);

    size_t cap = 256;
    size_t used = 0;
    char *entries = malloc(cap);
    if (!entries) {
        free(inner);
        return 9;
    }
    entries[0] = '\0';
    int has_entries = 0;
    
    if (inner && strlen(inner) > 0) {
        const char *p = inner;
        while (*p) {
            while (*p && (isspace((unsigned char)*p) || *p == ',')) ++p;
            if (!*p) break;
            
            if (*p == '\"') {
                const char *start = p;
                ++p;
                while (*p && *p != '\"') ++p;
                if (*p == '\"') {
                    ++p;
                    size_t len = (size_t)(p - start);
                    if (len > 2) {
                        size_t need = used + (has_entries ? 2 : 0) + len + 1;
                        if (need > cap) {
                            while (cap < need) cap *= 2;
                            char *tmp = realloc(entries, cap);
                            if (!tmp) {
                                free(entries);
                                free(inner);
                                return 9;
                            }
                            entries = tmp;
                        }
                        if (has_entries) {
                            entries[used++] = ',';
                            entries[used++] = ' ';
                        }
                        memcpy(entries + used, start, len);
                        used += len;
                        entries[used] = '\0';
                        has_entries = 1;
                    }
                }
            } else {
                ++p;
            }
        }
    }
    
    size_t new_len = strlen(new_item);
    size_t need = used + (has_entries ? 2 : 0) + new_len + 1;
    if (need > cap) {
        while (cap < need) cap *= 2;
        char *tmp = realloc(entries, cap);
        if (!tmp) {
            free(entries);
            free(inner);
            return 9;
        }
        entries = tmp;
    }
    if (has_entries) {
        entries[used++] = ',';
        entries[used++] = ' ';
    }
    memcpy(entries + used, new_item, new_len);
    used += new_len;
    entries[used] = '\0';
    has_entries = 1;

    FILE *out = fopen(json_path, "w");
    if (!out) {
        fprintf(stderr, "Failed to open JSON for writing: %s\n", strerror(errno));
        free(inner);
        free(entries);
        return 9;
    }

    fprintf(out, "{\n");
    fprintf(out, "  \"currentAz\": %d,\n", newAz);
    fprintf(out, "  \"currentEl\": %d,\n", newEl);
    fprintf(out, "  \"LastMovements\": [\n");
    if (has_entries && entries[0] != '\0') {
        fprintf(out, "    %s\n", entries);
    }
    fprintf(out, "  ]\n");
    fprintf(out, "}\n");
    fclose(out);

    fprintf(stderr, "Updated JSON %s (Az:%d El:%d)\n", json_path, newAz, newEl);
    free(inner);
    free(entries);
    return 0;
}
