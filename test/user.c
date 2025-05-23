#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/stealthmem.h"

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <pid> <address> <size>\n", argv[0]);
        return 1;
    }

    char *endptr = NULL;
    const pid_t pid = strtol(argv[1], &endptr, 10);
    if (pid <= 0 || endptr == argv[1] || *endptr != '\0') {
        fprintf(stderr, "invalid pid: %s\n", argv[1]);
        return 1;
    }

    char *base_address = argv[2];
    int base = 10;
    if (!strncmp(argv[2], "0x", 2)) {
        base_address = argv[2] + 2;
        base = 16;
    } else if (base_address[0] == '-') {
        fprintf(stderr, "invalid negative address: %s\n", argv[2]);
        return 1;
    }

    const unsigned long address = strtoul(base_address, &endptr, base);

    if (endptr == base_address || *endptr != '\0') {
        fprintf(stderr, "invalid address: %s\n", argv[2]);
        return 1;
    }

    const size_t size = strtoul(argv[3], &endptr, 10);
    if (!size || endptr == argv[3] || *endptr != '\0') {
        fprintf(stderr, "invalid size: %s\n", argv[3]);
        return 1;
    }

    char *buffer = malloc(size);
    if (!buffer) {
        perror("malloc");
        return 1;
    }

    int fd = open("/dev/stealthmem", O_RDWR);
    if (fd < 0) {
        perror("open");
        free(buffer);
        return 1;
    }

    const struct memory_params params = {
        .pid = pid,
        .addr = address,
        .size = size,
        .buf = buffer,
    };
    const int bytes_read = ioctl(fd, IOCTL_READ_MEM, &params);
    if (bytes_read < 0) {
        perror("ioctl");
        close(fd);
        free(buffer);
        return 1;
    }

    printf("read %d bytes from pid %d at 0x%lx\n", bytes_read, pid, address);
    printf("read value: [");
    for (int i = 0; i < bytes_read; i++) {
        printf(i == 0 ? "%02x" : ", %02x", (unsigned char)buffer[i]);
    }
    printf("]\n");

    close(fd);
    free(buffer);
    return 0;
}
