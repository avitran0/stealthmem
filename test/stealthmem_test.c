#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

struct read_memory_params {
    pid_t pid;
    unsigned long addr;
    size_t size;
    void *buf;
};

#define IOCTL_MAGIC 0xBC
#define IOCTL_READ_MEM _IOWR(IOCTL_MAGIC, 1, struct read_memory_params)

int main() {
    int fd = open("/dev/stealthmem", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    const char buffer[] = "hello, world";

    struct read_memory_params params;
    params.pid = getpid();
    params.addr = (unsigned long)buffer;
    params.size = sizeof(buffer);
    params.buf = malloc(params.size);

    if (!params.buf) {
        perror("malloc");
        close(fd);
        return 1;
    }

    const int ret = ioctl(fd, IOCTL_READ_MEM, &params);

    if (ret < 0) {
        perror("ioctl");
    } else {
        printf("read %d bytes from pid %d at 0x%lx\n", ret, params.pid, params.addr);
        printf("read: %s\n", (char *)params.buf);
    }

    free(params.buf);
    close(fd);
    return 0;
}
