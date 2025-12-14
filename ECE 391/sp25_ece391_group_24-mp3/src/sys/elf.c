// elf.c - ELF file loader
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "elf.h"
#include "conf.h"
#include "io.h"
#include "string.h"
#include "memory.h"
#include "assert.h"
#include "error.h"

#include <stdint.h>

// Offsets into e_ident

#define EI_CLASS        4   
#define EI_DATA         5
#define EI_VERSION      6
#define EI_OSABI        7
#define EI_ABIVERSION   8   
#define EI_PAD          9  


// ELF header e_ident[EI_CLASS] values

#define ELFCLASSNONE 0
#define ELFCLASS32 1
#define ELFCLASS64 2

// ELF header e_ident[EI_DATA] values

#define ELFDATANONE 0
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

// ELF header e_ident[EI_VERSION] values

#define EV_NONE     0
#define EV_CURRENT  1

#define ELF_LOAD_START 0x80100000
#define ELF_LOAD_END   0x81000000
// ELF header e_type values

enum elf_et {
    ET_NONE = 0,
    ET_REL,
    ET_EXEC,
    ET_DYN,
    ET_CORE
};

struct elf64_ehdr {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff; 
    uint32_t e_flags; 
    uint16_t e_ehsize; 
    uint16_t e_phentsize; 
    uint16_t e_phnum; 
    uint16_t e_shentsize; 
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

enum elf_pt {
	PT_NULL = 0, 
	PT_LOAD,
	PT_DYNAMIC,
	PT_INTERP,
	PT_NOTE,
	PT_SHLIB,
	PT_PHDR,
	PT_TLS
};

// Program header p_flags bits

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

struct elf64_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

// ELF header e_machine values (short list)

#define  EM_RISCV   243
//==============================================================================================
// int elf_load(struct io * elfio, void (**eptr)(void))
// inputs:struct io * elfio:pointer to an I/O stream 
//          void (**eptr)(void):pointer to a function pointer for entry point
// outputs: int: 0 on success,or a negative error code on failure:
//             :EINVAL    inputs or elf header is invalid
//             :100       fail to read the ELF header
//             :1         fail to seek ph offset
//             :2         fail to read a ph
//             :3         not in the range of valid address
//             :4         fail to seek a segament offset
//             :5         fail to read segament content
// description: use io to read a ELF file,loads PT_LOAD segments into memory, and stores the entry point address.
//===============================================================================================

int elf_load(struct io * elfio, void (**eptr)(void)) {
    // FIX ME
    struct elf64_ehdr ehdr;

    if(!elfio || !eptr){
        return -EINVAL;
    }

    if(ioreadat(elfio, 0, &ehdr, sizeof(ehdr)) != sizeof(ehdr)){
        return -EINVAL;
    }
        

    if(ehdr.e_ident[0]!=0x7f||ehdr.e_ident[1]!='E'||ehdr.e_ident[2]!='L'||ehdr.e_ident[3]!='F'){
        return -EINVAL;
    }
        

    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64 ||
        ehdr.e_ident[EI_DATA] != ELFDATA2LSB ||
        ehdr.e_ident[EI_VERSION] != EV_CURRENT)
        return -EINVAL;

    if (ehdr.e_machine!=EM_RISCV||ehdr.e_type!=ET_EXEC||ehdr.e_entry == 0)
        return -EINVAL;

    *eptr = (void (*)(void))ehdr.e_entry;

    for (int i = 0; i < ehdr.e_phnum; ++i) {
        struct elf64_phdr phdr;
        uint64_t offset = ehdr.e_phoff + i * ehdr.e_phentsize;

        // if (ioseek(elfio, offset) < 0)
        //     return -1;

        // if (ioread(elfio, &phdr, sizeof(phdr)) != sizeof(phdr))
        //     return -2;

        if (ioreadat(elfio, offset, &phdr, sizeof(phdr)) != sizeof(phdr))
            return -2;

        if (phdr.p_type != PT_LOAD)
            continue;

        if (phdr.p_vaddr < UMEM_START_VMA || phdr.p_vaddr + phdr.p_memsz > UMEM_END_VMA)
            return -3;

        // memset((void*)(uintptr_t)phdr.p_vaddr, 0, phdr.p_memsz);

        // void * buf = alloc_and_map_range(phdr.p_vaddr, phdr.p_memsz, PTE_R | PTE_W | PTE_U);
        // int perm = PTE_U | PTE_V | PTE_A | PTE_D | PTE_X; 
        // kprintf("Hardcoded perm = 0x%x\n", perm); // Should be 0xd9
        int perm = PTE_U;
        if (phdr.p_flags & PF_R) perm |= PTE_R;
        if (phdr.p_flags & PF_W) perm |= PTE_W;
        if (phdr.p_flags & PF_X) perm |= PTE_X;
        int size = ROUND_UP(phdr.p_memsz, PAGE_SIZE);

        void * buf = alloc_phys_pages(size/PAGE_SIZE);
        // void * buf = alloc_and_map_range(phdr.p_vaddr, phdr.p_memsz, perm);
        if (ioreadat(elfio, phdr.p_offset, buf, phdr.p_filesz) != phdr.p_filesz)
            return -5;

        map_range(phdr.p_vaddr, phdr.p_memsz, buf, perm);        

        // if (i == 1)
        //     *eptr += (uintptr_t)buf;
        // kprintf("Mapping [0x%lx - 0x%lx] with perm 0x%x\n", 
        //     phdr.p_vaddr, phdr.p_vaddr + phdr.p_memsz, perm);

        if (phdr.p_memsz > phdr.p_filesz) {
            memset((void*)(uintptr_t)(phdr.p_vaddr + phdr.p_filesz), 0,
                   phdr.p_memsz - phdr.p_filesz);
        }
    }
    return 0;
}