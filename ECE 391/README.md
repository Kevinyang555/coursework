# ECE 391: Computer Systems Engineering

**Spring 2025** | **University of Illinois Urbana-Champaign**

## Course Information

**Instructor:** Kirill Levchenko  
**Credits:** 4 credit hours  
**Prerequisites:** ECE 220 and CS 225  
**Architecture:** RISC-V (64-bit)

### Official Description

Design and implementation of computer systems components including bootloaders, device drivers, file systems, and system calls. Introduction to the concepts and abstractions central to the development of modern computing systems.

### Course Website
https://courses.grainger.illinois.edu/ECE391/fa2025/

---

## Course Topics

- Computer Organization & RISC-V Assembly
- Data Structures (queues, heaps, stacks, linked lists)
- Synchronization (mutexes, semaphores, race conditions)
- Interrupts & Exception Handling
- Virtual Memory & Paging
- Device Programming & Drivers
- File Systems (disk layout, inodes)
- I/O Interface & Memory Mapping
- Networking (sockets, protocols)

---

## Machine Problems

### [MP1: RISC-V Assembly & Graphics](./sp25_ece391_ky23-mp1/)
**Individual Project** | Due: Friday, February 7 at 18:00

Skyline screensaver in pure RISC-V assembly. Implement linked lists, arrays, and frame buffer graphics.

**Topics:** Assembly programming, stack management, data structures, graphics rendering

---

### [MP2: Device Drivers & Interrupts](./sp25_ece391_ky23-mp2/)
**Individual Project** | 3 Checkpoints (Feb 21, Feb 28, Mar 7)

UART driver, VirtIO entropy device, kernel threading.

**Topics:** Interrupt handling, device drivers, VirtIO, condition variables

---

### [MP3: Operating System Kernel](./sp25_ece391_group_24-mp3/)
**Group Project (4-5 students)** | 3 Checkpoints (Apr 7, Apr 28, May 4)

Complete OS kernel (KNIT OS) with processes, file system, virtual memory, system calls.

**Topics:** ELF loading, system calls, scheduling, file system, virtual memory

---

## Development Environment

**Tools:**
- RISC-V Toolchain (riscv64-unknown-elf-gcc)
- QEMU (qemu-system-riscv64)
- GDB (debugging)

**Hardware Target:**
- RISC-V 64-bit (RV64) architecture
- VirtIO devices
- CLINT timer
- PLIC interrupt controller

---

## Key Differences from Traditional x86 Course

| Aspect | Traditional | Spring 2025 |
|--------|-------------|-------------|
| Architecture | x86 (32-bit) | RISC-V (64-bit) |
| Assembly | x86 AT&T | RISC-V |
| Interrupt Controller | 8259 PIC | PLIC |
| Timer | PIT (8254) | CLINT |
| Devices | Real hardware | VirtIO |

---

## Assessment

- MP1: 15%
- MP2: 20%
- MP3: 25%
- Exams: 30%
- Quizzes: 10%

---

ECE 391 @ UIUC | Spring 2025
