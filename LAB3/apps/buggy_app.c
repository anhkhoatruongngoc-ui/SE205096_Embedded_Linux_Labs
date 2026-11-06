#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define ARRAY_SIZE 5

int numbers[ARRAY_SIZE] = {10, 20, 30, 40, 50};

void test_array_out_of_bounds(void)
{
    printf("\n[BUG 1] Array out of bounds\n");
    for (int i = 0; i <= ARRAY_SIZE; i++) {
        printf("numbers[%d] = %d\n", i, numbers[i]);
    }
}

char *create_message(const char *text)
{
    char *buf = malloc(strlen(text) + 1);

    if (buf == NULL) {
        return NULL;
    }

    strcpy(buf, text);
    return buf;
}

void test_use_after_free(void)
{
    printf("\n[BUG 2] Use after free\n");

    char *message = create_message("Hello from ARM");

    if (message == NULL) {
        printf("Memory allocation failed\n");
        return;
    }

    free(message);

    /* Intentionally accesses freed memory for debugging practice. */
    printf("Message: %s\n", message);
}

void test_integer_overflow(void)
{
    printf("\n[BUG 3] Integer overflow\n");

    volatile int value = INT_MAX;
    value = value + 1;

    printf("Result: %d\n", value);
}

int main(void)
{
    printf("=== LAB 03: GDB Debugging Demo ===\n");

    test_array_out_of_bounds();
    test_use_after_free();
    test_integer_overflow();

    printf("\nProgram finished.\n");
    return 0;
}
