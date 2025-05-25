# stealthmem

this is a linux kernel module to read and write process memory without getting detected.

## requirements

- kernel headers (see below)
- `make`
- `gcc`

make and gcc are usually pre-installed on most distros.
if not, search for how to install them on your specific os.

### kernel headers

- ubuntu/debian: `sudo apt-get install linux-headers-$(uname -r)`
- fedora: `sudo dnf install kernel-devel-$(uname -r)`
- centos/rhel (old): `sudo yum install kernel-devel-$(uname -r)`
- arch: `sudo pacman -Syu linux-headers`
- opensuse: `sudo zypper install kernel-devel`

## installation

1. clone repository with `git clone https://github.com/avitran0/stealthmem`
2. build the module using `make`
3. load the module using `make load`
4. a device will appear in `/dev/stealthmem`, check if it exists

## uninstallation

run `make unload`, or restart computer. the module has to be loaded on every restart manually.

## usage in a program

you can copy these into your program, or use the [header.](include/stealthmem.h)

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

## performance

on my system, time taken by the ioctl syscall is right between `process_vm_readv` and reading `/proc/{pid}/mem`.
the former takes 1.5 µs, the latter 1.75 µs. one stealthmem ioctl takes 1.6 µs.

## sample program

the makefile generates a sample command line program from [test/user.c](test/user.c).

this can be used with `./build/user <pid> <address> <size>`, where address might be in decimal or hexadecimal.

there is also a sample rust program in [user/src/main.rs](user/src/main.rs), which does exactly the same thing as the c version.
