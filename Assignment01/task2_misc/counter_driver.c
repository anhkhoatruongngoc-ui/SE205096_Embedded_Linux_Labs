#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/string.h>

#define DEVICE_NAME "counter"
#define PROC_NAME   "counter_info"
#define BUFFER_SIZE 64

/* Counter and statistics */
static atomic_t counter = ATOMIC_INIT(0);
static atomic_t read_count = ATOMIC_INIT(0);
static atomic_t write_count = ATOMIC_INIT(0);

/* Protect counter operations and statistics */
static DEFINE_MUTEX(counter_mutex);

/* -------------------------------------------------- */
/* Device open                                         */
/* -------------------------------------------------- */
static int counter_open(struct inode *inode, struct file *file)
{
    pr_info("counter_driver: device opened\n");
    return 0;
}

/* -------------------------------------------------- */
/* Device read                                         */
/* -------------------------------------------------- */
static ssize_t counter_read(struct file *file,
                            char __user *user_buf,
                            size_t count,
                            loff_t *ppos)
{
    char buf[BUFFER_SIZE];
    int value;
    int len;
    size_t bytes;

    if (*ppos > 0)
        return 0;

    if (count == 0)
        return 0;

    mutex_lock(&counter_mutex);

    /* Get current value, then increment */
    value = atomic_read(&counter);
    len = scnprintf(buf, sizeof(buf), "%d\n", value);

    atomic_inc(&counter);
    atomic_inc(&read_count);

    mutex_unlock(&counter_mutex);

    bytes = min(count, (size_t)len);

    if (copy_to_user(user_buf, buf, bytes))
        return -EFAULT;

    *ppos += bytes;

    pr_info("counter_driver: read value=%d\n", value);

    return bytes;
}

/* -------------------------------------------------- */
/* Device write                                        */
/* -------------------------------------------------- */
static ssize_t counter_write(struct file *file,
                             const char __user *user_buf,
                             size_t count,
                             loff_t *ppos)
{
    char buf[BUFFER_SIZE];
    int value;
    size_t len;

    if (count == 0)
        return 0;

    /* Leave room for the terminating null byte */
    len = min(count, sizeof(buf) - 1);

    if (copy_from_user(buf, user_buf, len))
        return -EFAULT;

    buf[len] = '\0';

    /* Remove trailing newline characters */
    while (len > 0 &&
           (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[--len] = '\0';
    }

    if (len == 0)
        return -EINVAL;

    /* Reject negative values */
    if (buf[0] == '-')
        return -EINVAL;

    /* Parse the input as a decimal integer */
    if (kstrtoint(buf, 10, &value) != 0)
        return -EINVAL;

    /* Counter only accepts non-negative values */
    if (value < 0)
        return -EINVAL;

    mutex_lock(&counter_mutex);

    atomic_set(&counter, value);
    atomic_inc(&write_count);

    mutex_unlock(&counter_mutex);

    pr_info("counter_driver: counter set to %d\n", value);

    return count;
}

/* -------------------------------------------------- */
/* File operations                                     */
/* -------------------------------------------------- */
static const struct file_operations counter_fops = {
    .owner  = THIS_MODULE,
    .open   = counter_open,
    .read   = counter_read,
    .write  = counter_write,
    .llseek = no_llseek,
};

/* -------------------------------------------------- */
/* Misc device                                         */
/* -------------------------------------------------- */
static struct miscdevice counter_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = DEVICE_NAME,
    .fops  = &counter_fops,
    .mode  = 0666,
};

/* -------------------------------------------------- */
/* /proc/counter_info                                  */
/* -------------------------------------------------- */
static int counter_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Counter value: %d\n", atomic_read(&counter));
    seq_printf(m, "Total reads: %d\n", atomic_read(&read_count));
    seq_printf(m, "Total writes: %d\n", atomic_read(&write_count));

    return 0;
}

static int counter_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, counter_proc_show, NULL);
}

static const struct proc_ops counter_proc_ops = {
    .proc_open    = counter_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* -------------------------------------------------- */
/* Module initialization                               */
/* -------------------------------------------------- */
static int __init counter_init(void)
{
    int ret;

    atomic_set(&counter, 0);
    atomic_set(&read_count, 0);
    atomic_set(&write_count, 0);

    ret = misc_register(&counter_misc_device);
    if (ret) {
        pr_err("counter_driver: misc_register failed: %d\n", ret);
        return ret;
    }

    if (!proc_create(PROC_NAME, 0444, NULL, &counter_proc_ops)) {
        misc_deregister(&counter_misc_device);
        pr_err("counter_driver: proc_create failed\n");
        return -ENOMEM;
    }

    pr_info("counter_driver: loaded successfully\n");
    pr_info("counter_driver: device=/dev/counter, major=10\n");

    return 0;
}

/* -------------------------------------------------- */
/* Module cleanup                                      */
/* -------------------------------------------------- */
static void __exit counter_exit(void)
{
    remove_proc_entry(PROC_NAME, NULL);
    misc_deregister(&counter_misc_device);

    pr_info("counter_driver: unloaded, resources cleaned up\n");
}

module_init(counter_init);
module_exit(counter_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Truong Ngoc Anh Khoa - SE205096");
MODULE_DESCRIPTION("Misc device driver implementing an atomic counter");
MODULE_VERSION("1.0");
