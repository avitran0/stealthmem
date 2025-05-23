# stealthmem

this is a linux kernel module to read process memory without getting detected

## installation

1. clone repository with `git clone https://github.com/avitran0/stealthmem`
2. build the module using `make`
3. load the module using `make load`
4. a device will appear in `/dev/stealthmem`, check if it exists

## uninstallation

run `make unload`, or restart computer. the module has to be loaded on every restart manually

## usage in a program

required struct and ioctl command:

```c
struct read_memory_params {
    pid_t pid;
    unsigned long addr;
    size_t size;
    void *buf;
};

#define IOCTL_MAGIC 0xBC
#define IOCTL_READ_MEM _IOWR(IOCTL_MAGIC, 1, struct read_memory_params)
```

example:

```c
int fd = open("/dev/stealthmem", O_RDWR);
// check if open was successful

// prepare parameters to read
struct read_memory_params params = {
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
```
