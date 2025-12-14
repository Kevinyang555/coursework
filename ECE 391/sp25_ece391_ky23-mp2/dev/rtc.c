// rtc.c - Goldfish RTC driver
// 
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef RTC_TRACE
#define TRACE
#endif

#ifdef RTC_DEBUG
#define DEBUG
#endif

#include "conf.h"
#include "assert.h"
#include "rtc.h"
#include "device.h"
#include "ioimpl.h"
#include "console.h"
#include "string.h"
#include "heap.h"

#include "error.h"

#include <stdint.h>

// INTERNAL TYPE DEFINITIONS
// 

struct rtc_regs {
    // TODO
    uint32_t low;
    uint32_t high;
};

struct rtc_device {
    // TODO
    volatile struct rtc_regs * regs;
    struct io io;
    int instno;
};

// INTERNAL FUNCTION DEFINITIONS
//

static int rtc_open(struct io ** ioptr, void * aux);
static void rtc_close(struct io * io);
static int rtc_cntl(struct io * io, int cmd, void * arg);
static long rtc_read(struct io * io, void * buf, long bufsz);

static uint64_t read_real_time(volatile struct rtc_regs * regs);

// EXPORTED FUNCTION DEFINITIONS
// 

void rtc_attach(void * mmio_base) {
    // TODO
    static const struct iointf intf={
        .close= &rtc_close,
        .read =  &rtc_read,
        .cntl = &rtc_cntl
    };
    struct rtc_device *rtc;
    rtc = kcalloc(1, sizeof(struct rtc_device));
    rtc->regs = mmio_base;
    ioinit0(&rtc->io, &intf);

    rtc->instno = register_device("rtc", rtc_open, rtc);
}

int rtc_open(struct io ** ioptr, void * aux) {
    // TODO
    struct rtc_device * const rtc = aux;
    *ioptr = ioaddref(&rtc->io);
    return 0;
}

void rtc_close(struct io * io) {
    // TODO
   // struct rtc_device * const rtc =
     //   (void*)io - offsetof(struct rtc_device, io);

    assert(iorefcnt(io) == 0);
    //kfree(rtc);
}

int rtc_cntl(struct io * io, int cmd, void * arg) {
    // TODO
    if (cmd == IOCTL_GETBLKSZ){
        return 8;
    }
    return -ENOTSUP;
}

long rtc_read(struct io * io, void * buf, long bufsz) {
    // TODO
    struct rtc_device *const rtc=
        (void*) io - offsetof(struct rtc_device, io);
    if(bufsz == 0){
        return 0;
    }
    if(bufsz < sizeof(uint64_t)){
        return -EINVAL;
    }
    uint64_t temp;
    temp = read_real_time(rtc->regs);
    memcpy(buf, &temp, sizeof(uint64_t));
    return sizeof(uint64_t);
}

uint64_t read_real_time(volatile struct rtc_regs * regs) {
    // TODO
    uint32_t lo;
    uint32_t high;

    lo = regs->low;
    high = regs->high;
    return ((uint64_t) high << 32) | lo;
}