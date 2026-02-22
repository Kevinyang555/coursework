// intr.h - External interrupt handling
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _INTR_H_
#define _INTR_H_

#include "riscv.h"
#include "plic.h"

// EXPORTED CONSTANT DEFINITIONS
//

#define INTR_PRIO_MIN PLIC_PRIO_MIN
#define INTR_PRIO_MAX PLIC_PRIO_MAX
#define INTR_SRC_CNT PLIC_SRC_CNT

// EXPORTED FUNCTION DECLARATIONS
// 

extern void intrmgr_init(void);
extern char intrmgr_initialized;

// The enable_intr_source() function enables an interrupt source at the
// interrupt controller (e.g. PLIC). The _srcno_ argument gives the source
// number (as known at the interrupt controller). The _prio_ argument gives the
// priority to be assigned to the source. The interrupt manager will call the
// _isr_ function when the source raises an interrupt, passing the source number
// and the _isr_aux_ argument to _isr_.

extern void enable_intr_source (
    int srcno, int prio,
    void (*isr)(int srcno, void * aux),
    void * isr_aux);

// The disable_intr_source() function disabled an interrupt source at the
// interrupt controller (e.g. PLIC). The _srcno_ argument gives the source
// number (as known at the interrupt controller).

extern void disable_intr_source(int srcno);

extern void handle_smode_interrupt(unsigned int cause); // called from trap.s

// The enable_interrupts() function enables interrupts globally. It returns an
// opaque value that can be passed to restore_interrupts() to restore the
// previous interrupt enable/disable state.

static inline long enable_interrupts(void) {
    return csrrsi_sstatus_SIE();
}

// The enable_interrupts() function enables interrupts globally. It returns an
// opaque value that can be passed to restore_interrupts() to restore the
// previous interrupt enable/disable state.

static inline long disable_interrupts(void) {
    return csrrci_sstatus_SIE();
}

// The restore_interrupts() function restores the previously saved interrupt
// enable/disable state. It should be passed a value returned by
// enable_interrupts() or disable_interrupts().

static inline void restore_interrupts(int prev_state) {
    csrwi_sstatus_SIE(prev_state);
}

// The interrupts_enabled() function returns 1 if interrupts are currently
// enabled, and 0 if they are currently disabled.

static inline int interrupts_enabled(void) {
    return ((csrr_sstatus() & RISCV_SSTATUS_SIE) != 0);
}

// The interrupts_disabled() function returns 1 if interrupts are currently
// disabled, and 0 if they are currently enabled.

static inline int interrupts_disabled(void) {
    return ((csrr_sstatus() & RISCV_SSTATUS_SIE) == 0);
}

extern void intr_install_isr (
    int srcno,
    void (*isr)(int srcno, void * aux),
    void * isr_aux);
#endif // _INTR_H_