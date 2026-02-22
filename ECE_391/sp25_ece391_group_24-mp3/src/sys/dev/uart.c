// uart.c - NS8250-compatible uart port
// 
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef UART_TRACE
#define TRACE
#endif

#ifdef UART_DEBUG
#define DEBUG
#endif

#include "conf.h"
#include "assert.h"
#include "uart.h"
#include "device.h"
#include "intr.h"
#include "heap.h"
#include "thread.h"

#include "ioimpl.h"
#include "console.h"

#include "error.h"

#include <stdint.h>

// COMPILE-TIME CONSTANT DEFINITIONS
//

#ifndef UART_RBUFSZ
#define UART_RBUFSZ 64
#endif

#ifndef UART_INTR_PRIO
#define UART_INTR_PRIO 1
#endif

#ifndef UART_NAME
#define UART_NAME "uart"
#endif

// INTERNAL TYPE DEFINITIONS
// 

struct uart_regs {
    union {
        char rbr; // DLAB=0 read
        char thr; // DLAB=0 write
        uint8_t dll; // DLAB=1
    };
    
    union {
        uint8_t ier; // DLAB=0
        uint8_t dlm; // DLAB=1
    };
    
    union {
        uint8_t iir; // read
        uint8_t fcr; // write
    };

    uint8_t lcr;
    uint8_t mcr;
    uint8_t lsr;
    uint8_t msr;
    uint8_t scr;
};

#define LCR_DLAB (1 << 7)
#define LSR_OE (1 << 1)
#define LSR_DR (1 << 0)
#define LSR_THRE (1 << 5)
#define IER_DRIE (1 << 0)
#define IER_THREIE (1 << 1)

struct ringbuf {
    unsigned int hpos; // head of queue (from where elements are removed)
    unsigned int tpos; // tail of queue (where elements are inserted)
    char data[UART_RBUFSZ];
};

struct uart_device {
    volatile struct uart_regs * regs;
    int irqno;
    int instno;

    struct io io;

    unsigned long rxovrcnt; // number of times OE was set

    struct ringbuf rxbuf;
    struct ringbuf txbuf;

    struct condition tx_not_full;
    struct condition rx_not_empty;
};

// INTERNAL FUNCTION DEFINITIONS
//

static int uart_open(struct io ** ioptr, void * aux);
static void uart_close(struct io * io);
static long uart_read(struct io * io, void * buf, long bufsz);
static long uart_write(struct io * io, const void * buf, long len);

static void uart_isr(int srcno, void * driver_private);

static void rbuf_init(struct ringbuf * rbuf);
static int rbuf_empty(const struct ringbuf * rbuf);
static int rbuf_full(const struct ringbuf * rbuf);
static void rbuf_putc(struct ringbuf * rbuf, char c);
static char rbuf_getc(struct ringbuf * rbuf);

// EXPORTED FUNCTION DEFINITIONS
// 

// void uart_attach(void * mmio_base, int irqno)
// Inputs: void * mmio_base - base address of the UART memory-mapped I/O
//         int irqno - interrupt number of the UART
// Outputs: none
// Description: Attaches the UART device to the system
// Side Effects: UART device is registered with the device manager and memory is allocated for the UART device
void uart_attach(void * mmio_base, int irqno) {
    static const struct iointf uart_iointf = {
        .close = &uart_close,
        .read = &uart_read,
        .write = &uart_write
    };

    struct uart_device * uart;

    uart = kcalloc(1, sizeof(struct uart_device));

    uart->regs = mmio_base;
    uart->irqno = irqno;

    ioinit0(&uart->io, &uart_iointf);

    // Check if we're trying to attach UART0, which is used for the console. It
    // had already been initialized and should not be accessed as a normal
    // device.

    if (mmio_base != (void*)UART0_MMIO_BASE) {

        uart->regs->ier = 0;
        uart->regs->lcr = LCR_DLAB;
        // fence o,o ?
        uart->regs->dll = 0x01;
        uart->regs->dlm = 0x00;
        // fence o,o ?
        uart->regs->lcr = 0; // DLAB=0

        uart->instno = register_device(UART_NAME, uart_open, uart);

    } else
        uart->instno = register_device(UART_NAME, NULL, NULL);
}

// int uart_open(struct io ** ioptr, void * aux)
// Inputs: struct io ** ioptr - pointer to the io interface
//         void * aux - pointer to the UART device
// Outputs: int - 0 on success, -EBUSY if the UART device is already open
// Description: Opens the UART device for communication
// Side Effects: UART device is initialized and the receive and transmit buffers are reset
//               UART device is registered with the interrupt manager
//               condition variables for transmit and receive are initialized
int uart_open(struct io ** ioptr, void * aux) {
    struct uart_device * const uart = aux;

    trace("%s()", __func__);

    if (iorefcnt(&uart->io) != 0)
        return -EBUSY;
    
    // Reset receive and transmit buffers
    
    rbuf_init(&uart->rxbuf);
    rbuf_init(&uart->txbuf);

    // Read receive buffer register to flush any stale data in hardware buffer

    uart->regs->rbr; // forces a read because uart->regs is volatile

    // enable the data ready interrupt
    uart->regs->ier |= IER_DRIE;

    // registers the UART's interrupt handler
    enable_intr_source(uart->irqno, UART_INTR_PRIO, uart_isr, uart);

    // Provides a UART device reference for system access.
    *ioptr = ioaddref(&uart->io);

    // initialize the transmit condition variable
    condition_init(&uart->tx_not_full, "uart_tx");
    condition_init(&uart->rx_not_empty, "uart_rx");

    return 0;
}

// void uart_close(struct io * io)
// Inputs: struct io * io - pointer to the io interface
// Outputs: none
// Description: Closes the UART device for communication
// Side Effects: UART device is disabled and unregistered with the interrupt manager
void uart_close(struct io * io) {
    struct uart_device * const uart =
        (void*)io - offsetof(struct uart_device, io);

    trace("%s()", __func__);
    assert (iorefcnt(io) == 0);

    // Disable interrupts and unregister the device
    uart->regs->ier = 0;
    disable_intr_source(uart->irqno);
}

// long uart_read(struct io * io, void * buf, long bufsz)
// Inputs: struct io * io - pointer to the io interface
//         void * buf - pointer to the buffer to read data into
//         long bufsz - size of the buffer
// Outputs: long - number of characters read
// Description: Reads data from the UART receive ring buffer
// Side Effects: UART receive ring buffer is read into the provided buffer, can switch to other threads while waiting for data
long uart_read(struct io * io, void * buf, long bufsz) {
    int pie;
    // check if requested buffer size is valid
    if (bufsz < 1)
        return -EINVAL;
    
    struct uart_device * const uart =
        (void*)io - offsetof(struct uart_device, io);


    char * cbuf = buf;

    for (long i = 0; i < bufsz; ++i){
        // wait until the receive ring buffer is not empty
        while (rbuf_empty(&uart->rxbuf)){
            pie = disable_interrupts();
            condition_wait(&uart->rx_not_empty);
            restore_interrupts(pie);
        }

        // copy characters from the receive ring buffer into the provided buffer
        cbuf[i] = rbuf_getc(&uart->rxbuf);
        uart->regs->ier |= IER_DRIE;
    }

    return bufsz;
}

// long uart_write(struct io * io, const void * buf, long len)
// Inputs: struct io * io - pointer to the io interface
//         const void * buf - pointer to the buffer to write data from
//         long len - size of the buffer
// Outputs: long - number of characters written
// Description: Writes data to the UART transmit ring buffer
// Side Effects: UART transmit ring buffer is written into the provided buffer, can switch to other threads while waiting for buffer space
long uart_write(struct io * io, const void * buf, long len) {
    int pie;
    // check if requested buffer size is valid
    if (len < 1)
        return -EINVAL;
    
    struct uart_device * const uart =
        (void*)io - offsetof(struct uart_device, io);

    const char * cbuf = buf;

    for (long i = 0; i < len; ++i){
        // wait until the receive ring buffer is not empty
        while (rbuf_full(&uart->txbuf)){
            pie = disable_interrupts();
            condition_wait(&uart->tx_not_full);
            restore_interrupts(pie);
        }

        // copy characters from the receive ring buffer into the provided buffer
        rbuf_putc(&uart->txbuf, cbuf[i]);
        uart->regs->ier |= IER_THREIE;
    }

    return len;

}

// void uart_isr(int srcno, void * aux)
// Inputs: int srcno - interrupt source number
//         void * aux - pointer to the UART device
// Outputs: none
// Description: UART interrupt service routine
// Side Effects: UART receive and transmit buffers are updated, condition variables are broadcasted to wake up waiting threads
void uart_isr(int srcno, void * aux) {
    struct uart_device * const uart = aux;
    
    // If data is available and receive buffer isn’t full, Store data from RBR to receive buffer
    if ((uart->regs->lsr & LSR_DR) && !rbuf_full(&uart->rxbuf)){
        char c = uart->regs->rbr;
        rbuf_putc(&uart->rxbuf, c);
    }

    // If transmit buffer isn’t empty, Write from transmit buffer to THR
    if ((uart->regs->lsr & LSR_THRE) && !rbuf_empty(&uart->txbuf)){
        char c = rbuf_getc(&uart->txbuf);
        uart->regs->thr = c;
    }

    // disable the corresponding interrupt if the transmit buffer is empty or receive buffer is full
    if (rbuf_empty(&uart->txbuf))
        uart->regs->ier &= ~IER_THREIE;
    if (rbuf_full(&uart->rxbuf))
        uart->regs->ier &= ~IER_DRIE;

    // broadcast the condition variable to wake up the waiting threads
    if (!rbuf_full(&uart->txbuf))
        condition_broadcast(&uart->tx_not_full);
    if (!rbuf_empty(&uart->rxbuf))
        condition_broadcast(&uart->rx_not_empty);
}

void rbuf_init(struct ringbuf * rbuf) {
    rbuf->hpos = 0;
    rbuf->tpos = 0;
}

int rbuf_empty(const struct ringbuf * rbuf) {
    return (rbuf->hpos == rbuf->tpos);
}

int rbuf_full(const struct ringbuf * rbuf) {
    return ((uint16_t)(rbuf->tpos - rbuf->hpos) == UART_RBUFSZ);
}

void rbuf_putc(struct ringbuf * rbuf, char c) {
    uint_fast16_t tpos;

    tpos = rbuf->tpos;
    rbuf->data[tpos % UART_RBUFSZ] = c;
    asm volatile ("" ::: "memory");
    rbuf->tpos = tpos + 1;
}

char rbuf_getc(struct ringbuf * rbuf) {
    uint_fast16_t hpos;
    char c;

    hpos = rbuf->hpos;
    c = rbuf->data[hpos % UART_RBUFSZ];
    asm volatile ("" ::: "memory");
    rbuf->hpos = hpos + 1;
    return c;
}

// The functions below provide polled uart input and output for the console.

#define UART0 (*(volatile struct uart_regs*)UART0_MMIO_BASE)

void console_device_init(void) {
    UART0.ier = 0x00;

    // Configure UART0. We set the baud rate divisor to 1, the lowest value,
    // for the fastest baud rate. In a physical system, the actual baud rate
    // depends on the attached oscillator frequency. In a virtualized system,
    // it doesn't matter.
    
    UART0.lcr = LCR_DLAB;
    UART0.dll = 0x01;
    UART0.dlm = 0x00;

    // The com0_putc and com0_getc functions assume DLAB=0.

    UART0.lcr = 0;
}

void console_device_putc(char c) {
    // Spin until THR is empty
    while (!(UART0.lsr & LSR_THRE))
        continue;

    UART0.thr = c;
}

char console_device_getc(void) {
    // Spin until RBR contains a byte
    while (!(UART0.lsr & LSR_DR))
        continue;
    
    return UART0.rbr;
}
