# MP3: KNIT Operating System Kernel

Complete RISC-V operating system kernel with processes, threads, virtual memory, file system, and system calls. KNIT (Kernel for Networked Interactive Tasks) provides a Unix-like environment with ELF program loading, scheduling, and POSIX-style I/O.

## System Architecture

```
┌─────────────────────────────────────────────┐
│         User Space (U-mode)                 │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │  Shell   │  │   cat    │  │   ls     │  │
│  └──────────┘  └──────────┘  └──────────┘  │
└─────────────────────────────────────────────┘
              ↕ ecall / sret
┌─────────────────────────────────────────────┐
│      Kernel Space (S-mode)                  │
│  ┌─────────────────────────────────────┐    │
│  │      System Call Interface          │    │
│  ├──────────────┬──────────────────────┤    │
│  │   Process    │   Virtual Memory     │    │
│  │  Management  │      (Sv39)          │    │
│  ├──────────────┼──────────────────────┤    │
│  │  Scheduler   │   File System (KTFS) │    │
│  ├──────────────┴──────────────────────┤    │
│  │       Device Drivers (UART, etc)    │    │
│  └─────────────────────────────────────┘    │
└─────────────────────────────────────────────┘
              ↕ MMIO / Interrupts
┌─────────────────────────────────────────────┐
│            Hardware (QEMU virt)             │
└─────────────────────────────────────────────┘
```

---

## Part 1: ELF Loading & System Calls

### ELF Binary Format

**Executable and Linkable Format** (ELF) structure:

```c
struct elf32_header {
    uint8_t  e_ident[16];      // Magic: 0x7F 'E' 'L' 'F'
    uint16_t e_type;           // ET_EXEC = 2
    uint16_t e_machine;        // EM_RISCV = 0xF3
    uint32_t e_version;
    uint32_t e_entry;          // Entry point address
    uint32_t e_phoff;          // Program header offset
    uint32_t e_shoff;          // Section header offset
    uint32_t e_flags;
    uint16_t e_ehsize;         // ELF header size
    uint16_t e_phentsize;      // Program header entry size
    uint16_t e_phnum;          // Number of program headers
    // ...
};

struct elf32_phdr {
    uint32_t p_type;           // PT_LOAD = 1
    uint32_t p_offset;         // Offset in file
    uint32_t p_vaddr;          // Virtual address
    uint32_t p_paddr;          // Physical address (ignored)
    uint32_t p_filesz;         // Size in file
    uint32_t p_memsz;          // Size in memory (>= filesz)
    uint32_t p_flags;          // PF_R|PF_W|PF_X
    uint32_t p_align;          // Alignment
};
```

### ELF Loader Implementation

```c
int load_elf(const char *path, uintptr_t *entry_point);
```

**Loading Steps:**
1. Open file and read ELF header
2. Verify magic number: `\x7FELF`
3. Verify architecture: `e_machine == EM_RISCV`
4. Read program headers from `e_phoff`
5. For each `PT_LOAD` segment:
   - Allocate pages for `[p_vaddr, p_vaddr + p_memsz)`
   - Read `p_filesz` bytes from file offset `p_offset`
   - Zero fill remaining `p_memsz - p_filesz` bytes (BSS section)
   - Set page permissions based on `p_flags`
6. Setup user stack (typically 8MB at high memory)
7. Return entry point: `*entry_point = ehdr.e_entry`

**Page Permissions:**
```c
int flags = 0;
if (phdr.p_flags & PF_R) flags |= PTE_R;
if (phdr.p_flags & PF_W) flags |= PTE_W;
if (phdr.p_flags & PF_X) flags |= PTE_X;
flags |= PTE_U;  // User-accessible
```

### System Call Interface

**Trap Handling:**
```c
void trap_handler(struct trap_frame *tf) {
    if (tf->cause == CAUSE_ECALL_U) {
        // System call: number in a7, args in a0-a6
        tf->epc += 4;  // Skip ecall instruction
        int syscall_num = tf->a7;
        tf->a0 = syscall_dispatch(syscall_num, tf);
    } else if (tf->cause == CAUSE_PAGE_FAULT) {
        handle_page_fault(tf);
    } else {
        // Unhandled exception
        process_kill(current);
    }
}
```

**System Call Numbers:**
```c
#define SYS_fork        1
#define SYS_exec        2
#define SYS_exit        3
#define SYS_wait        4
#define SYS_open        5
#define SYS_close       6
#define SYS_read        7
#define SYS_write       8
#define SYS_sbrk        9
#define SYS_getpid      10
#define SYS_sleep       11
```

### Process Management

**Process Control Block (PCB):**
```c
struct process {
    int pid;
    enum proc_state state;          // RUNNING, READY, BLOCKED, ZOMBIE
    struct process *parent;
    struct page_table *pgdir;       // Page table root
    struct trap_frame *tf;          // Saved trap frame
    void *kernel_stack;             // Kernel stack (for syscalls)
    uint32_t brk;                   // Heap break address
    struct file *files[MAX_FDS];    // Open file table
    struct process *next;           // Scheduler queue
    int exit_status;
};
```

### System Call Implementations

#### fork()
```c
int sys_fork(void) {
    struct process *child = alloc_process();
    child->parent = current;
    child->pid = alloc_pid();

    // Copy page table (copy-on-write)
    child->pgdir = copy_pgdir(current->pgdir);

    // Copy trap frame
    *child->tf = *current->tf;
    child->tf->a0 = 0;  // Child returns 0

    // Copy file descriptors (increment refcounts)
    for (int i = 0; i < MAX_FDS; i++) {
        if (current->files[i]) {
            child->files[i] = current->files[i];
            file_ref_inc(child->files[i]);
        }
    }

    // Add to ready queue
    child->state = READY;
    scheduler_enqueue(child);

    return child->pid;  // Parent returns child PID
}
```

#### exec()
```c
int sys_exec(const char *path) {
    // Load new program
    uintptr_t entry;
    if (load_elf(path, &entry) < 0)
        return -1;

    // Free old page table (except kernel mappings)
    free_user_pages(current->pgdir);

    // Close CLOEXEC files
    for (int i = 0; i < MAX_FDS; i++) {
        if (current->files[i] && (current->files[i]->flags & O_CLOEXEC)) {
            file_close(current->files[i]);
            current->files[i] = NULL;
        }
    }

    // Setup new trap frame
    memset(current->tf, 0, sizeof(*current->tf));
    current->tf->epc = entry;
    current->tf->sp = USER_STACK_TOP;

    return 0;  // Never returns on success
}
```

#### exit()
```c
void sys_exit(int status) {
    current->exit_status = status;
    current->state = ZOMBIE;

    // Close all files
    for (int i = 0; i < MAX_FDS; i++) {
        if (current->files[i]) {
            file_close(current->files[i]);
            current->files[i] = NULL;
        }
    }

    // Reparent children to init
    for (struct process *p = process_list; p; p = p->next) {
        if (p->parent == current)
            p->parent = init_process;
    }

    // Wake up parent if waiting
    if (current->parent->state == WAITING)
        current->parent->state = READY;

    // Never returns - scheduler finds next process
    schedule();
}
```

#### wait()
```c
int sys_wait(int *status) {
    // Check if any zombie children exist
    while (1) {
        struct process *zombie = NULL;
        for (struct process *p = process_list; p; p = p->next) {
            if (p->parent == current && p->state == ZOMBIE) {
                zombie = p;
                break;
            }
        }

        if (zombie) {
            int pid = zombie->pid;
            if (status)
                *status = zombie->exit_status;
            free_process(zombie);
            return pid;
        }

        // No zombies - check if any children exist
        int has_children = 0;
        for (struct process *p = process_list; p; p = p->next) {
            if (p->parent == current) {
                has_children = 1;
                break;
            }
        }

        if (!has_children)
            return -1;  // No children

        // Sleep until child exits
        current->state = WAITING;
        schedule();
    }
}
```

#### sbrk()
```c
void *sys_sbrk(intptr_t increment) {
    uint32_t old_brk = current->brk;
    uint32_t new_brk = old_brk + increment;

    if (increment > 0) {
        // Grow heap
        uint32_t old_pg = PGROUNDUP(old_brk);
        uint32_t new_pg = PGROUNDUP(new_brk);
        for (uint32_t a = old_pg; a < new_pg; a += PGSIZE) {
            void *mem = kalloc();
            if (!mem || map_page(current->pgdir, a, mem, PTE_R|PTE_W|PTE_U) < 0) {
                // Rollback
                for (uint32_t b = old_pg; b < a; b += PGSIZE)
                    unmap_page(current->pgdir, b);
                return (void *)-1;
            }
        }
    } else if (increment < 0) {
        // Shrink heap
        uint32_t new_pg = PGROUNDUP(new_brk);
        uint32_t old_pg = PGROUNDUP(old_brk);
        for (uint32_t a = new_pg; a < old_pg; a += PGSIZE)
            unmap_page(current->pgdir, a);
    }

    current->brk = new_brk;
    return (void *)old_brk;
}
```

---

## Part 2: File System & Scheduling

### KTFS File System Design

**KTFS** (KNIT File System) is a simple Unix-like file system with inodes.

**On-Disk Layout:**
```
┌──────────────┬──────────┬────────────┬───────────────┐
│ Superblock   │  Inodes  │  Bitmaps   │  Data Blocks  │
│  (1 block)   │          │ (free map) │               │
└──────────────┴──────────┴────────────┴───────────────┘
Block 0        Block 1    Block N      Block M
```

**Superblock:**
```c
struct superblock {
    uint32_t magic;           // KTFS_MAGIC = 0x4B544653
    uint32_t num_blocks;      // Total blocks
    uint32_t num_inodes;      // Total inodes
    uint32_t inode_start;     // First inode block
    uint32_t bitmap_start;    // Free block bitmap start
    uint32_t data_start;      // First data block
    uint32_t block_size;      // Typically 4096
};
```

**Inode:**
```c
#define NDIRECT 12
#define NINDIRECT (BLOCK_SIZE / 4)

struct inode {
    uint16_t type;            // T_FILE, T_DIR, T_DEV
    uint16_t major;           // Device number (for T_DEV)
    uint16_t minor;
    uint16_t nlink;           // Hard link count
    uint32_t size;            // File size in bytes
    uint32_t direct[NDIRECT]; // Direct block pointers
    uint32_t indirect;        // Indirect block pointer
};
```

**Maximum File Size:**
- Direct blocks: `12 * 4096 = 49152` bytes
- Indirect blocks: `1024 * 4096 = 4194304` bytes
- **Total: ~4.2 MB**

**Directory Entry:**
```c
#define DIRENT_SIZE 32
#define NAME_MAX 28

struct dirent {
    uint32_t inum;            // Inode number (0 = empty)
    char name[NAME_MAX];      // Null-terminated filename
};
```

### File System Operations

**Pathname Resolution:**
```c
struct inode *namei(const char *path) {
    struct inode *ip = (path[0] == '/') ? root_inode : current->cwd;
    inode_ref(ip);

    char name[NAME_MAX];
    while ((path = skipelem(path, name)) != NULL) {
        if (ip->type != T_DIR) {
            inode_put(ip);
            return NULL;
        }

        struct inode *next = dirlookup(ip, name);
        inode_put(ip);
        if (!next)
            return NULL;
        ip = next;
    }
    return ip;
}
```

**Directory Lookup:**
```c
struct inode *dirlookup(struct inode *dp, const char *name) {
    for (uint32_t off = 0; off < dp->size; off += sizeof(struct dirent)) {
        struct dirent de;
        if (readi(dp, &de, off, sizeof(de)) != sizeof(de))
            return NULL;
        if (de.inum && strcmp(de.name, name) == 0)
            return inode_get(de.inum);
    }
    return NULL;
}
```

**Block Allocation:**
```c
uint32_t balloc(void) {
    // Find first free bit in bitmap
    for (uint32_t b = sb.data_start; b < sb.num_blocks; b++) {
        uint32_t byte = b / 8;
        uint32_t bit = b % 8;
        if (!(bitmap[byte] & (1 << bit))) {
            bitmap[byte] |= (1 << bit);
            write_bitmap_block(byte / BLOCK_SIZE);
            memset(block_buffer, 0, BLOCK_SIZE);
            write_block(b, block_buffer);
            return b;
        }
    }
    return 0;  // Out of space
}
```

**File Descriptor Table:**
```c
struct file {
    enum file_type type;      // FD_INODE, FD_PIPE, FD_DEVICE
    int ref;                  // Reference count
    char readable;
    char writable;
    struct inode *ip;         // For FD_INODE
    uint32_t offset;          // Read/write position
};

// Global file table (shared across processes)
struct file file_table[MAX_FILES];
```

### File System System Calls

```c
int sys_open(const char *path, int flags);
```
- Resolve pathname to inode
- Allocate file descriptor in process table
- Allocate file structure in global file table
- Set readable/writable based on flags
- Initialize offset to 0 (or end if `O_APPEND`)

```c
int sys_read(int fd, void *buf, size_t n);
```
- Validate fd, check readable
- Call `readi(file->ip, buf, file->offset, n)`
- Increment `file->offset` by bytes read
- Return bytes read (0 on EOF)

```c
int sys_write(int fd, const void *buf, size_t n);
```
- Validate fd, check writable
- Call `writei(file->ip, buf, file->offset, n)`
- Increment `file->offset` by bytes written
- Update inode size if necessary
- Return bytes written

```c
int sys_close(int fd);
```
- Validate fd
- Decrement file refcount
- If refcount reaches 0: release file, decrement inode refcount
- Clear process file table entry
- Return 0

### Scheduler Design

**Round-Robin with Priority:**
```c
struct scheduler {
    struct process *queue[NUM_PRIORITIES];
    uint32_t ticks;
};

void schedule(void) {
    // Find highest priority non-empty queue
    for (int prio = 0; prio < NUM_PRIORITIES; prio++) {
        struct process *p = scheduler.queue[prio];
        if (!p) continue;

        // Find next ready process in this priority level
        struct process *start = p;
        do {
            p = p->next;
            if (p->state == READY) {
                p->state = RUNNING;
                current = p;
                switch_to(p);
                return;
            }
        } while (p != start);
    }

    // No ready processes - idle
    idle();
}
```

**Context Switch:**
```c
void switch_to(struct process *p) {
    // Save current process state (if not NULL)
    if (current) {
        current->tf = trap_frame_save();
        current->state = READY;
    }

    // Load new process state
    current = p;
    p->state = RUNNING;

    // Switch page table
    write_csr(satp, SATP_MODE_SV32 | (p->pgdir >> 12));
    sfence_vma();

    // Restore trap frame and return to userspace
    trap_frame_restore(p->tf);
}
```

---

## Part 3: Virtual Memory

### Sv39 Paging (2-Level for RV32)

**Virtual Address Layout (32-bit):**
```
 31        22 21        12 11           0
┌───────────┬────────────┬──────────────┐
│   VPN[1]  │   VPN[0]   │    Offset    │
│  (10 bits)│  (10 bits) │   (12 bits)  │
└───────────┴────────────┴──────────────┘
```

**Page Table Entry (PTE):**
```
 31                 10 9  8  7  6  5  4  3  2  1  0
┌────────────────────┬───┬──┬──┬──┬──┬──┬──┬──┬──┬──┐
│     PPN (20 bits)  │RSW│D │A │G │U │X │W │R │V │
└────────────────────┴───┴──┴──┴──┴──┴──┴──┴──┴──┴──┘
```

**PTE Flags:**
- `V` (Valid): PTE is valid
- `R` (Read): Page is readable
- `W` (Write): Page is writable
- `X` (Execute): Page is executable
- `U` (User): Page accessible in U-mode
- `A` (Accessed): Hardware sets on access
- `D` (Dirty): Hardware sets on write
- `G` (Global): Mapping valid for all address spaces

### Page Table Operations

**Walk Page Table:**
```c
pte_t *walk(pagetable_t pgdir, uint32_t va, int alloc) {
    for (int level = 1; level >= 0; level--) {
        pte_t *pte = &pgdir[VPN(va, level)];
        if (*pte & PTE_V) {
            pgdir = (pagetable_t)PTE2PA(*pte);
        } else {
            if (!alloc)
                return NULL;
            pgdir = (pagetable_t)kalloc();
            if (!pgdir)
                return NULL;
            memset(pgdir, 0, PGSIZE);
            *pte = PA2PTE(pgdir) | PTE_V;
        }
    }
    return &pgdir[VPN(va, 0)];
}
```

**Map Page:**
```c
int map_page(pagetable_t pgdir, uint32_t va, uint32_t pa, int perm) {
    pte_t *pte = walk(pgdir, va, 1);
    if (!pte)
        return -1;
    if (*pte & PTE_V)
        panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    return 0;
}
```

**Copy Page Table (for fork):**
```c
pagetable_t copy_pgdir(pagetable_t old) {
    pagetable_t new = (pagetable_t)kalloc();
    if (!new)
        return NULL;
    memset(new, 0, PGSIZE);

    // Copy user mappings
    for (uint32_t va = 0; va < USER_TOP; va += PGSIZE) {
        pte_t *pte = walk(old, va, 0);
        if (!pte || !(*pte & PTE_V))
            continue;
        if (*pte & (PTE_R|PTE_W|PTE_X)) {
            // Leaf PTE - copy page
            uint32_t pa = PTE2PA(*pte);
            uint32_t flags = PTE_FLAGS(*pte);
            void *mem = kalloc();
            if (!mem) {
                free_pgdir(new);
                return NULL;
            }
            memmove(mem, (void *)pa, PGSIZE);
            map_page(new, va, (uint32_t)mem, flags);
        }
    }

    return new;
}
```

### Memory Layout

```
0xFFFFFFFF ┌─────────────────────┐
           │   Kernel (direct)   │
0x80000000 ├─────────────────────┤
           │   User Heap (sbrk)  │
           ├─────────────────────┤
           │   User Code/Data    │
0x10000    ├─────────────────────┤
           │    (unmapped)       │
0x00001000 ├─────────────────────┤
           │  Null guard page    │
0x00000000 └─────────────────────┘

User stack grows downward from 0x80000000
```

### Page Fault Handling

```c
void handle_page_fault(struct trap_frame *tf) {
    uint32_t va = read_csr(stval);  // Faulting address
    uint32_t cause = tf->cause;

    if (cause == CAUSE_LOAD_PAGE_FAULT ||
        cause == CAUSE_STORE_PAGE_FAULT ||
        cause == CAUSE_FETCH_PAGE_FAULT) {

        // Check if address is in valid range
        if (va >= current->brk || va < USER_BASE) {
            printf("Segmentation fault\n");
            process_kill(current);
            return;
        }

        // Lazy allocation - allocate page on demand
        void *mem = kalloc();
        if (!mem || map_page(current->pgdir, PGROUNDDOWN(va), (uint32_t)mem, PTE_R|PTE_W|PTE_U) < 0) {
            process_kill(current);
            return;
        }
        memset(mem, 0, PGSIZE);
    } else {
        printf("Unhandled page fault\n");
        process_kill(current);
    }
}
```

---

## Shell Implementation

**Simple Shell Features:**
- Command parsing (whitespace tokenization)
- Built-in commands: `cd`, `exit`
- External program execution via `fork()` + `exec()`
- I/O redirection: `>`, `<`
- Pipes: `|`
- Background execution: `&`

```c
int main(void) {
    char buf[512];
    while (1) {
        printf("$ ");
        if (fgets(buf, sizeof(buf), stdin) == NULL)
            break;

        if (buf[0] == 0 || buf[0] == '\n')
            continue;

        // Parse command
        char *argv[MAX_ARGS];
        int argc = parse_cmd(buf, argv);
        if (argc == 0)
            continue;

        // Built-in commands
        if (strcmp(argv[0], "cd") == 0) {
            chdir(argv[1]);
            continue;
        }
        if (strcmp(argv[0], "exit") == 0) {
            exit(0);
        }

        // Fork and exec
        int pid = fork();
        if (pid == 0) {
            // Child
            exec(argv[0], argv);
            printf("exec failed\n");
            exit(1);
        } else {
            // Parent
            wait(NULL);
        }
    }
    return 0;
}
```

**Pipe Implementation:**
```c
// Command: cat file.txt | grep pattern
int pipe_fds[2];
pipe(pipe_fds);

if (fork() == 0) {
    // Left side: cat file.txt
    close(pipe_fds[0]);           // Close read end
    dup2(pipe_fds[1], STDOUT);    // Redirect stdout to pipe
    close(pipe_fds[1]);
    exec("cat", "file.txt");
    exit(1);
}

if (fork() == 0) {
    // Right side: grep pattern
    close(pipe_fds[1]);           // Close write end
    dup2(pipe_fds[0], STDIN);     // Redirect stdin from pipe
    close(pipe_fds[0]);
    exec("grep", "pattern");
    exit(1);
}

close(pipe_fds[0]);
close(pipe_fds[1]);
wait(NULL);
wait(NULL);
```

---

## Build System

```bash
# Build kernel and filesystem
make

# Create filesystem image
make fs

# Run in QEMU
make run

# Debug with GDB
make debug
```

### Filesystem Image Creation

```bash
# mkfs utility creates KTFS image
./mkfs fs.img \
    bin/init \
    bin/sh \
    bin/cat \
    bin/ls \
    bin/echo \
    README.txt
```

---

## Testing Strategy

### Unit Tests
1. **ELF Loader**: Load and execute simple "hello world" binary
2. **System Calls**: Test fork returns different values to parent/child
3. **File System**: Create/read/write/delete files
4. **Scheduler**: Create multiple processes, verify round-robin

### Integration Tests
1. **Shell**: Run interactive shell, execute commands
2. **Pipes**: `cat | grep` pipeline
3. **Redirection**: `cat > output.txt`
4. **Multi-process**: Fork bomb (limit processes!)

### Stress Tests
1. **File System**: Create 1000 files, fill disk
2. **Processes**: Fork many processes (test PID allocation)
3. **Memory**: sbrk() until OOM

---

## Common Pitfalls

**ELF Loading:**
- Not zero-filling BSS segment
- Incorrect page permissions (missing `PTE_U`)
- Stack not aligned to 16 bytes
- Entry point in wrong address space

**System Calls:**
- Not incrementing `epc` after ecall
- Forgetting to copy strings from user space
- User pointers in kernel mode (page fault)

**File System:**
- Not updating inode size after write
- Bitmap corruption (double free)
- Directory entry name not null-terminated
- Inode refcount leaks

**Virtual Memory:**
- Not calling `sfence.vma` after page table change
- Mapping kernel pages with `PTE_U`
- Off-by-one in va-to-pte conversion
- Forgetting to free page tables on exit

**Scheduler:**
- Priority inversion
- Zombie process leaks
- Not saving/restoring full context

---

## Code Organization

```
mp3/
├── kernel/
│   ├── main.c           # Kernel entry, initialization
│   ├── trap.c           # Trap/interrupt handling
│   ├── syscall.c        # System call dispatcher
│   ├── proc.c           # Process management
│   ├── sched.c          # Scheduler
│   ├── vm.c             # Virtual memory
│   ├── fs.c             # File system core
│   ├── file.c           # File descriptor ops
│   ├── exec.c           # ELF loader
│   └── pipe.c           # Pipe implementation
├── user/
│   ├── init.c           # First user program
│   ├── sh.c             # Shell
│   ├── cat.c            # cat utility
│   ├── ls.c             # ls utility
│   └── ulib.c           # User library (wrappers)
├── mkfs/
│   └── mkfs.c           # Filesystem image builder
├── Makefile
└── README.md
```

---

## Performance Optimizations

- **Copy-on-write fork**: Share pages until write
- **Demand paging**: Allocate pages on page fault
- **Buffer cache**: Cache disk blocks in memory
- **Inode cache**: Keep active inodes in memory

---

## References

- **RISC-V Privileged Spec**: Virtual memory, traps
- **ELF Specification**: Binary format
- **xv6 RISC-V**: Reference OS implementation
- **Linux System Programming**: POSIX API
