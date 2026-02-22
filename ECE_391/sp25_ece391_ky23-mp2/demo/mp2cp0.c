// mp2cp0.c - demo for MP2 checkpoint 0 (rtc driver)
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
#include "dev/rtc.h"

void main(void) {
    extern char _kimg_end[];
    uint64_t time_now;
    struct io * rtcio;
    int result;
    
    console_init();
    intrmgr_init();
    devmgr_init();

    heap_init(_kimg_end, RAM_END);

    rtc_attach((void*)RTC_MMIO_BASE);

    result = open_device("rtc", 0, &rtcio);
    assert (result == 0);

    result = ioread(rtcio, &time_now, sizeof(time_now));
    assert (result == sizeof(time_now));
    ioclose(rtcio);

    kprintf("UNIX time: %llu\n", time_now / 1000 / 1000 / 1000);

}