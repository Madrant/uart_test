#ifndef _UART_OPTIONS_H_
#define _UART_OPTIONS_H_

#include <inttypes.h>
#include <stddef.h>

struct uart_options_t {
    char device[64];

    uint32_t speed;
    uint8_t parity;    /* 0 - none, 1 - odd, 2 - even */
    uint8_t stop_bits; /* 1, 2 */
    uint8_t bits;      /* 5, 6, 7, 8 */

    uint32_t timeout_msec;
    uint32_t bytes_limit;
};

struct uart_options_t uart_default_options();

void uart_print_usage(const char *prog);

struct uart_options_t uart_parse_options(int argc, char** argv);

#endif /* _UART_OPTIONS_H_ */
