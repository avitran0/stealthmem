#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/stealthmem.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <x> <y>\n", argv[0]);
        return 1;
    }

    char *endptr = NULL;
    const int x = strtol(argv[1], &endptr, 10);
    if (endptr == argv[1] || *endptr != '\0') {
        fprintf(stderr, "invalid x: %s\n", argv[1]);
        return 1;
    }

    const int y = strtol(argv[1], &endptr, 10);
    if (endptr == argv[1] || *endptr != '\0') {
        fprintf(stderr, "invalid y: %s\n", argv[2]);
        return 1;
    }

    int fd = open("/dev/stealthmem", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    const struct mouse_move move = {.x = x, .y = y};
    const int bytes_read = ioctl(fd, IOCTL_MOUSE_MOVE, &move);
    if (bytes_read < 0) {
        perror("ioctl");
        close(fd);
        return 1;
    }
    printf("moved mouse by %d/%d\n", move.x, move.y);

    close(fd);
    return 0;
}
