// ioimpl.h - Header for implementors of struct io
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _IOIMPL_H_
#define _IOIMPL_H_

#include "io.h"

// EXPORTED TYPE DEFINITIONS
//

struct io {
    const struct iointf * intf;
    unsigned long refcnt;
};

struct iointf {
    void (*close) (         // closes i/o endpoint
        struct io * io          // pointer to i/o endpoint
    );

    int (*cntl) (           // control operation on i/o endpoint
        struct io * io,         // an i/o endpoint to control/interrogate
        int cmd,                // command (IOCTL_* defined in io.h)
        void * arg              // argument for operation
    );

    long (*read) (          // reads from sequential access i/o endpoint
        struct io * io,         // a sequential access i/o endpoint
        void * buf,             // buffer read into
        long bufsz              // buffer size in bytes
    );

    long (*write) (         // writes to sequential access i/o endpoint
        struct io * io,         // a sequential access i/o endpoint
        const void * buf,       // buffer to read from
        long len                // number of bytes to read
    );

    long (*readat) (        // reads from random access i/o endpoint
        struct io * io,         // a random access i/o endpoint
        unsigned long long pos, // position to read from
        void * buf,             // buffer to read into
        long len                // number of bytes to read
    );

    long (*writeat) (       // writes to random access i/o endpoint
        struct io * io,         // a random access i/o endpoint
        unsigned long long pos, // position to write to
        const void * buf,       // buffer to write from
        long len                // number of bytes to write
    );
};

// EXPORTED FUNCTION DECLARATIONS
//

// The ioinit0() and ioinit1() functions initialize an I/O endpoint struct
// allocated by the caller. They sets the _intf_ member of the I/O endpoint to
// the _intf_ argument. The ioinit0() function initializes the reference count
// of the I/O endpoint to 0, while the ioinit1() function initializes it to 1.
// The function always succeeds and returns its first argument.

extern struct io * ioinit0(struct io * io, const struct iointf * intf);
extern struct io * ioinit1(struct io * io, const struct iointf * intf);

#endif // _IOIMPL_H_