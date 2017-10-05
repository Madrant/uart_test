#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <assert.h>
#include <stdlib.h>

#ifdef __powerpc__
/*
 * On PowerPC there is only struct termios, which includes
 * fields c_ispeed and c_ospeed, so we just includes <sys/ioctl.h>
 * which internally includes <termios.h>
 */
#include <sys/ioctl.h>
/*
 * BOTHER constant defined in <asm/tembits.h> which conflicts
 * with <termios.h> by redefining struct termios
 * so we just define BOTHER constant manually
 */
#ifndef BOTHER
#define BOTHER 00037
#endif
#else
/*
 * Header files <asm/termbits.h> and <asm/ioctls.h>
 * included instead of <termios.h> to get access
 * to struct termios2 and avoid conflicts with GLIBC
 * header file <termios.h> which defines struct termios only
 */
#include <asm/termbits.h>
#include <asm/ioctls.h>
#include <sys/ioctl.h>
#endif /* __powerpc__ */

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
    int ret = 0;

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

    ret = uart_set_interface_attribs(uart, uart->speed, uart->bits, uart->parity, uart->stop_bits);
    if (ret != 0) {
        errprintf("uart_set_interface_attribs() failed\n");
        return NULL;
    }

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
    strncpy(instance->dev, serial_device, sizeof(instance->dev));
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

int uart_set_interface_attribs (struct uart_t *instance, unsigned int speed, int bits, int parity, int stop_bits) {
        assert(instance != NULL);

#ifdef __powerpc__
#define IOCTL_GETS TCGETS
#define IOCTL_SETS TCSETS
        /* On PowerPC struct termios include fields c_ispeed and c_ospeed */
        struct termios tty;
#else
#define IOCTL_GETS TCGETS2
#define IOCTL_SETS TCSETS2
        struct termios2 tty;
#endif
        memset (&tty, 0, sizeof(tty));

        if (ioctl(instance->fd, IOCTL_GETS, &tty) != 0)
        {
                strerr("ioctl(TCGETS) failed");
                return -1;
        }

        /* Clear standard baud rates flag */
        tty.c_cflag &= ~CBAUD;
        tty.c_cflag |= BOTHER;

        /* Set custom speed */
        tty.c_ispeed = speed;
        tty.c_ospeed = speed;

        /* Set bits per byte */
        switch (instance->bits) {
            case 5:
                tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS5;
                break;
            case 6:
                tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS6;
                break;
            case 7:
                tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS7;
                break;
            case 8:
                tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
                break;
            default:
                errprintf("Bad param: bits: %d\n", instance->bits);
                return -1;
        }

        /* Disable parity bits before parity parsing */
        tty.c_cflag &= ~(PARENB | PARODD);

        /* Set parity mode */
        switch (instance->parity) {
            case UART_PARITY_NONE:
                break;
            case UART_PARITY_ODD:
                tty.c_cflag |= (PARENB | PARODD);
                break;
            case UART_PARITY_EVEN:
                tty.c_cflag |= PARENB;
                break;
            default:
                errprintf("Bad param: parity: %d\n", instance->parity);
                return -1;
        }

        /* Set stop bits */
        switch (instance->stop_bits) {
            case 1:
                tty.c_cflag &= ~CSTOPB;
                break;
            case 2:
                tty.c_cflag |= CSTOPB;
                break;
            default:
                errprintf("Bad param: stop bits: %d\n", instance->stop_bits);
                return -1;
        }

        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 1;            // blocking read
        tty.c_cc[VTIME] = 0;            // no timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // enable xon/xoff ctrl
        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls, enable reading
        tty.c_cflag &= ~CRTSCTS;

        if (ioctl(instance->fd, IOCTL_SETS, &tty) != 0)
        {
            strerr("ioctl(TCSETS) failed");
            return -1;
        }

        /* Check installed parameters */
        if (ioctl(instance->fd, IOCTL_GETS, &tty) != 0)
        {
            strerr("ioctl(TCGETS) failed");
            return -1;
        }

        dprintf("Read back params:\n");
        dprintf("Speed: %d %d\n", tty.c_ospeed, tty.c_ispeed);
        dprintf("c_cflag: 0x%08x\n", tty.c_cflag);
        dprintf("c_iflag: 0x%08x\n", tty.c_iflag);
        dprintf("c_oflag: 0x%08x\n", tty.c_oflag);
        dprintf("c_lflag: 0x%08x\n", tty.c_lflag);

        /* TODO: check params set */

        instance->speed = speed;
        instance->bits = bits;
        instance->parity = parity;
        instance->stop_bits = stop_bits;

        /* Set exclusive mode for tty */
        if (ioctl(instance->fd, TIOCEXCL) != 0)
        {
            strerr("ioctl(TIOCEXCL) failed");
            return -1;
        }

        return 0;
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

ssize_t uart_read(struct uart_t *instance, void *buf, size_t count) {
    assert(instance != NULL);
    assert(buf != NULL);

    ssize_t bytes = 0;
    size_t bytes_read = 0;
    size_t bytes_total = count;

    uint32_t tries_counter = 0;

    while(bytes_read != bytes_total)
    {
        bytes = read(instance->fd, buf, count);
        if(bytes == -1) {
            strerr("uart_read() : read() error");
            return bytes_read;
        }

        buf += bytes;
        bytes_read += bytes;
        count -= bytes;

        /* Non-blocking read exit */
        if(tries_counter++ > 10000) {
            errprintf("read() retry counter exceeds: only %i of %i bytes read\n"
                      ,bytes_read, bytes_total);
            return bytes_read;
        }
    } /* while */

    assert(bytes_read <= bytes_total);

    return bytes_read;
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

        /* Non-blocking read exit */
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

const struct serial_icounter_struct* uart_get_icounter(struct uart_t *instance) {
    assert(instance != NULL);

    static struct serial_icounter_struct icount;
    int ret = 0;

    ret = ioctl(instance->fd, TIOCGICOUNT, &icount);
    if(ret != 0) {
        strerr("ioctl(TIOCGICOUNT) failed");
        return NULL;
    }

    return &icount;
}

void uart_print_icounter(struct uart_t *instance) {
    assert(instance != NULL);

    const struct serial_icounter_struct *icount = uart_get_icounter(instance);
    if(icount == NULL)
        return;

    printf("UART '%s' icounters:\n", instance->dev);
    printf("rx: %i tx: %i ferr: %i overrun: %i perr: %i brk: %i buf_overrun: %i\n",
            icount->rx,  icount->tx, icount->frame, icount->overrun, icount->parity,
            icount->brk, icount->buf_overrun);

    return;
}
