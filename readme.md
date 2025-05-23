# stealthmem

this is a linux kernel module to read and write process memory without getting detected.

## installation

1. clone repository with `git clone https://github.com/avitran0/stealthmem`
2. build the module using `make`
3. load the module using `make load`
4. a device will appear in `/dev/stealthmem`, check if it exists

## uninstallation

run `make unload`, or restart computer. the module has to be loaded on every restart manually.

## usage in a program

you can copy these into your program, or use the header in the `include` directory.

required struct and ioctl commands:

```c
#include <fcntl.h>
#include <stddef.h>
#include <sys/ioctl.h>

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
```

example:

```c
int fd = open("/dev/stealthmem", O_RDWR);
// check if open was successful

// prepare parameters to read
struct memory_params params = {
    // pid of the process to read from
    .pid = getpid(),
    // address in the process to read from
    .addr = 0x12345678,
    // how many bytes to read
    .size = 12,
    // any buffer to write the memory to, can also be stack-allocated
    .buf = malloc(12),
};

const int bytes_read = ioctl(fd, IOCTL_READ_MEM, &params);
if (bytes_read < 0) {
  // returns a negative error code (-EINVAL etc.) on failure
} else {
  // success
}

// if writing/reading string, don't forget the null terminator
params.buf = "hello, world";
params.size = 12;

const int bytes_written = ioctl(fd, IOCTL_WRITE_MEM, &params);
if (bytes_written < 0) {
  // returns a negative error code (-EINVAL etc.) on failure
} else {
  // success
}
```
