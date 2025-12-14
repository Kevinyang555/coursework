# MP3: Operating System Kernel (KNIT OS)

**ECE 391 Spring 2025** | **Group Project (4-5 students)**

- **CP1:** Friday, Apr 7 (Program Loading & Syscalls)
- **CP2:** Friday, Apr 28 (File System & Scheduling)
- **CP3:** Sunday, May 4 (Integration & Polish)

---

## Overview

Complete OS kernel for RISC-V with processes, threads, file system, virtual memory, and system calls.

---

## Checkpoint 1: Program Loading & System Calls

### ELF Loader
- Parse ELF headers
- Load segments to memory
- Setup stack and registers
- Jump to entry point

### System Calls
```c
int fork(void);
int exec(const char *path);
void exit(int status);
int open(const char *path, int flags);
int read/write/close(int fd, ...);
void *sbrk(intptr_t increment);
```

---

## Checkpoint 2: File System & Scheduling

### KTFS File System
- Inode-based design
- Directory hierarchy
- Block allocation
- File descriptors

### Scheduler
- Round-robin
- Context switching
- Process states
- PCB management

---

## Checkpoint 3: Final Integration

### Deliverables
- Shell (fork/exec/wait)
- Utilities (cat, ls, echo)
- Comprehensive testing
- Documentation

---

## Architecture

```
User Space (U-mode)
  ↕ ecall/sret
Kernel Space (S-mode)
  ├── System Calls
  ├── Process/Thread Mgmt
  ├── Virtual Memory
  ├── File System (KTFS)
  └── Device Drivers
```

---

## Virtual Memory

- 4KB pages
- Sv39 paging (2-level)
- User: 0x00000000-0x7FFFFFFF
- Kernel: 0x80000000-0xFFFFFFFF

---

## Build

```bash
cd src
make
make run
make debug
make fs  # Create filesystem image
```

---

## Team Coordination

**Division of Labor:**
- Person 1: System calls, traps
- Person 2: File system
- Person 3: Process/thread mgmt
- Person 4: Virtual memory, ELF

---

## Grading

| Checkpoint | Points |
|------------|--------|
| CP1: Loading & Syscalls | 30 |
| CP2: FS & Scheduling | 35 |
| CP3: Integration | 35 |

**Bonus:** Networking (+10), User threads (+5), Shell features (+5)

---

MP3 ECE 391 | Spring 2025 | UIUC
