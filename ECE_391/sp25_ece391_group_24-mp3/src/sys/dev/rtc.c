// rtc.c - Goldfish RTC driver
// 
// Copyright (c) 2024-2025 University of Illinois
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

// INTERNAL TYPE DEFINITIONS
// 

struct rtc_regs {
    uint32_t low;
    uint32_t high;
};

struct rtc_device {
    volatile struct rtc_regs * regs;
    struct io io;
    int instno;
};

// INTERNAL FUNCTION DEFINITIONS
//

static int rtc_open(struct io ** ioptr, void * aux);
static void rtc_close(struct io * io);
static int rtc_cntl(struct io * io, int cmd, void * arg);
static long rtc_read(struct io * io, void * buf, long bufsz);

static uint64_t read_real_time(struct rtc_regs * regs);

// EXPORTED FUNCTION DEFINITIONS
// 

// void rtc_attach(void * mmio_base)
// Inputs: void * mmio_base - base address of the RTC memory-mapped I/O registers
// Outputs: none
// Description: This function initializes the RTC device and registers it with the device manager.
// Side Effects: initializes the RTC device and registers it with the device manager
void rtc_attach(void * mmio_base) {
    static const struct iointf intf={
        .close= &rtc_close,
        .read = &rtc_read,
        .cntl = &rtc_cntl
    };
    struct rtc_device *rtc;
    rtc = kcalloc(1, sizeof(struct rtc_device));
    rtc->regs = mmio_base;
    ioinit0(&rtc->io, &intf);

    rtc->instno = register_device("rtc", rtc_open, rtc);
}

// int rtc_open(struct io ** ioptr, void * aux)
// Inputs: struct io ** ioptr - pointer to the io object pointer
//         aux - pointer to the rtc device
// Outputs: 0 on success, error code on failure
// Description: This function opens the RTC device.
// Side Effects: increments the reference count of the io object
int rtc_open(struct io ** ioptr, void * aux) {
    struct rtc_device * const rtc = aux;
    *ioptr = ioaddref(&rtc->io);
    return 0;
}

// void rtc_close(struct io * io)
// Inputs: struct io * io - pointer to the io object
// Outputs: none
// Description: This function closes the RTC device.
// Side Effects: none
void rtc_close(struct io * io) {
    trace("%s()", __func__);
    assert(iorefcnt(io) == 0);
}

// int rtc_cntl(struct io * io, int cmd, void * arg)
// Inputs: struct io * io - pointer to the io object
//         int cmd - command code to execute
//         void * arg - pointer to the argument
// Outputs: 0 on success, error code on failure
// Description: This function handle the input command on the RTC device.
// Side Effects: none
int rtc_cntl(struct io * io, int cmd, void * arg) {
    if (cmd == IOCTL_GETBLKSZ){
        return 8;
    }

    return -ENOTSUP;
}

// long rtc_read(struct io * io, void * buf, long bufsz)
// Inputs: struct io * io - pointer to the io object
//         viod * buf - pointer to the buffer
//         long bufsz - size of the buffer
// Outputs: long - number of bytes read
// Description: This function reads the current time from the RTC device.
// Side Effects: time_now is copied to the buffer
long rtc_read(struct io * io, void * buf, long bufsz) {
    struct rtc_device *const rtc=
        (void*) io - offsetof(struct rtc_device, io);
    if(bufsz == 0){
        return 0;
    }
    if(bufsz < sizeof(uint64_t)){
        return -EINVAL;
    }

    uint64_t temp;
    struct rtc_regs *regs = (struct rtc_regs *)rtc->regs;
    temp = read_real_time(regs);
    memcpy(buf, &temp, sizeof(uint64_t));
    return sizeof(uint64_t);
}

// uint64_t read_real_time(struct rtc_regs * regs)
// Inputs: struct rtc_regs * regs - pointer to the rtc registers
// Outputs: uint64_t - the current time
// Description: This function reads the current time from the RTC registers.
// Side Effects: none
uint64_t read_real_time(struct rtc_regs * regs) {
    uint32_t lo;
    uint32_t high;

    lo = regs->low;
    high = regs->high;
    return ((uint64_t) high << 32) | lo;
}