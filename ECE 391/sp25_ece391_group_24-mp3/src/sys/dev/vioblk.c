// vioblk.c - VirtIO serial port (console)
// 
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef VIOBLK_TRACE
#define TRACE
#endif

#ifdef VIOBLK_DEBUG
#define DEBUG
#endif

#include "virtio.h"
#include "intr.h"
#include "assert.h"
#include "heap.h"
#include "io.h"
#include "device.h"
#include "thread.h"
#include "error.h"
#include "string.h"
#include "assert.h"
#include "ioimpl.h"
#include "io.h"
#include "conf.h"

#include <limits.h>

// COMPILE-TIME PARAMETERS
//

#ifndef VIOBLK_INTR_PRIO
#define VIOBLK_INTR_PRIO 1
#endif

#ifndef VIOBLK_NAME
#define VIOBLK_NAME "vioblk"
#endif

#ifndef VIOBLK_BUFSZ
#define VIOBLK_BUFSZ 512
#endif

// INTERNAL CONSTANT DEFINITIONS
//

#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1

// VirtIO block device feature bits (number, *not* mask)

#define VIRTIO_BLK_F_SIZE_MAX       1
#define VIRTIO_BLK_F_SEG_MAX        2
#define VIRTIO_BLK_F_GEOMETRY       4
#define VIRTIO_BLK_F_RO             5
#define VIRTIO_BLK_F_BLK_SIZE       6
#define VIRTIO_BLK_F_FLUSH          9
#define VIRTIO_BLK_F_TOPOLOGY       10
#define VIRTIO_BLK_F_CONFIG_WCE     11
#define VIRTIO_BLK_F_MQ             12
#define VIRTIO_BLK_F_DISCARD        13
#define VIRTIO_BLK_F_WRITE_ZEROES   14

// INTERNAL FUNCTION DECLARATIONS
//

static int vioblk_open(struct io ** ioptr, void * aux);
static void vioblk_close(struct io * io);

static long vioblk_readat (
    struct io * io,
    unsigned long long pos,
    void * buf,
    long bufsz);

static long vioblk_writeat (
    struct io * io,
    unsigned long long pos,
    const void * buf,
    long len);

static int vioblk_cntl (
    struct io * io, int cmd, void * arg);

static void vioblk_isr(int srcno, void * aux);

// EXPORTED FUNCTION DEFINITIONS
//

struct vioblk_request_header{
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

struct vioblk_device {
    volatile struct virtio_mmio_regs * regs;
    struct io io;
    uint16_t instno;
    uint16_t irqno;

    uint32_t blksz;
    uint64_t blkcnt;

    struct {
        struct condition used_updated;
        uint16_t last_used_idx;

        union {
            struct virtq_avail avail;
            char _avail_filler[VIRTQ_AVAIL_SIZE(1)];
        };

        union {
            volatile struct virtq_used used;
            char _used_filler[VIRTQ_USED_SIZE(1)];
        };

        struct virtq_desc desc[4];
        struct vioblk_request_header virt_header;
        uint8_t status; 
    } vq;
    char * blkbuf;
    struct lock lock;
    
};

// Attaches a VirtIO block device. Declared and called directly from virtio.c.
//==============================================================================================
// void vioblk_attach(volatile struct virtio_mmio_regs * regs, int irqno)
// inputs:  volatile struct virtio_mmio_regs * regs:pointer to mmio_regs
//          int irqno:interrupt number 
// outputs: none
// description:
//     attaches and initializes a virtio block device driver
//==============================================================================================

void vioblk_attach(volatile struct virtio_mmio_regs * regs, int irqno) {

    // Negotiate features. We need:
    //  - VIRTIO_F_RING_RESET and
    //  - VIRTIO_F_INDIRECT_DESC
    // We want:
    //  - VIRTIO_BLK_F_BLK_SIZE and
    //  - VIRTIO_BLK_F_TOPOLOGY.

    // Allocate the vioblk device structure.
    struct vioblk_device * vioblk;
    static const struct iointf vioblk_iointf = {
        .close = vioblk_close,
        .readat = vioblk_readat,
        .writeat = vioblk_writeat,
        .cntl = vioblk_cntl
    };

    vioblk = (struct vioblk_device *) kmalloc(sizeof(struct vioblk_device));
    vioblk->regs = regs;
    vioblk->irqno = irqno;

    ioinit0(&vioblk->io, &vioblk_iointf);

    regs->status |= VIRTIO_STAT_DRIVER;

    virtio_featset_t enabled_features;
    virtio_featset_t wanted_features;
    virtio_featset_t needed_features;
    int result;

    virtio_featset_init(needed_features);
    virtio_featset_add(needed_features, VIRTIO_F_RING_RESET);
    virtio_featset_add(needed_features, VIRTIO_F_INDIRECT_DESC);
    virtio_featset_init(wanted_features);
    virtio_featset_add(wanted_features, VIRTIO_BLK_F_BLK_SIZE);
    virtio_featset_add(wanted_features, VIRTIO_BLK_F_TOPOLOGY);
    result = virtio_negotiate_features(regs,
        enabled_features, wanted_features, needed_features);

    if (result != 0) {
        kprintf("%p: virtio feature negotiation failed\n", regs);
        return;
    }

    uint32_t blksz;
    // If the device provides a block size, use it. Otherwise, use 512.
    if (virtio_featset_test(enabled_features, VIRTIO_BLK_F_BLK_SIZE))
        blksz = regs->config.blk.blk_size;
    else
        blksz = 512;

    // blksz must be a power of two
    assert (((blksz - 1) & blksz) == 0);

    vioblk->blksz = blksz;
    vioblk->blkbuf = (char *) kmalloc(vioblk->blksz);
    vioblk->blkcnt = regs->config.blk.capacity;
    vioblk->vq.last_used_idx = 0;

    regs->queue_sel = 0;
    __sync_synchronize(); // fence o,o

    virtio_attach_virtq(regs, 0, 1,(uint64_t) &vioblk->vq.desc,(uint64_t) &vioblk->vq.used,(uint64_t) &vioblk->vq.avail);

    // descriptor table
    vioblk->vq.desc[0].addr=(uint64_t)&vioblk->vq.desc[1];
    vioblk->vq.desc[0].len=3 * sizeof(vioblk->vq.desc);
    vioblk->vq.desc[0].flags=VIRTQ_DESC_F_INDIRECT;
    
    vioblk->vq.desc[1].addr=(uint64_t)&vioblk->vq.virt_header;
    vioblk->vq.desc[1].len=sizeof(vioblk->vq.virt_header);
    vioblk->vq.desc[1].flags=VIRTQ_DESC_F_NEXT;
    vioblk->vq.desc[1].next=1;
    
    vioblk->vq.desc[2].addr=(uint64_t)vioblk->blkbuf;
    vioblk->vq.desc[2].len=blksz;
    vioblk->vq.desc[2].flags=VIRTQ_DESC_F_NEXT;
    vioblk->vq.desc[2].next=2;
    
    vioblk->vq.desc[3].addr=(uint64_t)&vioblk->vq.status;
    vioblk->vq.desc[3].len=sizeof(vioblk->vq.status);
    vioblk->vq.desc[3].flags=VIRTQ_DESC_F_WRITE;
    vioblk->vq.desc[3].next=0;

    vioblk->instno = register_device(VIOBLK_NAME, vioblk_open, vioblk);

    regs->status |= VIRTIO_STAT_DRIVER_OK;
}   
//==============================================================================================
// int vioblk_open(struct io ** ioptr, void * aux)
// inputs:  struct io ** ioptr:output pointer to store io
//          void * aux:pointer to the vioblk device
// outputs: int 0:success
// description:
//     prepares a virtio block device for use
//==============================================================================================

int vioblk_open(struct io ** ioptr, void * aux){
    struct vioblk_device * vioblk = (struct vioblk_device *) aux;
    
    vioblk->vq.avail.idx = 0;
    vioblk->vq.avail.flags = 0;
    vioblk->vq.avail.ring[0] = 0;

    vioblk->vq.used.idx = 0;
    vioblk->vq.used.flags = 0;
    vioblk->vq.used.ring[0].id = 0;
    vioblk->vq.used.ring[0].len = 0;

    virtio_enable_virtq(vioblk->regs, 0);

    enable_intr_source(vioblk->irqno, VIOBLK_INTR_PRIO, vioblk_isr, vioblk);

    *ioptr = ioaddref(&vioblk->io);
    lock_init(&vioblk->lock);;

    condition_init(&vioblk->vq.used_updated, "vioblk_used");
    return 0;
}
//==============================================================================================
// void vioblk_close(struct io *io)
// inputs:  struct io * io :pointer to io
// outputs: none
// description:
//     closes the VirtIO block device
//==============================================================================================

void vioblk_close(struct io *io){
    struct vioblk_device * const blk =
    (void*)io - offsetof(struct vioblk_device, io);
    virtio_reset_virtq(blk->regs, 0);
    disable_intr_source(blk->irqno);
    kfree(blk->blkbuf);
}

//==============================================================================================
// long vioblk_readat(struct io * io, unsigned long long pos, void * buf, long bufsz)
// inputs:  struct io * io: pointer to the device I/O interface
//          unsigned long long pos: byte offset
//          void * buf: pointer to buffer
//          long bufsz:number of bytes to read 
// outputs: long :number of bytes read
// description:
//     Reads blocks from the device starting at the pos.
//==============================================================================================

long vioblk_readat(struct io*io, unsigned long long pos, void * buf,long bufsz){
    if(io == NULL){
        return 0;
    }
    struct vioblk_device * const blk = (void*)io - offsetof(struct vioblk_device, io);
    if( bufsz % blk->blksz != 0 || bufsz <= 0){
        return 0;
    }
    lock_acquire(&blk->lock);
    unsigned int blkno;
   // unsigned int blkoff;
    int pie;
    char * new_buf = (char *)buf;
    blkno = pos / blk->blksz;
   // blkoff = pos % blk->blksz;
    unsigned int blocks_need = bufsz / blk->blksz;

    for(int i =0; i < blocks_need; i++){
        blk->vq.virt_header.type = VIRTIO_BLK_T_IN; //read only
        if(blkno + i > blk->blkcnt){
            return blk->blksz * i;
        }
        blk->vq.status = 0xFF;
        blk->vq.virt_header.sector = blkno+i;
        blk->vq.desc[2].flags |= VIRTQ_DESC_F_WRITE;
        blk->vq.avail.idx +=1;
        virtio_notify_avail(blk->regs, 0);

        pie = disable_interrupts();
        while(blk->vq.last_used_idx != blk->vq.avail.idx){
            condition_wait(&(blk->vq.used_updated));
        }
        restore_interrupts(pie);
        memcpy((new_buf + (i * blk->blksz)), blk->blkbuf, blk->blksz);

    }
    lock_release(&blk->lock);
    return bufsz;

}
//==============================================================================================
// int vioblk_cntl(struct io * io, int cmd, void * arg)
// inputs:  struct io * io:pointer to io
//          int cmd:control command 
//          void * arg :pointer to output or input buffer
// outputs: int 0:success
//              EINVAL: if input is invalid
//             ENOTSUP: if the command is not supported
// description:
//     handles control operations on the block device
//==============================================================================================

int vioblk_cntl(struct io *io, int cmd, void *arg) {
    struct vioblk_device * const dev = (void*)io - 
        offsetof(struct vioblk_device, io);

    if (!dev || !arg) return -EINVAL;

    switch (cmd) {
    case IOCTL_GETEND:
		    *(uint64_t*)arg = dev->blkcnt * dev->blksz;
        return 0;
    case IOCTL_GETBLKSZ:
        return dev->blksz;
    default:
        return -ENOTSUP;
    }
}
//==============================================================================================
// void vioblk_isr(int srcno, void * aux)
// Inputs:  int srcno :interrupt source number
//          void * aux :pointer to the vioblk_device
// outputs: none
// description:
//     Interrupt service routine (ISR) for the VirtIO block device.
//==============================================================================================

void vioblk_isr(int srcno, void *aux){
    struct vioblk_device * dev = aux;
    uint32_t interrupt_status;
    
    interrupt_status = dev->regs->interrupt_status;
    if (interrupt_status == 0) {
        return;
    }
    
    dev->regs->interrupt_ack = interrupt_status;
    
    __sync_synchronize();
    
    if(dev->vq.last_used_idx != dev->vq.used.idx){
        dev->vq.last_used_idx += 1;
        condition_broadcast(&dev->vq.used_updated);
    }
    
    __sync_synchronize();
    
}

long vioblk_writeat(struct io *io, unsigned long long pos, const void *buf, long len){
    if(io == NULL){
        return 0;
    }
    struct vioblk_device * const blk = (void*)io - offsetof(struct vioblk_device, io);
    if( len % blk->blksz != 0 || len <= 0){
        return 0;
    }
    lock_acquire(&blk->lock);
    unsigned int blkno;
   // unsigned int blkoff;
    int pie;
    char * new_buf = (char *)buf;
    blkno = pos / blk->blksz;
   // blkoff = pos % blk->blksz;
    unsigned int blocks_need = len / blk->blksz;

    for(int i =0; i < blocks_need; i++){
        blk->vq.virt_header.type = VIRTIO_BLK_T_OUT; // write only
        if(blkno + i > blk->blkcnt){
            return blk->blksz * i;
        }
        blk->vq.virt_header.sector = blkno+i;
        blk->vq.desc[2].flags &= ~VIRTQ_DESC_F_WRITE;
        blk->vq.avail.idx +=1;
        memcpy(blk->blkbuf, (new_buf + (i * blk->blksz)), blk->blksz);
        virtio_notify_avail(blk->regs, 0);
        pie = disable_interrupts();
        while(blk->vq.avail.idx != blk->vq.last_used_idx){
            condition_wait(&(blk->vq.used_updated));
        }
        restore_interrupts(pie);
    }
    lock_release(&blk->lock);
    return len;
}