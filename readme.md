# stealthmem

this is a linux kernel module to read and write process memory without getting detected.
it is also able to send arbitrary mouse move and key presses.

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

get the required structs and ioctl commands from the [header.](include/stealthmem.h)
you should be able to just include it in your program.

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

// all ioctls return a negative error code
// this returns the number of bytes read on success
const int bytes_read = ioctl(fd, IOCTL_READ_MEM, &params);

// if writing/reading string, don't forget the null terminator
params.buf = "hello, world";
params.size = 12;

const int bytes_written = ioctl(fd, IOCTL_WRITE_MEM, &params);

// mouse ioctls return 0 on success, or a negative error code
const struct mouse_move move = {.x = 20, .y = -35};
ioctl(fd, IOCTL_MOUSE_MOVE, &move);

// 1 to press, 0 to release a key. keys are defined in <linux/input-event-codes.h>
const struct key_press key = {.key = KEY_A, .press = 1};
// as always, negative error code, or 0 on success
ioctl(fd, IOCTL_KEY_PRESS, &key);
```

## performance

on my system, time taken by the ioctl memory syscalls is right between `process_vm_readv` and reading `/proc/{pid}/mem`.
the former takes 1.5 µs, the latter 1.75 µs. one stealthmem ioctl memory read takes 1.6 µs.

## sample programs

the makefile generates multiple sample command line programs from [test/mem.c](test/mem.c), [test/mouse.c](test/mouse.c) and [test/key.c](test/key.c).

they can be used as references on how to use the kernel module.
