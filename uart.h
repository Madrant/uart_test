#ifndef RS232_H
#define RS232_H

#include <inttypes.h>
#include <stddef.h>

#include "uart_options.h"

#define UART_DEFAULT_DEVICE      "/dev/ttyS0"
#define UART_DEFAULT_SPEED       (9600)

#define UART_PARITY_NONE 0
#define UART_PARITY_ODD  1
#define UART_PARITY_EVEN 2

struct uart_t {
    int fd;
    char dev[64];

    int speed;
    int parity;    /* 0 - none, 1 -odd, 2 - even */
    int stop_bits; /* 1, 2 */
    int bits;      /* 5, 6, 7, 8 */

    int timeout_msec;
    int bytes_limit;
};

#define UART_TIMEOUT_MSEC 300
#define UART_BYTES_LIMIT  1024

struct uart_t* uart_init(char* dev, struct uart_options_t options);

struct uart_t* uart_open(const char* serial_device);
int uart_close(struct uart_t *instance);

int uart_set_interface_attribs (struct uart_t *instance, unsigned int speed, int bits, int parity, int stop_bits);
void uart_set_blocking (struct uart_t *instance, int should_block);

int uart_poll(struct uart_t *instance, int timeout_msec);

ssize_t  uart_read(struct uart_t *instance, void *buf, size_t count);
uint8_t  uart_read_byte(struct uart_t *instance);
uint32_t uart_read_word(struct uart_t *instance);

int uart_write(struct uart_t *instance, const void* buf, size_t count);

#endif /* RS232_H */
