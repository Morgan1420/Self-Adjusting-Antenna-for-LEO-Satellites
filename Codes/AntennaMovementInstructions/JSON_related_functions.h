#ifndef JSON_RELATED_FUNCTIONS_H
#define JSON_RELATED_FUNCTIONS_H

#include <time.h>

/* Get absolute path to JSON file based on executable location */
void get_absolute_json_path(const char *argv0, char *json_path, size_t json_path_len);

/* Read whole file into malloc'd buffer (caller must free). Returns NULL on error. */
char *read_file_alloc(const char *path);

/* Simple JSON integer extraction: find the key and parse the number after ':' */
int parse_json_int(const char *buf, const char *key, int *out_value);

/* Extract the textual contents of the LastMovements array (between [ and ]) into a newly allocated string.
 * Caller must free. Returns NULL on failure (treat as empty array). */
char *extract_last_movements_inner(const char *buf);

/* Helper: get current timestamp string */
void timestamp_now(char *out, size_t n);

/* Parse both currentAz and currentEl from JSON in one call. Returns 0 on success. */
int parse_json_position(const char *buf, int *out_az, int *out_el);

#endif // JSON_RELATED_FUNCTIONS_H
