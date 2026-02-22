// uart.c - NS8250-compatible uart port
// 
// Copyright (c) 2024-2025 University of Illinois
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

#include "ioimpl.h"
#include "console.h"
#include "thread.h"

#include "error.h"

#include <stdint.h>

// COMPILE-TIME CONSTANT DEFINITIONS
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

// INTERNAL TYPE DEFINITIONS
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
    unsigned int hpos; // head of queue (from where elements are removed)
    unsigned int tpos; // tail of queue (where elements are inserted)
    char data[UART_RBUFSZ];
};

struct uart_device {
    volatile struct uart_regs * regs;
    int irqno;
    int instno;

    struct io io;

    unsigned long rxovrcnt; // number of times OE was set

    struct ringbuf rxbuf;
    struct ringbuf txbuf;
    struct condition rx_non_empty;
    struct condition tx_full;
};

// INTERNAL FUNCTION DEFINITIONS
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

// EXPORTED FUNCTION DEFINITIONS
// 

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

    // Check if we're trying to attach UART0, which is used for the console. It
    // had already been initialized and should not be accessed as a normal
    // device.

 

    if (mmio_base != (void*)UART0_MMIO_BASE) {

        uart->regs->ier = 0;
        uart->regs->lcr = LCR_DLAB;
        // fence o,o ?
        uart->regs->dll = 0x01;
        uart->regs->dlm = 0x00;
        // fence o,o ?
        uart->regs->lcr = 0; // DLAB=0

        uart->instno = register_device(UART_NAME, uart_open, uart);

    } else
        uart->instno = register_device(UART_NAME, NULL, NULL);
}


//============================
//Function: uart_open
//      
//       Summary:
//              This function opens the uart device that passed in for future operations such as write and read
//
//       Arguments:
//              -pointer to the io abstraction
//              -The aux parameter that represent the uart we need to open 
//
//       Return Value:
//               return int value 0
//
//       Side Effects:
//              -Clears stale data in RBR
//              -enable data ready interrupt
//              -register the interrupt handler for this uart
//              -set the passed ioptr to the io abstraction of this uart device
//              -Initialize the two conditions struct 
//
//       Notes:
//              NONE
// ============================

int uart_open(struct io ** ioptr, void * aux) {
    struct uart_device * const uart = aux;

    trace("%s()", __func__);

    if (iorefcnt(&uart->io) != 0)
        return -EBUSY;
    
    // Reset receive and transmit buffers
    
    rbuf_init(&uart->rxbuf);
    rbuf_init(&uart->txbuf);

    // Read receive buffer register to flush any stale data in hardware buffer

    uart->regs->rbr; // forces a read because uart->regs is volatile
    uart->regs->ier |= IER_DRIE;//enable dr interrupt
    enable_intr_source(uart->irqno, UART_INTR_PRIO, uart_isr, uart);//register interrupt handler
    *ioptr = ioaddref(&uart->io);//set ioptr to this uart's io
    condition_init(&uart->rx_non_empty, "rxbuf_condtion");
    condition_init(&uart->tx_full, "txbuf_condtion" );
    // FIXME your code goes here

    return 0;
}
//============================
//Function: uart_close
//      
//       Summary:
//              This function closes this uart from further operations and disable the interrupt handler
//
//       Arguments:
//              -pointer to the io abstraction
//
//       Return Value:
//               NONE
//
//       Side Effects:
//              NONE
//
//       Notes:
//              NONE
// ============================
void uart_close(struct io * io) {
    if(io == NULL){
        return;
    }
    struct uart_device * const uart =
        (void*)io - offsetof(struct uart_device, io);

    trace("%s()", __func__);
    assert (iorefcnt(io) == 0);

    // FIXME your code goes here
    disable_intr_source(uart->irqno);//disable the interrupt handler for this uart


}
//============================
//Function: uart_read
//      
//       Summary:
//              This function read from this uart's receive buffer given the length of the content needed
//
//       Arguments:
//              -pointer to the io abstraction
//              -buffer to store data from uart's receive buffer
//              -Length of content needed
//
//       Return Value:
//               -return 0 if length of content invalid
//               -return the length of content needed
//
//       Side Effects:
//              -Enable data ready interrupt
//              -We will disable all interrupts when calling condition wait to avoid concurrency
//
//       Notes:
//              NONE
// ============================

long uart_read(struct io * io, void * buf, long bufsz) {
    // FIXME your code goes here
    trace("%s()", __func__);
    int pie;
    if(io == NULL){
        return 0;
    }
    struct uart_device * const uart =
        (void*)io - offsetof(struct uart_device, io);

    if(bufsz==0){ // if nothing needs to be read, return
        return 0;
    }
    
    
    //uart->regs->ier |= IER_DRIE; // enable interrupt
    char * new_buf = (char *)buf;
    for( int i =0; i < bufsz; i++){// keep getting required content
        pie = disable_interrupts();
        while(rbuf_empty(&uart->rxbuf)){ //wait until there is something in the receive buffer
            condition_wait(&uart->rx_non_empty);//switch to another thread to do useful work
        }
        restore_interrupts(pie);
        new_buf[i] = rbuf_getc(&uart->rxbuf);
        uart->regs->ier |= IER_DRIE; // enable DRIE interrupt
    }
    
    return bufsz;

}
//============================
//Function: uart_write
//      
//       Summary:
//              This function writte data from the provided buffer to this uart's transmit buffer 
//
//       Arguments:
//              -pointer to the io abstraction
//              -buffer that stores the data
//              -Length of the buffer provided
//
//       Return Value:
//               -return 0 if length of provided buffer invalid
//               -return the length of buffer
//
//       Side Effects:
//              -Enable THRE interrupt
//              -We will disable all interrupts when calling condition wait to avoid concurrency
//
//
//       Notes:
//              NONE
// ============================
long uart_write(struct io * io, const void * buf, long len) {
    // FIXME your code goes here
    trace("%s()", __func__);
    int pie;
    if(io==NULL){
        return 0;
    }
    struct uart_device * const uart =
        (void*)io - offsetof(struct uart_device, io);
    if(len <= 0){ // if provided buffer is empty, return
        return 0;
    }
    
  
    const char *new_buf = (const char *)buf;
    for(int i =0; i < len; i ++){//keep writing to transmit buffer as long as there are space
        pie = disable_interrupts();
        while(rbuf_full(&uart->txbuf)){ // wait until transmit buffer has space
            condition_wait(&uart->tx_full);//switch to another thread to do useful work
        }
        restore_interrupts(pie);
        rbuf_putc(&uart->txbuf, new_buf[i]);
        uart->regs->ier |= IER_THREIE;//enable THREIE interrupt

        
    }
    
    return len;

}
//============================
//Function: uart_isr
//      
//       Summary:
//              This function serves as the interrupt handler for uart triggered by hardwares
//
//       Arguments:
//              -pointer to the io abstraction
//              -The uart that got interrupted
//
//       Return Value:
//               NONE
//
//       Side Effects:
//              -Disables interrupts if recieve buffer is full and transmit buffer is empty
//              -We broadcast two different condition struct when rxbuf has new things and transmit buffer has something taken out
//
//       Notes:
//              NONE
// ============================
void uart_isr(int srcno, void * aux) {
    // FIXME your code goes here
    trace("%s()", __func__);
    struct uart_device *const uart = aux;
    if(uart->regs->lsr & LSR_DR){ // checking if data is ready
        if(!rbuf_full(&uart->rxbuf)){ // if receive buffer not full, we store data from rbr to the buffer
            rbuf_putc(&uart->rxbuf, uart->regs->rbr);
            condition_broadcast(&uart->rx_non_empty);// broadcast condition to resume uart_read 
       }else{
            uart->regs->ier &= ~IER_DRIE; //disable b/c rxbuf is full

        }
    } 
    if(uart->regs->lsr & LSR_THRE){ //checking if THRE is ready
        if(!rbuf_empty(&uart->txbuf)){ // if not empty get data from transmit buffer to THR
            uart->regs->thr = rbuf_getc(&uart->txbuf);
            condition_broadcast(&uart->tx_full);//broadcast condition to resume uart_write
        }else{
            uart->regs->ier &= ~IER_THREIE; //disable interrupt b/c txbuf is empty
       }
    }
    

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

// The functions below provide polled uart input and output for the console.

#define UART0 (*(volatile struct uart_regs*)UART0_MMIO_BASE)

void console_device_init(void) {
    UART0.ier = 0x00;

    // Configure UART0. We set the baud rate divisor to 1, the lowest value,
    // for the fastest baud rate. In a physical system, the actual baud rate
    // depends on the attached oscillator frequency. In a virtualized system,
    // it doesn't matter.
    
    UART0.lcr = LCR_DLAB;
    UART0.dll = 0x01;
    UART0.dlm = 0x00;

    // The com0_putc and com0_getc functions assume DLAB=0.

    UART0.lcr = 0;
}

void console_device_putc(char c) {
    // Spin until THR is empty
    while (!(UART0.lsr & LSR_THRE))
        continue;

    UART0.thr = c;
}

char console_device_getc(void) {
    // Spin until RBR contains a byte
    while (!(UART0.lsr & LSR_DR))
        continue;
    
    return UART0.rbr;
}
