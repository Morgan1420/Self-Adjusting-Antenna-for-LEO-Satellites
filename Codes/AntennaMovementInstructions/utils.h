#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

/* Invert a command: MAL -> MAR, MAR -> MAL, MEU -> MED, MED -> MEU, C -> C */
void invert_command(const char *cmd, char *out, size_t out_len);

/* Wait for a line from fd with timeout seconds; the line is stored in buf (buflen-1), returns bytes read or -1 on timeout/error */
int read_line_with_timeout(int fd, char *buf, size_t buflen, int timeout_seconds);

/* Helper function to send a command to Arduino and update the JSON file */
int send_command_and_update_json(const char *command, const char *json_path, int newAz, int newEl);

#endif // UTILS_H
