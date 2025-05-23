#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

struct memory_params {
    pid_t pid;
    unsigned long addr;
    size_t size;
    void *buf;
};

#define IOCTL_MAGIC_READ 0xBC
#define IOCTL_MAGIC_WRITE 0xBD
#define IOCTL_READ_MEM _IOWR(IOCTL_MAGIC_READ, 1, struct memory_params)
#define IOCTL_WRITE_MEM _IOWR(IOCTL_MAGIC_WRITE, 1, struct memory_params)

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
    } else {
        printf("read %d bytes from pid %d at 0x%lx\n", ret_read, params.pid, params.addr);
        printf("read: %s : %s\n", (char *)params.buf, buffer);
    }

    params.buf = "goodbye, me";
    params.size = 12;
    const int ret_write = ioctl(fd, IOCTL_WRITE_MEM, &params);

    if (ret_write < 0) {
        perror("ioctl");
    } else {
        printf("wrote %d bytes from pid %d at 0x%lx\n", ret_write, params.pid, params.addr);
        printf("wrote: %s : %s\n", (char *)params.buf, buffer);
    }

    free(alloced_buf);
    close(fd);
    return 0;
}
