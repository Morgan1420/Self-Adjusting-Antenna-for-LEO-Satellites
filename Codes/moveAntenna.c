/*
 * moveAntenna.c
 *
 * Usage: ./moveAntenna <action> [step]
 *   action: one of T (test), C (calibrate), B (back/undo), O (origin/undo all), F (force 0-90), U (up), D (down), L (left), R (right)
 *   step: optional step size for movement commands (U, D, L, R), defaults to 1
 *
 * Behavior:
 *  - Reads the JSON file at ./comm_files/antennaStatus.json (expects fields `currentAz` and `currentEl` and `LastMovements` array)
 *  - Validates the requested action and clamps against allowed ranges
 *  - Constructs Arduino commands (MAH, MAA, MEU, MED for relative movements)
 *  - Finds and opens the Arduino serial port using existing helpers in Arduino_stuff
 *  - Sends the command and waits briefly for a reply line ("DONE" or other)
 *  - Updates the JSON file: writes new `currentAz`, `currentEl` and appends a record to `LastMovements`
 *
 * Build:
 *   gcc -o moveAntenna moveAntenna.c Arduino_stuff/arduino_communications.c AntennaMovementInstructions/utils.c AntennaMovementInstructions/JSON_related_functions.c AntennaMovementInstructions/command_handler_functions.c -IArduino_stuff -Wall -Wextra
 
 * Examples:
 *   # Calibrate (move to Az=0, El=90)
 *   ./moveAntenna C
 *
 *   # Test sequence (360 left, 90 up, 360 right, 90 down)
 *   ./moveAntenna T
 *
 *   # Move left by one step
 *   ./moveAntenna L
 *
 *   # Move left by 5 steps
 *   ./moveAntenna L 5
 *
 *   # Move up (increase elevation)
 *   ./moveAntenna U
 *
 *   # Move down (decrease elevation)
 *   ./moveAntenna D
 *
 *   # Undo last movement
 *   ./moveAntenna B
 *
 *   # Undo all movements (return to origin)
 *   ./moveAntenna O
 *
 *   # Force 0-90 position
 *   ./moveAntenna F
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
#include "AntennaMovementInstructions/JSON_related_functions.h"
#include "AntennaMovementInstructions/utils.h"
#include "AntennaMovementInstructions/command_handler_functions.h"

#define AZ_MIN -360
#define AZ_MAX 360
#define EL_MIN 0
#define EL_MAX 90

/* Hardcoded JSON path */
#define JSON_PATH "./comm_files/antennaStatus.json"

/* default step sizes (degrees) for each action */
#define AZ_STEP 1
#define EL_STEP 1

int main(int argc, char **argv) {
    // ------- 1: Tractem els arguments
    // Comprovem que hi hagi un nombre correcte d'arguments 
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <action (T|C|B|O|F|U|D|L|R)> [step]\n", argv[0]);
        return 2;
    }

    // Comprovem que l'acció (1r arg) sigui vàlida  
    char action = argv[1][0];
    if (action == 0) { fprintf(stderr, "Invalid action\n"); return 3; }
    action = (char)toupper((unsigned char)action);

    if (!(action == 'T' || action == 'C' || action == 'B' || action == 'O' || action == 'F' || action == 'U' || action == 'D' || action == 'L' || action == 'R')) {
        fprintf(stderr, "Unknown action '%c'\n", action);
        return 4;
    }

    // Comprovem el "step" (2n arg) sigui correcte, en cas que el necessitem
    int step = 1; // default step
    if (argc == 3) {
        step = atoi(argv[2]);
        if (step < 1 || step > 360) {
            fprintf(stderr, "Invalid step value: %d (must be 1-360)\n", step);
            return 3;
        }
    }
    
    // ------- 2: Llegim el fitxer JSON per obtenir la posició actual
    // Aconseguim el "path" absolut del fitxer JSON
    char json_path[1024];
    get_absolute_json_path(argv[0], json_path, sizeof(json_path));
    
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
    // ------- 3: Tractem l'acció sol·licitada
    int result = 0;
    if (action == 'O') {
        result = handle_origin_action(json, json_path);
    } else if (action == 'B') {
        result = handle_back_action(json, json_path);
    } else if (action == 'T') {
        result = handle_test_action(json_path, curAz, curEl);
    } else if (action == 'C') {
        result = handle_calibrate_action(json_path, curAz, curEl);
    } else if (action == 'F') {
        result = handle_force_action(json_path, curAz, curEl);
    } else if (action == 'L' || action == 'R' || action == 'U' || action == 'D') {
        result = handle_movement_action(action, step, json, json_path, curAz, curEl);
    }
    
    free(json);
    return result;
}
