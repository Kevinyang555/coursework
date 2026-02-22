/*! @file memory.c
    @brief Physical and virtual memory manager    
    @copyright Copyright (c) 2024-2025 University of Illinois
    @license SPDX-License-identifier: NCSA

*/

#ifdef MEMORY_TRACE
#define TRACE
#endif

#ifdef MEMORY_DEBUG
#define DEBUG
#endif

#include "memory.h"
#include "conf.h"
#include "riscv.h"
#include "heap.h"
#include "console.h"
#include "assert.h"
#include "string.h"
#include "thread.h"
#include "process.h"
#include "error.h"

// COMPILE-TIME CONFIGURATION
//

// Minimum amount of memory in the initial heap block.

#ifndef HEAP_INIT_MIN
#define HEAP_INIT_MIN 256
#endif

// INTERNAL CONSTANT DEFINITIONS
//

#define MEGA_SIZE ((1UL << 9) * PAGE_SIZE) // megapage size
#define GIGA_SIZE ((1UL << 9) * MEGA_SIZE) // gigapage size

#define PTE_ORDER 3
#define PTE_CNT (1U << (PAGE_ORDER - PTE_ORDER))

#ifndef PAGING_MODE
#define PAGING_MODE RISCV_SATP_MODE_Sv39
#endif

#ifndef ROOT_LEVEL
#define ROOT_LEVEL 2
#endif

// IMPORTED GLOBAL SYMBOLS
//

// linker-provided (kernel.ld)
extern char _kimg_start[];
extern char _kimg_text_start[];
extern char _kimg_text_end[];
extern char _kimg_rodata_start[];
extern char _kimg_rodata_end[];
extern char _kimg_data_start[];
extern char _kimg_data_end[];
extern char _kimg_end[];

// EXPORTED GLOBAL VARIABLES
//

char memory_initialized = 0;

// INTERNAL TYPE DEFINITIONS
//

// We keep free physical pages in a linked list of _chunks_, where each chunk
// consists of several consecutive pages of memory. Initially, all free pages
// are in a single large chunk. To allocate a block of pages, we break up the
// smallest chunk on the list.

/**
 * @brief Section of consecutive physical pages. We keep free physical pages in a
 * linked list of chunks. Initially, all free pages are in a single large chunk. To
 * allocate a block of pages, we break up the smallest chunk in the list
 */
struct page_chunk {
    struct page_chunk * next; ///< Next page in list
    unsigned long pagecnt; ///< Number of pages in chunk
};

/**
 * @brief RISC-V PTE. RTDC (RISC-V docs) for what each of these fields means!
 */
struct pte {
    uint64_t flags:8;
    uint64_t rsw:2;
    uint64_t ppn:44;
    uint64_t reserved:7;
    uint64_t pbmt:2;
    uint64_t n:1;
};

// INTERNAL MACRO DEFINITIONS
//
#define VPN(vma) ((vma) / PAGE_SIZE)
#define VPN2(vma) ((VPN(vma) >> (2*9)) % PTE_CNT)
#define VPN1(vma) ((VPN(vma) >> (1*9)) % PTE_CNT)
#define VPN0(vma) ((VPN(vma) >> (0*9)) % PTE_CNT)

#define MIN(a,b) (((a)<(b))?(a):(b))

#define ROUND_UP(n,k) (((n)+(k)-1)/(k)*(k)) 
#define ROUND_DOWN(n,k) ((n)/(k)*(k))

// The following macros test is a PTE is valid, global, or a leaf. The argument
// is a struct pte (*not* a pointer to a struct pte).

#define PTE_VALID(pte) (((pte).flags & PTE_V) != 0)
#define PTE_GLOBAL(pte) (((pte).flags & PTE_G) != 0)
#define PTE_LEAF(pte) (((pte).flags & (PTE_R | PTE_W | PTE_X)) != 0)

#define PT_INDEX(lvl, vpn) (((vpn) & (0x1FF << (lvl * (PAGE_ORDER - PTE_ORDER)))) \
                             >> (lvl * (PAGE_ORDER - PTE_ORDER)))
// INTERNAL FUNCTION DECLARATIONS
//



static inline mtag_t active_space_mtag(void);
static inline mtag_t ptab_to_mtag(struct pte * root, unsigned int asid);
static inline struct pte * mtag_to_ptab(mtag_t mtag);
static inline struct pte * active_space_ptab(void);

static inline void * pageptr(uintptr_t n);
static inline uintptr_t pagenum(const void * p);
static inline int wellformed(uintptr_t vma);

static inline struct pte leaf_pte(const void * pp, uint_fast8_t rwxug_flags);
static inline struct pte ptab_pte(const struct pte * pt, uint_fast8_t g_flag);
static inline struct pte null_pte(void);

// INTERNAL GLOBAL VARIABLES
//

static mtag_t main_mtag;

static struct pte main_pt2[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096)));

static struct pte main_pt1_0x80000[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096)));

static struct pte main_pt0_0x80000[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096)));

static struct page_chunk * free_chunk_list;

// EXPORTED FUNCTION DECLARATIONS
// 
// ==============================================================================================
// void memory_init(void)
// inputs: none
// outputs: none
// description: initializes the memory manager
//              - sets up the main page table with a direct mapping of the kernel image
//              - identity maps the MMIO region as two gigapage mappings
//              - sets up the heap allocator with the memory between the end of the kernel image
//              - initializes the free chunk list with the remaining memory
//              - enables paging
//              - initializes the memory manager
//              - sets the memory manager initialized flag
//===============================================================================================
void memory_init(void) {
    const void * const text_start = _kimg_text_start;
    const void * const text_end = _kimg_text_end;
    const void * const rodata_start = _kimg_rodata_start;
    const void * const rodata_end = _kimg_rodata_end;
    const void * const data_start = _kimg_data_start;
    
    void * heap_start;
    void * heap_end;

    uintptr_t pma;
    const void * pp;

    trace("%s()", __func__);

    assert (RAM_START == _kimg_start);

    kprintf("           RAM: [%p,%p): %zu MB\n",
        RAM_START, RAM_END, RAM_SIZE / 1024 / 1024);
    kprintf("  Kernel image: [%p,%p)\n", _kimg_start, _kimg_end);

    // Kernel must fit inside 2MB megapage (one level 1 PTE)
    
    if (MEGA_SIZE < _kimg_end - _kimg_start)
        panic(NULL);

    // Initialize main page table with the following direct mapping:
    // 
    //         0 to RAM_START:           RW gigapages (MMIO region)
    // RAM_START to _kimg_end:           RX/R/RW pages based on kernel image
    // _kimg_end to RAM_START+MEGA_SIZE: RW pages (heap and free page pool)
    // RAM_START+MEGA_SIZE to RAM_END:   RW megapages (free page pool)
    //
    // RAM_START = 0x80000000
    // MEGA_SIZE = 2 MB
    // GIGA_SIZE = 1 GB
    
    // Identity mapping of MMIO region as two gigapage mappings
    for (pma = 0; pma < RAM_START_PMA; pma += GIGA_SIZE)
        main_pt2[VPN2(pma)] = leaf_pte((void*)pma, PTE_R | PTE_W | PTE_G);
    
    // Third gigarange has a second-level subtable
    main_pt2[VPN2(RAM_START_PMA)] = ptab_pte(main_pt1_0x80000, PTE_G);

    // First physical megarange of RAM is mapped as individual pages with
    // permissions based on kernel image region.

    main_pt1_0x80000[VPN1(RAM_START_PMA)] = ptab_pte(main_pt0_0x80000, PTE_G);

    for (pp = text_start; pp < text_end; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_X | PTE_G);
    }

    for (pp = rodata_start; pp < rodata_end; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_G);
    }

    for (pp = data_start; pp < RAM_START + MEGA_SIZE; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_W | PTE_G);
    }

    // Remaining RAM mapped in 2MB megapages

    for (pp = RAM_START + MEGA_SIZE; pp < RAM_END; pp += MEGA_SIZE) {
        main_pt1_0x80000[VPN1((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_W | PTE_G);
    }

    // Enable paging; this part always makes me nervous.

    main_mtag = ptab_to_mtag(main_pt2, 0);
    csrw_satp(main_mtag);

    // Give the memory between the end of the kernel image and the next page
    // boundary to the heap allocator, but make sure it is at least
    // HEAP_INIT_MIN bytes.

    heap_start = _kimg_end;
    heap_end = (void*)ROUND_UP((uintptr_t)heap_start, PAGE_SIZE);

    if (heap_end - heap_start < HEAP_INIT_MIN) {
        heap_end += ROUND_UP (
            HEAP_INIT_MIN - (heap_end - heap_start), PAGE_SIZE);
    }

    if (RAM_END < heap_end)
        panic("out of memory");
    
    // Initialize heap memory manager

    heap_init(heap_start, heap_end);

    kprintf("Heap allocator: [%p,%p): %zu KB free\n",
        heap_start, heap_end, (heap_end - heap_start) / 1024);
    
    // TODO: Initialize the free chunk list here
    uintptr_t stsrt = (uintptr_t)heap_end;
    uintptr_t end = (uintptr_t)RAM_END;

    if (end <= stsrt) {
        panic("No free memory after heap!");
    }

    free_chunk_list = (struct page_chunk *)stsrt;
    free_chunk_list->next = NULL;
    free_chunk_list->pagecnt = (end - stsrt) / PAGE_SIZE;

    // kprintf("Free physical page chunk: [%p, %p) -> %lu pages\n",
    //     (void*)stsrt, (void*)end, free_chunk_list->pagecnt);
    
    // Allow supervisor to access user memory. We could be more precise by only
    // enabling supervisor access to user memory when we are explicitly trying
    // to access user memory, and disable it at other times. This would catch
    // bugs that cause inadvertent access to user memory (due to bugs).

    csrs_sstatus(RISCV_SSTATUS_SUM);

    memory_initialized = 1;
}

// --------------------------------------------------------------
// mtag_t active_mspace(void)
// inputs: none
// outputs: mtag_t: the current active memory space tag
// description: returns the current active memory space tag
// ---------------------------------------------------------------
mtag_t active_mspace(void) {
    return active_space_mtag();
}

// --------------------------------------------------------------
// mtag_t switch_mspace(mtag_t mtag)
// inputs: mtag_t mtag: the memory space tag to switch to
// outputs: mtag_t: the previous active memory space tag
// description: switches to the specified memory space and returns the previous active memory space tag
//              - flushes the TLB to ensure that the new memory space is used
//              - returns the previous active memory space tag
// ---------------------------------------------------------------
mtag_t switch_mspace(mtag_t mtag) {
    mtag_t prev;
    
    prev = csrrw_satp(mtag);
    sfence_vma();
    return prev;
}

// --------------------------------------------------------------
// mtag_t clone_active_mspace(void)
// inputs: none
// outputs: mtag_t: the new memory space tag
// description: clones the current active memory space and returns the new memory space tag
//              - creates a new page table for the new memory space
//              - copies the current active memory space page table to the new page table
//              - sets the new memory space tag
//              - returns the new memory space tag
// ---------------------------------------------------------------
mtag_t clone_active_mspace(void) {
    struct pte *pt2 = active_space_ptab();

    // allocate a new page table for the new memory space
    struct pte *new_pt2 = alloc_phys_page();
    assert(new_pt2 != NULL);

    // copy the current active page table to the new page table
    for (uint32_t i = 0; i < USER_ROOT_INDEX; i++) {
            new_pt2[i] = pt2[i];
    }
    
    // allocate a new page table for the new memory space
    struct pte * new_pt1 = alloc_phys_page();
    new_pt2[USER_ROOT_INDEX] = ptab_pte(new_pt1, 0);
    memcpy(new_pt1, pageptr(pt2[USER_ROOT_INDEX].ppn), PAGE_SIZE);

    for (uint32_t i = 0; i < PTE_CNT; i++) {
        if (PTE_VALID(new_pt1[i])) {
            struct pte * new_pt0 = alloc_phys_page();
            memcpy(new_pt0, pageptr(new_pt1[i].ppn), PAGE_SIZE);
            // deep copy the pages
            for (uint32_t j = 0; j < PTE_CNT; j++) {
                if (PTE_VALID(new_pt0[j])) {
                    void * new_pt0_page = alloc_phys_page();
                    uint_fast8_t flags = new_pt0[j].flags;
                    memcpy(new_pt0_page, pageptr(new_pt0[j].ppn), PAGE_SIZE);
                    new_pt0[j] = leaf_pte(new_pt0_page, flags);
                }

            }
            new_pt1[i] = ptab_pte(new_pt0, 0);
        }
    }

    return ptab_to_mtag(new_pt2, 0);
}

// --------------------------------------------------------------
// void reset_active_mspace(void)
// inputs: none
// outputs: none
// description: resets the current active memory space by unmapping and freeing all pages in the active address space
//              - unmaps and frees all pages in the active address space
//              - frees the page tables for the active address space
//              - flushes the TLB to ensure that the new memory space is used
// ---------------------------------------------------------------
void reset_active_mspace(void)
{
    struct pte *pt2 = active_space_ptab();

    if (PTE_VALID(pt2[USER_ROOT_INDEX]))
    {
        struct pte *pt1 = (struct pte *)pageptr(pt2[USER_ROOT_INDEX].ppn);

        for (uint32_t i = 0; i < PTE_CNT; i++)
        {
            if (PTE_VALID(pt1[i]))
            {
                struct pte *pt0 = (struct pte *)pageptr(pt1[i].ppn);

                for (uint32_t j = 0; j < PTE_CNT; j++)
                {
                    if (PTE_VALID(pt0[j]) && !PTE_GLOBAL(pt0[j]))
                    {
                        uintptr_t vma = UMEM_START_VMA + i * PTE_CNT * PAGE_SIZE + j * PAGE_SIZE;
                        unmap_and_free_range((void *)vma, PAGE_SIZE);
                        pt0[j] = null_pte();
                        // unsigned long after_unmap = free_phys_page_count();
                        // kprintf("Free pages after alloc+map: %lu\n", after_unmap);
                    }
                }
                
                if (!PTE_GLOBAL(pt1[i]))
                {
                    free_phys_page(pt0);
                    pt1[i] = null_pte();
                }
            }
        }
        
        if (!PTE_GLOBAL(pt2[USER_ROOT_INDEX]))
        {
            free_phys_page(pt1);
            pt2[USER_ROOT_INDEX] = null_pte();
        }
    }
    sfence_vma();
}

// --------------------------------------------------------------
// mtag_t discard_active_mspace(void)
// inputs: none
// outputs: mtag_t: main memory space tag
// description: discards the current active memory space by resetting it and switching to the main memory space
//              - resets the current active memory space
//              - switches to the main memory space
//              - returns the main memory space tag
// ---------------------------------------------------------------
mtag_t discard_active_mspace(void) {
    reset_active_mspace();
    switch_mspace(main_mtag);
    return main_mtag;
}

// The map_page() function maps a single page into the active address space at
// the specified address. The map_range() function maps a range of contiguous
// pages into the active address space. Note that map_page() is a special case
// of map_range(), so it can be implemented by calling map_range(). Or
// map_range() can be implemented by calling map_page() for each page in the
// range. The current implementation does the latter.

// We currently map 4K pages only. At some point it may be disirable to support
// mapping megapages and gigapages.

void * map_page(uintptr_t vma, void * pp, int rwxug_flags) {
    assert(wellformed(vma));
    assert(((uintptr_t)pp % PAGE_SIZE) == 0);
    assert((vma % PAGE_SIZE) == 0);

    struct pte *pt2 = active_space_ptab();
    struct pte *pt1, *pt0;

    if (!PTE_VALID(pt2[VPN2(vma)])) {
        void *new_pt1 = alloc_phys_page();
        assert(new_pt1 != NULL);
        memset(new_pt1, 0, PAGE_SIZE);
        pt2[VPN2(vma)] = ptab_pte(new_pt1, 0);
    }

    pt1 = pageptr(pt2[VPN2(vma)].ppn);

    if (!PTE_VALID(pt1[VPN1(vma)])) {
        void *new_pt0 = alloc_phys_page();
        assert(new_pt0 != NULL);
        memset(new_pt0, 0, PAGE_SIZE);
        pt1[VPN1(vma)] = ptab_pte(new_pt0, 0);
    }

    pt0 = pageptr(pt1[VPN1(vma)].ppn);

    pt0[VPN0(vma)] = leaf_pte(pp, rwxug_flags);

    return (void *)vma;
}

// ---------------------------------------------------------------
// void * map_range(uintptr_t vma, size_t size, void * pp, int rwxug_flags)
// inputs: uintptr_t vma: the virtual memory address to map
//         size_t size: the size of the range to map
//         void * pp: the physical memory address to map
//         int rwxug_flags: the flags for the mapping
// outputs: void *: the virtual memory address of the mapped range
// description: maps a range of physical memory pages to a range of virtual memory addresses
//              - rounds up the size to the nearest page size
//              - iterates over the range and maps each page
//              - returns the virtual memory address of the mapped range
// ---------------------------------------------------------------
void * map_range(uintptr_t vma, size_t size, void * pp, int rwxug_flags) {
    size = ROUND_UP(size, PAGE_SIZE);

    for (uintptr_t off = 0; off < size; off += PAGE_SIZE) {
        map_page(vma + off, (void *)((uintptr_t)pp + off), rwxug_flags);
    }

    return (void *)vma;
}

// ---------------------------------------------------------------
// void * alloc_and_map_range(uintptr_t vma, size_t size, int rwxug_flags)
// inputs: uintptr_t vma: the virtual memory address to map
//         size_t size: the size of the range to map
//         int rwxug_flags: the flags for the mapping
// outputs: void *: the virtual memory address of the mapped range
// description: allocates and maps a range of physical memory pages to a range of virtual memory addresses
//              - rounds up the size to the nearest page size
//              - allocates physical pages for the range
//              - maps the range to the virtual memory address
//              - returns the virtual memory address of the mapped range
// ---------------------------------------------------------------
void * alloc_and_map_range(uintptr_t vma, size_t size, int rwxug_flags) {
    size = ROUND_UP(size, PAGE_SIZE);
    void * pp = alloc_phys_pages(size / PAGE_SIZE);
    if (!pp) return NULL;

    return map_range(vma, size, pp, rwxug_flags);
}

// ---------------------------------------------------------------
// void set_range_flags(const void * vp, size_t size, int rwxug_flags)
// inputs: const void * vp: the virtual memory address to set flags
//         size_t size: the size of the range to set flags
//         int rwxug_flags: the flags to set
// outputs: none
// description: sets the flags for a range of virtual memory addresses
//              - rounds up the size to the nearest page size
//              - iterates over the range and sets the flags for each page
//              - flushes the TLB to ensure that the new flags are used
// ---------------------------------------------------------------
void set_range_flags(const void * vp, size_t size, int rwxug_flags) {
    uintptr_t vma = (uintptr_t)vp;
    size = ROUND_UP(size, PAGE_SIZE);

    for (uintptr_t off = 0; off < size; off += PAGE_SIZE) {
        uintptr_t addr = vma + off;

        struct pte * pt2 = active_space_ptab();
        if (!PTE_VALID(pt2[VPN2(addr)])) continue;
        struct pte * pt1 = pageptr(pt2[VPN2(addr)].ppn);
        if (!PTE_VALID(pt1[VPN1(addr)])) continue;
        struct pte * pt0 = pageptr(pt1[VPN1(addr)].ppn);
        struct pte * pte = &pt0[VPN0(addr)];

        if (PTE_VALID(*pte) && PTE_LEAF(*pte)) {
            pte->flags = rwxug_flags | PTE_V | PTE_A | PTE_D;
        }
    }

    sfence_vma();
}

// ---------------------------------------------------------------
// void unmap_and_free_range(void * vp, size_t size)
// inputs: void * vp: the virtual memory address to unmap and free
//         size_t size: the size of the range to unmap and free
// outputs: none
// description: unmaps and frees a range of virtual memory addresses
//              - rounds up the size to the nearest page size
//              - iterates over the range and unmaps and frees each page
//              - flushes the TLB to ensure that the new memory space is used
// ---------------------------------------------------------------
void unmap_and_free_range(void * vp, size_t size) {
    uintptr_t vaddr = (uintptr_t)vp;
    size = ROUND_UP(size, PAGE_SIZE);

    for (uintptr_t va = vaddr; va < vaddr + size; va += PAGE_SIZE) {
        if (!wellformed(va)) continue;

        struct pte * pt2 = active_space_ptab();
        if (!PTE_VALID(pt2[VPN2(va)])) continue;
        struct pte * pt1 = pageptr(pt2[VPN2(va)].ppn);
        if (!PTE_VALID(pt1[VPN1(va)])) continue;
        struct pte * pt0 = pageptr(pt1[VPN1(va)].ppn);
        struct pte * pte = &pt0[VPN0(va)];

        if (PTE_VALID(*pte) && PTE_LEAF(*pte)) {
            free_phys_page(pageptr(pte->ppn));
            *pte = null_pte();
        }
    }

    sfence_vma();
}

// ---------------------------------------------------------------
// void * alloc_phys_page(void)
// inputs: none
// outputs: void *: the physical memory address of the allocated page
// description: allocates a single physical memory page
//              - searches for a free page chunk in the free chunk list
//              - if a free chunk is found, it is split and the page is returned
// -------------------------------------------------------------------
void * alloc_phys_page(void) {
    return alloc_phys_pages(1);
}

// ---------------------------------------------------------------
// void free_phys_page(void * pp)
// inputs: void * pp: the physical memory address of the page to free
// outputs: none
// description: frees a single physical memory page
//              - adds the page to the free chunk list
// -------------------------------------------------------------------
void free_phys_page(void * pp) {
    free_phys_pages(pp, 1);
}

// ---------------------------------------------------------------
// void * alloc_phys_pages(unsigned int cnt)
// inputs: unsigned int cnt: the number of pages to allocate
// outputs: void *: the physical memory address of the allocated pages
// description: allocates a range of physical memory pages
//              - searches for a free page chunk in the free chunk list
//              - if a free chunk is found, it is split and the pages are returned
//              - if no free chunk is found, it returns NULL
// -------------------------------------------------------------------
void * alloc_phys_pages(unsigned int cnt) {
    struct page_chunk * best_fit = NULL;
    struct page_chunk * best_fit_prev = NULL;

    struct page_chunk * prev = NULL;
    struct page_chunk * curr = free_chunk_list;

    while (curr) {
        if (curr->pagecnt >= cnt) {
            if (!best_fit || curr->pagecnt < best_fit->pagecnt) {
                best_fit = curr;
                best_fit_prev = prev;

                if (curr->pagecnt == cnt)
                    break;
            }
        }
        prev = curr;
        curr = curr->next;
    }
    
    if (!best_fit) {
        return NULL;
    }

    void * result;

    if (best_fit->pagecnt == cnt) {
        result = (void *)best_fit;

        if (best_fit_prev) {
            best_fit_prev->next = best_fit->next;
        } else {
            free_chunk_list = best_fit->next;
        }
    } else {
        result = (void *)((uintptr_t)best_fit + PAGE_SIZE * (best_fit->pagecnt - cnt));
        best_fit->pagecnt -= cnt;
    }

    return result;
}

// ---------------------------------------------------------------
// void free_phys_pages(void * pp, unsigned int cnt)
// inputs: void * pp: the physical memory address of the pages to free
//         unsigned int cnt: the number of pages to free
// outputs: none
// description: frees a range of physical memory pages
//              - adds the pages to the free chunk list
// -------------------------------------------------------------------
void free_phys_pages(void * pp, unsigned int cnt) {
    struct page_chunk *chunk = (struct page_chunk *)pp;
   // memset(chunk, 0, sizeof(struct page_chunk));
    chunk->pagecnt = cnt;
    chunk->next = free_chunk_list;
    free_chunk_list = chunk;
}

// ---------------------------------------------------------------
// unsigned long free_phys_page_count(void)
// inputs: none
// outputs: unsigned long: the number of free physical pages
// description: counts the number of free physical pages in the free chunk list
//              - iterates over the free chunk list and sums the page counts
//              - returns the total number of free physical pages
// -------------------------------------------------------------------
unsigned long free_phys_page_count(void) {
    unsigned long total = 0;
    struct page_chunk * curr = free_chunk_list;

    while (curr) {
        total += curr->pagecnt;
        curr = curr->next;
    }

    return total;
}

// ---------------------------------------------------------------
// int handle_umode_page_fault(struct trap_frame * tfr, uintptr_t vma)
// inputs: struct trap_frame * tfr: the trap frame of the faulting process
//         uintptr_t vma: the virtual memory address that caused the fault
// outputs: int: 1 if the fault was handled, 0 otherwise
// description: handles a user mode page fault by allocating a new page and mapping it
//              - checks if the virtual memory address is well-formed
//              - checks if the address is in the user memory range
//              - allocates a new physical page
//              - maps the new page to the virtual memory address
//              - returns 1 if the fault was handled, 0 otherwise
// -------------------------------------------------------------------
int handle_umode_page_fault(struct trap_frame * tfr, uintptr_t vma) {
//     return 0; // no handled
    if (!wellformed(vma)) return 0;

    // kprintf("U-mode page fault!\n");
    // kprintf("  sepc = %p\n", tfr->sepc);
    // kprintf("  bad vaddr = %lx\n", csrr_stval());   // 0x1000000b0
    // kprintf("  sp = %p\n", tfr->sp);
    // kprintf("  a0 = %lx a1 = %lx\n", tfr->a0, tfr->a1);

    if (vma < UMEM_START_VMA || vma >= UMEM_END_VMA) return 0;

    void *pp = alloc_phys_page();
    if (!pp) return 0;

    map_page(ROUND_DOWN(vma, PAGE_SIZE) , pp, PTE_R | PTE_W | PTE_U);
    return 1;
}

// ---------------------------------------------------------------
// mtag_t active_space_mtag(void)
// inputs: none
// outputs: mtag_t: the current active memory space tag
// description: returns the current active memory space tag
//              - retrieves the current memory space tag from the SATP register
//              - returns the memory space tag
// -------------------------------------------------------------------
mtag_t active_space_mtag(void) {
    return csrr_satp();
}

// ---------------------------------------------------------------
// mtag_t ptab_to_mtag(struct pte * ptab, unsigned int asid)
// inputs: struct pte * ptab: the page table to convert
//         unsigned int asid: the address space ID
// outputs: mtag_t: the memory space tag
// description: converts a page table to a memory space tag
//              - constructs the memory space tag from the page table and address space ID
//              - returns the memory space tag
// -------------------------------------------------------------------
static inline mtag_t ptab_to_mtag(struct pte * ptab, unsigned int asid) {
    return (
        ((unsigned long)PAGING_MODE << RISCV_SATP_MODE_shift) |
        ((unsigned long)asid << RISCV_SATP_ASID_shift) |
        pagenum(ptab) << RISCV_SATP_PPN_shift);
}

// ---------------------------------------------------------------
// struct pte * mtag_to_ptab(mtag_t mtag)
// inputs: mtag_t mtag: the memory space tag to convert
// outputs: struct pte *: the page table
// description: converts a memory space tag to a page table
//              - extracts the page table from the memory space tag
//              - returns the page table
// -------------------------------------------------------------------
static inline struct pte * mtag_to_ptab(mtag_t mtag) {
    return (struct pte *)((mtag << 20) >> 8);
}

// ---------------------------------------------------------------
// struct pte * active_space_ptab(void)
// inputs: none
// outputs: struct pte *: the page table of the active memory space
// description: returns the page table of the active memory space
//              - retrieves the current memory space tag
//              - converts the memory space tag to a page table
//              - returns the page table
// -------------------------------------------------------------------
static inline struct pte * active_space_ptab(void) {
    return mtag_to_ptab(active_space_mtag());
}

// ---------------------------------------------------------------
// void * pageptr(uintptr_t n)
// inputs: uintptr_t n: the page number
// outputs: void *: the physical memory address of the page
// description: converts a page number to a physical memory address
//              - shifts the page number left by the page order
//              - returns the physical memory address of the page
// -------------------------------------------------------------------
static inline void * pageptr(uintptr_t n) {
    return (void*)(n << PAGE_ORDER);
}

// ---------------------------------------------------------------
// unsigned long pagenum(const void * p)
// inputs: const void * p: the physical memory address
// outputs: unsigned long: the page number
// description: converts a physical memory address to a page number
//              - shifts the physical memory address right by the page order
//              - returns the page number
// -------------------------------------------------------------------
static inline unsigned long pagenum(const void * p) {
    return (unsigned long)p >> PAGE_ORDER;
}

// ---------------------------------------------------------------
// int wellformed(uintptr_t vma)
// inputs: uintptr_t vma: the virtual memory address
// outputs: int: 1 if the address is well-formed, 0 otherwise
// description: checks if a virtual memory address is well-formed
//              - checks if the address is well-formed according to the RISC-V Sv39 specification
//              - checks if the address bits 63:38 are all 0 or all 1
//              - returns 1 if the address is well-formed, 0 otherwise
// -------------------------------------------------------------------
static inline int wellformed(uintptr_t vma) {
    // Address bits 63:38 must be all 0 or all 1
    uintptr_t const bits = (intptr_t)vma >> 38;
    return (!bits || !(bits+1));
}

// ---------------------------------------------------------------
// struct pte leaf_pte(const void * pp, uint_fast8_t rwxug_flags)
// inputs: const void * pp: the physical memory address
//         uint_fast8_t rwxug_flags: the flags for the page table entry
// outputs: struct pte: the page table entry
// description: creates a leaf page table entry
//              - constructs a page table entry with the specified physical memory address and flags
//              - sets the flags for the page table entry
//              - returns the page table entry
// -------------------------------------------------------------------
static inline struct pte leaf_pte(const void * pp, uint_fast8_t rwxug_flags) {
    return (struct pte) {
        .flags = rwxug_flags | PTE_A | PTE_D | PTE_V,
        .ppn = pagenum(pp)
    };
}

// ---------------------------------------------------------------
// struct pte ptab_pte(const struct pte * pt, uint_fast8_t g_flag)
// inputs: const struct pte * pt: the page table
//         uint_fast8_t g_flag: the global flag for the page table entry
// outputs: struct pte: the page table entry
// description: creates a page table entry for a page table
//              - constructs a page table entry with the specified page table and global flag
//              - sets the flags for the page table entry
//              - returns the page table entry
// -------------------------------------------------------------------
static inline struct pte ptab_pte(const struct pte * pt, uint_fast8_t g_flag) {
    return (struct pte) {
        .flags = g_flag | PTE_V,
        .ppn = pagenum(pt)
    };
}

// ---------------------------------------------------------------
// struct pte null_pte(void)
// inputs: none
// outputs: struct pte: a null page table entry
// description: creates a null page table entry
//              - constructs a null page table entry with all fields set to 0
//              - returns the null page table entry
// -------------------------------------------------------------------
static inline struct pte null_pte(void) {
    return (struct pte) { };
}

// debug function to read virtual memory
// uintptr_t read_virt_mem(uintptr_t va) {
//     struct pte *pt2 = active_space_ptab();
//     if (!PTE_VALID(pt2[VPN2(va)])) {
//         kprintf("VPN2[%lx] invalid\n", VPN2(va));
//         return;
//     }

//     struct pte *pt1 = pageptr(pt2[VPN2(va)].ppn);
//     if (!PTE_VALID(pt1[VPN1(va)])) {
//         kprintf("VPN1[%lx] invalid\n", VPN1(va));
//         return;
//     }

//     struct pte *pt0 = pageptr(pt1[VPN1(va)].ppn);
//     if (!PTE_VALID(pt0[VPN0(va)])) {
//         kprintf("VPN0[%lx] invalid\n", VPN0(va));
//         return;
//     }

//     struct pte pte = pt0[VPN0(va)];

//     if (!(pte.flags & PTE_R)) {
//         kprintf("PTE does not have read permission (PTE_R missing)\n");
//         return;
//     }

//     // Compute physical page base address
//     uintptr_t ppn = pte.ppn;
//     uintptr_t phys_base = (ppn << 12); // page size = 4KB

//     // Compute offset into page
//     uintptr_t offset = va & (PAGE_SIZE - 1);

//     // Final physical address
//     uintptr_t *phys_str = phys_base + offset;

//     // Print the physical address and the string at that location
//     // kprintf("VA 0x%lx is mapped to PA 0x%lx with flags 0x%x\n", va, phys_base, pte.flags);
//     // kprintf("String at VA 0x%lx: \"%s\"\n", va, phys_str);
//     return phys_str;
// }
