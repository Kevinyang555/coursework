# MP2: Device Drivers & Kernel Threads

Implementation of interrupt-driven device drivers (UART serial, VirtIO entropy) and kernel threading primitives for RISC-V. Covers ISR handling, DMA ring buffers, context switching, and synchronization.

## Architecture Overview

Three major components built progressively:
1. **UART Serial Driver**: Interrupt-based I/O with circular buffers
2. **VirtIO Entropy Device**: Hardware RNG using VirtIO virtqueue protocol
3. **Kernel Threading**: Preemptive scheduler with synchronization primitives

All components integrate with RISC-V interrupt/exception handling and run in supervisor mode (S-mode).

---

## Part 1: UART Serial Driver

Interrupt-driven serial port driver for asynchronous character I/O.

### Hardware Interface

**UART Registers** (memory-mapped at base address):
```c
#define UART_RBR   0x00  // Receiver Buffer (R)
#define UART_THR   0x00  // Transmit Holding (W)
#define UART_IER   0x01  // Interrupt Enable
#define UART_IIR   0x02  // Interrupt ID (R)
#define UART_FCR   0x02  // FIFO Control (W)
#define UART_LCR   0x03  // Line Control
#define UART_MCR   0x04  // Modem Control
#define UART_LSR   0x05  // Line Status
#define UART_MSR   0x06  // Modem Status
```

**Key Status Bits (LSR):**
- `LSR_DR` (0x01): Data ready to read
- `LSR_THRE` (0x20): Transmit holding register empty

**Interrupt Enable (IER):**
- `IER_RDI` (0x01): Enable RX data interrupt
- `IER_THRI` (0x02): Enable TX holding empty interrupt

### Circular Buffer Implementation

```c
struct circular_buffer {
    char data[BUFFER_SIZE];
    int read_idx;
    int write_idx;
    int count;
};
```

**Operations:**
- **Enqueue**: `buf[write_idx++] = ch; write_idx %= SIZE; count++;`
- **Dequeue**: `ch = buf[read_idx++]; read_idx %= SIZE; count--;`
- **Full**: `count == BUFFER_SIZE`
- **Empty**: `count == 0`

**Thread Safety**: Access only from ISR or with interrupts disabled

### Driver Functions

```c
void uart_init(void);
```
- Configure UART: 8N1 (8 data bits, no parity, 1 stop bit)
- Enable RX interrupt in IER
- Initialize RX/TX circular buffers
- Register ISR with interrupt controller

```c
int uart_getc(void);
```
- **Blocking read** from RX buffer
- Spin-wait while buffer empty
- Disable interrupts during buffer access
- Return character (0-255)

```c
int uart_putc(char c);
```
- **Non-blocking write** to TX buffer
- Return -1 if buffer full
- Enable TX interrupt if not already enabled
- Return 0 on success

```c
void uart_isr(void);
```
- Read IIR to determine interrupt type
- **RX interrupt**: Read all available chars from RBR to RX buffer
- **TX interrupt**: Write all buffered chars to THR, disable TX int if empty
- Clear interrupt by reading LSR

### Critical Sections

```c
// Accessing circular buffers
cli();                    // Disable interrupts
/* ... read/write buffer ... */
sti();                    // Re-enable interrupts
```

### Common Pitfalls

- **Buffer overflow**: Not checking `count == BUFFER_SIZE` before enqueue
- **Lost interrupts**: Forgetting to clear interrupt status by reading LSR
- **Busy-wait deadlock**: `uart_getc()` spinning forever if RX interrupt disabled
- **TX starvation**: Not re-enabling TX interrupt when new data added

---

## Part 2: VirtIO Entropy Device

Hardware random number generator driver using VirtIO protocol.

### VirtIO Architecture

**VirtIO** is a standard for virtual device I/O:
- **Virtqueue**: Circular ring buffer for DMA descriptors
- **Split virtqueue**: Separate descriptor, available, and used rings
- **DMA buffers**: Physical memory shared between CPU and device

### Virtqueue Structure

```c
struct virtqueue {
    struct vring_desc *desc;        // Descriptor table
    struct vring_avail *avail;      // Available ring (driver→device)
    struct vring_used *used;        // Used ring (device→driver)
    uint16_t queue_size;
    uint16_t last_used_idx;
};
```

**Descriptor Entry:**
```c
struct vring_desc {
    uint64_t addr;                  // Physical address of buffer
    uint32_t len;                   // Buffer length
    uint16_t flags;                 // VRING_DESC_F_WRITE, etc.
    uint16_t next;                  // Next descriptor (for chaining)
};
```

**Available Ring:**
```c
struct vring_avail {
    uint16_t flags;
    uint16_t idx;                   // Next descriptor to consume
    uint16_t ring[QUEUE_SIZE];      // Descriptor indices
};
```

**Used Ring:**
```c
struct vring_used {
    uint16_t flags;
    uint16_t idx;                   // Next used entry
    struct vring_used_elem ring[QUEUE_SIZE];
};

struct vring_used_elem {
    uint32_t id;                    // Descriptor index
    uint32_t len;                   // Bytes written
};
```

### VirtIO MMIO Registers

```c
#define VIRTIO_MMIO_MAGIC           0x000  // 0x74726976 ("virt")
#define VIRTIO_MMIO_VERSION         0x004  // Should be 2
#define VIRTIO_MMIO_DEVICE_ID       0x008  // 4 = entropy
#define VIRTIO_MMIO_STATUS          0x070
#define VIRTIO_MMIO_QUEUE_SEL       0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX   0x034
#define VIRTIO_MMIO_QUEUE_NUM       0x038
#define VIRTIO_MMIO_QUEUE_READY     0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY    0x050
#define VIRTIO_MMIO_QUEUE_DESC_LOW  0x080
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW 0x090
#define VIRTIO_MMIO_QUEUE_USED_LOW  0x0A0
```

**Status Register Bits:**
- `ACKNOWLEDGE` (1): OS recognized device
- `DRIVER` (2): OS knows how to drive device
- `FEATURES_OK` (8): Feature negotiation complete
- `DRIVER_OK` (4): Driver ready
- `FAILED` (128): Unrecoverable error

### Driver Implementation

```c
void virtio_entropy_init(void);
```
1. Verify magic value and device ID
2. Reset device (write 0 to STATUS)
3. Set status: ACKNOWLEDGE → DRIVER
4. Negotiate features (write 0 to DEVICE_FEATURES_SEL, read, write to DRIVER_FEATURES)
5. Set FEATURES_OK, verify it stayed set
6. Allocate virtqueue structures (page-aligned)
7. Configure queue: NUM, DESC_LOW, AVAIL_LOW, USED_LOW
8. Write DRIVER_OK to STATUS
9. Pre-fill descriptors with DMA buffers

```c
int entropy_read(void *buf, size_t len);
```
1. Allocate DMA buffer for request
2. Get free descriptor from pool
3. Set descriptor: `addr = phys(buf), len = len, flags = WRITE`
4. Add descriptor index to available ring
5. Update `avail->idx`
6. Kick device: write queue index to QUEUE_NOTIFY
7. Wait for device to update used ring
8. Copy from DMA buffer to user buffer
9. Free descriptor

### DMA Buffer Management

**Physical vs Virtual Addressing:**
- CPU uses virtual addresses
- Device uses physical addresses
- Must translate: `phys_addr = virt_to_phys(virt_addr)`

**Alignment Requirements:**
- Descriptor table: 16-byte aligned
- Available ring: 2-byte aligned
- Used ring: 4-byte aligned
- Virtqueue start: 4KB aligned

**Allocation Strategy:**
```c
// Allocate page-aligned memory for virtqueue
void *vq_mem = aligned_alloc(4096, vq_size);
memset(vq_mem, 0, vq_size);

// Layout within page
desc = (struct vring_desc *)vq_mem;
avail = (struct vring_avail *)(vq_mem + desc_size);
used = (struct vring_used *)(vq_mem + desc_size + avail_size);
```

### Common Pitfalls

- **Endianness**: VirtIO uses little-endian (match RISC-V)
- **Virtual vs physical**: Forgetting to translate addresses for DMA
- **Alignment**: Improper alignment causes device to reject queue
- **Descriptor leaks**: Not recycling descriptors from used ring
- **Race conditions**: Device updating used ring while driver reads
- **Index wraparound**: `idx % queue_size` for circular indexing

---

## Part 3: Kernel Threading

Preemptive multithreading with synchronization primitives.

### Thread Control Block (TCB)

```c
struct thread {
    uint32_t tid;                   // Thread ID
    enum thread_state state;        // READY, RUNNING, BLOCKED, EXITED
    void *stack;                    // Stack base
    uint32_t sp;                    // Saved stack pointer
    void (*entry)(void *);          // Entry function
    void *arg;                      // Entry argument
    struct thread *next;            // Scheduler queue link
};
```

### Context Switching

**Saved Context** (on thread's stack):
```c
struct context {
    uint32_t ra;                    // Return address
    uint32_t sp;                    // Stack pointer
    uint32_t s0, s1, s2, ..., s11;  // Callee-saved registers
};
```

**Context Switch Assembly:**
```asm
switch_context:
    # Save current context
    addi sp, sp, -64
    sw   ra, 0(sp)
    sw   s0, 4(sp)
    sw   s1, 8(sp)
    # ... save s2-s11 ...

    # Switch stacks: current->sp = sp; sp = next->sp
    sw   sp, TCB_SP_OFFSET(a0)  # Save old SP
    lw   sp, TCB_SP_OFFSET(a1)  # Load new SP

    # Restore new context
    lw   s11, 60(sp)
    # ... restore s1-s10 ...
    lw   s0, 4(sp)
    lw   ra, 0(sp)
    addi sp, sp, 64
    ret
```

### Scheduler

**Round-Robin Queue:**
```c
struct scheduler {
    struct thread *ready_queue;     // Circular linked list
    struct thread *current;
    int num_threads;
};
```

**Scheduling Algorithm:**
```c
void schedule(void) {
    struct thread *prev = current;
    struct thread *next = current->next;

    // Move current to end of queue if still ready
    if (current->state == RUNNING)
        current->state = READY;

    // Find next ready thread
    while (next->state != READY)
        next = next->next;

    next->state = RUNNING;
    current = next;

    switch_context(&prev->sp, &next->sp);
}
```

### Threading API

```c
int thread_create(void (*func)(void *), void *arg);
```
- Allocate TCB and stack (typically 8KB)
- Initialize context on stack (set ra = func, a0 = arg)
- Set state = READY
- Add to scheduler queue
- Return TID

```c
void thread_yield(void);
```
- Voluntarily give up CPU
- Call `schedule()` to switch to next thread

```c
void thread_exit(void);
```
- Set state = EXITED
- Call `schedule()` (never returns)
- Reaper thread reclaims resources

### Synchronization Primitives

**Mutex:**
```c
struct mutex {
    int locked;
    struct thread *wait_queue;
};

void mutex_lock(struct mutex *m) {
    while (__sync_lock_test_and_set(&m->locked, 1)) {
        enqueue(&m->wait_queue, current);
        current->state = BLOCKED;
        schedule();
    }
}

void mutex_unlock(struct mutex *m) {
    if (m->wait_queue) {
        struct thread *t = dequeue(&m->wait_queue);
        t->state = READY;
    }
    __sync_lock_release(&m->locked);
}
```

**Condition Variable (Mesa semantics):**
```c
struct cond {
    struct thread *wait_queue;
};

void cond_wait(struct cond *cv, struct mutex *m) {
    enqueue(&cv->wait_queue, current);
    current->state = BLOCKED;
    mutex_unlock(m);
    schedule();                     // Gives up CPU
    mutex_lock(m);                  // Re-acquire on wakeup
}

void cond_signal(struct cond *cv) {
    if (cv->wait_queue) {
        struct thread *t = dequeue(&cv->wait_queue);
        t->state = READY;
    }
    // Does NOT release mutex or yield
}
```

**Mesa vs Hoare:**
- **Mesa**: Signaler keeps running, waiter wakes when scheduled
- **Hoare**: Signaler immediately switches to waiter

**Why Mesa?**
- Simpler to implement (no immediate context switch)
- Waiter must re-check condition in while loop

```c
// Correct Mesa pattern
mutex_lock(&m);
while (!condition)
    cond_wait(&cv, &m);
// ... critical section ...
mutex_unlock(&m);
```

### Timer Interrupts

**Preemption** requires periodic timer interrupts:

```c
void timer_init(void) {
    // RISC-V: Set mtimecmp for next interrupt
    uint64_t *mtime = (uint64_t *)MTIME_ADDR;
    uint64_t *mtimecmp = (uint64_t *)MTIMECMP_ADDR;
    *mtimecmp = *mtime + TIMER_INTERVAL;

    // Enable timer interrupt
    set_csr(mie, MIE_MTIE);
    set_csr(mstatus, MSTATUS_MIE);
}

void timer_isr(void) {
    // Reset timer
    uint64_t *mtime = (uint64_t *)MTIME_ADDR;
    uint64_t *mtimecmp = (uint64_t *)MTIMECMP_ADDR;
    *mtimecmp = *mtime + TIMER_INTERVAL;

    // Preempt current thread
    schedule();
}
```

### Common Pitfalls

**Context Switching:**
- **Forgetting to save registers**: Corruption when thread resumes
- **Stack overflow**: Insufficient stack size for deep calls
- **Misaligned SP**: RISC-V requires 16-byte stack alignment

**Synchronization:**
- **Forgetting to re-check**: Must use `while (!cond)`, not `if (!cond)`
- **Deadlock**: Thread holding mutex blocked waiting for itself
- **Priority inversion**: Low-priority thread holding lock needed by high-priority
- **Lost wakeup**: Signal before wait (waiter sleeps forever)

**Scheduler:**
- **Queue corruption**: Not handling empty ready queue
- **Never yielding**: CPU-bound thread starves others without preemption
- **Zombie threads**: Not reaping exited threads (memory leak)

---

## Build System

```bash
make                # Build kernel
make run            # Run in QEMU
make debug          # Run with GDB server
make clean          # Remove artifacts
```

### QEMU Arguments
```bash
qemu-system-riscv32 \
    -machine virt \
    -nographic \
    -serial mon:stdio \
    -device virtio-rng-device \
    -kernel kernel.elf
```

### Debugging

```bash
# Terminal 1
make debug

# Terminal 2
riscv32-unknown-elf-gdb kernel.elf
(gdb) target remote :1234
(gdb) break uart_isr
(gdb) continue
```

**Useful GDB Commands:**
- `info threads`: List all threads
- `thread <n>`: Switch to thread n
- `bt`: Backtrace (call stack)
- `x/16x $sp`: Examine stack

---

## Testing Strategy

### UART Testing
1. Echo test: `uart_putc(uart_getc())`
2. Stress test: Fill buffer, verify no drops
3. Interrupt verification: Confirm ISR called

### VirtIO Testing
1. Read 16 bytes, verify non-zero
2. Large buffer (1KB): Check for DMA alignment issues
3. Multiple reads: Ensure descriptor recycling

### Threading Testing
1. Create 10 threads printing TID
2. Mutex test: Shared counter with 100 increments
3. Producer-consumer: Test cond_wait/cond_signal

---

## Code Organization

```
mp2/
├── uart/
│   ├── uart.c          # Driver implementation
│   └── uart.h          # Public interface
├── virtio/
│   ├── virtio.c        # VirtIO entropy driver
│   └── virtio.h
├── thread/
│   ├── thread.c        # Thread management
│   ├── switch.S        # Context switch assembly
│   ├── mutex.c         # Synchronization
│   └── thread.h
├── kernel/
│   ├── main.c          # Kernel entry point
│   ├── interrupt.c     # ISR dispatcher
│   └── mm.c            # Memory allocation
├── Makefile
└── README.md
```

---

## Performance Considerations

- **UART**: Interrupt overhead vs polling trade-off
- **VirtIO**: Batching requests reduces MMIO writes
- **Threading**: Context switch cost ~100 cycles
- **Mutex**: Spinning vs blocking trade-off

---

## References

- **RISC-V Privileged Spec**: Interrupt/exception handling
- **VirtIO Spec 1.1**: Virtqueue protocol
- **UART 16550**: Standard serial controller
