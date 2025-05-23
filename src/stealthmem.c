#include "../include/stealthmem.h"

#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#define DEVICE_NAME "stealthmem"
#define MAX_MEM_SIZE (1024 * 1024)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("avitrano");
MODULE_DESCRIPTION("stealth process memory read/write module");
MODULE_VERSION("1.0");

static int major;
static struct class *stealthmem_class;
static struct device *stealthmem_device;
static struct cdev stealthmem_cdev;

// validate memory read parameters, so that id does not read bs data
static bool validate_params(const struct memory_params *params) {
    if (params->size == 0 || params->size > MAX_MEM_SIZE) {
        pr_warn("%s: invalid size: %zu\n", DEVICE_NAME, params->size);
        return false;
    }
    if (params->pid <= 0) {
        pr_warn("%s: invalid pid: %d\n", DEVICE_NAME, params->pid);
        return false;
    }
    if (!params->buf) {
        pr_warn("%s: user buffer is null\n", DEVICE_NAME);
        return false;
    }
    return true;
}

// returns bytes read, or negative error code
static int memory_read(
    struct task_struct *task, const unsigned long address, void *kbuf, const size_t size) {
    struct mm_struct *mm = get_task_mm(task);
    if (!mm) {
        return -ESRCH;
    }

    down_read(&mm->mmap_lock);
    const int bytes_read = access_process_vm(task, address, kbuf, (int)size, 0);
    up_read(&mm->mmap_lock);

    mmput(mm);
    return bytes_read;
}

static int memory_write(
    struct task_struct *task, const unsigned long address, void *kbuf, const size_t size) {
    struct mm_struct *mm = get_task_mm(task);
    if (!mm) {
        return -ESRCH;
    }

    down_read(&mm->mmap_lock);
    const int bytes_written =
        access_process_vm(task, address, kbuf, (int)size, FOLL_WRITE | FOLL_FORCE);
    up_read(&mm->mmap_lock);

    mmput(mm);
    return bytes_written;
}

static long device_ioctl(struct file *file, const unsigned int cmd, const unsigned long arg) {
    // only valid ioctl is our self-defined one
    if (cmd != IOCTL_READ_MEM && cmd != IOCTL_WRITE_MEM) {
        pr_warn("%s: invalid command\n", DEVICE_NAME);
        return -ENOTTY;
    }

    // copy actual read parameters from user space
    struct memory_params params;
    if (copy_from_user(&params, (void __user *)arg, sizeof(params))) {
        pr_warn("%s: could not copy parameters from user space\n", DEVICE_NAME);
        return -EFAULT;
    }

    if (!validate_params(&params)) {
        pr_warn("%s: invalid parameters\n", DEVICE_NAME);
        return -EINVAL;
    }

    // validate_params assures that size is a proper value
    void *kbuf = kvmalloc(params.size, GFP_KERNEL);
    if (!kbuf) {
        pr_warn(
            "%s: could not allocate temporary buffer for %zu bytes\n", DEVICE_NAME, params.size);
        return -ENOMEM;
    }

    // find_get_task_by_vpid does not work, have to do this instead
    rcu_read_lock();
    struct pid *pid_struct = find_get_pid(params.pid);
    if (!pid_struct) {
        pr_warn("%s: could not locate process by pid %d\n", DEVICE_NAME, params.pid);
        kvfree(kbuf);
        rcu_read_unlock();
        return -ESRCH;
    }

    struct task_struct *task = pid_task(pid_struct, PIDTYPE_PID);
    put_pid(pid_struct);

    if (!task) {
        pr_warn("%s: could not locate process by pid %d\n", DEVICE_NAME, params.pid);
        kvfree(kbuf);
        rcu_read_unlock();
        return -ESRCH;
    }

    get_task_struct(task);
    rcu_read_unlock();

    if (cmd == IOCTL_READ_MEM) {
        const int bytes_read = memory_read(task, params.addr, kbuf, params.size);
        put_task_struct(task);

        if (bytes_read < 0) {
            pr_warn("%s: unable to read memory from process\n", DEVICE_NAME);
            kvfree(kbuf);
            return (long)bytes_read;
        }

        // copy kernel buffer back to user space
        if (bytes_read > 0) {
            if (copy_to_user(params.buf, kbuf, (size_t)bytes_read)) {
                pr_warn("%s: unable to copy buffer to user space\n", DEVICE_NAME);
                kvfree(kbuf);
                return -EFAULT;
            }
        }

        kvfree(kbuf);
        return (long)bytes_read;
    } else if (cmd == IOCTL_WRITE_MEM) {
        // copy memory from user space buffer into kernel buffer
        if (copy_from_user(kbuf, params.buf, params.size)) {
            pr_warn("%s: unable to copy buffer from user space\n", DEVICE_NAME);
            put_task_struct(task);
            kvfree(kbuf);
            return -EFAULT;
        }

        const int bytes_written = memory_write(task, params.addr, kbuf, params.size);
        put_task_struct(task);

        if (bytes_written < 0) {
            pr_warn("%s: unable to write memory to process\n", DEVICE_NAME);
            kvfree(kbuf);
            return (long)bytes_written;
        }

        kvfree(kbuf);
        return (long)bytes_written;
    }

    return 0;
}

// defines operations possible on the char device
static const struct file_operations file_ops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = device_ioctl,
    .open = nonseekable_open,
    .llseek = NULL,
};

static int __init stealthmem_init(void) {
    int result = 0;
    dev_t device = 0;
    pr_info("%s: initializing module\n", DEVICE_NAME);

    // allocates a major device number
    result = alloc_chrdev_region(&device, 0, 1, DEVICE_NAME);
    if (result < 0) {
        pr_err("%s: failed to allocate chrdev region, error %d\n", DEVICE_NAME, result);
        return result;
    }

    major = MAJOR(device);
    cdev_init(&stealthmem_cdev, &file_ops);

    result = cdev_add(&stealthmem_cdev, device, 1);
    if (result < 0) {
        pr_err("%s: failed to add cdev, error %d\n", DEVICE_NAME, result);
        unregister_chrdev_region(device, 1);
        return result;
    }

    // class_create changed signature from kernel 6.4
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    stealthmem_class = class_create(DEVICE_NAME);
#else
    stealthmem_class = class_create(THIS_MODULE, DEVICE_NAME);
#endif
    if (IS_ERR(stealthmem_class)) {
        result = PTR_ERR(stealthmem_class);
        pr_err("%s: failed to create device class, error %d\n", DEVICE_NAME, result);
        cdev_del(&stealthmem_cdev);
        unregister_chrdev_region(device, 1);
        return result;
    }

    stealthmem_device = device_create(stealthmem_class, NULL, device, NULL, DEVICE_NAME);
    if (IS_ERR(stealthmem_device)) {
        result = PTR_ERR(stealthmem_device);
        pr_err("%s: failed to create device, error %d\n", DEVICE_NAME, result);
        class_destroy(stealthmem_class);
        cdev_del(&stealthmem_cdev);
        unregister_chrdev_region(device, 1);
        return result;
    }

    pr_info("%s: loaded successfully\n", DEVICE_NAME);
    return 0;
}

static void __exit stealthmem_exit(void) {
    pr_info("%s: unloading module\n", DEVICE_NAME);
    const dev_t device = MKDEV(major, 0);

    device_destroy(stealthmem_class, device);
    class_destroy(stealthmem_class);
    cdev_del(&stealthmem_cdev);
    unregister_chrdev_region(device, 1);

    pr_info("%s: unloaded\n", DEVICE_NAME);
}

module_init(stealthmem_init);
module_exit(stealthmem_exit);
