#include "../include/stealthmem.h"

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#define DEVICE_NAME "stealthmem"
#define MAX_MEM_SIZE (256 * 1024 * 1024)
#define CHUNK_SIZE (1024 * 1024)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("avitrano");
MODULE_DESCRIPTION("stealth process memory read/write module");
MODULE_VERSION("1.0");

static int major;
static struct class *stealthmem_class;
static struct device *stealthmem_device;
static struct cdev stealthmem_cdev;
static struct input_dev *input_device;

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

static int handle_memory(const unsigned int cmd, const unsigned long arg) {
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

    struct mm_struct *mm = get_task_mm(task);
    if (!mm) {
        pr_warn("%s: no memory map for process %d\n", DEVICE_NAME, params.pid);
        put_task_struct(task);
        kvfree(kbuf);
        return -ESRCH;
    }

    size_t processed = 0;
    int result = 0;
    int bytes_handled = 0;
    // have to handle larger reads in chunks
    while (processed < params.size) {
        const size_t chunk = min_t(size_t, params.size - processed, CHUNK_SIZE);
        const unsigned long addr = params.addr + processed;

        down_read(&mm->mmap_lock);
        if (cmd == IOCTL_READ_MEM) {
            bytes_handled = access_process_vm(task, addr, kbuf, chunk, 0);
        } else if (cmd == IOCTL_WRITE_MEM) {
            if (copy_from_user(kbuf, params.buf + processed, chunk)) {
                up_read(&mm->mmap_lock);
                pr_warn("%s: failed to copy buffer from user space\n", DEVICE_NAME);
                result = -EFAULT;
                break;
            }
            bytes_handled = access_process_vm(task, addr, kbuf, chunk, FOLL_WRITE | FOLL_FORCE);
        }
        up_read(&mm->mmap_lock);

        // read/write failed
        if (bytes_handled <= 0) {
            if (processed == 0) {
                result = bytes_handled < 0 ? bytes_handled : -EIO;
            }
            break;
        }

        if (cmd == IOCTL_READ_MEM) {
            if (copy_to_user(params.buf + processed, kbuf, bytes_handled)) {
                pr_warn("%s: failed to copy buffer to user space\n", DEVICE_NAME);
                result = -EFAULT;
                break;
            }
        }

        processed += bytes_handled;
        result = processed;

        if ((size_t)bytes_handled < chunk) {
            break;
        }
    }

    return bytes_handled;
}

static long device_ioctl(struct file *file, const unsigned int cmd, const unsigned long arg) {
    // only valid ioctl is our self-defined one
    if (cmd != IOCTL_READ_MEM && cmd != IOCTL_WRITE_MEM && cmd != IOCTL_MOUSE_MOVE &&
        cmd != IOCTL_KEY_PRESS) {
        pr_warn("%s: invalid command\n", DEVICE_NAME);
        return -ENOTTY;
    }

    if (cmd == IOCTL_READ_MEM || cmd == IOCTL_WRITE_MEM) {
        return handle_memory(cmd, arg);
    } else if (cmd == IOCTL_MOUSE_MOVE) {
        struct mouse_move move;
        if (copy_from_user(&move, (void __user *)arg, sizeof(move))) {
            pr_warn("%s: could not copy parameters from user space\n", DEVICE_NAME);
            return -EFAULT;
        }

        if (abs(move.x) > 4096 || abs(move.y) > 4096) {
            pr_warn("%s: invalid mouse movement: %d/%d\n", DEVICE_NAME, move.x, move.y);
            return -EINVAL;
        }

        input_event(input_device, EV_REL, REL_X, move.x);
        input_event(input_device, EV_REL, REL_Y, move.y);
        input_sync(input_device);
    } else {
        struct key_press btn_event;
        if (copy_from_user(&btn_event, (void __user *)arg, sizeof(btn_event))) {
            pr_warn("%s: could not copy parameters from user space\n", DEVICE_NAME);
            return -EFAULT;
        }

        if (btn_event.key < KEY_ESC || btn_event.key >= KEY_POWER) {
            pr_warn("%s: invalid key: %d\n", DEVICE_NAME, btn_event.key);
            return -EINVAL;
        }

        input_event(input_device, EV_KEY, btn_event.key, btn_event.press);
        input_sync(input_device);
    }

    return 0;
}

static int device_open(struct inode *inode, struct file *file) {
    if (!try_module_get(THIS_MODULE)) {
        return -ENODEV;
    }

    file->f_mode &= ~(FMODE_PREAD | FMODE_PWRITE);
    return 0;
}

static int device_release(struct inode *inode, struct file *file) {
    module_put(THIS_MODULE);
    return 0;
}

// defines operations possible on the char device
static const struct file_operations file_ops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = device_ioctl,
    .open = device_open,
    .release = device_release,
    .llseek = NULL,
};

static int __init stealthmem_init(void) {
    int result = 0;
    dev_t device = 0;
    pr_info("%s: initializing module\n", DEVICE_NAME);

    // memory chardev
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
        goto clean_chrdev;
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
        goto clean_cdev;
    }

    stealthmem_device = device_create(stealthmem_class, NULL, device, NULL, DEVICE_NAME);
    if (IS_ERR(stealthmem_device)) {
        result = PTR_ERR(stealthmem_device);
        pr_err("%s: failed to create device, error %d\n", DEVICE_NAME, result);
        goto clean_class;
    }

    // input device
    input_device = input_allocate_device();
    if (!input_device) {
        result = -ENOMEM;
        pr_err("%s: failed to allocate input device\n", DEVICE_NAME);
        goto clean_device;
    }

    input_device->name = "@HFD QK65V2 Classic";
    input_device->id.vendor = 0xFFFE;
    input_device->id.product = 0x0040;
    input_device->id.version = 1;
    input_device->id.bustype = BUS_VIRTUAL;

    // keyboard bits
    set_bit(EV_KEY, input_device->evbit);
    for (int i = KEY_ESC; i < KEY_POWER; i++) {
        set_bit(i, input_device->keybit);
    }

    // mouse bits
    set_bit(EV_REL, input_device->evbit);
    set_bit(REL_X, input_device->relbit);
    set_bit(REL_Y, input_device->relbit);

    set_bit(BTN_LEFT, input_device->keybit);
    set_bit(BTN_RIGHT, input_device->keybit);

    result = input_register_device(input_device);
    if (result) {
        pr_err("%s: failed to register input device\n", DEVICE_NAME);
        goto clean_input_device;
    }

    pr_info("%s: loaded successfully\n", DEVICE_NAME);
    return 0;

clean_input_device:
    input_free_device(input_device);

clean_device:
    device_destroy(stealthmem_class, device);
clean_class:
    class_destroy(stealthmem_class);
clean_cdev:
    cdev_del(&stealthmem_cdev);
clean_chrdev:
    unregister_chrdev_region(device, 1);
    return result;
}

static void __exit stealthmem_exit(void) {
    pr_info("%s: unloading module\n", DEVICE_NAME);
    const dev_t device = MKDEV(major, 0);

    // memory chardev
    device_destroy(stealthmem_class, device);
    class_destroy(stealthmem_class);
    cdev_del(&stealthmem_cdev);
    unregister_chrdev_region(device, 1);

    // input device
    // this function implicitly frees the device
    // what the fuck
    input_unregister_device(input_device);

    pr_info("%s: unloaded\n", DEVICE_NAME);
}

module_init(stealthmem_init);
module_exit(stealthmem_exit);
