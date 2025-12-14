// see.h - Supervisor Execution Environment
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _SEE_H_
#define _SEE_H_

#include <stdint.h>

extern void __attribute__ ((noreturn)) halt_success(void);
extern void __attribute__ ((noreturn)) halt_failure(void);
extern void set_stcmp(uint64_t stcmp_value);

#endif // _SEE_H_