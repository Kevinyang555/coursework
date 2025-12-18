# MP1: RISC-V Skyline Screensaver

Pure RISC-V assembly implementation of an animated skyline screensaver with stars, windows, and a rotating beacon. Demonstrates low-level programming, data structure management in assembly, and frame buffer graphics.

## Architecture Overview

The skyline system manages three types of graphical objects rendered to a 640×480 frame buffer:
- **Stars**: Stored in a linked list for dynamic allocation
- **Windows**: Stored in a fixed-size array with compaction
- **Beacon**: Singleton animated object with rotation

All memory management and rendering implemented in RISC-V assembly without C library dependencies.

## Data Structures

### Star (Linked List Node)
```c
struct skyline_star {
    struct skyline_star *next;  // Next star in list (NULL if tail)
    uint16_t x, y;              // Screen coordinates
    uint16_t color;             // RGB565 color
};
```

- Head pointer stored in global variable
- New stars inserted at head (O(1))
- Removal requires traversal (O(n))
- Each star rendered as a single pixel

### Window (Array Element)
```c
struct skyline_window {
    uint16_t x, y;              // Top-left corner
    uint8_t w, h;               // Width and height (pixels)
    uint16_t color;             // RGB565 fill color
};
```

- Fixed array of MAX_WINDOWS entries
- Removal requires compaction (shift remaining elements)
- Rendered as filled rectangles with edge clipping

### Beacon (Singleton)
```c
struct skyline_beacon {
    const uint16_t *img;        // Pointer to sprite data
    uint16_t x, y;              // Center coordinates
    uint8_t dia;                // Diameter in pixels
    uint16_t period;            // Animation period (frames)
    uint16_t ontime;            // Visible duration (frames)
};
```

- Rotates through sprite frames based on time
- Visibility controlled by ontime/period ratio
- Center-based rendering with sprite offset

## API Reference

### Initialization
```asm
skyline_init()
```
Initialize global state (linked list head, window array count, beacon state). Call before any other functions.

### Star Management
```asm
add_star(a0: uint16_t x, a1: uint16_t y, a2: uint16_t color) -> a0: int
```
- Allocate new star node via `malloc()`
- Insert at head of linked list
- Return 1 on success, 0 if malloc fails

```asm
remove_star(a0: uint16_t x, a1: uint16_t y) -> a0: int
```
- Search list for star at (x, y)
- Unlink node and call `free()`
- Return 1 if found and removed, 0 if not found

```asm
draw_star(a0: uint16_t *fbuf, a1: skyline_star *star)
```
- Render star as single pixel: `fbuf[y*640 + x] = color`
- No bounds checking (caller must ensure valid coordinates)

### Window Management
```asm
add_window(a0: uint16_t x, a1: uint16_t y, a2: uint8_t w, a3: uint8_t h, a4: uint16_t color) -> a0: int
```
- Add window to array if space available
- Return 1 on success, 0 if array full

```asm
remove_window(a0: uint16_t x, a1: uint16_t y) -> a0: int
```
- Find window at (x, y)
- Compact array by shifting later elements left
- Return 1 if found, 0 otherwise

```asm
draw_window(a0: uint16_t *fbuf, a1: skyline_window *window)
```
- Render filled rectangle
- Clip to screen bounds (0 ≤ x < 640, 0 ≤ y < 480)
- Iterate: `fbuf[(y+j)*640 + (x+i)] = color` for all pixels in bounds

### Beacon Management
```asm
start_beacon(a0: uint16_t *img, a1: uint16_t x, a2: uint16_t y, a3: uint8_t dia, a4: uint16_t period, a5: uint16_t ontime)
```
- Initialize beacon singleton with sprite and animation parameters
- `img` points to sprite array (dia×dia pixels per frame)

```asm
draw_beacon(a0: uint16_t *fbuf, a1: uint16_t t, a2: skyline_beacon *beacon)
```
- Calculate visibility: visible if `(t % period) < ontime`
- Select sprite frame based on rotation: `frame = (t / period) % num_frames`
- Render dia×dia sprite centered at (x, y)

## Frame Buffer Format

**Resolution:** 640×480 pixels
**Color Format:** RGB565 (16-bit per pixel)
```
Bit layout: [R4 R3 R2 R1 R0 | G5 G4 G3 G2 G1 G0 | B4 B3 B2 B1 B0]
```

**Addressing:**
```asm
pixel_offset = (y * 640 + x) * 2  # Byte offset
fbuf[pixel_offset] = color         # Write 16-bit color
```

**Coordinate System:** Origin (0,0) at top-left, x increases right, y increases down

## RISC-V Calling Convention (RV32I)

### Register Usage
| Register | ABI Name | Purpose | Preserved Across Calls |
|----------|----------|---------|------------------------|
| x0 | zero | Constant 0 | N/A |
| x1 | ra | Return address | Caller-saved |
| x2 | sp | Stack pointer | Callee-saved |
| x8 | s0/fp | Frame pointer | Callee-saved |
| x9-x18 | s1-s11 | Saved registers | Callee-saved |
| x10-x11 | a0-a1 | Arguments/return values | Caller-saved |
| x12-x17 | a2-a7 | Arguments | Caller-saved |
| x5-x7, x28-x31 | t0-t6 | Temporaries | Caller-saved |

### Stack Frame Layout
```
High Addresses
    ┌──────────────┐
    │ Return addr  │ ← Pushed by caller before jal
    ├──────────────┤
    │ Saved s0-s11 │ ← Callee saves if used
    ├──────────────┤
    │ Local vars   │
    ├──────────────┤
    │ Spilled temps│
    └──────────────┘ ← sp (grows downward)
Low Addresses
```

### Function Prologue/Epilogue Pattern
```asm
function_name:
    # Prologue
    addi sp, sp, -16        # Allocate stack frame
    sw   ra, 12(sp)         # Save return address
    sw   s0, 8(sp)          # Save callee-saved registers

    # ... function body ...

    # Epilogue
    lw   s0, 8(sp)          # Restore callee-saved registers
    lw   ra, 12(sp)         # Restore return address
    addi sp, sp, 16         # Deallocate stack frame
    ret                     # Return (jr ra)
```

## Build System

### Compilation
```bash
make                # Build skyline.elf
make run            # Run in QEMU
make debug          # Run with GDB server on :1234
make clean          # Remove build artifacts
```

### Makefile Targets
- **skyline.elf**: Main executable (linked with runtime library)
- **skyline.o**: Assembled object file from skyline.S
- **run**: Execute in QEMU RISC-V emulator
- **debug**: Launch QEMU with GDB remote debugging

### Debugging with GDB
```bash
# Terminal 1
make debug

# Terminal 2
riscv32-unknown-elf-gdb skyline.elf
(gdb) target remote :1234
(gdb) break add_star
(gdb) continue
```

## Implementation Notes

### Memory Allocation
- Use `malloc()` for dynamic star allocation
- **Always check return value**: `malloc()` returns NULL on failure
- Call `free()` when removing stars to prevent leaks

### Linked List Operations
**Insertion at head:**
```asm
# new_star->next = head
sw   t0, 0(a0)      # t0 = old head, a0 = new star
# head = new_star
la   t1, star_head
sw   a0, 0(t1)
```

**Removal with traversal:**
```asm
# Handle head removal separately
# For middle/tail: prev->next = current->next
lw   t2, 0(t0)      # t2 = current->next
sw   t2, 0(t1)      # prev->next = current->next
```

### Array Compaction
When removing window at index i:
```asm
# Shift windows[i+1..count-1] left by 1
for j = i to count-2:
    windows[j] = windows[j+1]
count--
```

### Clipping Algorithm
```asm
# Clip rectangle to screen bounds
clip_x_start = max(0, window.x)
clip_x_end = min(640, window.x + window.w)
clip_y_start = max(0, window.y)
clip_y_end = min(480, window.y + window.h)

# Only draw if clipped region is non-empty
```

### Beacon Animation
```asm
# Frame selection
frame_index = (t / period) % total_frames
sprite_offset = frame_index * dia * dia * 2  # 2 bytes per pixel

# Visibility test
phase = t % period
visible = (phase < ontime) ? 1 : 0
```

## Common Pitfalls

### Stack Management Errors
- **Forgetting to save `ra`**: Causes return to wrong address after `jal`
- **Mismatched stack allocation**: `addi sp, sp, -16` must match epilogue `+16`
- **Wrong offset calculations**: Remember `sw/lw` use byte offsets, not word indices

### Pointer Arithmetic
- **Incorrect scaling**: Array indexing requires multiplication by element size
- **Frame buffer addressing**: `pixel_addr = base + (y*640 + x)*2` (2 bytes/pixel)
- **Structure field access**: Use correct byte offsets (x=0, y=2, color=4 for star)

### Boundary Conditions
- **Off-by-one in loops**: Use `blt` (less than) vs `ble` (less than or equal) carefully
- **Missing null checks**: Always verify `malloc()` didn't return NULL
- **Array bounds**: Check `window_count < MAX_WINDOWS` before adding

### Register Conflicts
- **Overwriting arguments**: Don't modify `a0-a7` before using them
- **Not saving temporaries**: If calling another function, save `t0-t6` first
- **Corrupting saved registers**: `s0-s11` must be preserved by callee

### Clipping Bugs
- **Negative coordinates**: Can cause wraparound if cast to unsigned
- **Partial rendering**: Beacon/window edge cases when centered near border
- **Integer overflow**: Large `x + w` may exceed 16-bit range

## Testing Strategy

### Unit Testing
1. **Star list**: Test add, remove, draw with various coordinates
2. **Window array**: Test full array, compaction, clipping edge cases
3. **Beacon**: Test animation timing, rotation, visibility toggling

### Edge Cases
- Malloc failure (simulate by limiting heap)
- Empty linked list removal
- Full window array
- Off-screen rendering (negative coords, > 640/480)
- Beacon at screen edges

### Visual Verification
- Clear frame buffer to black
- Add colored stars at known positions
- Render windows with distinct colors
- Verify beacon appears/disappears at correct timing

## Performance Considerations

- **Star traversal**: O(n) for removal - consider keeping tail pointer
- **Window compaction**: O(n) shift - unavoidable with array
- **Frame buffer writes**: Each pixel is 2 sequential byte writes
- **Beacon rendering**: Most expensive (dia² pixels per frame)

## Code Organization

```
mp1/
├── skyline.S           # Main assembly implementation
├── runtime.c           # C runtime support (malloc/free)
├── Makefile           # Build configuration
└── README.md          # This file
```

All required functions must be implemented in `skyline.S` using only RISC-V assembly.
