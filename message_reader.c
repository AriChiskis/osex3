#include <fcntl.h>      // For open()
#include <stdio.h>      // For perror() and printf()
#include <stdlib.h>     // For exit() and EXIT_FAILURE
#include <string.h>     // For strerror()
#include <sys/ioctl.h>  // For ioctl()
#include <unistd.h>     // For read(), write(), and close()
#include "message_slot.h"
int main(int argc, char *argv[]) {
    int fd, ret;
    unsigned long channel_id;
    char buffer[128];  // Assuming max message size is 128 as per kernel module

    // Validate the correct number of command-line arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <device file path> <channel id>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Convert channel id from string to long
    channel_id = strtoul(argv[2], NULL, 10);

    // Open the specified message slot device file
    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("Error opening device file");
        exit(EXIT_FAILURE);
    }

    // Set the channel id
    if (ioctl(fd, MSG_SLOT_CHANNEL, channel_id) != 0) {
        perror("Error setting channel id");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Read a message from the message slot file to a buffer
    ret = read(fd, buffer, sizeof(buffer));
    if (ret < 0) {
        perror("Error reading message");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Close the device file
    close(fd);

    // Print the message to standard output
    if (write(STDOUT_FILENO, buffer, ret) < 0) {
        perror("Error writing message to stdout");
        exit(EXIT_FAILURE);
    }

    // Exit the program with success
    return 0;
}
