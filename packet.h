#ifndef _PACKET_H
#define _PACKET_H

#include <inttypes.h>

struct packet_t {
    uint32_t number;
    uint32_t crc32;
    uint8_t* data;
    size_t   data_size;
};

struct data_t {
    uint8_t* ptr;
    size_t   size;
};

struct  packet_t create_packet(size_t packet_length);

struct  data_t   packet_to_data(struct packet_t packet);
struct  packet_t packet_from_data(struct data_t);

uint8_t*  generate_data(size_t length);
void      show_data_struct(struct data_t data);

void show_packet_info(struct packet_t packet);
void show_packet_data(struct packet_t packet);

extern uint32_t crc32(uint32_t crc, const void *buf, size_t size);

#endif /* _PACKET_H */
