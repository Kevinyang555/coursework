# MP1: RISC-V Assembly & Skyline Graphics

**ECE 391 Spring 2025** | **Individual Assignment**  
**Due:** Friday, February 7 at 18:00

---

## Overview

Skyline screensaver in pure RISC-V assembly. Implement stars (linked list), windows (array), and animated beacon. All graphics rendered to 640x480 frame buffer.

### Learning Objectives
- RISC-V assembly programming
- Stack management & calling conventions
- Data structures in assembly
- Frame buffer graphics

---

## Required Functions (9 total)

### Star Management (Linked List)
1. **skyline_init()** - Initialize globals
2. **add_star(x, y, color)** - Add to list head
3. **remove_star(x, y)** - Remove star
4. **draw_star(fbuf, star)** - Render to frame buffer

### Window Management (Array)
5. **add_window(x, y, w, h, color)** - Add to array
6. **remove_window(x, y)** - Remove and compact array
7. **draw_window(fbuf, window)** - Render with clipping

### Beacon
8. **start_beacon(img, x, y, dia, period, ontime)** - Initialize
9. **draw_beacon(fbuf, t, beacon)** - Animated rendering

---

## Data Structures

```c
struct skyline_star {
    struct skyline_star *next;
    uint16_t x, y, color;
};

struct skyline_window {
    uint16_t x, y;
    uint8_t w, h;
    uint16_t color;
};

struct skyline_beacon {
    const uint16_t *img;
    uint16_t x, y;
    uint8_t dia;
    uint16_t period, ontime;
};
```

---

## RISC-V Calling Convention

| Register | Usage | Saved by |
|----------|-------|----------|
| a0-a7 | Arguments/return | Caller |
| t0-t6 | Temporaries | Caller |
| s0-s11 | Saved registers | Callee |
| ra | Return address | Caller |
| sp | Stack pointer | Callee |

---

## Build & Run

```bash
make
make run
make debug  # For GDB
```

---

## Common Pitfalls

1. Forgetting to save ra before calls
2. Off-by-one in array indexing
3. Not checking malloc() returns
4. Incorrect frame buffer addressing
5. Clipping bugs for off-screen objects

---

## Grading

| Component | Points |
|-----------|--------|
| Star functions | 30 |
| Window functions | 35 |
| Beacon functions | 25 |
| Code style | 10 |
| **Total** | **100** |

---

MP1 ECE 391 | Spring 2025 | UIUC
