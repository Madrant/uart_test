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

struct udp_t *udp = NULL;

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
    "  -l --packet_length <length> - set packet length      \n"
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

    /* Initialization */
    struct uart_t *uart = NULL;

    uart = uart_init(options.uart_options.device, options.uart_options);
    if(uart == NULL) {
        printf("UART init failed - exit\n");
        exit(-1);
    }

    /* Do work */
    int ret = 0;

    char out_buf[] = "test";
    int out_bytes = sizeof(out_buf);

    printf("Performing test write:\n");
    show_data(out_buf, out_bytes);

    ret = uart_write(uart, out_buf, out_bytes);
    if (ret == -1) {
        printf("UART write failed\n");
    }

    if (ret != out_bytes) {
        printf("Warning: Partial write: %d of %d\n", ret, out_bytes);
    }

    printf("Wait for incoming data\n");

    unsigned char byte = 0x00;

    while (1) {
        byte = uart_read_byte(uart);
        printf("Byte: 0x%02x %d '%c'\n", byte, byte, byte);

        uart_write(uart, &byte, sizeof(byte));
    }

    /* Close devices */
    uart_close(uart);

    return 0;
}
