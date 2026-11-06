#ifndef SMS_BUFFER_H
#define SMS_BUFFER_H

#include <pthread.h>
#include <stddef.h>
#include <stdatomic.h>

/* Maximum number of sensor samples */
#define BUFFER_CAPACITY 64

/* One sensor sample */
typedef struct {
    unsigned long long timestamp_ms;
    float temperature;
    float humidity;
} sensor_sample_t;

/* Thread-safe circular buffer */
typedef struct {
    sensor_sample_t data[BUFFER_CAPACITY];

    size_t head;
    size_t tail;
    size_t size;

    unsigned long total_produced;
    unsigned long total_consumed;
    unsigned long overflow_count;

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
} circular_buffer_t;

/* Initialize the circular buffer */
int buffer_init(circular_buffer_t *buffer);

/* Add a sample to the buffer
 * Returns 0 on success, -1 if the buffer is full or an error occurs.
 */
int buffer_push(circular_buffer_t *buffer,
                const sensor_sample_t *sample);

/* Get a sample from the buffer
 * Returns 0 on success, -1 if stopped and no sample is available.
 */
int buffer_pop(circular_buffer_t *buffer,
               sensor_sample_t *sample,
               const atomic_bool *running);

/* Release synchronization resources */
void buffer_destroy(circular_buffer_t *buffer);

#endif /* SMS_BUFFER_H */
