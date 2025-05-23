#include "../include/stealthmem.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    int fd = open("/dev/stealthmem", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    const char buffer[] = "hello world";

    char *alloced_buf = malloc(sizeof(buffer));
    if (!alloced_buf) {
        perror("malloc");
        close(fd);
        return 1;
    }

    struct memory_params params;
    params.pid = getpid();
    params.addr = (unsigned long)buffer;
    params.size = sizeof(buffer);
    params.buf = alloced_buf;

    const int ret_read = ioctl(fd, IOCTL_READ_MEM, &params);

    if (ret_read < 0) {
        perror("ioctl");
        return 1;
    } else {
        printf("read %d bytes from pid %d at 0x%lx\n", ret_read, params.pid, params.addr);
        printf("read: %s : %s\n", (char *)params.buf, buffer);
    }

    params.buf = "goodbye, me";
    params.size = 12;
    const int ret_write = ioctl(fd, IOCTL_WRITE_MEM, &params);

    if (ret_write < 0) {
        perror("ioctl");
        return 1;
    } else {
        printf("wrote %d bytes from pid %d at 0x%lx\n", ret_write, params.pid, params.addr);
        printf("wrote: %s : %s\n", (char *)params.buf, buffer);
    }

    printf("stealthmem test success\n");
    free(alloced_buf);
    close(fd);
    return 0;
}
