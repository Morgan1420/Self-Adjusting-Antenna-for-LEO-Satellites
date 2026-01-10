/*
 * moveAntenna.c
 *
 * Usage: ./moveAntenna <path_to_json> <action>
 *   action: one of C (calibrate), T (top/up), B (bottom/down), L (left), R (right), O (original/return), S (test)
 *
 * Behavior:
 *  - Reads the JSON file (expects fields `currentAz` and `currentEl` and `LastMovements` array)
 *  - Validates the requested action and clamps against allowed ranges
 *  - Constructs Arduino commands (MAR, MAL, MEU, MED for relative movements)
 *  - Finds and opens the Arduino serial port using existing helpers in Arduino_stuff
 *  - Sends the command and waits briefly for a reply line ("DONE" or other)
 *  - Updates the JSON file: writes new `currentAz`, `currentEl` and appends a record to `LastMovements`
 *
 * Build:
 *   gcc -o moveAntenna moveAntenna.c Arduino_stuff/arduino_communications.c -IArduino_stuff -Wall -Wextra
 *
 * Examples:
 *   # Calibrate (move to Az=0, El=90)
 *   ./moveAntenna ../Codes/examplejson.json C
 *
 *   # Test sequence (360 left, 90 up, 360 right, 90 down)
 *   ./moveAntenna ../Codes/examplejson.json S
 *
 *   # Move left by one step (AZ_STEP)
 *   ./moveAntenna ../Codes/examplejson.json L
 *
 *   # Move right by one step
 *   ./moveAntenna ../Codes/examplejson.json R
 *
 *   # Move top (increase elevation)
 *   ./moveAntenna ../Codes/examplejson.json T
 *
 *   # Move bottom (decrease elevation)
 *   ./moveAntenna ../Codes/examplejson.json B
 *
 * Notes:
 *  - This file depends on `Arduino_stuff/arduino_communications.h` and its implementation.
 *  - Azimuth valid range: -360..360, Elevation valid range: 0..90
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <ctype.h>

#include "Arduino_stuff/arduino_communications.h"

#define AZ_MIN -360
#define AZ_MAX 360
#define EL_MIN 0
#define EL_MAX 90

/* default step sizes (degrees) for each action */
#define AZ_STEP 1
#define EL_STEP 1

/* Read whole file into malloc'd buffer (caller must free). Returns NULL on error. */
static char *read_file_alloc(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[r] = '\0';
    return buf;
}

/* Simple JSON integer extraction: find the key and parse the number after ':' */
static int parse_json_int(const char *buf, const char *key, int *out_value) {
    const char *p = strstr(buf, key);
    if (!p) return -1;
    p = strchr(p, ':');
    if (!p) return -1;
    ++p;
    // skip spaces
    while (*p && isspace((unsigned char)*p)) ++p;
    char *endptr;
    long v = strtol(p, &endptr, 10);
    if (p == endptr) return -1;
    *out_value = (int)v;
    return 0;
}

/* Extract the textual contents of the LastMovements array (between [ and ]) into a newly allocated string.
 * Caller must free. Returns NULL on failure (treat as empty array). */
static char *extract_last_movements_inner(const char *buf) {
    const char *p = strstr(buf, "LastMovements");
    if (!p) return NULL;
    p = strchr(p, '[');
    if (!p) return NULL;
    ++p; // start of inner
    const char *q = p;
    int depth = 1;
    // find matching ]
    while (*q) {
        if (*q == '[') depth++;
        else if (*q == ']') { depth--; if (depth == 0) break; }
        q++;
    }
    if (!*q) return NULL;
    size_t len = (size_t)(q - p);
    char *inner = malloc(len + 1);
    if (!inner) return NULL;
    strncpy(inner, p, len);
    inner[len] = '\0';
    return inner;
}

/* Helper: get current timestamp string */
static void timestamp_now(char *out, size_t n) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(out, n, "%Y-%m-%d %H:%M:%S", &tm);
}

/* Invert a command: MAL -> MAR, MAR -> MAL, MEU -> MED, MED -> MEU, C -> C */
static void invert_command(const char *cmd, char *out, size_t out_len) {
    if (!cmd || !out || out_len < 8) return;
    
    if (cmd[0] == 'C') {
        snprintf(out, out_len, "C\n");
    } else if (strncmp(cmd, "MAL", 3) == 0) {
        snprintf(out, out_len, "MAR%s\n", cmd + 3);
    } else if (strncmp(cmd, "MAR", 3) == 0) {
        snprintf(out, out_len, "MAL%s\n", cmd + 3);
    } else if (strncmp(cmd, "MEU", 3) == 0) {
        snprintf(out, out_len, "MED%s\n", cmd + 3);
    } else if (strncmp(cmd, "MED", 3) == 0) {
        snprintf(out, out_len, "MEU%s\n", cmd + 3);
    } else {
        snprintf(out, out_len, "%s\n", cmd);
    }
}

/* Wait for a line from fd with timeout seconds; the line is stored in buf (buflen-1), returns bytes read or -1 on timeout/error */
static int read_line_with_timeout(int fd, char *buf, size_t buflen, int timeout_seconds) {
    fd_set rfds;
    struct timeval tv;
    size_t idx = 0;
    while (1) {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = timeout_seconds;
        tv.tv_usec = 0;
        int sel = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (sel == -1) return -1;
        if (sel == 0) return -1; // timeout
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r <= 0) return -1;
        if (c == '\n' || c == '\r') {
            if (idx == 0) continue; // ignore leading CR/LF
            break;
        }
        if (idx + 1 < buflen) buf[idx++] = c;
    }
    buf[idx] = '\0';
    return (int)idx;
}

/* Helper function to send a command to Arduino and update the JSON file */
static int send_command_and_update_json(const char *command, const char *json_path, int newAz, int newEl) {
    // Find Arduino port
    char port_path[256];
    if (find_arduino_port(port_path, sizeof(port_path)) != 0) {
        fprintf(stderr, "No Arduino serial device found.\n");
        return -1;
    }

    int fd = open_and_configure_port(port_path, B9600);
    if (fd < 0) {
        fprintf(stderr, "Failed to open serial port %s\n", port_path);
        return -1;
    }

    // Wait for initial response
    char reply[128] = {0};
    int rr = read_line_with_timeout(fd, reply, sizeof(reply), 5);
    if (rr > 0) {
        fprintf(stderr, "Arduino initial reply: %s\n", reply);
    }

    // Send command
    char cmd_with_nl[64];
    snprintf(cmd_with_nl, sizeof(cmd_with_nl), "%s\n", command);
    printf("Sending: %s", cmd_with_nl);
    ssize_t w = write(fd, cmd_with_nl, strlen(cmd_with_nl));
    if (w < 0) {
        fprintf(stderr, "Failed to write to serial port: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    tcdrain(fd);

    // Wait for response
    reply[0] = '\0';
    rr = read_line_with_timeout(fd, reply, sizeof(reply), 5);
    if (rr > 0) {
        fprintf(stderr, "Arduino reply: %s\n", reply);
    }

    close(fd);

    // Update JSON file
    char *json = read_file_alloc(json_path);
    if (!json) {
        fprintf(stderr, "Failed to read JSON for update\n");
        return -1;
    }

    char *inner = extract_last_movements_inner(json);
    
    // Build new item for LastMovements
    char new_item[128];
    snprintf(new_item, sizeof(new_item), "\"%s\"", command);

    // Parse existing entries
    char entries[2048] = {0};
    int has_entries = 0;
    
    if (inner && strlen(inner) > 0) {
        const char *p = inner;
        while (*p) {
            while (*p && (isspace((unsigned char)*p) || *p == ',')) ++p;
            if (!*p) break;
            
            if (*p == '"') {
                const char *start = p;
                ++p;
                while (*p && *p != '"') ++p;
                if (*p == '"') {
                    ++p;
                    size_t len = (size_t)(p - start);
                    if (len > 2) {
                        if (has_entries) strcat(entries, ", ");
                        strncat(entries, start, len);
                        has_entries = 1;
                    }
                }
            } else {
                ++p;
            }
        }
    }
    
    // Build final entry list
    char *new_last_inner = NULL;
    if (has_entries) {
        asprintf(&new_last_inner, "%s, %s", entries, new_item);
    } else {
        asprintf(&new_last_inner, "%s", new_item);
    }

    // Write updated JSON
    FILE *out = fopen(json_path, "w");
    if (!out) {
        fprintf(stderr, "Failed to open JSON for writing: %s\n", strerror(errno));
        free(json);
        free(inner);
        free(new_last_inner);
        return -1;
    }

    fprintf(out, "{\n");
    fprintf(out, "  \"currentAz\": %d,\n", newAz);
    fprintf(out, "  \"currentEl\": %d,\n", newEl);
    fprintf(out, "  \"LastMovements\": [\n");
    if (new_last_inner && strlen(new_last_inner) > 0) {
        fprintf(out, "    %s\n", new_last_inner);
    }
    fprintf(out, "  ]\n");
    fprintf(out, "}\n");
    fclose(out);

    free(json);
    free(inner);
    free(new_last_inner);
    
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <path_to_json> <action (C|S|T|B|L|R|O)>\n", argv[0]);
        return 2;
    }

    const char *json_path = argv[1];
    char action = argv[2][0];
    if (action == 0) { fprintf(stderr, "Invalid action\n"); return 3; }
    action = (char)toupper((unsigned char)action);

    // Validate action
    if (!(action == 'C' || action == 'S' || action == 'T' || action == 'B' || action == 'L' || action == 'R' || action == 'O')) {
        fprintf(stderr, "Unknown action '%c'\n", action);
        return 4;
    }

    // Read JSON file
    char *json = read_file_alloc(json_path);
    if (!json) { fprintf(stderr, "Failed to read JSON file '%s'\n", json_path); return 5; }

    int curAz = 0, curEl = 0;
    if (parse_json_int(json, "currentAz", &curAz) != 0) {
        fprintf(stderr, "Failed to parse currentAz from JSON, defaulting to 0\n");
        curAz = 0;
    }
    if (parse_json_int(json, "currentEl", &curEl) != 0) {
        fprintf(stderr, "Failed to parse currentEl from JSON, defaulting to 0\n");
        curEl = 0;
    }

    int newAz = curAz;
    int newEl = curEl;

    if (action == 'O') {
        // Original/Return: replay movements in reverse to get back to start, then clear array
        char *inner = extract_last_movements_inner(json);
        if (!inner || strlen(inner) == 0) {
            fprintf(stderr, "No movement history to rewind.\n");
            free(json);
            return 0; // nothing to do
        }
        
        // Parse all movements from inner
        char movements[16][32]; // up to 16 movements
        int num_movements = 0;
        const char *p = inner;
        while (*p && num_movements < 16) {
            // Skip whitespace and commas
            while (*p && (isspace((unsigned char)*p) || *p == ',')) ++p;
            if (!*p) break;
            
            // Extract quoted string
            if (*p == '"') {
                ++p; // skip opening quote
                int i = 0;
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
        
        // Send commands in reverse with inverted direction
        for (int i = num_movements - 1; i >= 0; --i) {
            char inverted[32];
            invert_command(movements[i], inverted, sizeof(inverted));
            printf("Sending reverse command: %s\n", inverted);
            
            // Find Arduino port and send
            char port_path[256];
            if (find_arduino_port(port_path, sizeof(port_path)) != 0) {
                fprintf(stderr, "No Arduino serial device found.\n");
                free(json);
                return 6;
            }
            

            int fd = open_and_configure_port(port_path, B9600);
            if (fd < 0) {
                fprintf(stderr, "Failed to open serial port %s\n", port_path);
                free(json);
                return 7;
            }

            // Wait for a response line (timeout 5s)
            char reply[128] = {0};
            int rr = read_line_with_timeout(fd, reply, sizeof(reply), 5);
            if (rr > 0) {
                fprintf(stderr, "Arduino reply: %s\n", reply);
            } else {
                fprintf(stderr, "No reply from Arduino (timeout or error).\n");
            }

            // Send command
            printf("Writing to Arduino on %s: %s (%d)", port_path, inverted, (int)strlen(inverted));
            ssize_t w = write(fd, inverted, strlen(inverted));
            if (w < 0) {
                fprintf(stderr, "Failed to write to serial port: %s\n", strerror(errno));
                close(fd);
                free(json);
                return 8;
            }
            tcdrain(fd);

            // Wait for a response line (timeout 5s)
            reply[0] = '\0'; // clear reply before sending command
            rr = read_line_with_timeout(fd, reply, sizeof(reply), 5);
            if (rr > 0) {
                fprintf(stderr, "Arduino reply: %s\n", reply);
            } else {
                fprintf(stderr, "No reply from Arduino (timeout or error).\n");
            }
        
            close(fd);

        }
        
        // Reset position to 0,0 after rewinding
        newAz = 0;
        newEl = 0;
        
        // Write JSON with empty LastMovements
        FILE *out = fopen(json_path, "w");
        if (out) {
            fprintf(out, "{\n");
            fprintf(out, "  \"currentAz\": 0,\n");
            fprintf(out, "  \"currentEl\": 0,\n");
            fprintf(out, "  \"LastMovements\": [\n");
            fprintf(out, "  ]\n");
            fprintf(out, "}\n");
            fclose(out);
            fprintf(stderr, "Rewound to original position. Array cleared.\n");
        }
        
        free(json);
        free(inner);
        return 0;
    } else if (action == 'S') {
        // Test sequence: 360 left, 90 up, 360 right, 90 down
        fprintf(stderr, "Starting test sequence...\n");
        
        // Move 360 degrees left
        fprintf(stderr, "Moving 360 degrees left...\n");
        for (int i = 0; i < 360; i++) {
            if (send_command_and_update_json("MAL001", json_path, curAz - (i + 1), curEl) != 0) {
                fprintf(stderr, "Test failed during left movement\n");
                free(json);
                return 10;
            }
        }
        curAz -= 360;
        
        // Move 90 degrees up
        fprintf(stderr, "Moving 90 degrees up...\n");
        for (int i = 0; i < 90; i++) {
            if (send_command_and_update_json("MEU01", json_path, curAz, curEl + (i + 1)) != 0) {
                fprintf(stderr, "Test failed during up movement\n");
                free(json);
                return 11;
            }
        }
        curEl += 90;
        
        // Move 360 degrees right
        fprintf(stderr, "Moving 360 degrees right...\n");
        for (int i = 0; i < 360; i++) {
            if (send_command_and_update_json("MAR001", json_path, curAz + (i + 1), curEl) != 0) {
                fprintf(stderr, "Test failed during right movement\n");
                free(json);
                return 12;
            }
        }
        curAz += 360;
        
        // Move 90 degrees down
        fprintf(stderr, "Moving 90 degrees down...\n");
        for (int i = 0; i < 90; i++) {
            if (send_command_and_update_json("MED01", json_path, curAz, curEl - (i + 1)) != 0) {
                fprintf(stderr, "Test failed during down movement\n");
                free(json);
                return 13;
            }
        }
        curEl -= 90;
        
        fprintf(stderr, "Test sequence completed successfully.\n");
        free(json);
        return 0;
    } else if (action == 'C') {
        // Calibrate: move to Az=0, El=90
        fprintf(stderr, "Starting calibration to Az=0, El=90...\n");
        
        int targetAz = 0;
        int targetEl = 90;
        
        // Move azimuth to 0
        if (curAz < targetAz) {
            int steps = targetAz - curAz;
            fprintf(stderr, "Moving %d degrees right to reach Az=0...\n", steps);
            for (int i = 0; i < steps; i++) {
                if (send_command_and_update_json("MAR001", json_path, curAz + (i + 1), curEl) != 0) {
                    fprintf(stderr, "Calibration failed during azimuth adjustment\n");
                    free(json);
                    return 14;
                }
            }
            curAz = targetAz;
        } else if (curAz > targetAz) {
            int steps = curAz - targetAz;
            fprintf(stderr, "Moving %d degrees left to reach Az=0...\n", steps);
            for (int i = 0; i < steps; i++) {
                if (send_command_and_update_json("MAL001", json_path, curAz - (i + 1), curEl) != 0) {
                    fprintf(stderr, "Calibration failed during azimuth adjustment\n");
                    free(json);
                    return 15;
                }
            }
            curAz = targetAz;
        }
        
        // Move elevation to 90
        if (curEl < targetEl) {
            int steps = targetEl - curEl;
            fprintf(stderr, "Moving %d degrees up to reach El=90...\n", steps);
            for (int i = 0; i < steps; i++) {
                if (send_command_and_update_json("MEU01", json_path, curAz, curEl + (i + 1)) != 0) {
                    fprintf(stderr, "Calibration failed during elevation adjustment\n");
                    free(json);
                    return 16;
                }
            }
            curEl = targetEl;
        } else if (curEl > targetEl) {
            int steps = curEl - targetEl;
            fprintf(stderr, "Moving %d degrees down to reach El=90...\n", steps);
            for (int i = 0; i < steps; i++) {
                if (send_command_and_update_json("MED01", json_path, curAz, curEl - (i + 1)) != 0) {
                    fprintf(stderr, "Calibration failed during elevation adjustment\n");
                    free(json);
                    return 17;
                }
            }
            curEl = targetEl;
        }
        
        fprintf(stderr, "Calibration completed: Az=%d, El=%d\n", curAz, curEl);
        free(json);
        return 0;
    } else if (action == 'L') {
        newAz = curAz - AZ_STEP;
        if (newAz < AZ_MIN) newAz = AZ_MIN;
    } else if (action == 'R') {
        newAz = curAz + AZ_STEP;
        if (newAz > AZ_MAX) newAz = AZ_MAX;
    } else if (action == 'T') {
        newEl = curEl + EL_STEP;
        if (newEl > EL_MAX) newEl = EL_MAX;
    } else if (action == 'B') {
        newEl = curEl - EL_STEP;
        if (newEl < EL_MIN) newEl = EL_MIN;
    }

    // If no movement needed, exit early
    if (newAz == curAz && newEl == curEl && action != 'C') {
        fprintf(stderr, "No movement needed (already at requested limit).\n");
        free(json);
        return 0;
    }

    // Build command for the Arduino
    // For relative one-degree steps (L/R/U/D or synonyms T/B) we send MAR/MAL/MEU/MED
    char command[32];
    if (action == 'C') {
        snprintf(command, sizeof(command), "C\n");
    } else if (action == 'L' || action == 'R' || action == 'U' || action == 'D' || action == 'T' || action == 'B') {
        // Relative 1-degree moves
        if (action == 'L') {
            snprintf(command, sizeof(command), "MAL%03d\n", 1);
        } else if (action == 'R') {
            snprintf(command, sizeof(command), "MAR%03d\n", 1);
        } else if (action == 'U' || action == 'T') {
            snprintf(command, sizeof(command), "MEU%02d\n", 1);
        } else { // D or B
            snprintf(command, sizeof(command), "MED%02d\n", 1);
        }
    } else {
        // Fallback to absolute goto command: AXXXEYY (3-digit az, 2-digit el)
        if (newAz < 0) newAz = 0;
        if (newAz > 999) newAz = 999; // safety
        if (newEl < 0) newEl = 0;
        if (newEl > 99) newEl = 99; // safety
        snprintf(command, sizeof(command), "A%03dE%02d\n", newAz, newEl);
    }

    /* Print the command so the user can see it before execution */
    printf("Sending command: %s", command);

    // Find Arduino port
    char port_path[256];
    if (find_arduino_port(port_path, sizeof(port_path)) != 0) {
        fprintf(stderr, "No Arduino serial device found.\n");
        free(json);
        return 6;
    }

    int fd = open_and_configure_port(port_path, B9600);
    if (fd < 0) {
        fprintf(stderr, "Failed to open serial port %s\n", port_path);
        free(json);
        return 7;
    }

    
    // Wait for a response line (timeout 5s)
    char reply[128] = {0};
    int rr = read_line_with_timeout(fd, reply, sizeof(reply), 5);
    if (rr > 0) {
        fprintf(stderr, "Arduino reply: %s\n", reply);
    } else {
        fprintf(stderr, "No reply from Arduino (timeout or error).\n");
    }

    // Send command
    printf("Writing to Arduino on %s: %s (%d)", port_path, command, strlen(command)); 
    ssize_t w = write(fd, command, strlen(command));
    if (w < 0) {
        fprintf(stderr, "Failed to write to serial port: %s\n", strerror(errno));
        close(fd);
        free(json);
        return 8;
    }
    tcdrain(fd);

    // Wait for a response line (timeout 5s)
    reply[0] = '\0'; // clear reply before sending command
    rr = read_line_with_timeout(fd, reply, sizeof(reply), 5);
    if (rr > 0) {
        fprintf(stderr, "Arduino reply: %s\n", reply);
    } else {
        fprintf(stderr, "No reply from Arduino (timeout or error).\n");
    }

    close(fd);

    // Update JSON: preserve existing LastMovements entries and append a new command
    char *inner = extract_last_movements_inner(json);
    
    // Build new JSON content
    char cmd_no_nl[32];
    // remove trailing newline/carriage from command
    strncpy(cmd_no_nl, command, sizeof(cmd_no_nl)-1);
    cmd_no_nl[sizeof(cmd_no_nl)-1] = '\0';
    for (size_t i = 0; i < strlen(cmd_no_nl); ++i) {
        if (cmd_no_nl[i] == '\n' || cmd_no_nl[i] == '\r') { cmd_no_nl[i] = '\0'; break; }
    }
    char new_item[128];
    snprintf(new_item, sizeof(new_item), "\"%s\"", cmd_no_nl);

    // Parse existing entries: collect quoted strings, skip whitespace and malformed commas
    char entries[2048] = {0}; // buffer to collect all cleaned entries
    int has_entries = 0;
    
    if (inner && strlen(inner) > 0) {
        const char *p = inner;
        while (*p) {
            // Skip whitespace and commas
            while (*p && (isspace((unsigned char)*p) || *p == ',')) ++p;
            if (!*p) break;
            
            // Check for quoted string
            if (*p == '"') {
                const char *start = p;
                ++p; // skip opening quote
                while (*p && *p != '"') ++p;
                if (*p == '"') {
                    ++p; // skip closing quote
                    // extract the quoted string
                    size_t len = (size_t)(p - start);
                    if (len > 2) { // at least "" 
                        if (has_entries) strcat(entries, ", ");
                        strncat(entries, start, len);
                        has_entries = 1;
                    }
                }
            } else {
                // skip unknown characters
                ++p;
            }
        }
    }
    
    // Build final entry list (append new item)
    char *new_last_inner = NULL;
    if (has_entries) {
        asprintf(&new_last_inner, "%s, %s", entries, new_item);
    } else {
        asprintf(&new_last_inner, "%s", new_item);
    }

    // Build final JSON (simple, replaces the whole file). Keep indentation minimal.
    FILE *out = fopen(json_path, "w");
    if (!out) {
        fprintf(stderr, "Failed to open JSON for writing: %s\n", strerror(errno));
        free(json); free(inner); free(new_last_inner);
        return 9;
    }

    fprintf(out, "{\n");
    fprintf(out, "  \"currentAz\": %d,\n", newAz);
    fprintf(out, "  \"currentEl\": %d,\n", newEl);
    fprintf(out, "  \"LastMovements\": [\n");

    // Print cleaned-up LastMovements entries (skip leading/trailing whitespace)
    if (new_last_inner && strlen(new_last_inner) > 0) {
        fprintf(out, "    %s\n", new_last_inner);
    }

    fprintf(out, "  ]\n");
    fprintf(out, "}\n");
    fclose(out);

    fprintf(stderr, "Updated JSON %s (Az:%d El:%d)\n", json_path, newAz, newEl);

    free(json);
    free(inner);
    free(new_last_inner);
    return 0;
}
