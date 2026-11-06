;
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/ktime.h>
#include <linux/string.h>
#include <linux/ctype.h> 
#include "asgn1_ioctl.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Lab");
MODULE_DESCRIPTION("LAB-02: Simple Character Device Driver");
MODULE_VERSION("1.0");

#define DEVICE_NAME     "asgn1"
#define CLASS_NAME      "asgn1_class"
#define BUFFER_SIZE     1024
#define DRIVER_MAJOR    241
#define MAX_WRITE_SIZE 128

static int asgn1_mode = 1;       /* 0: Kernel, 1: Echo */
static int last_write_size = 0;
static bool device_is_open = false;
static int write_count = 0;
static int read_only = 0;
static int upper_mode = 0;
static unsigned long long last_write_timestamp = 0;
static int lower_mode = 0;
static int    major_number = DRIVER_MAJOR;
static char   device_buffer[BUFFER_SIZE];
static int    buffer_len = 0;
static int    open_count = 0;                                           
static int active_users = 0;
static int read_count = 0;
static size_t total_bytes_read = 0;
static size_t total_bytes_written = 0;
static struct class  *lab2_class  = NULL;
static struct device *lab2_device = NULL;
static DEFINE_MUTEX(lab2_mutex);
/* ===== PROCFS Implementation ===== */
static struct proc_dir_entry *proc_entry;

static int lab2_proc_show(struct seq_file *m, void *v)
{
    int line_count = 0;
    int i;

    mutex_lock(&lab2_mutex);

    for (i = 0; i < buffer_len; i++) {
        if (device_buffer[i] == '\n')
            line_count++;
    }

    seq_printf(m, "LAB-02 Driver Statistics\n");
    seq_printf(m, "=========================\n");
    seq_printf(m, "Driver name   : %s\n", DEVICE_NAME);
    seq_printf(m, "Major number  : %d\n", major_number);
    seq_printf(m, "Buffer size   : %d bytes\n", BUFFER_SIZE);
    seq_printf(m, "Data length   : %d bytes\n", buffer_len);
    seq_printf(m, "Line count    : %d\n", line_count);
    seq_printf(m, "Open count    : %d\n", open_count);
    seq_printf(m, "Read count    : %d\n", read_count);
    seq_printf(m, "last_write_timestamp: %llu ns\n",
           last_write_timestamp);
    seq_printf(m, "Active users  : %d\n", active_users);
    if (buffer_len > 0)
        seq_printf(m, "Last data     : [%s]\n", device_buffer);

    mutex_unlock(&lab2_mutex);

    return 0;
}

static int lab2_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, lab2_proc_show, NULL);
}

static const struct proc_ops lab2_proc_fops = {
    .proc_open    = lab2_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};/* ===== SYSFS Implementation ===== */

static ssize_t buffer_len_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);
    ret = sysfs_emit(buf, "%d\n", buffer_len);
    mutex_unlock(&lab2_mutex);

    return ret;
}

static ssize_t open_count_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);
    ret = sysfs_emit(buf, "%d\n", open_count);
    mutex_unlock(&lab2_mutex);

    return ret;
}

static ssize_t last_data_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);

    if (buffer_len > 0)
        ret = sysfs_emit(buf, "%s\n", device_buffer);
    else
        ret = sysfs_emit(buf, "(empty)\n");

    mutex_unlock(&lab2_mutex);

    return ret;
}
static ssize_t read_count_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);
    ret = sysfs_emit(buf, "%d\n", read_count);
    mutex_unlock(&lab2_mutex);

    return ret;
}
static ssize_t lower_mode_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    int value;

    mutex_lock(&lab2_mutex);
    value = lower_mode;
    mutex_unlock(&lab2_mutex);

    return scnprintf(buf, PAGE_SIZE, "%d\n", value);
}

static ssize_t lower_mode_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count)
{
    int value;
    int ret;

    ret = kstrtoint(buf, 10, &value);
    if (ret < 0)
        return ret;

    if (value != 0 && value != 1)
        return -EINVAL;

    mutex_lock(&lab2_mutex);
    lower_mode = value;
    mutex_unlock(&lab2_mutex);

    return count;
}
static ssize_t total_bytes_read_show(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
    size_t value;

    mutex_lock(&lab2_mutex);
    value = total_bytes_read;
    mutex_unlock(&lab2_mutex);

    return scnprintf(buf, PAGE_SIZE, "%zu\n", value);
}
static ssize_t total_bytes_written_show(struct device *dev,
                                        struct device_attribute *attr,
                                        char *buf)
{
    size_t value;

    mutex_lock(&lab2_mutex);
    value = total_bytes_written;
    mutex_unlock(&lab2_mutex);

    return scnprintf(buf, PAGE_SIZE, "%zu\n", value);
}
static ssize_t version_show(struct device *dev,
                            struct device_attribute *attr,
                            char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "1.0\n");
}
static ssize_t buffer_size_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
    ssize_t size;

    mutex_lock(&lab2_mutex);
    size = buffer_len;
    mutex_unlock(&lab2_mutex);

    return sysfs_emit(buf, "%zd\n", size);
}
static ssize_t status_show(struct device *dev,
                           struct device_attribute *attr,
                           char *buf)
{
    const char *status;

    mutex_lock(&lab2_mutex);

    if (buffer_len == 0)
        status = "EMPTY";
    else
        status = "DATA";

    mutex_unlock(&lab2_mutex);

    return sysfs_emit(buf, "%s\n", status);
}
static ssize_t upper_mode_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    int value;

    mutex_lock(&lab2_mutex);
    value = upper_mode;
    mutex_unlock(&lab2_mutex);

    return sysfs_emit(buf, "%d\n", value);
}

static ssize_t upper_mode_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf,
                                size_t count)
{
    int value;

    if (kstrtoint(buf, 10, &value) != 0)
        return -EINVAL;

    if (value != 0 && value != 1)
        return -EINVAL;

    mutex_lock(&lab2_mutex);
    upper_mode = value;
    mutex_unlock(&lab2_mutex);

    return count;
}
static ssize_t active_users_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
    int value;

    mutex_lock(&lab2_mutex);
    value = active_users;
    mutex_unlock(&lab2_mutex);

    return sysfs_emit(buf, "%d\n", value);
}
static ssize_t word_count_show(struct device *dev,
                               struct device_attribute *attr, char *buf) {
    int i, words = 0, in_word = 0;

    mutex_lock(&lab2_mutex);
    for (i = 0; i < buffer_len; i++) {
        if (isspace(device_buffer[i])) {           /* [MỚI] gặp khoảng trắng/\n/tab: hết từ */
            in_word = 0;
        } else if (!in_word) {                     /* [MỚI] chuyển từ "ngoài từ" sang "trong từ" */
            in_word = 1;
            words++;
        }
    }
    mutex_unlock(&lab2_mutex);

    return sprintf(buf, "%d\n", words);
}
static ssize_t read_only_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);
    ret = sysfs_emit(buf, "%d\n", read_only);
    mutex_unlock(&lab2_mutex);

    return ret;
}
static ssize_t read_only_store(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf,
                               size_t count)
{
    int value;
    int ret;

    ret = kstrtoint(buf, 10, &value);
    if (ret)
        return -EINVAL;

    if (value != 0 && value != 1)
        return -EINVAL;

    mutex_lock(&lab2_mutex);
    read_only = value;
    mutex_unlock(&lab2_mutex);

    return count;
} 
static ssize_t write_count_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);
    ret = sysfs_emit(buf, "%d\n", write_count);
    mutex_unlock(&lab2_mutex);

    return ret;
}
static DEVICE_ATTR_RW(read_only);
static DEVICE_ATTR_RO(word_count);  
static DEVICE_ATTR_RO(active_users);
static DEVICE_ATTR_RW(upper_mode);
static DEVICE_ATTR_RO(status);
static DEVICE_ATTR_RO(buffer_size);
static DEVICE_ATTR_RO(version);
static DEVICE_ATTR_RO(total_bytes_written);
static DEVICE_ATTR_RO(total_bytes_read);
static DEVICE_ATTR_RW(lower_mode);
static DEVICE_ATTR_RO(buffer_len);
static DEVICE_ATTR_RO(open_count);
static DEVICE_ATTR_RO(last_data);
static DEVICE_ATTR_RO(read_count);
static DEVICE_ATTR_RO(write_count);

static struct attribute *lab2_attrs[] = {
    &dev_attr_buffer_len.attr,
    &dev_attr_open_count.attr,
    &dev_attr_last_data.attr,
    &dev_attr_read_count.attr,
    &dev_attr_lower_mode.attr,
    &dev_attr_write_count.attr,
    &dev_attr_total_bytes_read.attr,
    &dev_attr_total_bytes_written.attr,
    &dev_attr_version.attr,
    &dev_attr_buffer_size.attr,
    &dev_attr_status.attr,
    &dev_attr_upper_mode.attr,
    &dev_attr_active_users.attr,
    &dev_attr_word_count.attr,
    &dev_attr_read_only.attr,
    NULL,
};

static struct attribute_group lab2_attr_group = {
    .attrs = lab2_attrs,
};

static int lab2_open(struct inode *inode, struct file *file) {
    mutex_lock(&lab2_mutex);
   if (device_is_open) {
    mutex_unlock(&lab2_mutex);
    return -EBUSY;
}

device_is_open = true;
    open_count++;
    active_users++;
    mutex_unlock(&lab2_mutex);
    pr_info("lab2_driver: device opened\n");
    return 0;
}

static int lab2_release(struct inode *inode, struct file *file)
{
    mutex_lock(&lab2_mutex);
    device_is_open = false;
    if (active_users > 0)
        active_users--;

    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: device closed\n");

    return 0;
}

static ssize_t lab2_read(struct file *file, char __user *buf,
                         size_t len, loff_t *offset)
{
    ssize_t bytes_read;
   
    mutex_lock(&lab2_mutex);

    /* Khong con du lieu de doc */
    if (*offset >= buffer_len) {
        mutex_unlock(&lab2_mutex);
        return 0;
    }

    /* Khong doc vuot qua du lieu hien co */
    if (len > buffer_len - *offset)
        len = buffer_len - *offset;

    if (len == 0) {
        mutex_unlock(&lab2_mutex);
        return 0;
    }
    /* Copy du lieu tu kernel-space sang user-space */
    if (copy_to_user(buf, device_buffer + *offset, len)) {
        mutex_unlock(&lab2_mutex);
        return -EFAULT;
    }

    *offset += len;
    bytes_read = len;

    /* Chi dem khi doc thanh cong */
    read_count++;
    total_bytes_read += bytes_read;
    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: sent %zd bytes to user\n", bytes_read);

    return bytes_read;
}
static ssize_t lab2_write(struct file *file,
                          const char __user *buf,
                          size_t len,
                          loff_t *offset)
{
    char temp[MAX_WRITE_SIZE + 1];
    char final_data[BUFFER_SIZE];
    const char prefix[] = "[KERNEL]";
    size_t write_len;
    size_t final_len;

    /* Khong cho phep ghi du lieu rong */
    if (len == 0)
        return 0;

    /* Gioi han moi lan ghi toi da 128 byte */
    write_len = len;
    if (write_len > MAX_WRITE_SIZE)
        write_len = MAX_WRITE_SIZE;

    /* Copy du lieu tu user-space vao kernel-space */
    if (copy_from_user(temp, buf, write_len))
        return -EFAULT;

    temp[write_len] = '\0';

    mutex_lock(&lab2_mutex);

    /* Kiem tra che do read_only cua LAB2 */
    if (read_only == 1) {
        mutex_unlock(&lab2_mutex);
        return -EPERM;
    }

    /* Xu ly Kernel mode va Echo mode */
    if (asgn1_mode == 0) {
        size_t prefix_len = sizeof(prefix) - 1;

        /* Kiem tra kich thuoc buffer */
        if (prefix_len + write_len > BUFFER_SIZE) {
            mutex_unlock(&lab2_mutex);
            return -EINVAL;
        }

        memcpy(final_data, prefix, prefix_len);
        memcpy(final_data + prefix_len, temp, write_len);

        final_len = prefix_len + write_len;
    }
    else if (asgn1_mode == 1) {
        memcpy(final_data, temp, write_len);
        final_len = write_len;
    }
    else {
        mutex_unlock(&lab2_mutex);
        return -EINVAL;
    }

    /* Xu ly cac che do chu hoa / chu thuong cua LAB2 */
    if (upper_mode || lower_mode) {
        size_t i;

        for (i = 0; i < final_len; i++) {
            if (upper_mode)
                final_data[i] = toupper(final_data[i]);

            if (lower_mode)
                final_data[i] = tolower(final_data[i]);
        }
    }

    /* Cap nhat device buffer */
    memcpy(device_buffer, final_data, final_len);
    buffer_len = final_len;

    if (buffer_len < BUFFER_SIZE)
        device_buffer[buffer_len] = '\0';

    /* Cap nhat thong ke */
    write_count++;
    total_bytes_written += final_len;
    last_write_size = final_len;
    last_write_timestamp = ktime_get_ns();

    mutex_unlock(&lab2_mutex);

    pr_info("asgn1_driver: received %zu bytes, mode=%d\n",
            final_len, asgn1_mode);

    return write_len;
} 



static struct file_operations lab2_fops = {
    .owner   = THIS_MODULE,
    .open    = lab2_open,
    .release = lab2_release,
    .read    = lab2_read,
    .write   = lab2_write,
};

static int __init lab2_init(void) {
    int ret;
    pr_info("lab2_driver: initializing module\n");
    ret = register_chrdev(major_number, DEVICE_NAME, &lab2_fops);
    if (ret < 0) {
        pr_err("lab2_driver: register_chrdev failed: %d\n", ret);
        return ret;
    }
    lab2_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(lab2_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(lab2_class);
    }
    lab2_device = device_create(lab2_class, NULL,
                    MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(lab2_device)) {
        class_destroy(lab2_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(lab2_device);
    }
      /* Create Procfs entry */
    proc_entry = proc_create("lab2_info", 0444, NULL,
                              &lab2_proc_fops);

    if (!proc_entry)
        pr_warn("lab2_driver: failed to create /proc/lab2_info\n");
    else
        pr_info("lab2_driver: created /proc/lab2_info\n");

    /* Create Sysfs attributes */
    ret = sysfs_create_group(&lab2_device->kobj,
                             &lab2_attr_group);

    if (ret)
        pr_warn("lab2_driver: failed to create sysfs group: %d\n",
                ret);
    else
        pr_info("lab2_driver: created sysfs attributes\n");

    pr_info("lab2_driver: loaded, major=%d\n", major_number);
    return 0;
}

static void __exit lab2_exit(void) {
     /* Remove Procfs entry */
    if (proc_entry)
        proc_remove(proc_entry);

    /* Remove Sysfs attributes */
    sysfs_remove_group(&lab2_device->kobj,
                       &lab2_attr_group);

    /* Destroy character device */
    device_destroy(lab2_class, MKDEV(major_number, 0));
    class_destroy(lab2_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    pr_info("lab2_driver: module unloaded\n");
}


  

module_init(lab2_init);
module_exit(lab2_exit);
