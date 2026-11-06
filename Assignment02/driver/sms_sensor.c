/*
 * sms_sensor.c
 *
 * Embedded Linux Lab - SMS Sensor Driver
 * Target kernel: Linux 5.15
 *
 * Features:
 *   1. Character device: /dev/sms_sensor
 *   2. Random temperature: 15.00 - 45.00 C
 *   3. Random humidity:    30.00 - 90.00 %
 *   4. CSV output: timestamp_ms,temperature,humidity\n
 *   5. Statistics: /proc/sms_stats
 *   6. ioctl SMS_SET_INTERVAL
 *   7. Minimum interval between samples: 100 ms by default
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/math64.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/ioctl.h>
#include <linux/errno.h>

/* =========================================================
 * 1. DEVICE CONFIGURATION
 * ========================================================= */

#define DEVICE_NAME       "sms_sensor"
#define CLASS_NAME        "sms_sensor_class"
#define PROC_NAME         "sms_stats"

#define DEFAULT_INTERVAL_MS 100
#define MIN_INTERVAL_MS      1
#define MAX_INTERVAL_MS  60000

#define SMS_IOC_MAGIC 'S'

/*
 * ioctl command:
 * Pass an unsigned int containing the interval in milliseconds.
 */
#define SMS_SET_INTERVAL \
    _IOW(SMS_IOC_MAGIC, 1, unsigned int)

/* =========================================================
 * 2. GLOBAL VARIABLES
 * ========================================================= */

/* Character device information */
static dev_t sms_dev_number;
static struct cdev sms_cdev;
static struct class *sms_class;
static struct device *sms_device;

/* Proc filesystem entry */
static struct proc_dir_entry *sms_proc_entry;

/* Protect shared sensor data */
static DEFINE_MUTEX(sensor_mutex);

/* Sensor configuration */
static unsigned int min_interval_ms = DEFAULT_INTERVAL_MS;

/* Sensor statistics */
static unsigned long long read_count = 0;

static int last_temperature = 0;
static int last_humidity = 0;

static unsigned long long start_time_ms = 0;
static unsigned long long last_sample_ms = 0;

static bool has_sample = false;

/*
 * Temperature and humidity are stored as integers multiplied
 * by 100 to avoid floating-point operations inside the kernel.
 *
 * Example:
 *   3250 represents 32.50 C
 *   6540 represents 65.40 %
 */

/* =========================================================
 * 3. PER-OPEN FILE DATA
 * ========================================================= */

#define SAMPLE_BUFFER_SIZE 128

struct sms_file_data {
    char buffer[SAMPLE_BUFFER_SIZE];
    size_t length;
    bool valid;

    /*
     * Protect the buffer when multiple threads access the
     * same open file descriptor.
     */
    struct mutex lock;
};

/* =========================================================
 * 4. GENERATE A SENSOR SAMPLE
 * ========================================================= */

/*
 * Generate one new sample.
 *
 * Temperature:
 *   1500 - 4500, representing 15.00 - 45.00 C
 *
 * Humidity:
 *   3000 - 9000, representing 30.00 - 90.00 %
 *
 * This function also enforces the minimum interval between
 * samples and updates the statistics.
 */
static void generate_sensor_sample(void)
{
    unsigned long long now_ms;
    unsigned long long elapsed_ms;
    unsigned long long remaining_ms;

    unsigned int temperature;
    unsigned int humidity;

    /*
     * Lock and recheck the interval after every sleep.
     * This prevents concurrent readers from generating
     * samples too close together.
     */
    mutex_lock(&sensor_mutex);

    while (has_sample) {
        now_ms = ktime_to_ms(ktime_get());

        elapsed_ms = now_ms - last_sample_ms;

        if (elapsed_ms >= min_interval_ms)
            break;

        remaining_ms = min_interval_ms - elapsed_ms;

        mutex_unlock(&sensor_mutex);

        /*
         * Release the mutex while sleeping so other
         * operations, including ioctl, can proceed.
         */
        if (remaining_ms > 0)
            msleep((unsigned int)remaining_ms);

        mutex_lock(&sensor_mutex);
    }

    /*
     * Generate random temperature and humidity.
     *
     * Temperature range: 1500 - 4500 inclusive.
     * Humidity range:    3000 - 9000 inclusive.
     */
    temperature = 1500 +
                  (get_random_u32() % 3001);

    humidity = 3000 +
               (get_random_u32() % 6001);

    /* Update the latest sensor values */
    last_temperature = (int)temperature;
    last_humidity = (int)humidity;

    /* Update sample timestamp */
    last_sample_ms = ktime_to_ms(ktime_get());

    /* Update statistics */
    read_count++;

    has_sample = true;

    mutex_unlock(&sensor_mutex);
}

/* =========================================================
 * 5. DEVICE OPEN
 * ========================================================= */

static int sms_open(struct inode *inode, struct file *file)
{
    struct sms_file_data *data;

    data = kzalloc(sizeof(*data), GFP_KERNEL);

    if (!data)
        return -ENOMEM;

    mutex_init(&data->lock);

    data->length = 0;
    data->valid = false;

    file->private_data = data;

    pr_info("sms_sensor: device opened\n");

    return 0;
}

/* =========================================================
 * 6. DEVICE READ
 * ========================================================= */

/*
 * Output format:
 *
 * timestamp_ms,temperature,humidity\n
 *
 * Example:
 *
 * 123456,32.50,65.40
 *
 * Each new complete sample increments read_count once.
 */
static ssize_t sms_read(
    struct file *file,
    char __user *user_buffer,
    size_t count,
    loff_t *position)
{
    struct sms_file_data *data;
    ssize_t bytes_to_copy;

    int temperature;
    int humidity;

    unsigned long long timestamp_ms;

    int formatted_length;

    if (count == 0)
        return 0;

    data = file->private_data;

    if (!data)
        return -EINVAL;

    mutex_lock(&data->lock);

    /*
     * If the previous sample has been completely consumed,
     * generate a new one.
     *
     * If a previous read copied only part of the sample,
     * continue reading the remaining bytes instead.
     */
    if (!data->valid || *position >= data->length) {

        data->valid = false;

        *position = 0;

        generate_sensor_sample();

        /*
         * Obtain a consistent snapshot of the latest sample.
         */
        mutex_lock(&sensor_mutex);

        temperature = last_temperature;
        humidity = last_humidity;
        timestamp_ms = last_sample_ms;

        mutex_unlock(&sensor_mutex);

        /*
         * Format the CSV record.
         *
         * Integer arithmetic is used to print two decimal
         * places without floating-point operations.
         */
        formatted_length = snprintf(
            data->buffer,
            sizeof(data->buffer),
            "%llu,%d.%02d,%d.%02d\n",
            timestamp_ms,
            temperature / 100,
            temperature % 100,
            humidity / 100,
            humidity % 100
        );

        if (formatted_length < 0 ||
            formatted_length >= sizeof(data->buffer)) {

            mutex_unlock(&data->lock);

            return -EIO;
        }

        data->length = (size_t)formatted_length;
        data->valid = true;
    }

    /*
     * Determine the number of bytes remaining in the sample.
     */
    bytes_to_copy = data->length - *position;

    if (count < bytes_to_copy)
        bytes_to_copy = count;

    /*
     * Copy the sample to userspace.
     */
    if (copy_to_user(
            user_buffer,
            data->buffer + *position,
            bytes_to_copy)) {

        mutex_unlock(&data->lock);

        return -EFAULT;
    }

    /*
     * Advance the file position.
     */
    *position += bytes_to_copy;

    mutex_unlock(&data->lock);

    return bytes_to_copy;
}

/* =========================================================
 * 7. DEVICE IOCTL
 * ========================================================= */

/*
 * Userspace calls:
 *
 * ioctl(fd, SMS_SET_INTERVAL, &interval_ms);
 *
 * The interval is specified in milliseconds.
 */
static long sms_ioctl(
    struct file *file,
    unsigned int command,
    unsigned long argument)
{
    unsigned int interval_ms;

    switch (command) {

    case SMS_SET_INTERVAL:

        /*
         * Copy the interval value from userspace.
         */
        if (copy_from_user(
                &interval_ms,
                (unsigned int __user *)argument,
                sizeof(interval_ms))) {

            return -EFAULT;
        }

        /*
         * Validate the interval.
         */
        if (interval_ms < MIN_INTERVAL_MS ||
            interval_ms > MAX_INTERVAL_MS) {

            pr_warn(
                "sms_sensor: invalid interval %u ms\n",
                interval_ms
            );

            return -EINVAL;
        }

        /*
         * Update the configuration safely.
         */
        mutex_lock(&sensor_mutex);

        min_interval_ms = interval_ms;

        mutex_unlock(&sensor_mutex);

        pr_info(
            "sms_sensor: interval set to %u ms\n",
            interval_ms
        );

        return 0;

    default:

        return -ENOTTY;
    }
}

/* =========================================================
 * 8. DEVICE RELEASE
 * ========================================================= */

static int sms_release(struct inode *inode, struct file *file)
{
    struct sms_file_data *data = file->private_data;

    if (data)
        kfree(data);

    file->private_data = NULL;

    pr_info("sms_sensor: device closed\n");

    return 0;
}

/* =========================================================
 * 9. CHARACTER DEVICE OPERATIONS
 * ========================================================= */

static const struct file_operations sms_fops = {
    .owner          = THIS_MODULE,
    .open           = sms_open,
    .read           = sms_read,
    .unlocked_ioctl = sms_ioctl,
    .release        = sms_release,
    .llseek         = no_llseek,
};

/* =========================================================
 * 10. /PROC STATISTICS
 * ========================================================= */

/*
 * Display sensor statistics through:
 *
 * cat /proc/sms_stats
 */
static int sms_stats_show(struct seq_file *seq, void *v)
{
    unsigned long long current_time_ms;
    unsigned long long uptime_seconds;

    unsigned long long samples;

    unsigned long long last_time;

    int temperature;
    int humidity;

    unsigned int interval;

    current_time_ms = ktime_to_ms(ktime_get());

    uptime_seconds =
        div_u64(current_time_ms - start_time_ms, 1000);

    /*
     * Copy shared values while holding the mutex.
     */
    mutex_lock(&sensor_mutex);

    samples = read_count;

    temperature = last_temperature;
    humidity = last_humidity;

    interval = min_interval_ms;

    last_time = last_sample_ms;

    mutex_unlock(&sensor_mutex);

    seq_printf(seq, "read_count: %llu\n", samples);

    seq_printf(
        seq,
        "last_temperature: %d.%02d\n",
        temperature / 100,
        temperature % 100
    );

    seq_printf(
        seq,
        "last_humidity: %d.%02d\n",
        humidity / 100,
        humidity % 100
    );

    seq_printf(
        seq,
        "uptime_seconds: %llu\n",
        uptime_seconds
    );

    seq_printf(
        seq,
        "min_interval_ms: %u\n",
        interval
    );

    seq_printf(
        seq,
        "last_sample_ms: %llu\n",
        last_time
    );

    return 0;
}

static int sms_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, sms_stats_show, NULL);
}

static const struct proc_ops sms_proc_ops = {
    .proc_open    = sms_stats_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* =========================================================
 * 11. MODULE INITIALIZATION
 * ========================================================= */

static int __init sms_sensor_init(void)
{
    int result;

    pr_info("sms_sensor: initializing driver\n");

    /* Initialize statistics and default configuration */
    mutex_lock(&sensor_mutex);

    read_count = 0;

    last_temperature = 0;
    last_humidity = 0;

    min_interval_ms = DEFAULT_INTERVAL_MS;

    start_time_ms = ktime_to_ms(ktime_get());
    last_sample_ms = 0;

    has_sample = false;

    mutex_unlock(&sensor_mutex);

    /*
     * Allocate a device number dynamically.
     */
    result = alloc_chrdev_region(
        &sms_dev_number,
        0,
        1,
        DEVICE_NAME
    );

    if (result < 0) {
        pr_err(
            "sms_sensor: failed to allocate device number\n"
        );

        return result;
    }

    /*
     * Initialize and register the character device.
     */
    cdev_init(&sms_cdev, &sms_fops);

    sms_cdev.owner = THIS_MODULE;

    result = cdev_add(
        &sms_cdev,
        sms_dev_number,
        1
    );

    if (result < 0) {
        pr_err(
            "sms_sensor: failed to add character device\n"
        );

        goto error_unregister_region;
    }

    /*
     * Create the device class.
     *
     * Kernel 5.15 uses class_create(THIS_MODULE, name).
     */
    sms_class = class_create(
        THIS_MODULE,
        CLASS_NAME
    );

    if (IS_ERR(sms_class)) {
        result = PTR_ERR(sms_class);

        pr_err(
            "sms_sensor: failed to create device class\n"
        );

        goto error_delete_cdev;
    }

    /*
     * Create the device.
     *
     * When devtmpfs/udev is configured, this creates
     * /dev/sms_sensor automatically.
     */
    sms_device = device_create(
        sms_class,
        NULL,
        sms_dev_number,
        NULL,
        DEVICE_NAME
    );

    if (IS_ERR(sms_device)) {
        result = PTR_ERR(sms_device);

        pr_err(
            "sms_sensor: failed to create device\n"
        );

        goto error_destroy_class;
    }

    /*
     * Create /proc/sms_stats.
     */
    sms_proc_entry = proc_create(
        PROC_NAME,
        0444,
        NULL,
        &sms_proc_ops
    );

    if (!sms_proc_entry) {
        result = -ENOMEM;

        pr_err(
            "sms_sensor: failed to create proc entry\n"
        );

        goto error_destroy_device;
    }

    pr_info(
        "sms_sensor: driver loaded successfully\n"
    );

    pr_info(
        "sms_sensor: device /dev/%s\n",
        DEVICE_NAME
    );

    pr_info(
        "sms_sensor: statistics /proc/%s\n",
        PROC_NAME
    );

    pr_info(
        "sms_sensor: default interval %u ms\n",
        DEFAULT_INTERVAL_MS
    );

    return 0;

/*
 * Error handling:
 * Release resources in reverse order of allocation.
 */

error_destroy_device:

    device_destroy(
        sms_class,
        sms_dev_number
    );

error_destroy_class:

    class_destroy(sms_class);

error_delete_cdev:

    cdev_del(&sms_cdev);

error_unregister_region:

    unregister_chrdev_region(
        sms_dev_number,
        1
    );

    return result;
}

/* =========================================================
 * 12. MODULE CLEANUP
 * ========================================================= */

static void __exit sms_sensor_exit(void)
{
    pr_info("sms_sensor: removing driver\n");

    /* Remove /proc/sms_stats */
    if (sms_proc_entry) {
        proc_remove(sms_proc_entry);
        sms_proc_entry = NULL;
    }

    /* Remove /dev/sms_sensor */
    if (sms_device && !IS_ERR(sms_device)) {
        device_destroy(
            sms_class,
            sms_dev_number
        );
    }

    /* Destroy the device class */
    if (sms_class && !IS_ERR(sms_class)) {
        class_destroy(sms_class);
    }

    /* Remove the character device */
    cdev_del(&sms_cdev);

    /* Release the allocated device number */
    unregister_chrdev_region(
        sms_dev_number,
        1
    );

    pr_info(
        "sms_sensor: driver removed successfully\n"
    );
}

/* =========================================================
 * 13. REGISTER MODULE
 * ========================================================= */

module_init(sms_sensor_init);
module_exit(sms_sensor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Lab");
MODULE_DESCRIPTION(
    "SMS Sensor Character Device Driver"
);
MODULE_VERSION("1.0");
