#include <stdio.h>

#include "utils.h"

void show_data(const uint8_t* data, size_t size) {
    printf("  Buffer size: %i bytes \n", size);

    for(int i = 0; i < size; ++i) {
        if(i % 16 == 0) {
            if(i != 0) printf("\n");
            printf("  0x%.8x: ", i);
        }
        printf("0x%.2x ", *(data + i));
    }
    printf("\n");
}

struct timespec timespec_diff(struct timespec start, struct timespec stop) {
    struct timespec diff;

    if(stop.tv_nsec - start.tv_nsec < 0) {
        diff.tv_sec  = stop.tv_sec - start.tv_sec - 1;
        diff.tv_nsec = 1.0e9 + stop.tv_nsec - start.tv_nsec;
    } else {
        diff.tv_sec  = stop.tv_sec - start.tv_sec;
        diff.tv_nsec = stop.tv_nsec - start.tv_nsec;
    }

    return diff;
}
