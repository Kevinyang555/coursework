/*! @file syscall.c
    @brief system call handlers 
    @copyright Copyright (c) 2024-2025 University of Illinois
    @license SPDX-License-identifier: NCSA
*/



#ifdef SYSCALL_TRACE
#define TRACE
#endif

#ifdef SYSCALL_DEBUG
#define DEBUG
#endif

#include "conf.h"
#include "assert.h"
#include "scnum.h"
#include "process.h"
#include "memory.h"
#include "io.h"
#include "device.h"
#include "fs.h"
#include "intr.h"
#include "timer.h"
#include "error.h"
#include "thread.h"
#include "process.h"
#include "ktfs.h"

// EXPORTED FUNCTION DECLARATIONS
//

extern void handle_syscall(struct trap_frame * tfr); // called from excp.c

// INTERNAL FUNCTION DECLARATIONS
//

static int64_t syscall(const struct trap_frame * tfr);

static int sysexit(void);
static int sysexec(int fd, int argc, char ** argv);
static int sysfork(const struct trap_frame * tfr);
static int syswait(int tid);
static int sysprint(const char * msg);
static int sysusleep(unsigned long us);

static int sysdevopen(int fd, const char * name, int instno);
static int sysfsopen(int fd, const char * name);

static int sysclose(int fd);
static long sysread(int fd, void * buf, size_t bufsz);
static long syswrite(int fd, const void * buf, size_t len);
static int sysioctl(int fd, int cmd, void * arg);
static int syspipe(int * wfdptr, int * rfdptr);

static int sysfscreate(const char* name); 
static int sysiodup(int oldfd, int newfd);
static int sysfsdelete(const char* name);
// EXPORTED FUNCTION DEFINITIONS
//


//==============================================================================================
// void handle_syscall(struct trap_frame *tfr)
// inputs: struct trap_frame * tfr: pointer to the trap frame
// outputs: none
// description:
//     handle syscall based on syscall number in tfr->a7.
//     result is stored back into tfr->a0, and advances sepc by 4.
//==============================================================================================
void handle_syscall(struct trap_frame * tfr) {    
    int64_t result=syscall(tfr);
    tfr->a0=result;
    // uintptr_t sepc = (uintptr_t) csrr_sepc();
    // csrw_sepc((void *)(sepc + 4));
    tfr->sepc=(void *)((uintptr_t)tfr->sepc+4);
}

// INTERNAL FUNCTION DEFINITIONS
//


//==============================================================================================
// int64_t syscall(const struct trap_frame * tfr)
// inputs: const struct trap_frame * tfr: trap frame 
// outputs: int64_t: result of the system call
// description:
//     syscall handler based on tfr->a7.
//==============================================================================================
static int64_t syscall(const struct trap_frame * tfr){
    switch(tfr->a7){
        case SYSCALL_EXIT:
            sysexit();return 0;
        case SYSCALL_EXEC:
            return sysexec((int)tfr->a0,(int)tfr->a1,(char **)tfr->a2);
        case SYSCALL_FORK:
            return sysfork(tfr);
        case SYSCALL_WAIT:
            return syswait((int)tfr->a0);
        case SYSCALL_PRINT:
            return sysprint((const char *)tfr->a0);
        case SYSCALL_USLEEP:
            return sysusleep((unsigned long)tfr->a0);
        case SYSCALL_DEVOPEN:
            return sysdevopen((int)tfr->a0,(const char *)tfr->a1,(int)tfr->a2);
        case SYSCALL_FSOPEN:
            return sysfsopen((int)tfr->a0,(const char *)tfr->a1);
        case SYSCALL_CLOSE:
            return sysclose((int)tfr->a0);
        case SYSCALL_READ:
            return sysread((int)tfr->a0,(void *)tfr->a1,(size_t)tfr->a2);
        case SYSCALL_WRITE:
            return syswrite((int)tfr->a0,(const void *)tfr->a1,(size_t)tfr->a2);
        case SYSCALL_IOCTL:
            return sysioctl((int)tfr->a0,(int)tfr->a1,(void *)tfr->a2);
        case SYSCALL_PIPE:
            return syspipe((int *)tfr->a0,(int *)tfr->a1);
        case SYSCALL_FSCREATE:
            return sysfscreate((const char *)tfr->a0);
        case SYSCALL_FSDELETE:
            return sysfsdelete((const char *)tfr->a0);
        case SYSCALL_IODUP:
            return sysiodup((int)tfr->a0, (int)tfr->a1);
        default:
            return -ENOTSUP;  // syscall not supported
    }
}



//==============================================================================================
// int sysexit(void)
// inputs: none
// outputs: int
// description:
//     kill the current process.
//==============================================================================================
static int sysexit(void) {
    process_exit();
}




static int sysexec(int fd, int argc, char ** argv) {
    struct process *proc=current_process();
    if(fd<0||fd>=PROCESS_IOMAX||proc->iotab[fd]==NULL){
        return -EBADFD;
    }
    struct io *a = proc->iotab[fd];
    proc->iotab[fd] = NULL;
    int result=process_exec(a, argc, argv);
    return result;
}

static int sysfork(const struct trap_frame * tfr) {
    if(tfr == NULL) {
        return -EINVAL;
    }
    return process_fork(tfr);
}

//==============================================================================================
// int syswait(int tid)
// inputs: int tid: thread id to wait for
// outputs: int: 0 on success, or negative error code
// description:
//     wait for the given thread to finish.
//==============================================================================================
static int syswait(int tid) {
    return thread_join(tid);
}


//==============================================================================================
// int sysprint(const char * msg)
// inputs: const char * msg: message string to print
// outputs: int: 0 on success
// description:
//     orints a message 
//==============================================================================================
static int sysprint(const char * msg) {
    // int result;
    // trace("%s(msg=%p)", __func__, msg);
    // result = memory_validate_vstr(msg, PTE_U);
    // if (result != 0)
    //     return result;
    // char * haha = (char *)msg)
    kprintf("Thread <%s:%d> says: %s\n", thread_name(running_thread()), running_thread(), msg);
    return 0;
}

//==============================================================================================
// int sysusleep(unsigned long us)
// inputs: unsigned long us: microseconds to sleep
// outputs: int: 0 on success
// description:
//     sleeps the current thread 
//==============================================================================================
static int sysusleep(unsigned long us) {
    sleep_us(us);
    return 0;
}


//==============================================================================================
// int sysdevopen(int fd, const char * name, int instno)
// inputs: int fd: file descriptor
//         const char * name: device name
//         int instno: instance number
// outputs: int: new file descriptor on success, or negative error code
// description:
//     open
//==============================================================================================
static int sysdevopen(int fd, const char * name, int instno) {
    struct process *proc = current_process();
    //if specify the fd
    if(fd>=0){
        if(fd>=PROCESS_IOMAX||proc->iotab[fd]!=NULL){
            return -EBADFD;
        }
        int result = open_device(name, instno, &proc->iotab[fd]);
        if(result < 0){
            return result;
        }
        return fd;
    }
    //if fd == -1
    for(int i=0;i<PROCESS_IOMAX;i++){
        if (proc->iotab[i] == NULL) {
            int result = open_device(name, instno, &proc->iotab[fd]);
            if(result < 0){
                return result;
            }
            return i;
        }
    }
    return -EMFILE; 
}

//==============================================================================================
// int sysfsopen(int fd, const char *name)
// inputs: int fd: file descriptor (or -1 to allocate)
//         const char * name: file system object name
// outputs: int: new file descriptor on success, or negative error code
// description:
//     open
//==============================================================================================
static int sysfsopen(int fd, const char * name) {
    struct process *proc = current_process();
    if(fd>=0){
        if(fd>=PROCESS_IOMAX||proc->iotab[fd]!=NULL){
            return -EBADFD;
        }
        int result=fsopen(name, &proc->iotab[fd]);
        if(result<0){
            return result;
        }
        return fd;
    }
    for(int i=0;i<PROCESS_IOMAX;i++){
        if(proc->iotab[i]==NULL){
            int result = fsopen(name, &proc->iotab[i]);
            if(result<0){
                return result;
            }
            return i;
        }
    }
    return -EMFILE;  
}



//==============================================================================================
// int sysclose(int fd)
// inputs: int fd: file descriptor to close
// outputs: int: 0 on success, or negative error code
// description:
//     closes
//==============================================================================================
static int sysclose(int fd) {
    struct process *proc=current_process();
    if(fd<0||fd>=PROCESS_IOMAX||proc->iotab[fd]==NULL){
        return -EBADFD;
    }
    // perform the ioclose operation on the device or file
    ioclose(proc->iotab[fd]);
    proc->iotab[fd]= NULL;
    return 0;
}


//==============================================================================================
// long sysread(int fd, void *buf, size_t bufsz)
// inputs: int fd: file descriptor
//         void *buf: buffer to read into
//         size_t bufsz: size of the buffer
// outputs: long: number of bytes read, or negative error code
// description:
//     reads data 
//==============================================================================================
static long sysread(int fd, void * buf, size_t bufsz) {
    struct process *proc=current_process();
    if(fd<0||fd>=PROCESS_IOMAX||proc->iotab[fd]==NULL){
        return -EBADFD;
    }
    if(buf==NULL||bufsz<0){
        return -EINVAL;
    }
    // perform the ioread operation on the device or file
    int result=ioread(proc->iotab[fd],buf,bufsz);
    return result;
}


//==============================================================================================
// long syswrite(int fd, const void *buf, size_t len)
// inputs: int fd: file descriptor
//         const void *buf: data buffer 
//         size_t len: number of bytes 
// outputs: long: number of bytes written, or negative error code
// description:
//     writes data to an open file descriptor.
//==============================================================================================
static long syswrite(int fd, const void * buf, size_t len) {
    struct process *proc=current_process();
    if(fd<0||fd>=PROCESS_IOMAX||proc->iotab[fd]==NULL){
        return -EBADFD;
    }
    // if(buf==NULL||len<0){
    //     return -EINVAL;
    // }
    if(len <0){
        return -EINVAL;
    }
    // perform the iowrite operation on the device or file
    int result=iowrite(proc->iotab[fd],buf,len);
    return result;
}

//==============================================================================================
// int sysioctl(int fd, int cmd, void *arg)
// inputs: int fd: file descriptor
//         int cmd: ioctl command
//         void *arg: pointer to argument structure
// outputs: int: 0 on success, or negative error code
// description:
//     Performs device-specific input/output operations.
//==============================================================================================
static int sysioctl(int fd, int cmd, void * arg) {
    struct process *proc=current_process();
    if(fd<0||fd>=PROCESS_IOMAX||proc->iotab[fd]==NULL){
        return -EBADFD;
    }
    // if(arg==NULL){
    //     return -EINVAL;
    // }
    if(cmd<0){
        return -EINVAL;
    }
    // perform the ioctl operation on the device or file
    int result=ioctl(proc->iotab[fd],cmd,arg);
    return result;
}



static int syspipe(int * wfdptr, int * rfdptr){
    if(wfdptr==NULL||rfdptr==NULL){
        return -EINVAL;
    }
    if(*wfdptr == *rfdptr && *wfdptr >= 0 && *rfdptr >=0){
        return -EINVAL;
    }
    if(*wfdptr >= PROCESS_IOMAX || *rfdptr >= PROCESS_IOMAX){
        return -EBADFD;
    }
    
    struct process *proc=current_process();
   
    int write_index = -1;
    int read_index = -1;
    if(*wfdptr <0 && *rfdptr<0){
        for(int i =0; i < PROCESS_IOMAX; i++){
            if(proc->iotab[i] ==NULL){
                if(write_index == -1 ){
                    write_index = i;
                }else if( read_index == -1){
                    read_index = i;
                }else{
                    break;
                }
            }
        }
    }else if( *wfdptr <0 || *rfdptr <0){
        int i;
        for( i =0; i < PROCESS_IOMAX; i++){
            if(proc->iotab[i] == NULL){
                break;
            }
        }
        if(i== PROCESS_IOMAX){
            return -EMFILE;
        }
        if( *wfdptr<0){
            write_index = i;
            read_index = *rfdptr;
        }else{
            read_index = i;
            write_index = *wfdptr;
        }
    }else{
        write_index = *wfdptr;
        read_index = *rfdptr;
    }
    if(write_index < 0 || read_index <0 || write_index == read_index){
        return -EBADFD;
    }

    create_pipe(&proc->iotab[write_index], &proc->iotab[read_index]);
    if(proc->iotab[write_index] == NULL || proc->iotab[read_index] == NULL){
        return -EINVAL;
    }
    *wfdptr = write_index;
    *rfdptr = read_index;
    return 0;
}

//==============================================================================================
// int sysfscreate(const char *name)
// inputs: const char *name: name of the new file
// outputs: int: 0 on success, or negative error code
// description:
//     creates a new file.
//==============================================================================================

static int sysfscreate(const char* name) {
    return fscreate(name);
}



//==============================================================================================
// int sysfsdelete(const char *name)
// inputs: const char *name: name of the file 
// outputs: int: 0 on success, or negative error code
// description:
//     deletes a file
//==============================================================================================
static int sysfsdelete(const char* name) {
    return fsdelete(name);
}

//==============================================================================================
// int sysiodup(int oldfd, int newfd)
// inputs: int oldfd: old file descriptor to duplicate
//         int newfd: new file descriptor to assign
// outputs: int 0 on success, or -EBADFD on invalid file descriptors
// description:
//     duplicates a file descriptor by making newfd refer to the same struct io as oldfd.
//     increments refcnt on the io structure.
//==============================================================================================
static int sysiodup(int oldfd, int newfd) {
    struct process *proc = current_process();
    if(oldfd < 0|| oldfd >= PROCESS_IOMAX){
        return -EBADFD;
    }
    if(newfd < 0 || newfd >= PROCESS_IOMAX){
        return -EBADFD;
    }
    if(proc->iotab[oldfd] == NULL){
        return -EBADFD;
    }
    if(proc->iotab[newfd] != NULL){
        return -EBADFD;  
    }
    proc->iotab[newfd] = proc->iotab[oldfd];
    proc->iotab[newfd]->refcnt++;

    return 0;
}
