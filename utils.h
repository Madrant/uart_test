#ifndef UTILS_H_
#define UTILS_H_

#include <inttypes.h>

void show_data(const uint8_t* data, size_t size);

#include <time.h>

struct timespec timespec_diff(struct timespec start, struct timespec stop);
struct timespec timespec_from_ms(uint32_t ms);

#endif /* UTILS_H_ */
