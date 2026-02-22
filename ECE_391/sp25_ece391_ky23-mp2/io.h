// io.h - Unified I/O object
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _IO_H_
#define _IO_H_

#include <stddef.h>

// EXPORTED TYPE DEFINITIONS
//

struct io; // opaque (defined in ioimpl.h)

#define IOCTL_GETBLKSZ  0 // arg is ignored

// EXPORTED FUNCTION DECLARATIONS
//

// The ioaddref function increments the reference count of an I/O endpoint. It
// returns its first argument. To ensure proper referene counting, we recommend
// using the ioaddref function at the point where the reference is created by
// assignment to a persistent variable, e.g.,
//
//     io1 = ioaddref(io2);
//

extern unsigned long iorefcnt(const struct io * io);
extern struct io * ioaddref(struct io * io);

// The ioclose() function closes the I/O endpoint. No further operations should
// be attempted on the endpoint.

extern void ioclose(struct io * io);

// The ioctl() function performs a special operation on the device. The _cmd_
// argument specifies the operation to perform, of the IOCTL constants above.
// The _arg_ argument is an optional argument to the command.

extern int ioctl (
    struct io * io,
    int cmd,
    void * arg
);

// The ioread() function reads up to _bufsz_ bytes from an I/O endpoint into a
// buffer _buf_. It returns the number of bytes actually read into the buffer if
// successful (this may be fewer bytes than requested). The ioread() function
// returns 0 when no more data is available now nor will be available in the
// future (all future calls to ioread() will return 0). For files, ioread()
// returns 0 when it reaches the end of the file. If an error occurs, ioread()
// returns a negative error code.

extern long ioread (
    struct io * io,
    void * buf,
    long bufsz
);

// The iofill() function reads _bufsz_ bytes from an I/O endpoint into a buffer
// _buf_ if possible. It returns the number of bytes actually read into the
// buffer if successful (this may be fewer bytes than requested). The iofill()
// function returns 0 when no more data is available now nor will be available
// in the future (all future calls to ioread() will return 0). If an error
// occurs, iofill() returns a negative error code.
//
// The iofill() function differs from ioread() in that it will only return fewer
// than the requested number of bytes if no more data will be available, now or
// in the future. Otherwise, it reads exactly _bufsz_ bytes.

extern long iofill (
    struct io * io,
    void * buf,
    long bufsz
);

// The iowrite() function writes _len_bytes pointed to by _buf_ to the I/O
// endpoint. It returns the number of bytes actually written if successful. It
// returns 0 if no more bytes can be written now or in the future. (For files,
// this happens when iowrite() reaches the end of the file and the file system
// does not automatically increase the file size.)

extern long iowrite (
    struct io * io,
    const void * buf,
    long len
);

extern long ioreadat (
    struct io * io,
    unsigned long long pos,
    void * buf,
    long bufsz
);

extern long iowriteat (
    struct io * io,
    unsigned long long pos,
    const void * buf,
    long len
);

// The ioblksz() function is equivalent to:
//     return ioctl(io, IOCTL_GETBLKSZ, NULL);
//

extern int ioblksz(struct io * io);

#endif // _IO_H_