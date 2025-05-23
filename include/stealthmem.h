#pragma once

#ifdef __KERNEL__

#include <linux/ioctl.h>
#include <linux/pid.h>

struct memory_params {
    pid_t pid;
    unsigned long addr;
    size_t size;
    void __user *buf;
};

#else

#include <fcntl.h>
#include <stddef.h>
#include <sys/ioctl.h>

struct memory_params {
    pid_t pid;
    unsigned long addr;
    size_t size;
    void *buf;
};

#endif

#define IOCTL_MAGIC_READ 0xBC
#define IOCTL_MAGIC_WRITE 0xBD
#define IOCTL_READ_MEM _IOWR(IOCTL_MAGIC_READ, 1, struct memory_params)
#define IOCTL_WRITE_MEM _IOWR(IOCTL_MAGIC_WRITE, 1, struct memory_params)
