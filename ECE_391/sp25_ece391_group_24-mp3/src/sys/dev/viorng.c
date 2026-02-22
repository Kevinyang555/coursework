// viorng.c - VirtIO rng device
// 
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "virtio.h"
#include "intr.h"
#include "heap.h"
#include "io.h"
#include "device.h"
#include "error.h"
#include "string.h"
#include "ioimpl.h"
#include "assert.h"
#include "conf.h"
#include "intr.h"
#include "console.h"
#include "thread.h"

// INTERNAL CONSTANT DEFINITIONS
//

#ifndef VIORNG_BUFSZ
#define VIORNG_BUFSZ 256
#endif

#ifndef VIORNG_NAME
#define VIORNG_NAME "rng"
#endif

#ifndef VIORNG_IRQ_PRIO
#define VIORNG_IRQ_PRIO 1
#endif

// INTERNAL TYPE DEFINITIONS
//

struct viorng_device {
    volatile struct virtio_mmio_regs * regs;
    int irqno;
    int instno;

    struct io io;

    struct {
        uint16_t last_used_idx;

        union {
            struct virtq_avail avail;
            char _avail_filler[VIRTQ_AVAIL_SIZE(1)];
        };

        union {
            volatile struct virtq_used used;
            char _used_filler[VIRTQ_USED_SIZE(1)];
        };

        // The first descriptor is a regular descriptor and is the one used in
        // the avail and used rings.

        struct virtq_desc desc[1];
    } vq;

    // bufcnt is the number of bytes left in buffer. The usable bytes are
    // between buf+0 and buf+bufcnt. (We read from the end of the buffer.)

    unsigned int bufcnt;
    char buf[VIORNG_BUFSZ];
    struct condition buf_not_empty;
};

// INTERNAL FUNCTION DECLARATIONS
//

static int viorng_open(struct io ** ioptr, void * aux);
static void viorng_close(struct io * io);
static long viorng_read(struct io * io, void * buf, long bufsz);
static void viorng_isr(int irqno, void * aux);

// EXPORTED FUNCTION DEFINITIONS
//

// void viorng_attach(volatile struct virtio_mmio_regs * regs, int irqno)
// Inputs: volatile struct virtio_mmio_regs * regs - pointer to the VirtIO MMIO registers
//         int irqno - interrupt number
// Outputs: none
// Description: Attaches the VirtIO rng device to the system
// Side Effects: memory is allocated for the device, device is registered, and the device is attached to the system
void viorng_attach(volatile struct virtio_mmio_regs * regs, int irqno) {
    //           FIXME add additional declarations here if needed
    struct viorng_device * viorng;
    static const struct iointf viorng_iointf = {
        .close = viorng_close,
        .read = viorng_read,
        .write = NULL
    };

    virtio_featset_t enabled_features, wanted_features, needed_features;
    int result;
    
    assert (regs->device_id == VIRTIO_ID_RNG);

    viorng = kcalloc(1, sizeof(struct viorng_device));
    viorng->regs = regs;
    viorng->irqno = irqno;

    ioinit0(&viorng->io, &viorng_iointf);

    // Signal device that we found a driver
    regs->status |= VIRTIO_STAT_DRIVER;

    // fence o,io
    __sync_synchronize();

    virtio_featset_init(needed_features);
    virtio_featset_init(wanted_features);
    result = virtio_negotiate_features(regs,
        enabled_features, wanted_features, needed_features);

    if (result != 0) {
        kprintf("%p: virtio feature negotiation failed\n", regs);
        return;
    }

    // set last used index
    viorng->vq.last_used_idx = 0;

    // fill out descriptor table
    viorng->vq.desc[0].addr = (uint64_t)viorng->buf;
    viorng->vq.desc[0].len = VIORNG_BUFSZ;
    viorng->vq.desc[0].flags = VIRTQ_DESC_F_WRITE;
    viorng->vq.desc[0].next = 0;

    // attach avail and used rings
    virtio_attach_virtq(regs, 0, 1, (uint64_t)&viorng->vq.desc[0],
        (uint64_t)&viorng->vq.used, (uint64_t)&viorng->vq.avail);

    // register device
    viorng->instno = register_device(VIORNG_NAME, viorng_open, viorng);

    // fence o,oi
    regs->status |= VIRTIO_STAT_DRIVER_OK;    
    //           fence o,oi
    __sync_synchronize();
}

// int viorng_open(struct io ** ioptr, void * aux)
// Inputs: struct io ** ioptr - pointer to the io structure
//         void * aux - pointer to the device
// Outputs: int - 0 if the device is successfully opened, -EBUSY if the device is already open
// Description: Opens the VirtIO rng device
// Side Effects: isr is registered and the device is enabled
int viorng_open(struct io ** ioptr, void * aux) {
    struct viorng_device * viorng = aux;

    trace("%s()", __func__);

    if (iorefcnt(&viorng->io) != 0)
        return -EBUSY;

    // makes the virtq avail and virtq used queues available for use
    viorng->vq.avail.flags = 0;
    viorng->vq.avail.idx = 0;
    viorng->vq.avail.ring[0] = 0;

    viorng->vq.used.flags = 0;
    viorng->vq.used.idx = 0;
    viorng->vq.used.ring[0].id = 0;
    viorng->vq.used.ring[0].len = 0;

    virtio_enable_virtq(viorng->regs, 0);
    
    
    // enable interrupt
    enable_intr_source(viorng->irqno, VIORNG_IRQ_PRIO, viorng_isr, viorng);

    // IO operations returned
    *ioptr = ioaddref(&viorng->io);

    // initialize the condition
    condition_init(&viorng->buf_not_empty, "viorng_buf");

    return 0;
}

// void viorng_close(struct io * io)
// Inputs: struct io * io - pointer to the io structure
// Outputs: none
// Description: Closes the VirtIO rng device
// Side Effects: interrupts are disabled and the device is unregistered
void viorng_close(struct io * io) {
    struct viorng_device * viorng = (void*)io - offsetof(struct viorng_device, io);

    trace("%s()", __func__);
    assert (iorefcnt(io) == 0);

    // disable interrupt and unregister device
    disable_intr_source(viorng->irqno);
    virtio_reset_virtq(viorng->regs, 0);
}

// long viorng_read(struct io * io, void * buf, long bufsz)
// Inputs: struct io * io - pointer to the io structure
//         void * buf - pointer to the buffer
//         long bufsz - size of the buffer
// Outputs: long - number of bytes read
// Description: Reads data from the VirtIO rng device, will switch to the next thread while waiting for data
// Side Effects: if buffer is empty, data is read from the device and copied to the buffer
long viorng_read(struct io * io, void * buf, long bufsz) {
    int pie;
    if (bufsz < 1)
        return -EINVAL;

    struct viorng_device * viorng = (void*)io - offsetof(struct viorng_device, io);

    char * cbuf = buf;

    // read data from the device
    for (long i = 0; i < bufsz; ++i) {
        // if the buffer is empty, request data from the device
        if (viorng->bufcnt == 0) {
            viorng->vq.avail.idx++;
            virtio_notify_avail(viorng->regs, 0);
        }
        // wait until the buffer is not empty, then copy the data to the buffer
        while (viorng->bufcnt == 0) {
            pie = disable_interrupts();
            condition_wait(&viorng->buf_not_empty);
            restore_interrupts(pie);
        }

        viorng->bufcnt--;
        cbuf[i] = viorng->buf[viorng->bufcnt];
    }

    /*
    // request entropy from the device
    if (viorng->bufcnt == 0) {
        viorng->vq.avail.idx++;
        virtio_notify_avail(viorng->regs, 0);
    }

    // wait until the buffer is not empty, then copy the data to the buffer
    while (viorng->bufcnt == 0){
        pie = disable_interrupts();
        condition_wait(&viorng->buf_not_empty);
        restore_interrupts(pie);
    }

    char * cbuf = buf;
    long n = 0;
    while (n < bufsz && viorng->bufcnt > 0) {
        viorng->bufcnt--;
        cbuf[n++] = viorng->buf[viorng->bufcnt];
    }
    */

    return bufsz;
}

// void viorng_isr(int irqno, void * aux)
// Inputs: int irqno - interrupt number
//         void * aux - pointer to the device
// Outputs: none
// Description: VirtIO rng device interrupt service routine
// Side Effects: update the buffer count and signal the condition variable
//               acknowledge the interrupt
void viorng_isr(int irqno, void * aux) {
    struct viorng_device * viorng = aux;

    uint32_t status = viorng->regs->interrupt_status;

    // check bit 0 for device buffer usage, if yes, check if the device has serviced a request
    if (status & (1 << 0)) {
        // Check if the device has serviced a request
        if (viorng->vq.last_used_idx != viorng->vq.used.idx) {
            uint16_t idx = viorng->vq.used.ring[viorng->vq.last_used_idx % 1].id;

            // Check if the descriptor is the one we submitted
            if (idx == 0) {
                // Update the buffer count
                viorng->bufcnt = viorng->vq.used.ring[viorng->vq.last_used_idx % 1].len;

                // Update the last used index
                viorng->vq.last_used_idx++;

                // Signal the condition variable
                condition_broadcast(&viorng->buf_not_empty);
            }
        }
    }

    // acknowledge the interrupt
    viorng->regs->interrupt_ack = status & (1 << 0);

}