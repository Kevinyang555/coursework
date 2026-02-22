// viorng.c - VirtIO rng device
// 
// Copyright (c) 2024-2025 University of Illinois
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

// INTERNAL CONSTANT DEFINITIONS
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

// INTERNAL TYPE DEFINITIONS
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

        // The first descriptor is a regular descriptor and is the one used in
        // the avail and used rings.

        struct virtq_desc desc[1];
    } vq;

    // bufcnt is the number of bytes left in buffer. The usable bytes are
    // between buf+0 and buf+bufcnt. (We read from the end of the buffer.)

    struct condition virqueue_not_ready;
    unsigned int bufcnt;
    char buf[VIORNG_BUFSZ];
};

// INTERNAL FUNCTION DECLARATIONS
//

static int viorng_open(struct io ** ioptr, void * aux);
static void viorng_close(struct io * io);
static long viorng_read(struct io * io, void * buf, long bufsz);
static void viorng_isr(int irqno, void * aux);

// EXPORTED FUNCTION DEFINITIONS
//

// Attaches a VirtIO rng device. Declared and called directly from virtio.c.

void viorng_attach(volatile struct virtio_mmio_regs * regs, int irqno) {
    //           FIXME add additional declarations here if needed
    static const struct iointf viorng_iointf = { //intialize the IO operations
        .close = &viorng_close,
        .read = &viorng_read
    };

    struct viorng_device *rng = kcalloc(1, sizeof(struct viorng_device));
    virtio_featset_t enabled_features, wanted_features, needed_features;
    int result;

    assert (regs->device_id == VIRTIO_ID_RNG);

    // Signal device that we found a driver
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

    //           FIXME Finish viorng initialization here! 

    //initialize the descriptor table and corresponding rng device members
    rng->vq.desc[0].addr = (uint64_t)&rng->buf;
    rng->vq.desc[0].len = VIORNG_BUFSZ;
    rng->vq.desc[0].flags = VIRTQ_DESC_F_WRITE;
    rng->vq.desc[0].next = 0;
    rng->irqno = irqno;
    rng->regs = regs;

    //initalize io refcount and attach the avail and used queues
    ioinit0(&rng->io, &viorng_iointf);
    virtio_attach_virtq(regs,0,1,(uint64_t)&rng->vq.desc, (uint64_t)&rng->vq.used, (uint64_t)& rng->vq.avail);
    
    rng->instno = register_device(VIORNG_NAME, viorng_open, rng);


    // fence o,oi
    regs->status |= VIRTIO_STAT_DRIVER_OK;    
    //           fence o,oi
    __sync_synchronize();
}    

//============================
//Function: viorng_open
//      
//       Summary:
//              This function opens the rng device that passed in for future operations such as read
//
//       Arguments:
//              -pointer to the io abstraction
//              -The aux parameter that represent the rng device we need to open 
//
//       Return Value:
//               return int value 0
//
//       Side Effects:
//              -register the interrupt handler for this rng
//              -makes avail and used queues available
//              -set the passed ioptr to the io abstraction of this rng device
//              -initalize condition struct 
//
//       Notes:
//              -if passed in rng iorefcount is not zero, we exit since it is opened already
// ============================
int viorng_open(struct io ** ioptr, void * aux) {
    //           FIXME your code here
    struct viorng_device * const rng = aux;
    if(iorefcnt(&rng->io)!=0){ // check if it is already opened
        return -EBUSY;
    }
    //enable avail and used queues and interrupt handler
    virtio_enable_virtq(rng->regs, 0); 
    enable_intr_source(rng->irqno, VIORNG_IRQ_PRIO, viorng_isr, rng);
    *ioptr = ioaddref(&rng->io);
    condition_init(&rng->virqueue_not_ready, "virqueue_condition");//initalize condition struct
    
    
    return 0;
}

//============================
//Function: viorng_close
//      
//       Summary:
//              This function closes the rng device that passed in
//
//       Arguments:
//              -pointer to the io abstraction
//
//       Return Value:
//               NONE
//
//       Side Effects:
//              -reset virtqueues
//              -disable interrupt for this device
//          
//       Notes:
//              NONE
// ============================
void viorng_close(struct io * io) {
    //           FIXME your code here
    struct viorng_device * const rng =
    (void*)io - offsetof(struct viorng_device, io);
    virtio_reset_virtq(rng->regs, 0);
    disable_intr_source(rng->irqno);
    

    
}

//============================
//Function: viorng_read
//      
//       Summary:
//              This function reads the random numbers from the device to the given buffer given the number of bytes needed
//
//       Arguments:
//              -pointer to the io abstraction
//              -buffer where we want to store information
//              -the number of bytes needed to read
//
//       Return Value:
//               return how many bytes read
//
//       Side Effects:
//              -We condition wait when we are waiting the isr to fill the device buffer with contents again
//              -We will notify the driver to request contents from rng when device buffer is empty and we still need more data
//              -We will disable interrupts and restore after condition wait
//
//       Notes:
//              -if the passed bufsz is invalid, we return 0 bytes read and exit
// ============================
long viorng_read(struct io * io, void * buf, long bufsz) {
    //           FIXME your code here
    int pie;
    struct viorng_device * const rng =
    (void*)io - offsetof(struct viorng_device, io);
    if(bufsz <=0){ // check if bufsz is invalid
        return 0;
    }

    char * new_buf = (char *)buf;
    int i;
    for( i =0; i < bufsz; i++){// keep getting required content
        if(rng->bufcnt <=0){ // if we used up all rng buffer contents
            rng->vq.avail.idx +=1;// set up avail idx for new requests
            virtio_notify_avail(rng->regs, 0);// request for more entropy
            
            pie = disable_interrupts();
            while(rng->vq.avail.idx != rng->vq.last_used_idx){ // we yield to other thread to do useful work while we wait
                
                condition_wait(&rng->virqueue_not_ready);
                
            }
            restore_interrupts(pie);

        }
        rng->bufcnt--;// decrement bufcnt since we read one byte
        new_buf[i] = rng->buf[rng->bufcnt];
        
    }
    
    return bufsz;

}

//============================
//Function: viorng_isr
//      
//       Summary:
//              This function fills out device buffer by setting bufcnt to VIORNG_BUFSZ
//
//       Arguments:
//              -Interrupt source #
//              -The aux parameter that represent the rng device we need to open 
//
//       Return Value:
//               NONE
//
//       Side Effects:
//              -We set the acknowlegde interrupt bit to signal we are handlering this interrupt
//              -We update last_used_idx once the job is finished
//              -We broadcast to resume viorng_read 
//
//       Notes:
//              -we only update bufcnt when used.idx is different from last_used_idx b/c this means the device handled this interrupt
// ============================
void viorng_isr(int irqno, void * aux) {
    //           FIXME your code here
    struct viorng_device * const rng = aux;
    rng->regs->interrupt_ack = rng->regs->interrupt_status;//Acknowledge interrupt
    if(rng->vq.last_used_idx != rng->vq.used.idx){//check if device is finished
        rng->bufcnt = VIORNG_BUFSZ;
        rng->vq.last_used_idx +=1;
        condition_broadcast(&rng->virqueue_not_ready);//we resume thread viorng_read
    }
 
}