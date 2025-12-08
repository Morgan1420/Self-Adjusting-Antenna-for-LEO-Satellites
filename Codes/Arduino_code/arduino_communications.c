#include <stdio.h>    // Standard input/output definitions
#include <string.h>   // String function definitions
#include <unistd.h>   // UNIX standard function definitions (read, write, close)
#include <fcntl.h>    // File control definitions
#include <errno.h>    // Error number definitions
#include <termios.h>  // POSIX terminal control definitions
#include <dirent.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>
#include <ctype.h>


// Read first non-empty line from a file into buf (null-terminated). Returns 0 on success.
static int read_first_line(const char *path, char *buf, size_t len) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char *res = fgets(buf, (int)len, f);
    fclose(f);
    if (!res) return -1;
    // Trim newline and carriage return
    size_t l = strlen(buf);
    while (l > 0 && (buf[l-1] == '\n' || buf[l-1] == '\r')) {
        buf[--l] = '\0';
    }
    return 0;
}

// Check whether the string contains case-insensitive substring
static bool contains_ci(const char *hay, const char *needle) {
    if (!hay || !needle) return false;
    char *h = strdup(hay);
    char *n = strdup(needle);
    if (!h || !n) { free(h); free(n); return false; }
    for (char *p = h; *p; ++p) *p = (char)tolower((unsigned char)*p);
    for (char *p = n; *p; ++p) *p = (char)tolower((unsigned char)*p);
    bool ok = strstr(h, n) != NULL;
    free(h); free(n);
    return ok;
}

// Try to detect a connected Arduino by scanning /dev/ttyACM* and /dev/ttyUSB* and
// inspecting sysfs for USB vendor/product/manufacturer hints. Writes the first match
// into out_path and returns 0; returns -1 if nothing found.
int find_arduino_port(char *out_path, size_t out_len) {
    const char *prefixes[] = { "/dev/ttyACM", "/dev/ttyUSB" };
    char dev[PATH_MAX];
    char sysbase[PATH_MAX];
    char buf[256];

    for (size_t p = 0; p < sizeof(prefixes)/sizeof(prefixes[0]); ++p) {
        for (int i = 0; i < 16; ++i) {
            snprintf(dev, sizeof(dev), "%s%d", prefixes[p], i);
            if (access(dev, F_OK) != 0) continue;

            // Build sysfs path for the tty device
            snprintf(sysbase, sizeof(sysbase), "/sys/class/tty/%s/device", basename(dev));

            // Try a few likely parent levels to find idVendor/manufacturer
            char probe[PATH_MAX];
            char candidate[PATH_MAX];
            // levels: device, device/.., device/../.. up to 4
            strcpy(candidate, sysbase);
            for (int level = 0; level < 5; ++level) {
                // check manufacturer
                snprintf(probe, sizeof(probe), "%s/manufacturer", candidate);
                if (read_first_line(probe, buf, sizeof(buf)) == 0) {
                    if (contains_ci(buf, "arduino") || contains_ci(buf, "arduino.cc") || contains_ci(buf, "arduino.org")) {
                        strncpy(out_path, dev, out_len-1);
                        out_path[out_len-1] = '\0';
                        return 0;
                    }
                }

                // check idVendor
                snprintf(probe, sizeof(probe), "%s/idVendor", candidate);
                if (read_first_line(probe, buf, sizeof(buf)) == 0) {
                    // known usb serial vendors: Arduino (0x2341/0x2A03), ch340 (0x1a86), ftdi (0x0403), pl2303 (0x067b), cdc (0x239a), silabs (0x10c4)
                    int vid = (int)strtol(buf, NULL, 16);
                    if (vid == 0x2341 || vid == 0x2A03 || vid == 0x1A86 || vid == 0x0403 || vid == 0x067B || vid == 0x10C4) {
                        strncpy(out_path, dev, out_len-1);
                        out_path[out_len-1] = '\0';
                        return 0;
                    }
                }

                // move up one level: append /.. and normalize
                snprintf(probe, sizeof(probe), "%s/..", candidate);
                if (realpath(probe, candidate) == NULL) break;
            }

            // As fallback, consider a ttyACM/ttyUSB present as candidate
            // prefer ttyACM over ttyUSB by prefix order
            strncpy(out_path, dev, out_len-1);
            out_path[out_len-1] = '\0';
            return 0;
        }
    }
    return -1;
}


/**
 * @brief Opens and configures the serial port.
 * * @param port_name The file path of the serial port (e.g., "/dev/ttyACM0").
 * @param baud_rate The baud rate to use (e.g., 9600).
 * @return int The file descriptor of the opened port, or -1 on error.
 */
int open_and_configure_port(const char *port_name, speed_t baud_rate) {
    // O_RDWR: Open for reading and writing
    // O_NOCTTY: Terminal controlling options are not to be used
    // O_NDELAY: Allows the program to run even if the port is not ready
    int fd = open(port_name, O_RDWR | O_NOCTTY | O_NDELAY);

    if (fd == -1) {
        perror("Error opening serial port");
        return -1;
    }

    // Clear the O_NDELAY flag, so the read() and write() calls are blocking
    // in case they need to wait for data (though here we only write).
    fcntl(fd, F_SETFL, 0);

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    // Get the current attributes
    if (tcgetattr(fd, &tty) != 0) {
        printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
        close(fd);
        return -1;
    }

    // Set the input and output baud rates
    cfsetospeed(&tty, baud_rate);
    cfsetispeed(&tty, baud_rate);

    // --- Configuration Flags ---

    // 8N1 (8 data bits, no parity, 1 stop bit)
    tty.c_cflag &= ~PARENB;      // Disable parity bit
    tty.c_cflag &= ~CSTOPB;      // Use one stop bit
    tty.c_cflag &= ~CSIZE;       // Clear all size bits
    tty.c_cflag |= CS8;          // 8 data bits
    tty.c_cflag |= CREAD;        // Enable receiver
    tty.c_cflag |= CLOCAL;       // Ignore modem control lines

    // Raw input mode (no processing of input characters)
    tty.c_lflag &= ~ICANON;      // Disable canonical mode (line-based input)
    tty.c_lflag &= ~ECHO;        // Disable echo
    tty.c_lflag &= ~ECHOE;       // Disable erasure
    tty.c_lflag &= ~ECHONL;      // Disable new-line echo

    // Disable software flow control
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    // Raw output mode (no output processing)
    tty.c_oflag &= ~OPOST;

    // Apply the settings
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
        close(fd);
        return -1;
    }

    printf("Successfully configured serial port %s at 9600 baud.\n", port_name);
    return fd;
}
