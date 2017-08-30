#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <assert.h>
#include <stdlib.h>

#include "uart.h"

#include "uart_options.h"

#define N_ "UART: "
#define N_ERR "UART ERROR: "

//#define UART_DEBUG_BYTES

#ifdef UART_DEBUG
#define dprintf(format, ...) \
        printf(N_ format, ##__VA_ARGS__)
#else
#define dprintf
#endif

#define strerr(format, ...) \
        printf(N_ERR format " %s : %i\n", ##__VA_ARGS__, strerror(errno), errno)

#define errprintf(format, ...) \
        printf(N_ERR format, ##__VA_ARGS__)

struct uart_t* uart_init(char* dev, struct uart_options_t options) {
    struct uart_t *uart = uart_open(dev);
    if(uart == NULL) {
        strerr("UART init failed - exit \n");
        exit(-1);
    }

    uart->speed = options.speed;
    uart->bits = options.bits;
    uart->parity = options.parity;
    uart->stop_bits = options.stop_bits;

    uart->timeout_msec = options.timeout_msec;
    uart->bytes_limit = options.bytes_limit;

    uart_set_interface_attribs(uart, uart->speed, 0); // set speed to 9,600 bps, 8n1 (no parity)
    uart_set_blocking(uart, 1);                       // set blocking

    return uart;
}

struct uart_t* uart_open(const char* serial_device) {
    assert(serial_device != NULL);

    struct uart_t *instance = (struct uart_t*)malloc(sizeof(struct uart_t));
    memset(instance, 0x00, sizeof(struct uart_t));

    dprintf("Opening %s \n", serial_device);

    int fd = open(serial_device, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        strerr("open() %s error", serial_device);

        free(instance);
        return NULL;
    }

    assert(fd > 0);

    instance->fd = fd;
    instance->dev = serial_device;
    instance->timeout_msec = UART_TIMEOUT_MSEC;
    instance->bytes_limit  = UART_BYTES_LIMIT;

    dprintf("Device %s opened successfully \n", serial_device);

    return instance;
}

int uart_close(struct uart_t *instance) {
    assert(instance != NULL);

    dprintf("Closing device %s with fd %i \n", instance->dev, instance->fd);

    int ret = close(instance->fd);
    if (ret < 0) {
        strerr("close() error");
        return -1;
    }

    dprintf("Device %s closed\n", instance->dev);
    free(instance);

    return 0;
}

int uart_set_interface_attribs (struct uart_t *instance, int speed, int parity) {
        assert(instance != NULL);

        struct termios tty;
        memset (&tty, 0, sizeof tty);

        if (tcgetattr (instance->fd, &tty) != 0)
        {
                strerr("tcgetattr() error");
                return -1;
        }

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // enable xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (instance->fd, TCSANOW, &tty) != 0)
        {
                strerr("tcsetattr() error");
                return -1;
        }

        instance->speed = speed;
        instance->parity = parity;

        return 0;
}

void uart_set_blocking (struct uart_t *instance, int should_block) {
        assert(instance != NULL);

        struct termios tty;
        memset (&tty, 0, sizeof tty);

        if (tcgetattr (instance->fd, &tty) != 0)
        {
            dprintf("error %d from tggetattr\n", errno);
            return;
        }

        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        if (tcsetattr (instance->fd, TCSANOW, &tty) != 0)
            dprintf("error %d setting term attributes\n", errno);

        if (should_block) {
            int flags = fcntl(instance->fd, F_GETFL, 0);
            fcntl(instance->fd, F_SETFL, flags | ~O_NONBLOCK);
        } else {
            int flags = fcntl(instance->fd, F_GETFL, 0);
            fcntl(instance->fd, F_SETFL, flags & O_NONBLOCK);
        }
}

/*
 * return codes:
 * -1 - error
 * 0  - timeout
 * 1  - data avaiable
 */
int uart_poll(struct uart_t *instance, int timeout_msec) {
    assert(instance != NULL);

    struct pollfd fds;
    memset(&fds, 0x00, sizeof(struct pollfd));

    fds.fd = instance->fd;
    fds.events = POLLIN;

    int ret = poll(&fds, 1, timeout_msec);
    if(ret == -1) {
        strerr("uart_poll() : poll() failed");
        return -1;
    }

    if(ret == 0) {
        dprintf("uart_poll(): timeout \n");
        return 0;
    }

    if(fds.revents != POLLIN) {
        printf("uart_poll(): unexpected revents returned");
    }

    return 1;
}

unsigned char uart_read_byte(struct uart_t *instance) {
    assert(instance != NULL);

    unsigned char c = 0x00;
    int ret = 0;
    int counter = 0;

    while(ret != 1) {
        ret = read(instance->fd, (unsigned char*)&c, 1);
        if(ret == -1) {
            strerr("uart_read_byte() : read() error");
        }

        if(counter++ > 10000) {
            errprintf("read() retry counter exceeds - return 0x00 \n");
            return 0x00;
        }

        #ifdef UART_DEBUG_BYTES
        if(ret == 1) {
            dprintf("c = 0x%02x \n", c);
        }
        #endif
    } /* while */

    return c;
}

unsigned int uart_read_word(struct uart_t *instance) {
    assert(instance != NULL);

    uint32_t word;
    uint8_t w1, w2, w3, w4;

    /*
     *  read(fd, (uint8_t*)&word, sizeof(uint32_t))
     *  is not used to avoid read 1,2 or 3 bytes
     */
    w1 = uart_read_byte(instance);
    w2 = uart_read_byte(instance);
    w3 = uart_read_byte(instance);
    w4 = uart_read_byte(instance);

    word = w1 << 24 | w2 << 15 | w3 << 8 | w4;

    #ifdef UART_DEBUG_WORDS
    dprintf("word = 0x%08x \n", word);
    #endif

    return word;
}

int uart_write(struct uart_t *instance, const void* buf, size_t count) {
    assert(instance != NULL);
    assert(buf != NULL);

    int ret = write(instance->fd, (const uint8_t*)buf, count);

    if(ret == -1) {
        strerr("write() error");
        return -1;
    }

    return ret;
}
