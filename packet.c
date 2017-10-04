#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h> /* rand() */
#include <time.h>

#include "packet.h"

struct packet_t create_packet(size_t packet_length) {
    struct packet_t packet;

    static int packet_number = 1;
    static int header_size = sizeof(packet.number) + sizeof(packet.data_size) + sizeof(packet.crc32);

    assert(header_size == PACKET_HEADER_SIZE);

    packet.number = packet_number++;
    packet.data_size = packet_length - header_size;

    assert(packet.data_size >= 0);

    packet.data = generate_data(packet.data_size);
    packet.crc32  = crc32(0x00, packet.data, packet.data_size);

    return packet;
}

unsigned char* generate_data(size_t length) {
    unsigned char* buffer = (unsigned char*)malloc(length);

    if (buffer == NULL) {
        printf("generate_data: malloc() failed\n");
        exit(1);
    }

    static int srandomized = 0;

    if(!srandomized) {
        srand(time(NULL));
        srandomized = 1;
    }

    for(int i  = 0; i < length; ++i) {
        buffer[i] = (unsigned char)rand();
    }

    return buffer;
}

struct data_t packet_to_data(struct packet_t packet) {
    struct data_t data;

    int header_size = sizeof(packet.number) + sizeof(packet.data_size) + sizeof(packet.crc32);

    assert(header_size == PACKET_HEADER_SIZE);

    data.ptr = (unsigned char*)malloc(header_size + packet.data_size);
    if (data.ptr == NULL) {
        printf("packet_to_data: malloc() failed\n");
        exit(1);
    }

    unsigned int offset = 0;

    /* Copy header */
    memcpy((void*)(data.ptr + offset), (void*)&packet.number, sizeof(packet.number));
    offset += sizeof(packet.number);

    memcpy((void*)(data.ptr + offset), (void*)&packet.data_size, sizeof(packet.data_size));
    offset += sizeof(packet.data_size);

    memcpy((void*)(data.ptr + offset), (void*)&packet.crc32, sizeof(packet.crc32));
    offset += sizeof(packet.crc32);

    /* Copy data */
    memcpy((void*)(data.ptr + offset), (void*)packet.data, packet.data_size);
    offset += packet.data_size;

    data.size = packet.data_size + header_size;

    return data;
}

struct packet_t packet_from_data(struct data_t data) {
    struct packet_t packet;
    int header_size = sizeof(packet.number) + sizeof(packet.data_size) + sizeof(packet.crc32);

    assert(header_size == PACKET_HEADER_SIZE);

    unsigned char* buffer = (unsigned char*)malloc(data.size - header_size);
    if (buffer == NULL) {
        printf("packet_from_data: malloc() failed\n");
        exit(1);
    }

    unsigned int offset = 0;

    memcpy((void*)&packet.number, (void*)(data.ptr + offset), sizeof(packet.number));
    offset += sizeof(packet.number);

    memcpy((void*)&packet.data_size, (void*)(data.ptr + offset), sizeof(packet.data_size));
    offset += sizeof(packet.data_size);

    memcpy((void*)&packet.crc32, (void*)(data.ptr + offset), sizeof(packet.crc32));
    offset += sizeof(packet.crc32);

    memcpy((void*)buffer, (void*)(data.ptr + offset), data.size - header_size);
    offset += data.size - header_size;

    packet.data = buffer;
    packet.data_size = data.size - header_size;

    return packet;
}

void show_packet_info(struct packet_t *packet) {
    printf("Packet: [Number: %.8i   Data size: %i   CRC32: 0x%.8x]\n", packet->number, packet->data_size, packet->crc32);
}

void show_packet_data(struct packet_t *packet) {
    printf("Packet data: [\n");
    for(int i = 0; i < packet->data_size; ++i) {
        if(i % 4 == 0) {
            printf("\n");
        }
        printf("0x%.8x ", (unsigned int)packet->data[i]);
    }
    printf(" ]\n");
}

void show_data_struct(struct data_t *data) {
    printf("Packet data: \n");
    for(int i = 0; i < data->size; ++i) {
        if(i % 16 == 0) {
            printf("\n 0x%.8x: ", i);
        }
        printf("0x%.2x ", *(data->ptr + i));
    }
    printf("\n");
}
