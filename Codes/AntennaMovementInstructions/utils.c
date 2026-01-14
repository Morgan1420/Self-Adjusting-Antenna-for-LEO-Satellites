#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <ctype.h>
#include <termios.h>

#include "utils.h"
#include "JSON_related_functions.h"
#include "../Arduino_stuff/arduino_communications.h"

/* Invert a command while preserving numeric width (e.g., MAR010 -> MAL010) */
void invert_command(const char *cmd, char *out, size_t out_len) {
    if (!cmd || !out || out_len < 8) return;

    // Map prefix
    const char *prefix = NULL;
    if (cmd[0] == 'C') {
        // Calibration command; keep as is with single newline
        snprintf(out, out_len, "C\n");
        return;
    } else if (strncmp(cmd, "MAL", 3) == 0) {
        prefix = "MAR";
    } else if (strncmp(cmd, "MAR", 3) == 0) {
        prefix = "MAL";
    } else if (strncmp(cmd, "MEU", 3) == 0) {
        prefix = "MED";
    } else if (strncmp(cmd, "MED", 3) == 0) {
        prefix = "MEU";
    }

    if (prefix) {
        // Extract only the numeric payload (preserve zero padding and width)
        const char *digits = cmd + 3;
        char num[16];
        size_t n = 0;
        while (digits[n] && isdigit((unsigned char)digits[n]) && n < sizeof(num) - 1) {
            num[n] = digits[n];
            n++;
        }
        num[n] = '\0';
        snprintf(out, out_len, "%s%s\n", prefix, num);
    } else {
        // Unknown prefix; send as-is with a single newline, strip any existing CR/LF
        char clean[32];
        size_t m = 0;
        for (size_t i = 0; cmd[i] && m < sizeof(clean) - 1; ++i) {
            if (cmd[i] != '\n' && cmd[i] != '\r') {
                clean[m++] = cmd[i];
            }
        }
        clean[m] = '\0';
        snprintf(out, out_len, "%s\n", clean);
    }
}

/* Wait for a line from fd with timeout seconds; the line is stored in buf (buflen-1), returns bytes read or -1 on timeout/error */
int read_line_with_timeout(int fd, char *buf, size_t buflen, int timeout_seconds) {
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

// Funció que envia una comanda a l'Arduino i actualitza el fitxer JSON
int send_command_and_update_json(const char *command, const char *json_path, int newAz, int newEl) {
    // ------ 1: Establim connexió amb l'Arduino
    // Busquem el port
    char port_path[256];
    if (find_arduino_port(port_path, sizeof(port_path)) != 0) {
        fprintf(stderr, "No Arduino serial device found.\n");
        return -1;
    }
    // Obrim i configurem el port
    int fd = open_and_configure_port(port_path, B9600);
    if (fd < 0) {
        fprintf(stderr, "Failed to open serial port %s\n", port_path);
        return -1;
    }
    // Esperem resposta inicial
    char reply[128] = {0};
    int rr = read_line_with_timeout(fd, reply, sizeof(reply), 30);
    if (rr > 0) {
        fprintf(stderr, "Arduino initial reply: %s\n", reply);
    }

    // ------ 2: Enviem la comanda
    // Enviem la comanda amb \n al final perquè l'Arduino reconegui el final
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

    // Esperem resposta
    reply[0] = '\0';
    rr = read_line_with_timeout(fd, reply, sizeof(reply), 30);
    if (rr > 0) {
        fprintf(stderr, "Arduino reply: %s\n", reply);
    }

    // Tanquem el port
    close(fd);

    // ------ 3: Actualitzem el fitxer JSON
    // Llegim el fitxer JSON
    char *json = read_file_alloc(json_path);
    if (!json) {
        fprintf(stderr, "Failed to read JSON for update\n");
        return -1;
    }

    // Extraiem el vector de "LastMovements"
    char *inner = extract_last_movements_inner(json);
    
    // Construïm un nou vector de manera segura (dinàmic per evitar overflow)
    char new_item[128];
    snprintf(new_item, sizeof(new_item), "\"%s\"", command);

    size_t cap = 256;
    size_t len = 0;
    char *entries = malloc(cap);
    if (!entries) {
        free(json);
        free(inner);
        return -1;
    }
    entries[0] = '\0';
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
                    size_t seglen = (size_t)(p - start);
                    if (seglen > 2) {
                        size_t need = len + (has_entries ? 2 : 0) + seglen + 1;
                        if (need > cap) {
                            while (cap < need) cap *= 2;
                            char *tmp = realloc(entries, cap);
                            if (!tmp) { free(entries); free(json); free(inner); return -1; }
                            entries = tmp;
                        }
                        if (has_entries) { strcat(entries, ", "); len += 2; }
                        strncat(entries, start, seglen); len += seglen;
                        has_entries = 1;
                    }
                }
            } else {
                ++p;
            }
        }
    }

    // Afegim la nova entrada al principi
    char *new_last_inner = NULL;
    if (has_entries) {
        asprintf(&new_last_inner, "%s, %s", entries, new_item);
    } else {
        asprintf(&new_last_inner, "%s", new_item);
    }

    free(entries);

    // Scrivim el nou fitxer JSON
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

    // Alliberem memòria
    free(json);
    free(inner);
    free(new_last_inner);
    
    return 0;
}
