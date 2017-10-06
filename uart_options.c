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

    options.bits = UART_DEFAULT_BITS;
    options.parity = UART_DEFAULT_PARITY;
    options.stop_bits = UART_DEFAULT_STOP_BITS;

    options.timeout_msec = UART_TIMEOUT_MSEC;
    options.bytes_limit = UART_BYTES_LIMIT;

    return options;
}

void uart_print_usage(const char *prog) {
    printf("UART options: %s [-Dsbpth] \n", prog);
    puts("  -D --device <device>       - set UART device to use  \n"
         "  -s --speed <baud rate>     - set UART baud rate (any)\n"
         "  -b --bits <bits>           - set UART bits (5, 6, 7, 8) \n"
         "  -p --parity <parity>       - set parity (0 - none, 1 - odd, 2 - even) \n"
         "  -t --stop_bits <stop bits> - set stop bits (1, 2)    \n"
         "  -h --help                  - print help \n");
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

    /* check options */
    if(options.bits != UART_BITS_5 &&
       options.bits != UART_BITS_6 &&
       options.bits != UART_BITS_7 &&
       options.bits != UART_BITS_8) {
        printf("UART: wrong bits selected: %i\n", options.bits);
        uart_print_usage(argv[0]);
        exit(1);
    }
    if(options.parity != UART_PARITY_NONE &&
       options.parity != UART_PARITY_EVEN &&
       options.parity != UART_PARITY_ODD) {
        printf("UART: wrong parity selected: %i\n", options.parity);
        uart_print_usage(argv[0]);
        exit(1);
    }

    if(options.stop_bits != UART_STOP_BITS_1 &&
       options.stop_bits != UART_STOP_BITS_2) {
        printf("UART: wrong stop bits selected: %i\n", options.stop_bits);
        uart_print_usage(argv[0]);
        exit(1);
    }

    return options;
}
