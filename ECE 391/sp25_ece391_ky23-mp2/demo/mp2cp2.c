// mp2cp2.c - demo for MP2 checkpoint 2 (run trek in a window)
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "conf.h"
#include "assert.h"
#include "console.h"
#include "intr.h"
#include "device.h"
#include "heap.h"
#include "dev/virtio.h"
#include "dev/uart.h"

void main(void) {
    extern void trek_start(struct io * io);
    extern char _kimg_end[];
    unsigned char rand[8];
    struct io * rngio;
    struct io * uartio;
    int result;
    int i;
    
    console_init();
    intrmgr_init();
    devmgr_init();

    heap_init(_kimg_end, RAM_END);

    uart_attach((void*)UART0_MMIO_BASE, UART0_INTR_SRCNO+0);
    uart_attach((void*)UART1_MMIO_BASE, UART0_INTR_SRCNO+1);


    for (i = 0; i < 8; i++)
        virtio_attach((void*)VIRTIO_MMIO_BASE(i), VIRTIO_INTR_SRCNO(i));

    enable_interrupts();

    // Read random numbers from RNG and print them as lottery numbers

    result = open_device("rng", 0, &rngio);
    assert (result == 0);

    result = iofill(rngio, rand, sizeof(rand));
    assert (result == sizeof(rand));
    ioclose(rngio);

    kprintf("Your lucky numbers are:");

    for (i = 0; i < 8; i++)
        kprintf(" %d", rand[i] % 69 + 1);
    kputc('\n');

    // Run trek on UART 1

    result = open_device("uart", 1, &uartio);
    assert (result == 0);
    
    trek_start(uartio);
}