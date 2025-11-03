#include <stdio.h>    // Standard input/output definitions
#include <string.h>   // String function definitions
#include <unistd.h>   // UNIX standard function definitions (read, write, close)
#include <fcntl.h>    // File control definitions
#include <errno.h>    // Error number definitions
#include <termios.h>  // POSIX terminal control definitions



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
