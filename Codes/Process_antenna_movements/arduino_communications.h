// Header for Arduino serial communication helpers
#ifndef ARDUINO_COMMUNICATIONS_H
#define ARDUINO_COMMUNICATIONS_H

#include <termios.h>
#include <unistd.h>

// Open and configure a serial port. Returns file descriptor or -1 on error.
int open_and_configure_port(const char *port_name, speed_t baud_rate);

#endif // ARDUINO_COMMUNICATIONS_H