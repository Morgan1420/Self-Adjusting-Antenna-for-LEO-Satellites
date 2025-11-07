// Header for Arduino serial communication helpers
#ifndef ARDUINO_COMMUNICATIONS_H
#define ARDUINO_COMMUNICATIONS_H

#include <termios.h>
#include <unistd.h>

// Open and configure a serial port. Returns file descriptor or -1 on error.
int open_and_configure_port(const char *port_name, speed_t baud_rate);

// Find an Arduino serial device. Writes the device path (e.g. /dev/ttyACM0)
// into out_path (size out_len). Returns 0 on success, -1 if none found.
int find_arduino_port(char *out_path, size_t out_len);

#endif // ARDUINO_COMMUNICATIONS_H