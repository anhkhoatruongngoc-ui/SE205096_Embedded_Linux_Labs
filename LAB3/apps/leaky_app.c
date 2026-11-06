#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_RECORDS 100
#define MSG_SIZE 256

typedef struct {
    int id;
    char name[64];
    char *data;
} Record;

Record *create_record(int id)
{
    Record *r = malloc(sizeof(Record));

    if (r == NULL) {
        return NULL;
    }

    r->id = id;
    snprintf(r->name, sizeof(r->name), "Record_%d", id);

    r->data = malloc(MSG_SIZE);
    if (r->data == NULL) {
        free(r);
        return NULL;
    }

    snprintf(r->data, MSG_SIZE, "Data for record %d", id);

    return r;
}

void process_messages(void)
{
    printf("Processing messages...\n");

    for (int i = 0; i < 50; i++) {
        char *buf = malloc(MSG_SIZE);

        if (buf == NULL) {
            continue;
        }

        snprintf(buf, MSG_SIZE, "Message #%d: data payload here", i);
    }

    printf("Messages processed.\n");
}

int main(void)
{
    printf("=== LAB 03: Memory Leak Demo ===\n");

    Record **records = malloc(sizeof(Record *) * NUM_RECORDS);

    if (records == NULL) {
        fprintf(stderr, "Failed to allocate records array\n");
        return 1;
    }

    for (int i = 0; i < NUM_RECORDS; i++) {
        records[i] = create_record(i);

        if (records[i] == NULL) {
            fprintf(stderr, "Failed to create record %d\n", i);
            continue;
        }
    }

    process_messages();

    printf("Created %d records.\n", NUM_RECORDS);
    printf("Program finished.\n");

    return 0;
}
