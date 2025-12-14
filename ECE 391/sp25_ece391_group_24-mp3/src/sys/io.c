// io.c - Unified I/O object
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "io.h"
#include "ioimpl.h"
#include "assert.h"
#include "string.h"
#include "heap.h"
#include "error.h"
#include "thread.h"
#include "memory.h"

#include <stddef.h>
#include <limits.h>
// INTERNAL TYPE DEFINITIONS
//

struct memio {
    struct io io; // I/O struct of memory I/O
    void * buf; // Block of memory
    size_t size; // Size of memory block
};

// #define PIPE_BUFSZ PAGE_SIZE

struct seekio {
    struct io io; // I/O struct of seek I/O
    struct io * bkgio; // Backing I/O supporting _readat_ and _writeat_
    unsigned long long pos; // Current position within backing endpoint
    unsigned long long end; // End position in backing endpoint
    int blksz; // Block size of backing endpoint
};

struct pipeio {
    struct io io;            
    struct pipe *pipe;      
    int flag;           
};

struct pipe {
    char *buffer;   
    size_t start;  
    size_t tail;  
    size_t len;  

    struct lock lock;
    struct condition read_condition;
    struct condition write_condition;

    unsigned long readers;
    unsigned long writers;
};
// INTERNAL FUNCTION DEFINITIONS
//
static void pipeio_close(struct io *io);
static long pipeio_read(struct io *io, void *buf, long bufsz);
static long pipeio_write(struct io *io, const void *buf, long bufsz);

static const struct iointf pipeio_intf_reader = {
    .close   = &pipeio_close,
    .cntl    = NULL,
    .read    = &pipeio_read,
    .write   = NULL,
    .readat  = NULL,
    .writeat = NULL
};

static const struct iointf pipeio_intf_writer = {
    .close   = &pipeio_close,
    .cntl    = NULL,
    .read    = NULL,
    .write   = &pipeio_write,
    .readat  = NULL,
    .writeat = NULL
};

static int memio_cntl(struct io * io, int cmd, void * arg);

static long memio_readat (
    struct io * io, unsigned long long pos, void * buf, long bufsz);

static long memio_writeat (
    struct io * io, unsigned long long pos, const void * buf, long len);

static void seekio_close(struct io * io);

static int seekio_cntl(struct io * io, int cmd, void * arg);

static long seekio_read(struct io * io, void * buf, long bufsz);

static long seekio_write(struct io * io, const void * buf, long len);

static long seekio_readat (
    struct io * io, unsigned long long pos, void * buf, long bufsz);

static long seekio_writeat (
    struct io * io, unsigned long long pos, const void * buf, long len);


// INTERNAL GLOBAL CONSTANTS
static const struct iointf seekio_iointf = {
    .close = &seekio_close,
    .cntl = &seekio_cntl,
    .read = &seekio_read,
    .write = &seekio_write,
    .readat = &seekio_readat,
    .writeat = &seekio_writeat
};

// EXPORTED FUNCTION DEFINITIONS
//

struct io * ioinit0(struct io * io, const struct iointf * intf) {
    assert (io != NULL);
    assert (intf != NULL);
    io->intf = intf;
    io->refcnt = 0;
    return io;
}

struct io * ioinit1(struct io * io, const struct iointf * intf) {
    assert (io != NULL);
    io->intf = intf;
    io->refcnt = 1;
    return io;
}

unsigned long iorefcnt(const struct io * io) {
    assert (io != NULL);
    return io->refcnt;
}

struct io * ioaddref(struct io * io) {
    assert (io != NULL);
    io->refcnt += 1;
    return io;
}

void ioclose(struct io * io) {
    assert (io != NULL);
    assert (io->intf != NULL);
    
    assert (io->refcnt != 0);
    io->refcnt -= 1;

    if (io->refcnt == 0 && io->intf->close != NULL)
        io->intf->close(io);
}

long ioread(struct io * io, void * buf, long bufsz) {
    assert (io != NULL);
    assert (io->intf != NULL);

    if (io->intf->read == NULL)
        return -ENOTSUP;
    
    if (bufsz < 0)
        return -EINVAL;
    
    return io->intf->read(io, buf, bufsz);
}

long iofill(struct io * io, void * buf, long bufsz) {
	long bufpos = 0; // position in buffer for next read
    long nread; // result of last read

    assert (io != NULL);
    assert (io->intf != NULL);

    if (io->intf->read == NULL)
        return -ENOTSUP;

    if (bufsz < 0)
        return -EINVAL;

    while (bufpos < bufsz) {
        nread = io->intf->read(io, buf+bufpos, bufsz-bufpos);
        
        if (nread <= 0)
            return (nread < 0) ? nread : bufpos;
        
        bufpos += nread;
    }

    return bufpos;
}

long iowrite(struct io * io, const void * buf, long len) {
	long bufpos = 0; // position in buffer for next write
    long n; // result of last write

    assert (io != NULL);
    assert (io->intf != NULL);
    
    if (io->intf->write == NULL)
        return -ENOTSUP;

    if (len < 0)
        return -EINVAL;
    
    do {
        n = io->intf->write(io, buf+bufpos, len-bufpos);

        if (n <= 0)
            return (n < 0) ? n : bufpos;

        bufpos += n;
    } while (bufpos < len);

    return bufpos;
}

long ioreadat (
    struct io * io, unsigned long long pos, void * buf, long bufsz)
{
    assert (io != NULL);
    assert (io->intf != NULL);
    
    if (io->intf->readat == NULL)
        return -ENOTSUP;
    
    if (bufsz < 0)
        return -EINVAL;
    
    return io->intf->readat(io, pos, buf, bufsz);
}

long iowriteat (
    struct io * io, unsigned long long pos, const void * buf, long len)
{
    assert (io != NULL);
    assert (io->intf != NULL);
    
    if (io->intf->writeat == NULL)
        return -ENOTSUP;
    
    if (len < 0)
        return -EINVAL;
    
    return io->intf->writeat(io, pos, buf, len);
}

int ioctl(struct io * io, int cmd, void * arg) {
    assert (io != NULL);
    assert (io->intf != NULL);

	if (io->intf->cntl != NULL)
        return io->intf->cntl(io, cmd, arg);
    else if (cmd == IOCTL_GETBLKSZ)
        return 1; // default block size
    else
        return -ENOTSUP;
}

int ioblksz(struct io * io) {
    return ioctl(io, IOCTL_GETBLKSZ, NULL);
}

int ioseek(struct io * io, unsigned long long pos) {
    return ioctl(io, IOCTL_SETPOS, &pos);
}
//==============================================================================================
// struct io * create_memory_io(void * buf, size_t size)
// Inputs:  void * buf:pointer to the buffer
//          size_t size :size of the memory buffer
// outputs: struct io *: pointer to io
// description:
//     creates an I/O interface backed by a given memory buffer. 
//==============================================================================================

struct io * create_memory_io(void * buf, size_t size) {
    struct memio * mio = kcalloc(1, sizeof(struct memio));
    static const struct iointf memio_iointf = {
        .cntl = &memio_cntl,
        .readat = &memio_readat,
        .writeat = &memio_writeat,
        .close = &ioclose
    };

    assert (buf != NULL);
    assert(size > 0);
    mio->buf = buf;
    mio->size = size;

    return ioinit1(&mio->io, &memio_iointf);
}

struct io * create_seekable_io(struct io * io) {
    struct seekio * sio;
    unsigned long end;
    int result;
    int blksz;
    
    blksz = ioblksz(io);
    assert (0 < blksz);
    
    // block size must be power of two
    assert ((blksz & (blksz - 1)) == 0);

    result = ioctl(io, IOCTL_GETEND, &end);
    assert (result == 0);
    
    sio = kcalloc(1, sizeof(struct seekio));

    sio->pos = 0;
    sio->end = end;
    sio->blksz = blksz;
    sio->bkgio = ioaddref(io);

    return ioinit1(&sio->io, &seekio_iointf);

};

// INTERNAL FUNCTION DEFINITIONS
//
//==============================================================================================
// long memio_readat(struct io * io, unsigned long long pos, void * buf, long bufsz)
// Inputs:  struct io * io :pointer to io
//          unsigned long long pos :byte offset to start read
//          void * buf: buffer to store read data
//          long bufsz: number of bytes to read
// outputs: long :number of bytes read
//             EINVAL: invalid input
// description:
//     reads bytes from an io at the specified offset pos.
//==============================================================================================

long memio_readat (
    struct io * io,
    unsigned long long pos,
    void * buf, long bufsz)
{
    struct memio * const mio = (void*)io - offsetof(struct memio, io);
    long rcnt;

    // Check for valid position
    if (pos >= mio->size)
        return -EINVAL;
    
    // Check for valid size
    if (bufsz < 0)
        return -EINVAL;
    
    // Truncate buffer size to maximum size
    if (pos + bufsz > mio->size)
        bufsz = mio->size - pos;

    rcnt = bufsz;
    memcpy(buf, mio->buf + pos, bufsz);
    return rcnt;
}
//==============================================================================================
// long memio_writeat(struct io * io, unsigned long long pos, const void * buf, long len)
// inputs:  struct io * io：pointer to io
//          unsigned long long pos ：byte offset to start writing
//          const void * buf ：source buffer
//          long len ： umber of bytes to write
// outputs: long :number of bytes actually written, or a negative error code
//             EINVAL： if pos is beyond buffer or len is negative
// description:
//     writes `len` bytes into an in-memory I/O device at offset `pos`.
//==============================================================================================

long memio_writeat (
    struct io * io,
    unsigned long long pos,
    const void * buf, long len)
{
    struct memio * const mio = (void*)io - offsetof(struct memio, io);
    long wcnt;

    // Check for valid position
    if (pos >= mio->size)
        return -EINVAL;
    
    // Check for valid size
    if (len < 0)
        return -EINVAL;
    
    // Truncate length to maximum size
    if (pos + len > mio->size)
        len = mio->size - pos;

    wcnt = len;
    memcpy(mio->buf + pos, buf, len);
    return wcnt;
}
//==============================================================================================
// int memio_cntl(struct io * io, int cmd, void * arg)
// inputs:  struct io * io :pointer to io
//          int cmd :control command 
//          void * arg :argument
// outputs: int - 0: sucess
//             -EINVAL:SETEND exceeds buffer size
//             -ENOTSUP:the command is unsupported
// description:
//     handles memory I/O control operations. 
//==============================================================================================

int memio_cntl(struct io * io, int cmd, void * arg) {
    struct memio * const mio = (void*)io - offsetof(struct memio, io);
    unsigned long * ularg = arg;

    switch (cmd) {
    case IOCTL_GETBLKSZ:
        return 1;
    case IOCTL_GETEND:
        *ularg = mio->size;
        return 0;
    case IOCTL_SETEND:
        // Check for valid size
        if (*ularg > mio->size)
            return -EINVAL;
        mio->size = *ularg;
        return 0;
    default:
        return -ENOTSUP;
    }
}

void seekio_close(struct io * io) {
    struct seekio * const sio = (void*)io - offsetof(struct seekio, io);
    ioclose(sio->bkgio);
    kfree(sio);
}

int seekio_cntl(struct io * io, int cmd, void * arg) {
    struct seekio * const sio = (void*)io - offsetof(struct seekio, io);
    unsigned long long * ullarg = arg;
    int result;

    switch (cmd) {
    case IOCTL_GETBLKSZ:
        return sio->blksz;
    case IOCTL_GETPOS:
        *ullarg = sio->pos;
        return 0;
    case IOCTL_SETPOS:
        // New position must be multiple of block size
        if ((*ullarg & (sio->blksz - 1)) != 0)
            return -EINVAL;
        
        // New position must not be past end
        if (*ullarg > sio->end)
            return -EINVAL;
        
        sio->pos = *ullarg;
        return 0;
    case IOCTL_GETEND:
        *ullarg = sio->end;
        return 0;
    case IOCTL_SETEND:
        // Call backing endpoint ioctl and save result
        result = ioctl(sio->bkgio, IOCTL_SETEND, ullarg);
        if (result == 0)
            sio->end = *ullarg;
        return result;
    default:
        return ioctl(sio->bkgio, cmd, arg);
    }
}

long seekio_read(struct io * io, void * buf, long bufsz) {
    struct seekio * const sio = (void*)io - offsetof(struct seekio, io);
    unsigned long long const pos = sio->pos;
    unsigned long long const end = sio->end;
    long rcnt;

    // Cannot read past end
    if (end - pos < bufsz)
        bufsz = end - pos;

    if (bufsz == 0)
        return 0;
        
    // Request must be for at least blksz bytes if not zero
    if (bufsz < sio->blksz)
        return -EINVAL;

    // Truncate buffer size to multiple of blksz
    bufsz &= ~(sio->blksz - 1);

    rcnt = ioreadat(sio->bkgio, pos, buf, bufsz);
    sio->pos = pos + ((rcnt < 0) ? 0 : rcnt);
    return rcnt;
}


long seekio_write(struct io * io, const void * buf, long len) {
    struct seekio * const sio = (void*)io - offsetof(struct seekio, io);
    unsigned long long const pos = sio->pos;
    unsigned long long end = sio->end;
    int result;
    long wcnt;

    if (len == 0)
        return 0;
    
    // Request must be for at least blksz bytes
    if (len < sio->blksz)
        return -EINVAL;
    
    // Truncate length to multiple of blksz
    len &= ~(sio->blksz - 1);

    // Check if write is past end. If it is, we need to change end position.

    if (end - pos < len) {
        if (ULLONG_MAX - pos < len)
            return -EINVAL;
        
        end = pos + len;

        result = ioctl(sio->bkgio, IOCTL_SETEND, &end);
        
        if (result != 0)
            return result;
        
        sio->end = end;
    }

    wcnt = iowriteat(sio->bkgio, sio->pos, buf, len);
    sio->pos = pos + ((wcnt < 0) ? 0 : wcnt);
    return wcnt;
}

long seekio_readat (
    struct io * io, unsigned long long pos, void * buf, long bufsz)
{
    struct seekio * const sio = (void*)io - offsetof(struct seekio, io);
    return ioreadat(sio->bkgio, pos, buf, bufsz);
}

long seekio_writeat (
    struct io * io, unsigned long long pos, const void * buf, long len)
{
    struct seekio * const sio = (void*)io - offsetof(struct seekio, io);
    return iowriteat(sio->bkgio, pos, buf, len);
}


void create_pipe(struct io ** wioptr, struct io ** rioptr){
    struct pipe *p = kmalloc(sizeof(struct pipe));
    if(p==NULL){
        kprintf("Failed to allocate pipe struct, io.c:548\n");
        return;
    }
    p->buffer = alloc_phys_page();
    if(p->buffer==NULL){
        kfree(p);
        return;
    }
    memset(p->buffer, 0, PAGE_SIZE); 
    
    p->start=0;
    p->tail=0;
    p->len=0;
    
    lock_init(&p->lock);
    condition_init(&p->read_condition,"pipe_read");
    condition_init(&p->write_condition,"pipe_write");
    
    p->readers=1;
    p->writers=1;

    struct pipeio *r=kcalloc(1,sizeof(struct pipeio));
    struct pipeio *w=kcalloc(1,sizeof(struct pipeio));

    r->pipe=p;r->flag=1;r->io.intf=&pipeio_intf_reader;
    r->io.refcnt=1;

    w->pipe=p;w->flag=0;w->io.intf=&pipeio_intf_writer;
    w->io.refcnt=1;

    *rioptr=&r->io;
    *wioptr=&w->io;
}

static long pipeio_read(struct io *io, void *buf, long bufsz) {
    if(bufsz<0){
        return -EINVAL;
    }
    struct pipeio *pio = (struct pipeio *)((char *)io - offsetof(struct pipeio, io));
    struct pipe *p = pio->pipe;
    char *c = buf;
    long byte_read = 0;

    lock_acquire(&p->lock);
    // kprintf("reader acquired lock. Requested size: %ld\n", bufsz);
    while(byte_read==0){
        while(p->len==0&&p->writers>0){
            // kprintf("[waiting for data (pipe is empty)...\n");
            lock_release(&p->lock);
            condition_wait(&p->read_condition);
            lock_acquire(&p->lock);
        }
        //no more writers, pipe is done
        if(p->len==0&&p->writers==0){
           lock_release(&p->lock);
            return -EPIPE;
        }

        while(byte_read<bufsz&&p->len>0){
            c[byte_read++]=p->buffer[p->start];
            p->start=(p->start+1)%PAGE_SIZE;
            p->len--;
        }
        // kprintf("read %ld bytes\n", byte_read);
        condition_broadcast(&p->write_condition);
    }
    lock_release(&p->lock);
    return byte_read;
}

static long pipeio_write(struct io *io, const void *buf, long bufsz) {
    if(bufsz<0){
        return -EINVAL;
    }
    struct pipeio *pio=(struct pipeio *)((char *)io - offsetof(struct pipeio, io));
    struct pipe *p=pio->pipe;
    const char *s=buf;
    long byte_written=0;

    if(bufsz>PAGE_SIZE){
        bufsz=PAGE_SIZE;
    }
         
   lock_acquire(&p->lock);
//    kprintf("writer acquired lock. Writing %ld bytes\n", bufsz);

    if (p->readers==0){
       lock_release(&p->lock);
        return -EPIPE;
    }
    while(byte_written==0){
        while(p->len==PAGE_SIZE&&p->readers>0){
           lock_release(&p->lock);
            condition_wait(&p->write_condition);
           lock_acquire(&p->lock);
        }
        if(p->len==PAGE_SIZE&&p->readers==0){
           lock_release(&p->lock);
            return 0; 
        }
        while(byte_written<bufsz&&p->len<PAGE_SIZE){
            p->buffer[p->tail]=s[byte_written++];
            p->tail=(p->tail+1)%PAGE_SIZE;
            p->len++;
        }
        // kprintf("wrote %ld bytes\n", byte_written);
        condition_broadcast(&p->read_condition);
    }
    lock_release(&p->lock);
    return byte_written;
}

static void pipeio_close(struct io *io) {
    struct pipeio *pio = (struct pipeio *)((char *)io - offsetof(struct pipeio, io));
    struct pipe *p = pio->pipe;
    lock_acquire(&p->lock);
    if(pio->flag==1){
        p->readers--;
        // kprintf("reader closed. Remaining readers: %d\n", p->readers);
        if(p->readers==0){
            condition_broadcast(&p->write_condition);
        }
    }else{
        p->writers--;
        // kprintf("writer closed. Remaining writers: %d\n", p->writers);
        if(p->writers==0){
            condition_broadcast(&p->read_condition);
        }
    }
    lock_release(&p->lock);
    if(p->readers==0&&p->writers==0){
        // kprintf("no readers or writers, freeing pipe\n");
        free_phys_page(p->buffer);
        kfree(p);
    }
    kfree(pio);
}

