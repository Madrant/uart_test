#include "uart_options.h"

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <linux/spi/spidev.h>

#include "uart.h"

static const char* uart_default_device = UART_DEFAULT_DEVICE;

struct uart_options_t uart_default_options() {
    struct uart_options_t options;

    strncpy(options.device, uart_default_device, sizeof(options.device));
    options.speed = UART_DEFAULT_SPEED;

    options.bits = 8;
    options.parity = 0; /* None */
    options.stop_bits = 1;

    options.timeout_msec = UART_TIMEOUT_MSEC;
    options.bytes_limit = UART_BYTES_LIMIT;

    return options;
}

void uart_print_usage(const char *prog) {
    printf("Usage: %s [-Dsbpth] \n", prog);
    puts("  -D --device    <device to use> \n"
         "  -s --speed     <UART baud rate>\n"
         "  -b --bits      <bits>          \n"
         "  -p --parity    <parity>        \n"
         "  -t --stop_bits <stop bits>     \n"
         "  -h --help      print help      \n");
}

struct uart_options_t uart_parse_options(int argc, char** argv) {
    struct uart_options_t options = uart_default_options();

    while (1) {
        static const struct option lopts[] = {
            { "device",      1, 0, 'D' },
            { "speed",       1, 0, 's' },
            { "bits",        1, 0, 'b' },
            { "parity",      1, 0, 'p' },
            { "stop_bits",   1, 0, 't' },
            { "help",        0, 0, 'h' },
            { NULL,          0, 0, 0   },
        };
        int c;

        c = getopt_long(argc, argv, "D:s:b:p:t:h", lopts, NULL);
        if (c == -1)
            break;

        switch (c) {
            case 'D':
                strncpy(options.device, optarg, sizeof(options.device));
                break;
            case 's':
                options.speed = atoi(optarg);
                break;
            case 'b':
                options.bits = atoi(optarg);
                break;
            case 'p':
                options.parity = atoi(optarg);
                break;
            case 't':
                options.stop_bits = atoi(optarg);
                break;
            case 'h':
                uart_print_usage(argv[0]);
                break;
            case '?':
                break;
            default:
                uart_print_usage(argv[0]);
                break;
        }
    } /* while */

    return options;
}
