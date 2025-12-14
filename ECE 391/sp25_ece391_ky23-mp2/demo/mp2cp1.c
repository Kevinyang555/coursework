// mp2cp1.c - demo for MP2 checkpoint 1 (run trek in a window)
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
#include "dev/uart.h"

void main(void) {
    extern void trek_start(struct io * io);
    extern char _kimg_end[];
    struct io * uartio;
    int result;
    
    console_init();
    intrmgr_init();
    devmgr_init();

    heap_init(_kimg_end, RAM_END);

    uart_attach((void*)UART0_MMIO_BASE, UART0_INTR_SRCNO+0);
    uart_attach((void*)UART1_MMIO_BASE, UART0_INTR_SRCNO+1);

    result = open_device("uart", 1, &uartio);
    assert (result == 0);

    enable_interrupts();

    trek_start(uartio);
}