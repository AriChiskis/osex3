#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "message_slot.h"  // Include the header for MSG_SLOT_CHANNEL and other definitions

int main(int argc, char *argv[]) {
    int fd;
    unsigned long channel_id;

    // Validate the correct number of arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <device file path> <channel id> <message>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Convert channel id from string to long
    channel_id = strtoul(argv[2], NULL, 10);
    if (errno == ERANGE) {
        perror("Error converting channel id");
        exit(EXIT_FAILURE);
    }

    // Open the specified message slot device file
    fd = open(argv[1], O_WRONLY);
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

    // Write the specified message to the message slot file
    if (write(fd, argv[3], strlen(argv[3])) < 0) {
        perror("Error writing message");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Close the device file
    close(fd);

    // Exit the program with success
    exit(EXIT_SUCCESS);
}
