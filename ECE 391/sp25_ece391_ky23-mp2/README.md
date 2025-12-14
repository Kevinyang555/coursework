# MP2: Device Drivers & Interrupts

**ECE 391 Spring 2025** | **Individual Assignment** | **3 Checkpoints**

- **CP1:** Friday, Feb 21 (UART Driver)
- **CP2:** Friday, Feb 28 (VirtIO Entropy)
- **CP3:** Friday, Mar 7 (Kernel Threads)

---

## Checkpoint 1: UART Serial Driver

Interrupt-based UART driver with circular buffers.

### Key Components
- ISR for RX/TX interrupts
- Circular buffers for async I/O
- Blocking uart_getc()
- Non-blocking uart_putc()

---

## Checkpoint 2: VirtIO Entropy Device

VirtIO interface for hardware RNG.

### VirtIO Concepts
- Virtqueues (ring buffers)
- Descriptor tables
- DMA buffer management
- Available/Used rings

---

## Checkpoint 3: Kernel Threads

Threading primitives and scheduler.

### Threading API
```c
int thread_create(void (*func)(void*), void *arg);
void thread_yield(void);
void thread_exit(void);

void cond_wait(cond_t *cv, mutex_t *lock);
void cond_signal(cond_t *cv);
```

### Implementation
- Context switching (assembly)
- Round-robin scheduling
- Condition variables (Mesa semantics)

---

## Build & Test

```bash
make
make run
make debug
```

---

## Common Pitfalls

1. Race conditions in ISRs
2. Circular buffer full/empty bugs
3. VirtIO descriptor leaks
4. Missing register saves in context switch
5. Deadlocks with mutexes

---

## Grading

| Checkpoint | Points |
|------------|--------|
| CP1: UART | 33 |
| CP2: VirtIO | 33 |
| CP3: Threads | 34 |

---

MP2 ECE 391 | Spring 2025 | UIUC
