// plic.c - RISC-V PLIC
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef PLIC_TRACE
#define TRACE
#endif

#ifdef PLIC_DEBUG
#define DEBUG
#endif

#include "conf.h"
#include "plic.h"
#include "assert.h"

#include <stdint.h>

// INTERNAL MACRO DEFINITIONS
//

// CTX(i,0) is hartid /i/ M-mode context
// CTX(i,1) is hartid /i/ S-mode context

#define CTX(i,s) (2*(i)+(s))

// INTERNAL TYPE DEFINITIONS
// 

struct plic_regs {
	union {
		uint32_t priority[PLIC_SRC_CNT];
		char _reserved_priority[0x1000];
	};

	union {
		uint32_t pending[PLIC_SRC_CNT/32];
		char _reserved_pending[0x1000];
	};

	union {
		uint32_t enable[PLIC_CTX_CNT][32];
		char _reserved_enable[0x200000-0x2000];
	};

	struct {
		union {
			struct {
				uint32_t threshold;
				uint32_t claim;
			};
			
			char _reserved_ctxctl[0x1000];
		};
	} ctx[PLIC_CTX_CNT];
};

#define PLIC (*(volatile struct plic_regs*)PLIC_MMIO_BASE)

// INTERNAL FUNCTION DECLARATIONS
//

static void plic_set_source_priority (
	uint_fast32_t srcno, uint_fast32_t level);
static int plic_source_pending(uint_fast32_t srcno);
static void plic_enable_source_for_context (
	uint_fast32_t ctxno, uint_fast32_t srcno);
static void plic_disable_source_for_context (
	uint_fast32_t ctxno, uint_fast32_t srcno);
static void plic_set_context_threshold (
	uint_fast32_t ctxno, uint_fast32_t level);
static uint_fast32_t plic_claim_context_interrupt (
	uint_fast32_t ctxno);
static void plic_complete_context_interrupt (
	uint_fast32_t ctxno, uint_fast32_t srcno);

static void plic_enable_all_sources_for_context(uint_fast32_t ctxno);
static void plic_disable_all_sources_for_context(uint_fast32_t ctxno);

// We currently only support single-hart operation, sending interrupts to S mode
// on hart 0 (context 0). The low-level PLIC functions already understand
// contexts, so we only need to modify the high-level functions (plit_init,
// plic_claim_request, plic_finish_request)to add support for multiple harts.

// EXPORTED FUNCTION DEFINITIONS
// 

void plic_init(void) {
	int i;

	// Disable all sources by setting priority to 0

	for (i = 0; i < PLIC_SRC_CNT; i++)
		plic_set_source_priority(i, 0);
	
	// Route all sources to S mode on hart 0 only

	for (int i = 0; i < PLIC_CTX_CNT; i++)
		plic_disable_all_sources_for_context(i);
	
	plic_enable_all_sources_for_context(CTX(0,1));
}

extern void plic_enable_source(int srcno, int prio) {
	trace("%s(srcno=%d,prio=%d)", __func__, srcno, prio);
	assert (0 < srcno && srcno <= PLIC_SRC_CNT);
	assert (prio > 0);

	plic_set_source_priority(srcno, prio);
}

extern void plic_disable_source(int irqno) {
	if (0 < irqno)
		plic_set_source_priority(irqno, 0);
	else
		debug("plic_disable_irq called with irqno = %d", irqno);
}

extern int plic_claim_interrupt(void) {
	// FIXME: Hardwired S-mode hart 0
	trace("%s()", __func__);
	return plic_claim_context_interrupt(CTX(0,1));
}

extern void plic_finish_interrupt(int irqno) {
	// FIXME: Hardwired S-mode hart 0
	trace("%s(irqno=%d)", __func__, irqno);
	plic_complete_context_interrupt(CTX(0,1), irqno);
}

// INTERNAL FUNCTION DEFINITIONS
//


//This function sets the priority of an interrupt source(srcno) to given priority(level)
static inline void plic_set_source_priority(uint_fast32_t srcno, uint_fast32_t level) {
	// FIXME your code goes here
	PLIC.priority[srcno] = level;
	
}


//This function returns whether if an interrupt source(srcno) is pending
static inline int plic_source_pending(uint_fast32_t srcno) {
	// FIXME your code goes here
	uint32_t data = PLIC.pending[srcno /32]; // getting the index of this srcno
	int result = (data >>(srcno % 32)) & 1; //getting the bit information for pending of this srcno
	return result;
}


//This function enable the correct bit of interrupt source(srcno) for the PLIC context of ctxno
static inline void plic_enable_source_for_context(uint_fast32_t ctxno, uint_fast32_t srcno) {
	// FIXME your code goes here
	uint32_t temp = 0;
	temp = 1 <<(srcno %32); //push one to the corresponding bit of interrupt source srcno
	uint32_t final = temp | PLIC.enable[ctxno][srcno/32]; //or the temp and the current bit of srcno of context ctxno for enable
	PLIC.enable[ctxno][srcno/32] = final;//enable 
	  

}


//This function disable the correct bit of interrupt source(srcno) for the PLIC context of ctxno
static inline void plic_disable_source_for_context(uint_fast32_t ctxno, uint_fast32_t srcid) {
	// FIXME your code goes here
	uint32_t temp = 0;
	temp = ~(1 <<(srcid %32));//push zero to the corresponding bit of interrupt source srcid by NOT
	uint32_t final = temp & PLIC.enable[ctxno][srcid/32]; // and the temp and the corresponding bit for context of target interrupt for disable
	PLIC.enable[ctxno][srcid/32] = final;//disable

}


//This function sets the priority threshold(level) for context(ctxno)
static inline void plic_set_context_threshold(uint_fast32_t ctxno, uint_fast32_t level) {
	// FIXME your code goes here
	PLIC.ctx[ctxno].threshold = level;
	
}


//This function returns the highest priority interrupt to cpu of PLIC context(ctxno)
static inline uint_fast32_t plic_claim_context_interrupt(uint_fast32_t ctxno) {
	// FIXME your code goes here
	return PLIC.ctx[ctxno].claim;
}


//This function signals cpu serviced the interrupt by writing the interrupt source(srcno) back to the PLIC context(ctxno)'s claim register
static inline void plic_complete_context_interrupt(uint_fast32_t ctxno, uint_fast32_t srcno) {
	// FIXME your code goes here
	PLIC.ctx[ctxno].claim = srcno;
}


//This function enable all interrupt of a context(ctxno)
static void plic_enable_all_sources_for_context(uint_fast32_t ctxno) {
	// FIXME your code goes here
	for(int i =0; i<(32); i++){ //loop over all interrupt, the number 32 here refers to the number of columns in the enable array 
		PLIC.enable[ctxno][i] = 0xFFFFFFFF;//setting all bits to one
	}
	
}


//This function disable all interrupt of a context(ctxno)
static void plic_disable_all_sources_for_context(uint_fast32_t ctxno) {
	// FIXME your code goes here
	for(int i =0; i<(32); i++){//loop over all interrupt, the number 32 here refers to the number of columns in the enable array 
		PLIC.enable[ctxno][i] = 0;//setting all bits to 0
	}
	
}
