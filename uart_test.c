#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <inttypes.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>

#include "config.h"

#include "uart.h"
#include "uart_options.h"

#include "packet.h"

#include "utils.h"

#define DIRECTION_SEND 1
#define DIRECTION_RECV 0

struct options_t {
    struct uart_options_t uart_options;

    uint32_t packet_length;
    uint32_t packets_num;
    uint32_t send_delay_ms;
    uint8_t  direction; /* 0 - receive, 1 - send */
    uint8_t  verbose;
};

struct options_t options;

static uint8_t test_in_action = 1;

void register_signal_handler();
void signal_handler(int signal);

#define N_    "UART_TEST: "
#define N_ERR "UART_TEST_ERROR: "

#ifdef D_DEBUG
#define dprintf(format, ...) \
        printf(N_ format, ##__VA_ARGS__)
#else
#define dprintf
#endif /* D_DEBUG */

#define strerr(format, ...) \
        printf(N_ERR format "%s : %i\n", strerror(errno), errno)

#define errprintf(format, ...) \
        printf(N_ERR format, ##__VA_ARGS__)

#ifdef D_DEBUG
#include "utils.h"
#define dshow_data(data, size) \
        show_data(data, size)
#else
#define dshow_data
#endif /* D_DEBUG */

void print_usage(const char *prog) {
    printf("Common options: %s [-lndRvh] \n", prog);
    puts(
    "  -l --packet_length <length> - set packet length (min 12 bytes for header) \n"
    "  -n --packets_num <num>      - set packets number     \n"
    "  -d --delay <msec>           - set send delay in msec \n"
    "  -R --receive                - receive packets only   \n"
    "  -v --verbose                - enable verbose mode    \n"
    "  -h --help                   - print help\n");
}

struct options_t parse_options(int argc, char** argv) {
    struct options_t options;

    /* Default options */
    options.packet_length = 32;
    options.packets_num = 4;
    options.send_delay_ms = 0;
    options.direction = DIRECTION_SEND;
    options.verbose = 0;

    /* disable getopt_long error messages */
    opterr = 0;

    /* copy argv to avoid getopt_long argv corruption */
    char* argv_uart[argc];

    for(int i = 0; i < argc; i++) {
        argv_uart[i] = strdup(argv[i]);
    }

    while (1) {
        static const struct option lopts[] = {
            { "help",          0, 0, 'h' },
            { "packet_length", 1, 0, 'l' },
            { "packets_num",   1, 0, 'n' },
            { "delay",         1, 0, 'd' },
            { "receive",       1, 0, 'R' },
            { "verbose",       1, 0, 'v' },
            { NULL,        0, 0, 0   },
        };
        int c;

        c = getopt_long(argc, argv, "hl:n:d:Rv", lopts, NULL);
        if (c == -1)
            break;

        switch (c) {
            case 'h':
                print_usage(argv[0]);
                uart_print_usage(argv[0]);
                exit(1);
                break;
            case 'l':
                options.packet_length = atoi(optarg);
                if (options.packet_length < PACKET_HEADER_SIZE) {
                    printf("Wrong packet length: min is 12 bytes\n");
                    exit(1);
                }
                break;
            case 'n':
                options.packets_num = atoi(optarg);
                break;
            case 'd':
                options.send_delay_ms = atoi(optarg);
                break;
            case 'R':
                options.direction = DIRECTION_RECV;
                break;
            case 'v':
                options.verbose = 1;
                break;
            case '?':
                break;
            default:
                print_usage(argv[0]);
                uart_print_usage(argv[0]);
                exit(1);
                break;
        }

    } /* while */

    optind = 1; /* reset getopt_long index */
    options.uart_options = uart_parse_options(argc, argv_uart);

    for(int i = 0; i < argc; i++) {
        free(argv_uart[i]);
    }

    return options;
}

void send_packets(struct uart_t *uart, struct options_t *options) {
    unsigned int packets_send = 0;
    int bytes = 0;

    assert(options != NULL);
    assert(uart != NULL);
    assert(uart->fd > 0);

    /* Convert delay from msec to struct timespec */
    struct timespec sleep_time;
    if(options->send_delay_ms >= 1000) {
        sleep_time.tv_sec  = options->send_delay_ms / 1000;
        sleep_time.tv_nsec = 0;
    } else {
        sleep_time.tv_sec  = 0;
        sleep_time.tv_nsec = options->send_delay_ms * 1000000L;
    }

    for(int i = 0; i < options->packets_num; ++i) {
        struct packet_t packet = create_packet(options->packet_length);
        struct data_t data = packet_to_data(packet);

        show_packet_info(&packet);

        bytes = uart_write(uart, (const void*)data.ptr, data.size);
        if (bytes == -1) {
            strerr("UART write failed\n");
            exit(1);
        }

        if (bytes != data.size) {
             printf("Warning: Partial write: %d of %d\n", bytes, data.size);
        }

        if(options->verbose == 1) {
            printf("Packet dump: %i\n", bytes);
            show_data_struct(&data);
        }

        packets_send++;
        nanosleep(&sleep_time, NULL);

        if(test_in_action == 0) {
            break;
        }
    } /* for 0 to options->packets_num */

    printf("Transfer done:\n\tPackets send: %i\n", packets_send);
}

void read_packets(struct uart_t *uart, struct options_t *options) {
    unsigned int packets_received = 0;
    unsigned int crc_errors       = 0;
    unsigned int packets_lost     = 0;
    unsigned int prev_packet_num  = 0;
    unsigned int crc              = 0;

    struct packet_t packet;
    struct data_t data;

    assert(options != NULL);
    assert(uart != NULL);

    data.ptr = (unsigned char*)malloc(options->packet_length);
    data.size = options->packet_length;

    if (data.ptr == NULL) {
        printf("read_packets: malloc() failed\n");
        exit(1);
    }

    assert(uart->fd > 0);

    while(test_in_action != 0) {
        int bytes = uart_read(uart, data.ptr, data.size);
        if(bytes < 0) {
            strerr("UART read() failed\n");
            exit(1);
        }

        if(bytes == 0) {
            /* No data yet */
            continue;
        }

        if(bytes != options->packet_length) {
            printf("Error: Bytes read: %i, packet size: %i\n", bytes, options->packet_length);
            break;
        }
        data.size = bytes;

        packet = packet_from_data(data);
        show_packet_info(&packet);
        packets_received++;

        if(options->verbose == 1) {
            printf("Packet dump: %i\n", bytes);
            show_data_struct(&data);
        }

        if(prev_packet_num != 0 /*ignore first transfer*/ && packet.number > 1)
        if(packet.number - prev_packet_num != 1) {
            printf("Warning! Packet lost [%.8i ... %.8i]\n", prev_packet_num, packet.number);
            packets_lost++;
        }
        prev_packet_num = packet.number;

        crc = crc32(0x00, packet.data, packet.data_size);
        if(packet.crc32 != crc) {
            crc_errors++;
            printf("Warning! wrong crc [0x%.8x] for packet: #%.8i crc32[0x%.8x]\n", crc, packet.number, packet.crc32);
        } else {
            printf("CRC32 [0x%.8x]: OK\n", packet.crc32);
        }

        free(packet.data);
    }

    printf("Test completed:\n");
    printf("\tPackets received: %i\n", packets_received);
    printf("\tCRC errors:       %i\n", crc_errors);
    printf("\tPackets lost:     %i\n", packets_lost);
}

int main(int argc, char *argv[]) {
    options = parse_options(argc, argv);

    printf("UART test started\n");

    printf("UART options:\n");
    printf("    UART device:    %s \n", options.uart_options.device);
    printf("    UART speed:     %i \n", options.uart_options.speed);
    printf("    UART bits:      %i \n", options.uart_options.bits);
    printf("    UART parity:    %i \n", options.uart_options.parity);
    printf("    UART stop bits: %i \n", options.uart_options.stop_bits);

    printf("Common options:\n");
    printf("    Packet length:  %i \n", options.packet_length);
    printf("    Packets num:    %i \n", options.packets_num);
    printf("    Send delay, ms: %i \n", options.send_delay_ms);
    printf("    Direction:      %s \n", (options.direction == DIRECTION_SEND ? "Send" : "Receive"));
    printf("    Verbose mode:   %s \n", (options.verbose == 1 ? "Enabled" : "Disabled"));

    register_signal_handler();

    /* Initialization */
    struct uart_t *uart = NULL;

    uart = uart_init(options.uart_options.device, options.uart_options);
    if(uart == NULL) {
        printf("UART init failed - exit\n");
        exit(-1);
    }

    /* Print UART icounters */
    uart_print_icounter(uart);

    /* Do work */
    if(options.direction == DIRECTION_SEND)
        send_packets(uart, &options);
    else
        read_packets(uart, &options);

    /* Print UART icounters */
    uart_print_icounter(uart);

    /* Close devices */
    uart_close(uart);

    return 0;
}

void register_signal_handler() {
    struct sigaction sa;
    memset(&sa, 0, sizeof (sa));

    sa.sa_handler = signal_handler;

    sigfillset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
}

void signal_handler(int signal) {
    if(test_in_action)
        test_in_action = 0;
    else
        exit(0);
}
