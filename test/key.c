#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/stealthmem.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <key>\n", argv[0]);
        return 1;
    }

    char *endptr = NULL;
    const int key = strtol(argv[1], &endptr, 10);
    if (endptr == argv[1] || *endptr != '\0') {
        fprintf(stderr, "invalid x: %s\n", argv[1]);
        return 1;
    }

    int fd = open("/dev/stealthmem", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct key_press btn_event = {.key = key, .press = 1};
    int result = ioctl(fd, IOCTL_KEY_PRESS, &btn_event);
    if (result < 0) {
        perror("ioctl");
        close(fd);
        return 1;
    }

    btn_event.press = 0;
    result = ioctl(fd, IOCTL_KEY_PRESS, &btn_event);
    if (result < 0) {
        perror("ioctl");
        close(fd);
        return 1;
    }

    printf("pressed key %d\n", btn_event.key);

    close(fd);
    return 0;
}
