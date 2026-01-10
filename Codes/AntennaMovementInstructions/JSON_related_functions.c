#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

#include "JSON_related_functions.h"

#define JSON_PATH "./comm_files/antennaStatus.json"

/* Get absolute path to JSON file based on executable location */
void get_absolute_json_path(const char *argv0, char *json_path, size_t json_path_len) {
    char exe_dir[512] = {0};
    
    // Use /proc/self/exe to get the actual executable path (most reliable)
    char proc_path[512] = {0};
    ssize_t len = readlink("/proc/self/exe", proc_path, sizeof(proc_path) - 1);
    
    if (len > 0) {
        proc_path[len] = '\0';
        // Extract directory from absolute path
        char *last_slash = strrchr(proc_path, '/');
        if (last_slash) {
            size_t dir_len = (size_t)(last_slash - proc_path);
            if (dir_len >= sizeof(exe_dir)) dir_len = sizeof(exe_dir) - 1;
            strncpy(exe_dir, proc_path, dir_len);
            exe_dir[dir_len] = '\0';
        }
    } else {
        // Fallback: if /proc/self/exe doesn't work, return relative path
        strncpy(exe_dir, "./comm_files", sizeof(exe_dir) - 1);
        snprintf(json_path, json_path_len, "%s/antennaStatus.json", exe_dir);
        return;
    }
    
    // Construct full JSON path
    snprintf(json_path, json_path_len, "%s/comm_files/antennaStatus.json", exe_dir);
}

/* Read whole file into malloc'd buffer (caller must free). Returns NULL on error. */
char *read_file_alloc(const char *path) {
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
int parse_json_int(const char *buf, const char *key, int *out_value) {
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
char *extract_last_movements_inner(const char *buf) {
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
void timestamp_now(char *out, size_t n) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(out, n, "%Y-%m-%d %H:%M:%S", &tm);
}

/* Parse both currentAz and currentEl from JSON in one call */
int parse_json_position(const char *buf, int *out_az, int *out_el) {
    if (!buf || !out_az || !out_el) return -1;
    
    int az = 0, el = 0;
    if (parse_json_int(buf, "currentAz", &az) != 0) {
        return -1;
    }
    if (parse_json_int(buf, "currentEl", &el) != 0) {
        return -1;
    }
    
    *out_az = az;
    *out_el = el;
    return 0;
}


