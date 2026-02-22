// thread.c - Threads
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef THREAD_TRACE
#define TRACE
#endif

#ifdef THREAD_DEBUG
#define DEBUG
#endif

#include "thread.h"

#include <stddef.h>
#include <stdint.h>

#include "assert.h"
#include "heap.h"
#include "string.h"
#include "riscv.h"
#include "intr.h"
#include "memory.h"
#include "error.h"
#include "process.h"

#include <stdarg.h>

// COMPILE-TIME PARAMETERS
//

// NTHR is the maximum number of threads

#ifndef NTHR
#define NTHR 16
#endif

#ifndef STACK_SIZE
#define STACK_SIZE 4000
#endif

// EXPORTED GLOBAL VARIABLES
//

char thrmgr_initialized = 0;

// INTERNAL TYPE DEFINITIONS
//

enum thread_state
{
    THREAD_UNINITIALIZED = 0,
    THREAD_WAITING,
    THREAD_RUNNING,
    THREAD_READY,
    THREAD_EXITED
};

struct thread_context
{
    uint64_t s[12];
    void *ra;
    void *sp;
};

struct thread_stack_anchor
{
    struct thread *ktp;
    void *kgp;
};

struct thread
{
    struct thread_context ctx; // must be first member (thrasm.s)
    int id;                    // index into thrtab[]
    enum thread_state state;
    const char *name;
    struct thread_stack_anchor *stack_anchor;
    void *stack_lowest;
    struct thread *parent;
    struct thread *list_next;
    struct condition *wait_cond;
    struct condition child_exit;
    struct lock *lock_list;
    struct process *proc;
};

// INTERNAL MACRO DEFINITIONS
//

// Pointer to running thread, which is kept in the tp (x4) register.

#define TP ((struct thread *)__builtin_thread_pointer())

// Macro for changing thread state. If compiled for debugging (DEBUG is
// defined), prints function that changed thread state.

#define set_thread_state(t, s)                                               \
    do                                                                       \
    {                                                                        \
        debug("Thread <%s:%d> state changed from %s to %s by <%s:%d> in %s", \
              (t)->name, (t)->id,                                            \
              thread_state_name((t)->state),                                 \
              thread_state_name(s),                                          \
              TP->name, TP->id,                                              \
              __func__);                                                     \
        (t)->state = (s);                                                    \
    } while (0)

// INTERNAL FUNCTION DECLARATIONS
//

// Initializes the main and idle threads. called from threads_init().

static void init_main_thread(void);
static void init_idle_thread(void);

// Sets the RISC-V thread pointer to point to a thread.

static void set_running_thread(struct thread *thr);

// Returns a string representing the state name. Used by debug and trace
// statements, so marked unused to avoid compiler warnings.

static const char *thread_state_name(enum thread_state state)
    __attribute__((unused));

// void thread_reclaim(int tid)
//
// Reclaims a thread's slot in thrtab and makes its parent the parent of its
// children. Frees the struct thread of the thread.

static void thread_reclaim(int tid);

// struct thread * create_thread(const char * name)
//
// Creates and initializes a new thread structure. The new thread is not added
// to any list and does not have a valid context (_thread_switch cannot be
// called to switch to the new thread).

static struct thread *create_thread(const char *name);

// void running_thread_suspend(void)
// Suspends the currently running thread and resumes the next thread on the
// ready-to-run list using _thread_swtch (in threasm.s). Must be called with
// interrupts enabled. Returns when the current thread is next scheduled for
// execution. If the current thread is TP, it is marked READY and placed
// on the ready-to-run list. Note that running_thread_suspend will only return if the
// current thread becomes READY.

static void running_thread_suspend(void);

// The following functions manipulate a thread list (struct thread_list). Note
// that threads form a linked list via the list_next member of each thread
// structure. Thread lists are used for the ready-to-run list (ready_list) and
// for the list of waiting threads of each condition variable. These functions
// are not interrupt-safe! The caller must disable interrupts before calling any
// thread list function that may modify a list that is used in an ISR.

static void tlclear(struct thread_list *list);
static int tlempty(const struct thread_list *list);
static void tlinsert(struct thread_list *list, struct thread *thr);
static struct thread *tlremove(struct thread_list *list);
static void tlappend(struct thread_list *l0, struct thread_list *l1);

static void idle_thread_func(void);

// IMPORTED FUNCTION DECLARATIONS
// defined in thrasm.s
//

extern struct thread *_thread_swtch(struct thread *thr);

extern void _thread_startup(void);

// INTERNAL GLOBAL VARIABLES
//

#define MAIN_TID 0
#define IDLE_TID (NTHR - 1)

static struct thread main_thread;
static struct thread idle_thread;

extern char _main_stack_lowest[]; // from start.s
extern char _main_stack_anchor[]; // from start.s

static struct thread main_thread = {
    .id = MAIN_TID,
    .name = "main",
    .state = THREAD_RUNNING,
    .stack_anchor = (void *)_main_stack_anchor,
    .stack_lowest = _main_stack_lowest,
    .child_exit.name = "main.child_exit",
};

extern char _idle_stack_lowest[]; // from thrasm.s
extern char _idle_stack_anchor[]; // from thrasm.s

static struct thread idle_thread = {
    .id = IDLE_TID,
    .name = "idle",
    .state = THREAD_READY,
    .parent = &main_thread,
    .stack_anchor = (void *)_idle_stack_anchor,
    .stack_lowest = _idle_stack_lowest,
    .ctx.sp = _idle_stack_anchor,
    .ctx.ra = &_thread_startup,
    .ctx.s[0] = (uint64_t)idle_thread_func // let s[0] be the entry point
};

static struct thread *thrtab[NTHR] = {
    [MAIN_TID] = &main_thread,
    [IDLE_TID] = &idle_thread};

static struct thread_list ready_list = {
    .head = &idle_thread,
    .tail = &idle_thread};

void lock_init(struct lock *lock)
{
    condition_init(&lock->cond, "locked");
    if (thrtab[running_thread()]->lock_list == NULL)
    {
        thrtab[running_thread()]->lock_list = kcalloc(1, sizeof(struct lock));
    }
    lock->tid = -1;
    lock->waiting_num = 0;
    lock->next = NULL;
}

void lock_acquire(struct lock *lock)
{
    if(lock->tid == running_thread()){
        lock->waiting_num++;
    }else{
        while (lock->tid != -1)
        {
            condition_wait(&lock->cond);
        }
        lock->tid = running_thread();
        lock->waiting_num++;

        // add lock to the thread's lock list
        struct thread *curr = thrtab[lock->tid];
        if (curr->lock_list == NULL)
        {
            curr->lock_list = lock;
        }
        else
        {
            struct lock *temp = curr->lock_list;
            while (temp->next != NULL)
            {
                temp = temp->next;
            }
            temp->next = lock;
        }
    }
}

void lock_release(struct lock *lock)
{
    assert(lock->tid == running_thread());

    // remove lock from the thread's lock list
    struct thread *curr = thrtab[lock->tid];
    struct lock *temp = curr->lock_list;
    struct lock *prev = NULL;
    while (temp != NULL)
    {
        if (temp == lock)
        {
            if (prev == NULL)
            {
                curr->lock_list = temp->next;
            }
            else
            {
                prev->next = temp->next;
            }
            temp->next = NULL;
            break;
        }
        prev = temp;
        temp = temp->next;
    }
    lock->tid = -1;
    lock->waiting_num--;
    condition_broadcast(&lock->cond);
}

// EXPORTED FUNCTION DEFINITIONS
//

int running_thread(void)
{
    return TP->id;
}

void thrmgr_init(void)
{
    trace("%s()", __func__);
    init_main_thread();
    init_idle_thread();
    set_running_thread(&main_thread);
    thrmgr_initialized = 1;
}

// int thread_spawn(const char * name, void (*entry)(void), ...)
// Inputs: const char * name - name of the thread
//         void (*entry)(void) - entry function of the thread
//         ... - arguments to the entry function
// Outputs: int - tid of the spawned thread
// Description: Creates new thread and adds it to the ready list, then fills in the entry function arguments and context
// Side Effects: new thread is created and added to the ready list
int thread_spawn(
    const char *name,
    void (*entry)(void),
    ...)
{
    struct thread *child;
    va_list ap;
    int pie;
    int i;

    child = create_thread(name);

    if (child == NULL)
        return -EMTHR;

    set_thread_state(child, THREAD_READY);

    pie = disable_interrupts();
    tlinsert(&ready_list, child);
    restore_interrupts(pie);

    // FIXME your code goes here
    // filling in entry function arguments is given below, the rest is up to you

    va_start(ap, entry);
    for (i = 1; i < 9; i++)
        child->ctx.s[i] = va_arg(ap, uint64_t);
    va_end(ap);

    // update ra so _thread_swtch can return to the entry function
    child->ctx.s[0] = (uint64_t)entry;
    child->ctx.ra = _thread_startup;
    child->ctx.sp = child->stack_anchor;

    return child->id;
}

// void thread_exit(void)
// Inputs: none
// Outputs: none
// Description: Exits the current thread
// Side Effects: current thread is set to exited and the parent thread is signaled, then switches to the next ready thread (should not return)
void thread_exit(void)
{
    trace("%s() in <%s:%d>", __func__, TP->name, TP->id);
    // check if the current thread is the main thread
    if (TP->id == MAIN_TID)
        halt_success();

    // set the current thread state to exited
    set_thread_state(TP, THREAD_EXITED);

    // signal parent thread that we have exited
    condition_broadcast(&TP->parent->child_exit);
    struct lock *temp = TP->lock_list;
    while(temp != NULL){
        lock_release(TP->lock_list);
        temp = temp->next;
    }

    running_thread_suspend(); // should not return

    halt_failure();
}

void thread_yield(void)
{
    trace("%s() in <%s:%d>", __func__, TP->name, TP->id);
    running_thread_suspend();
}

// int thread_join(int tid)
// Inputs: int tid - thread id
// Outputs: int - tid of the exited thread
// Description: Waits for a child of the current thread to exit
// Side Effects: current thread yield until the identified child of the running thread exits, then the resource is reclaimed
int thread_join(int tid)
{
    if (tid < 0 || tid >= NTHR)
    {
        return -EINVAL;
    }

    // if tid is not zero, wait for the identified child of the running thread to exit
    if (tid != 0)
    {
        // check if the identified thread exists and is a child of the running thread
        if (thrtab[tid] == NULL || thrtab[tid]->parent != TP)
        {
            return -EINVAL;
        }

        // if the identified thread has exited, return its tid
        if (thrtab[tid]->state != THREAD_EXITED)
        {
            condition_wait(&thrtab[tid]->parent->child_exit);
        }
        thread_reclaim(tid);
        return tid;
    }
    else
    {
        // if tid is zero, wait for any child of the running thread
        int i;
        for (i = 1; i < NTHR; i++)
        {
            if (thrtab[i] != NULL && thrtab[i]->parent == TP)
            {
                if (thrtab[i]->state != THREAD_EXITED)
                {
                    condition_wait(&TP->child_exit);
                }
                thread_reclaim(i);
                return i;
            }
        }
        return -EINVAL;
    }
}

const char *thread_name(int tid)
{
    assert(0 <= tid && tid < NTHR);
    assert(thrtab[tid] != NULL);
    return thrtab[tid]->name;
}

const char *running_thread_name(void)
{
    return TP->name;
}

void condition_init(struct condition *cond, const char *name)
{
    tlclear(&cond->wait_list);
    cond->name = name;
}

void condition_wait(struct condition *cond)
{
    int pie;

    trace("%s(cond=<%s>) in <%s:%d>", __func__,
          cond->name, TP->name, TP->id);

    assert(TP->state == THREAD_RUNNING);

    // Insert current thread into condition wait list

    set_thread_state(TP, THREAD_WAITING);
    TP->wait_cond = cond;
    TP->list_next = NULL;

    pie = disable_interrupts();
    tlinsert(&cond->wait_list, TP);
    restore_interrupts(pie);

    running_thread_suspend();
}

// void condition_broadcast(struct condition * cond)
// Inputs: struct condition * cond - pointer to the condition variable
// Outputs: none
// Description: Broadcasts the condition variable to wake up the waiting threads
// Side Effects: all threads waiting on the condition are removed from the wait list and added to the ready list
void condition_broadcast(struct condition *cond)
{
    if (cond == NULL)
    {
        return;
    }

    struct thread *curr = cond->wait_list.head;
    int pie;

    // first change state to ready for all threads in the wait list
    while (curr != NULL)
    {
        curr->state = THREAD_READY;
        curr = curr->list_next;
    }

    // then append the wait list to the ready list
    pie = disable_interrupts();
    tlappend(&ready_list, &cond->wait_list);
    restore_interrupts(pie);
}

struct process *thread_process(int tid)
{
    return (thrtab[tid]->proc != NULL) ? thrtab[tid]->proc : NULL;
}

struct process *running_thread_process()
{
    return (thrtab[running_thread()]->proc != NULL) ? thrtab[running_thread()]->proc : NULL;
}

void thread_set_process(int tid, struct process *proc)
{
    if (proc == NULL || tid < 0)
    {
        return;
    }

    thrtab[tid]->proc = proc;
}

struct thread_stack_anchor *running_thread_anchor()
{
    return TP->stack_anchor;
}

// INTERNAL FUNCTION DEFINITIONS
//

void init_main_thread(void)
{
    // Initialize stack anchor with pointer to self
    main_thread.stack_anchor->ktp = &main_thread;
}

void init_idle_thread(void)
{
    // Initialize stack anchor with pointer to self
    idle_thread.stack_anchor->ktp = &idle_thread;
}

static void set_running_thread(struct thread *thr)
{
    asm inline("mv tp, %0" ::"r"(thr) : "tp");
}

const char *thread_state_name(enum thread_state state)
{
    static const char *const names[] = {
        [THREAD_UNINITIALIZED] = "UNINITIALIZED",
        [THREAD_WAITING] = "WAITING",
        [THREAD_RUNNING] = "RUNNING",
        [THREAD_READY] = "READY",
        [THREAD_EXITED] = "EXITED"};

    if (0 <= (int)state && (int)state < sizeof(names) / sizeof(names[0]))
        return names[state];
    else
        return "UNDEFINED";
};

void thread_reclaim(int tid)
{
    struct thread *const thr = thrtab[tid];
    int ctid;

    assert(0 < tid && tid < NTHR && thr != NULL);
    assert(thr->state == THREAD_EXITED);

    // Make our parent thread the parent of our child threads. We need to scan
    // all threads to find our children. We could keep a list of all of a
    // thread's children to make this operation more efficient.

    for (ctid = 1; ctid < NTHR; ctid++)
    {
        if (thrtab[ctid] != NULL && thrtab[ctid]->parent == thr)
            thrtab[ctid]->parent = thr->parent;
    }

    thrtab[tid] = NULL;
    kfree(thr);
}

struct thread *create_thread(const char *name)
{
    struct thread_stack_anchor *anchor;
    void *stack_page;
    struct thread *thr;
    int tid;

    trace("%s(name=\"%s\") in <%s:%d>", __func__, name, TP->name, TP->id);

    // Find a free thread slot.

    tid = 0;
    while (++tid < NTHR)
        if (thrtab[tid] == NULL)
            break;

    if (tid == NTHR)
        return NULL;

    // Allocate a struct thread and a stack

    thr = kcalloc(1, sizeof(struct thread));

    stack_page = alloc_phys_page();
    anchor = stack_page + STACK_SIZE;
    anchor -= 1; // anchor is at base of stack
    thr->stack_lowest = stack_page;
    thr->stack_anchor = anchor;
    anchor->ktp = thr;
    anchor->kgp = NULL;

    thrtab[tid] = thr;

    thr->id = tid;
    thr->name = name;
    thr->parent = TP;
    return thr;
}

// void running_thread_suspend(void)
// Inputs: none
// Outputs: none
// Description: Suspends the currently running thread and resumes the next thread on the ready-to-run list
// Side Effects: current thread's state is changed to ready, and the next thread is set to self
//               current thread is inserted into the back of ready list, and the next thread is removed from the front of ready list
void running_thread_suspend(void)
{
    int pie;

    pie = disable_interrupts();
    // insert in the back only if the current thread is running
    if (TP->state == THREAD_RUNNING)
    {
        TP->state = THREAD_READY;
        tlinsert(&ready_list, TP);
    }

    struct thread *next = tlremove(&ready_list);
    restore_interrupts(pie);

    // switch to the next thread
    next->state = THREAD_RUNNING;
    //switch also the memory space
    if(next->id != IDLE_TID && next->proc != NULL){
        switch_mspace(next->proc->mtag);
    }
    struct thread *temp = _thread_swtch(next);

    // free the stack of the exited thread
    if (temp->state == THREAD_EXITED)
    {
        // release all locks held by the thread
        // struct lock *lock = temp->lock_list;
        // while (lock != NULL)
        // {
        //     struct lock *next_lock = lock->next;
        //     lock_release(lock);
        //     lock = next_lock;
        // }
        // free the stack
        free_phys_page(temp->stack_lowest);
        // kfree(temp->lock_list);
    }
}

void tlclear(struct thread_list *list)
{
    list->head = NULL;
    list->tail = NULL;
}

int tlempty(const struct thread_list *list)
{
    return (list->head == NULL);
}

void tlinsert(struct thread_list *list, struct thread *thr)
{
    thr->list_next = NULL;

    if (thr == NULL)
        return;

    if (list->tail != NULL)
    {
        assert(list->head != NULL);
        list->tail->list_next = thr;
    }
    else
    {
        assert(list->head == NULL);
        list->head = thr;
    }

    list->tail = thr;
}

struct thread *tlremove(struct thread_list *list)
{
    struct thread *thr;

    thr = list->head;

    if (thr == NULL)
        return NULL;

    list->head = thr->list_next;

    if (list->head != NULL)
        thr->list_next = NULL;
    else
        list->tail = NULL;

    thr->list_next = NULL;
    return thr;
}

// Appends elements of l1 to the end of l0 and clears l1.

void tlappend(struct thread_list *l0, struct thread_list *l1)
{
    if (l0->head != NULL)
    {
        assert(l0->tail != NULL);

        if (l1->head != NULL)
        {
            assert(l1->tail != NULL);
            l0->tail->list_next = l1->head;
            l0->tail = l1->tail;
        }
    }
    else
    {
        assert(l0->tail == NULL);
        l0->head = l1->head;
        l0->tail = l1->tail;
    }

    l1->head = NULL;
    l1->tail = NULL;
}

void idle_thread_func(void)
{
    // The idle thread sleeps using wfi if the ready list is empty. Note that we
    // need to disable interrupts before checking if the thread list is empty to
    // avoid a race condition where an ISR marks a thread ready to run between
    // the call to tlempty() and the wfi instruction.

    for (;;)
    {
        // If there are runnable threads, yield to them.

        while (!tlempty(&ready_list))
            thread_yield();

        // No runnable threads. Sleep using the wfi instruction. Note that we
        // need to disable interrupts and check the runnable thread list one
        // more time (make sure it is empty) to avoid a race condition where an
        // ISR marks a thread ready before we call the wfi instruction.

        disable_interrupts();
        if (tlempty(&ready_list))
            asm("wfi");
        enable_interrupts();
    }
}