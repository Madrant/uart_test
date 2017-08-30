#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <assert.h>
#include <fcntl.h>
#include <termios.h>

#include "config.h"

#include "uart.h"
#include "uart_options.h"

#include "utils.h"

struct options_t {
    struct uart_options_t uart_options;
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
    printf("Usage: %s [-u [ udp port] -m --mode [mode] \n", prog);
    puts("  -m --mode work mode: 0 - OECA, 1 - BKB (default) \n"
         "  -u UDP port to use \n"
         "  -h print help\n");
}

struct options_t parse_options(int argc, char** argv) {
    struct options_t options;

    /* disable getopt_long error messages */
    opterr = 0;

    /* copy argv to avoid getopt_long argv corruption */
    char* argv_uart[argc];

    for(int i = 0; i < argc; i++) {
        argv_uart[i] = strdup(argv[i]);
    }

    while (1) {
        static const struct option lopts[] = {
            { "help",      0, 0, 'h' },
            { NULL,        0, 0, 0   },
        };
        int c;

        c = getopt_long(argc, argv, "h", lopts, NULL);
        if (c == -1)
            break;

        switch (c) {
            case 'h':
                print_usage(argv[0]);
                uart_print_usage(argv[0]);
                exit(1);
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

    printf("UART test \n");
    printf("\n");

    printf("    UART device: %s \n",      options.uart_options.device);
    printf("    UART speed:  %i \n",      options.uart_options.speed);
    printf("    UART bits:   %i \n",      options.uart_options.bits);
    printf("    UART parity: %i \n",      options.uart_options.parity);
    printf("    UART stop bits:  %i \n",  options.uart_options.stop_bits);


    /* Initialization */
    struct uart_t *uart = NULL;

    uart = uart_init(options.uart_options.device, options.uart_options);
    if(uart == NULL) {
        printf("UART init failed - exit\n");
        exit(-1);
    }

    return 0;
}
