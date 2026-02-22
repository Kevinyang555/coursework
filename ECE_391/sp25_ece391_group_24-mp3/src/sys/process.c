// process.c - user process
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef PROCESS_TRACE
#define TRACE
#endif

#ifdef PROCESS_DEBUG
#define DEBUG
#endif

#include "conf.h"
#include "assert.h"
#include "process.h"
#include "elf.h"
#include "fs.h"
#include "io.h"
#include "string.h"
#include "thread.h"
#include "riscv.h"
#include "trap.h"
#include "memory.h"
#include "heap.h"
#include "intr.h"
#include "error.h"


// COMPILE-TIME PARAMETERS
//

#ifndef NPROC
#define NPROC 16
#endif

// INTERNAL FUNCTION DECLARATIONS
//

static int build_stack(void *stack, int argc, char **argv, uintptr_t *argv_user_ptr);
static void fork_func(struct condition *forked, struct trap_frame *tfr);

// INTERNAL GLOBAL VARIABLES
//

static struct process main_proc;

static struct process *proctab[NPROC] = {
    &main_proc};

// EXPORTED GLOBAL VARIABLES
//

char procmgr_initialized = 0;

// EXPORTED FUNCTION DEFINITIONS
//

//==============================================================================================
// int progmgr_init(void)
// inputs: none
// outputs: none
// description: retroactively initializes the process manager around main thread
//              sets up the main process structure and associates it with the main thread
//              sets the process manager initialized flag
//===============================================================================================
void procmgr_init(void)
{
    assert(memory_initialized && heap_initialized);
    assert(!procmgr_initialized);

    main_proc.idx = 0;
    main_proc.tid = running_thread();
    main_proc.mtag = active_mspace();
    thread_set_process(main_proc.tid, &main_proc);
    procmgr_initialized = 1;
}

// ==============================================================================================
// int process_exec(struct io * exeio, int argc, char ** argv)
// inputs: struct io * exeio: pointer to the I/O stream of the executable
//         int argc: number of arguments
//         char ** argv: array of argument strings
// outputs: should not return if successful
// description: loads the executable file from the I/O stream, sets up the user stack with arguments,
//              and starts executing the program in user mode
//              - allocates a new page for the argument vector and copies the arguments from user space
//              - builds the user stack with the argument vector and the program entry point
//              - unmaps all pages mapped into the user address space
//              - loads the program image into memory and maps it into the user address space
//              - sets up the trap frame for the new process
//              - starts executing the program in user mode
//===============================================================================================
int process_exec(struct io *exeio, int argc, char **argv) {
    uint32_t argsz;
    uint32_t arg_size = 0;

    // copies arguments from user space to new page
    void *arg_page = alloc_phys_page();
    memset(arg_page, 0, PAGE_SIZE);
    assert(arg_page != NULL);

    // count size
    arg_size += (argc + 1) * sizeof(char *);
    assert(arg_size < PAGE_SIZE);

    for (int i = 0; i < argc; ++i) {
        argsz = strlen(argv[i]) + 1;
        assert(arg_size + argsz < PAGE_SIZE);
        arg_size += argsz;
    }

    uintptr_t argv_user_ptr;
    int stksz = build_stack((void *)arg_page, argc, argv, &argv_user_ptr);
    assert(stksz == ROUND_UP(arg_size, 16));

    // unmap all pages mapped into user address space
    reset_active_mspace();


    // load and map program image
    void (*entry)(void);
    int result = elf_load(exeio, &entry);
    assert(result == 0);
    ioclose(exeio);
    // main_proc.iotab[PROCESS_IOMAX - 1] = exeio;
    // maps stack page into new memory space
    uintptr_t user_sp = UMEM_END_VMA - PAGE_SIZE;
    map_page(user_sp, arg_page, PTE_R | PTE_W | PTE_U);

    // start execution in user space
    struct trap_frame *tf = kmalloc(sizeof(struct trap_frame));
    memset(tf, 0, sizeof(struct trap_frame));
    tf->a0 = argc;
    tf->a1 = UMEM_END_VMA - stksz;
    tf->sp = (void *) UMEM_END_VMA - stksz;
    tf->sepc = entry;
    tf->sstatus = csrr_sstatus();
    tf->sstatus &= ~RISCV_SSTATUS_SPP;
    // kprintf("sepc = %p\n", tf->sepc);
    // kprintf("sp   = %p\n", tf->sp);
    // kprintf("a0   = %p\n", tf->a0);
    // kprintf("a1   = %p\n", tf->a1);

    // uint64_t *tf_raw = (uint64_t *)tf;
    // for (int i = 0; i < sizeof(struct trap_frame) / 8; i++) {
    //     kprintf("trap_frame[%d] = 0x%016lx\n", i, tf_raw[i]);
    // }

    // kprintf("Debugging user stack contents before trap_frame_jump:\n");

    trap_frame_jump(tf, (void *)running_thread_anchor() - sizeof(struct trap_frame));

    return 0;
}

int process_fork(const struct trap_frame *tfr)
{
    struct process *proc = kmalloc(sizeof(struct process));
    assert(proc != NULL);

    proc->mtag = clone_active_mspace();
    proc->idx = -1;
    
    for(int i = 0; i < NPROC; i++){
        if(proctab[i] == NULL){
            proc->idx = i;
            proctab[i] = proc;
            break;
        }
    }
    if(proc->idx == -1){
        kfree(proc);
        kprintf("No free process slots");
        return -ENOMEM;
    }
    
    for(int i = 0; i < PROCESS_IOMAX; i++)
    {
        proc->iotab[i] = NULL;
    }
    // copy the I/O table from the parent process
    for (int i = 0; i < PROCESS_IOMAX; i++)
    {
        if (current_process()->iotab[i] != NULL){
            proc->iotab[i] = ioaddref(current_process()->iotab[i]);
        }
    }

    struct condition forked;
    condition_init(&forked, "forked");

    // deep copy the trap frame
    struct trap_frame tfr_copy;
    memcpy(&tfr_copy, tfr, sizeof(struct trap_frame)); 

    // spawn a new thread for the child process
    int tid = thread_spawn("forked process", (void(*)(void))fork_func, (uint64_t)&forked, (uint64_t)&tfr_copy);
    assert(tid >= 0);
    proc->tid = tid;
    thread_set_process(tid, proc);
    int pie;
    pie=disable_interrupts();
    condition_wait(&forked);
    restore_interrupts(pie);
    
    return proc->idx;
}

// ==============================================================================================
// void process_exit(void)
// inputs: none
// outputs: none
// description: cleans up the process and its resources
//              - closes all open I/O interfaces
//              - unmap memory space
//              - flushes the file system
//              - frees the process structure
//              - exits the thread
//              - if the main process exits, it panics
//===============================================================================================
void process_exit(void)
{
    struct process *proc = current_process();
    int pid = proc->idx;
    
    for (int i = 0; i < PROCESS_IOMAX; i++)
    {
        if (proc->iotab[i])
        {   
            ioclose(proc->iotab[i]);
            proc->iotab[i] = NULL;
        }
    }
    // Reclaim process memory space
    discard_active_mspace();
    
    //close the file of the game this process is running

   // ioclose()
   
    fsflush();
    if (running_thread() == 0){
        panic("main process exited");
    }
    // Free process structure
    proctab[pid] = NULL;
    kfree(proc);
    
    // Exit thread
    thread_exit();
}

// ==============================================================================================
// int build_stack(void *stack, int argc, char **argv, uintptr_t *argv_user_ptr)
// inputs: void *stack: pointer to the stack memory
//         int argc: number of arguments
//         char **argv: array of argument strings
//         uintptr_t *argv_user_ptr: pointer to the user space address of argv
// outputs: int: size of the stack in bytes
// description: builds the user stack with the argument vector and the program entry point
//              - allocates memory for the argument vector and copies the arguments from user space
//              - sets up the stack pointer and the argument vector
//              - rounds up the stack size to a multiple of 16 bytes
//              - returns the size of the stack in bytes
//===============================================================================================
int build_stack(void *stack, int argc, char **argv, uintptr_t *argv_user_ptr)
{
    size_t stksz, argsz;
    uintptr_t *newargv;
    char *p;
    int i;

    // We need to be able to fit argv[] on the initial stack page, so _argc_
    // cannot be too large. Note that argv[] contains argc+1 elements (last one
    // is a NULL pointer).

    if (PAGE_SIZE / sizeof(char *) - 1 < argc)
        return -ENOMEM;

    stksz = (argc + 1) * sizeof(char *);

    // Add the sizes of the null-terminated strings that argv[] points to.

    for (i = 0; i < argc; i++)
    {
        argsz = strlen(argv[i]) + 1;
        if (PAGE_SIZE - stksz < argsz)
            return -ENOMEM;
        stksz += argsz;
    }

    // Round up stksz to a multiple of 16 (RISC-V ABI requirement).

    stksz = ROUND_UP(stksz, 16);
    assert(stksz <= PAGE_SIZE);

    // Set _newargv_ to point to the location of the argument vector on the new
    // stack and set _p_ to point to the stack space after it to which we will
    // copy the strings. Note that the string pointers we write to the new
    // argument vector must point to where the user process will see the stack.
    // The user stack will be at the highest page in user memory, the address of
    // which is `(UMEM_END_VMA - PAGE_SIZE)`. The offset of the _p_ within the
    // stack is given by `p - newargv'.

    newargv = stack + PAGE_SIZE - stksz;
    p = (char *)(newargv + argc + 1);

    for (i = 0; i < argc; i++)
    {
        newargv[i] = (UMEM_END_VMA - PAGE_SIZE) + ((void *)p - (void *)stack);
        argsz = strlen(argv[i]) + 1;
        memcpy(p, argv[i], argsz);
        p += argsz;
    }

    newargv[argc] = 0;
    *argv_user_ptr = (UMEM_END_VMA - PAGE_SIZE) + ((uintptr_t)newargv - (uintptr_t)stack);
    return stksz;
}

void fork_func(struct condition *forked, struct trap_frame *tfr)
{
    tfr->a0 = 0; // child process
    tfr->sepc +=4; // argv
    condition_broadcast(forked);
    trap_frame_jump(tfr, (void *)running_thread_anchor() - sizeof(struct trap_frame));
    return;
}