#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <getopt.h>
#include <sys/ioctl.h>

/*
 * SMS Sensor Application
 * Task 2: Memory Management and Driver Integration
 *
 * Usage:
 *   ./sms_app -d /dev/sms_sensor -i 500
 *
 * IMPORTANT:
 * SMS_SET_INTERVAL must match the ioctl definition
 * in sms_sensor.c.
 */

#define DEFAULT_DEVICE "/dev/sms_sensor"
#define DEFAULT_INTERVAL 500

/*
 * Verify this definition against sms_sensor.c.
 * The ioctl type, number and argument type must match
 * the kernel driver exactly.
 */
#ifndef SMS_SET_INTERVAL
#define SMS_SET_INTERVAL _IOW('S', 1, unsigned int)
#endif

static char device_path[256] = DEFAULT_DEVICE;
static unsigned int interval_ms = DEFAULT_INTERVAL;
static int sensor_fd = -1;
static int simulation_mode = 0;
static atomic_bool running = 1;
static pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Print a message without interleaving output from threads. */
static void log_message(const char *message)
{
    pthread_mutex_lock(&output_mutex);
    fprintf(stderr, "%s\n", message);
    fflush(stderr);
    pthread_mutex_unlock(&output_mutex);
}

/* Wait for the specified number of milliseconds. */
static void sleep_ms(long milliseconds)
{
    struct timespec ts;

    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000L;

    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
        if (!atomic_load(&running)) {
            break;
        }
    }
}
/* Generate simulated sensor data for host-side testing. */
static void *simulation_reader(void *arg)
{
    (void)arg;

    unsigned int sample = 0;

    log_message("[Reader] Simulation reader thread started.");

    while (atomic_load(&running)) {
        char message[128];

        snprintf(message, sizeof(message),
                 "[SIMULATION] Sample %u: temperature=30.50, humidity=65.00\n",
                 ++sample);

        pthread_mutex_lock(&output_mutex);
        printf("%s", message);
        fflush(stdout);
        pthread_mutex_unlock(&output_mutex);

        sleep_ms(interval_ms);
    }

    log_message("[Reader] Simulation reader thread stopped.");

    return NULL;
}
/* Thread 1: Read sensor data from the kernel driver. */
static void *sensor_reader(void *arg)
{
    (void)arg;

    char buffer[512];

    log_message("[Reader] Sensor reader thread started.");

    while (atomic_load(&running)) {
        ssize_t bytes_read = read(sensor_fd, buffer, sizeof(buffer) - 1);

        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';

            pthread_mutex_lock(&output_mutex);
            printf("%s", buffer);

            /*
             * Ensure output is visible even when stdout
             * is redirected to a file.
             */
            fflush(stdout);
            pthread_mutex_unlock(&output_mutex);

            continue;
        }

        if (bytes_read == 0) {
            /*
             * EOF: avoid an infinite busy loop.
             * The application can stop or be restarted
             * after investigating the driver behavior.
             */
            log_message("[Reader] EOF: no more data.");
            break;
        }

        if (errno == EINTR) {
            /* Interrupted system call: retry. */
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* Device temporarily has no data. */
            sleep_ms(100);
            continue;
        }

        if (errno == ENODEV || errno == EIO) {
            log_message("[Reader] Device unavailable or I/O error.");
            break;
        }

        {
            char error_message[256];

            snprintf(error_message, sizeof(error_message),
                     "[Reader] read() failed: %s",
                     strerror(errno));

            log_message(error_message);
        }

        break;
    }

    log_message("[Reader] Sensor reader thread stopped.");

    return NULL;
}

/* Thread 2: Periodically report application status. */
static void *status_monitor(void *arg)
{
    (void)arg;

    unsigned int seconds = 0;

    log_message("[Monitor] Status monitor thread started.");

    while (atomic_load(&running)) {
        sleep_ms(1000);

        if (!atomic_load(&running)) {
            break;
        }

        seconds++;

        if (seconds % 5 == 0) {
            char message[128];

            snprintf(message, sizeof(message),
                     "[Monitor] Running for %u seconds; interval=%d ms.",
                     seconds, interval_ms);

            log_message(message);
        }
    }

    log_message("[Monitor] Status monitor thread stopped.");

    return NULL;
}

static void print_usage(const char *program)
{
    fprintf(stderr,
        "Usage: %s [-d device_path] [-i interval_ms] [-t]\n"
        "  -d PATH   Sensor device (default: %s)\n"
        "  -i MS     Sampling interval in milliseconds\n"
        "  -t        Enable host simulation mode\n"
        "  -h        Show this help\n",
        program, DEFAULT_DEVICE);
}
int main(int argc, char *argv[])
{
    int option;
    int exit_code = EXIT_FAILURE;
    int signal_number;

    pthread_t reader_thread;
    pthread_t monitor_thread;

    int reader_started = 0;
    int monitor_started = 0;

    sigset_t signal_set;

    /* Parse command-line arguments. */
    while ((option = getopt(argc, argv, "d:i:th")) != -1) {
        switch (option) {
        case 'd':
            if (strlen(optarg) >= sizeof(device_path)) {
                fprintf(stderr, "Device path is too long.\n");
                return EXIT_FAILURE;
            }

            strcpy(device_path, optarg);
            break;

        case 'i': {
            char *end_pointer;
            long value;

            errno = 0;
            value = strtol(optarg, &end_pointer, 10);

            if (errno != 0 ||
                *optarg == '\0' ||
                *end_pointer != '\0' ||
                value < 1 ||
                value > 60000) {
                fprintf(stderr,
                        "Invalid interval. Use a value from 1 to 60000 ms.\n");
                return EXIT_FAILURE;
            }

            interval_ms = (int)value;
            break;
        }
case 't':
    simulation_mode = 1;
    break;
        case 'h':
            print_usage(argv[0]);
            return EXIT_SUCCESS;

        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    /*
     * Block SIGINT before creating threads.
     * The main thread will receive Ctrl+C using sigwait().
     */
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);

    if (pthread_sigmask(SIG_BLOCK, &signal_set, NULL) != 0) {
        fprintf(stderr, "Failed to block SIGINT.\n");
        return EXIT_FAILURE;
    }

    printf("[Main] Device: %s\n", device_path);
    printf("[Main] Requested interval: %d ms\n", interval_ms);

    /* Open the sensor device. */
    sensor_fd = open(device_path, O_RDONLY);

    if (simulation_mode) {
    printf("[Main] Running in HOST SIMULATION mode.\n");
} else {
    /* Open the real sensor device. */
    sensor_fd = open(device_path, O_RDONLY);

    if (sensor_fd == -1) {
        fprintf(stderr, "[Main] open(%s) failed: %s\n",
                device_path, strerror(errno));
        goto cleanup;
    }

    printf("[Main] Device opened successfully, fd=%d\n", sensor_fd);

    /* Configure the driver's sampling interval. */
    if (ioctl(sensor_fd, SMS_SET_INTERVAL, &interval_ms) == -1) {
        fprintf(stderr,
                "[Main] ioctl(SMS_SET_INTERVAL) failed: %s\n",
                strerror(errno));
        goto cleanup;
    }

    printf("[Main] Driver interval configured to %d ms\n",
           interval_ms);
}

/* Start reader thread. */
if (pthread_create(
        &reader_thread,
        NULL,
        simulation_mode ? simulation_reader : sensor_reader,
        NULL) != 0) {

    fprintf(stderr, "[Main] Cannot create reader thread.\n");
    goto cleanup;
}

reader_started = 1;

    reader_started = 1;

    /* Start status monitor thread. */
    if (pthread_create(&monitor_thread, NULL,
                       status_monitor, NULL) != 0) {
        fprintf(stderr, "[Main] Cannot create monitor thread.\n");
        atomic_store(&running, 0);
        goto cleanup;
    }

    monitor_started = 1;

    printf("[Main] Application running. Press Ctrl+C to stop.\n");

    /*
     * Wait for Ctrl+C.
     * SIGINT is blocked in all threads and handled here.
     */
    int wait_result = sigwait(&signal_set, &signal_number);

    if (wait_result != 0) {
        fprintf(stderr, "[Main] sigwait failed: %s\n",
                strerror(wait_result));
    } else {
        printf("\n[Main] Received signal %d. Shutting down...\n",
               signal_number);
    }

    exit_code = EXIT_SUCCESS;

cleanup:
    /* Tell all worker threads to stop. */
    atomic_store(&running, 0);

    /*
     * Join the monitor first.
     * The reader may finish its current read before exiting.
     */
    if (monitor_started) {
        pthread_join(monitor_thread, NULL);
    }

    if (reader_started) {
        pthread_join(reader_thread, NULL);
    }

    /* Close the file descriptor exactly once. */
    if (sensor_fd != -1) {
        if (close(sensor_fd) == -1) {
            fprintf(stderr, "[Main] close() failed: %s\n",
                    strerror(errno));
            exit_code = EXIT_FAILURE;
        } else {
            printf("[Main] Device closed successfully.\n");
        }

        sensor_fd = -1;
    }

    pthread_mutex_destroy(&output_mutex);

    printf("[Main] Application exited.\n");

    return exit_code;
}
