#ifndef _UART_OPTIONS_H_
#define _UART_OPTIONS_H_

#include <inttypes.h>
#include <stddef.h>

struct uart_options_t {
    char *device;

    int speed;
    int parity;    /* 0 - none, 1 -odd, 2 - even */
    int stop_bits; /* 1, 2 */
    int bits;      /* 5, 6, 7, 8 */

    int timeout_msec;
    int bytes_limit;
};

struct uart_options_t uart_default_options();

void uart_print_usage(const char *prog);

struct uart_options_t uart_parse_options(int argc, char** argv);

#endif /* _UART_OPTIONS_H_ */
