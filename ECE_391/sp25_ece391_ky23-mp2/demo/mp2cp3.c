// mp2cp3.c - demo for MP2 checkpoint 3 (run trek and rule30 in a window)
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "conf.h"
#include "assert.h"
#include "console.h"
#include "intr.h"
#include "device.h"
#include "thread.h"
#include "heap.h"
#include "dev/virtio.h"
#include "dev/uart.h"
#include "timer.h"

static void trek_thrfn(void);
static void rule30_thrfn(void);

void main(void) {
    extern char _kimg_end[];
    int trektid, r30tid;
    int i;
    
    console_init();
    intrmgr_init();
    timer_init();
    devmgr_init();
    thrmgr_init();

    heap_init(_kimg_end, RAM_END);

    for (i = 0; i < 3; i++)
        uart_attach((void*)UART_MMIO_BASE(i), UART_INTR_SRCNO(i));

    for (i = 0; i < 8; i++)
        virtio_attach((void*)VIRTIO_MMIO_BASE(i), VIRTIO_INTR_SRCNO(i));

    enable_interrupts();

    trektid = thread_spawn("trek", trek_thrfn);
    assert (0 < trektid);

    r30tid = thread_spawn("rule30", rule30_thrfn);
    assert (0 < r30tid);
    
    thread_join(0);
}

void trek_thrfn(void) {
    extern void trek_start(struct io * io);
    struct io * termio;
    int result;

    result = open_device("uart", 1, &termio);
    assert (result == 0);
    trek_start(termio);
}

void rule30_thrfn(void) {
    extern void rule30_start(struct io * io);
    struct io * termio;
    int result;

    result = open_device("uart", 2, &termio);
    assert (result == 0);
    rule30_start(termio);
}