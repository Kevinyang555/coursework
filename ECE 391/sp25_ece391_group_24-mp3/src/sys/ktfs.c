// ktfs.c - KTFS implementation
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef KTFS_TRACE
#define TRACE
#endif

#ifdef KTFS_DEBUG
#define DEBUG
#endif


#include "heap.h"
#include "fs.h"
#include "ioimpl.h"
#include "ktfs.h"
#include "error.h"
#include "thread.h"
#include "string.h"
#include "console.h"
#include "cache.h"

// INTERNAL TYPE DEFINITIONS
//

struct ktfs_fs{
    struct cache * cache;
    struct ktfs_file *open_file;
    struct ktfs_superblock super;
    struct io *vioblk;
    unsigned long long inode_blk_pos;
    unsigned long long data_blk_pos;
};

struct ktfs_file {
    // Fill to fulfill spec
    struct io io;
    unsigned long long size;
    struct ktfs_dir_entry *dentry;
    uint32_t flags;
    uint32_t pos;
    struct ktfs_file *next;
    struct ktfs_dir_entry dentry_local;
};

static struct ktfs_fs file_sys = {
    .cache = NULL,
    .open_file = NULL,
    .super = { 0 },
    .vioblk = NULL,
    .data_blk_pos = 0,
    .inode_blk_pos = 0
};


// INTERNAL FUNCTION DECLARATIONS
//


int ktfs_mount(struct io * io);

int ktfs_open(const char * name, struct io ** ioptr);
void ktfs_close(struct io* io);
long ktfs_readat(struct io* io, unsigned long long pos, void * buf, long len);
long ktfs_writeat(struct io* io, unsigned long long pos, const void *buf, long len);
int ktfs_create(const char* name);
int ktfs_delete(const char *name);
int ktfs_cntl(struct io *io, int cmd, void *arg);

int ktfs_getblksz(struct ktfs_file *fd);
int ktfs_getend(struct ktfs_file *fd, void *arg);
uint32_t ktfs_get_data_block(uint32_t block_num,struct ktfs_inode *target_inode,void **block );
int ktfs_add_new_block(struct io *io, void * arg);
int ktfs_update_bitmap(uint32_t block_num, int delete_or_add);

int ktfs_flush(void);

static struct iointf ktfs_iointf = {
    .close = &ktfs_close,
    .readat = &ktfs_readat,
    .cntl = &ktfs_cntl,
    .writeat = &ktfs_writeat

};

// FUNCTION ALIASES
//

int fsmount(struct io * io)
    __attribute__ ((alias("ktfs_mount")));

int fsopen(const char * name, struct io ** ioptr)
    __attribute__ ((alias("ktfs_open")));

int fsflush(void)
    __attribute__ ((alias("ktfs_flush")));

int fscreate(const char* name)
    __attribute__ ((alias("ktfs_create")));

int fsdelete(const char* name)
    __attribute__ ((alias("ktfs_delete")));


// EXPORTED FUNCTION DEFINITIONS
//
//==============================================================================================
// int ktfs_mount(struct io * io)
// inputs: struct io * io - pointer to io
// outputs: 0:success
//          1:invalid input
//          2:fail to get block size
//          3:fail to read superblock
//          4:fail to create cache
// description:
//     mounts a KTFS file system through reading the superblock.
//==============================================================================================

int ktfs_mount(struct io * io)
{
    if(io == NULL){
        return -1;
    }
    uint32_t blksize;
    blksize = io->intf->cntl(io, IOCTL_GETBLKSZ, &blksize);
    if(blksize <= 0){
        return -2;
    }
    if (blksize == 1){
        blksize = CACHE_BLKSZ;
    }

    // read superblock
    void * buf = kmalloc(CACHE_BLKSZ);
    long super = ioreadat(io, 0, buf, CACHE_BLKSZ);
    if (super < 0) {
        return -3;
    }
    if( super != CACHE_BLKSZ){
        return super;
    }
    struct ktfs_superblock sb;
    memcpy(&sb, buf, sizeof(struct ktfs_superblock) < 14 ? sizeof(struct ktfs_superblock) : 14);
    file_sys.super = sb;
    if (create_cache(io, &file_sys.cache) != 0){
        return -4;
    }
    file_sys.vioblk = io;
    file_sys.inode_blk_pos = (1 + file_sys.super.bitmap_block_count) * blksize;
    file_sys.data_blk_pos = file_sys.inode_blk_pos + file_sys.super.inode_block_count * blksize;
    kfree(buf);

    return 0;
}
//==============================================================================================
// int ktfs_open(const char * name, struct io ** ioptr)
// inputs: const char * name:name of the file
//         struct io ** ioptr:output io pointer
// outputs: int 0:success
//              ENOENT:invlid input 
//              EBUSY:file already open
// description:
//     open a file in ktfs through searching directory entry in the root directory inode.
//==============================================================================================


int ktfs_open(const char * name, struct io ** ioptr)
{
    if (name == NULL || ioptr == NULL) {
        return -ENOENT;
    }

    // check if the file is opened
    struct ktfs_file * cur = file_sys.open_file;
    while (cur != NULL) {
        if (strcmp(name, cur->dentry->name) == 0) {
            return -EBUSY;
        }
        cur = cur->next;
    }

    struct ktfs_file *fd = kmalloc(sizeof(struct ktfs_file));
    fd->size = 0;
    fd->dentry = NULL;
    fd->pos = 0;
    fd->flags = 0;
    fd->next = NULL;

    // extract inodes block from cache
    void * inodes = NULL;
    uint32_t root_dir_inode_block_pos = file_sys.super.root_directory_inode / (CACHE_BLKSZ / sizeof(struct ktfs_inode)) * CACHE_BLKSZ; 
    uint32_t root_dir_inode_block_offset = file_sys.super.root_directory_inode % (CACHE_BLKSZ / sizeof(struct ktfs_inode));
    cache_get_block(file_sys.cache, file_sys.inode_blk_pos + root_dir_inode_block_pos, &inodes);
    cache_release_block(file_sys.cache, inodes, 0);
    struct ktfs_inode * root_inode_block = inodes;
    
    // try to find file dentry in direct block
    void * dentries_ptr = NULL;
    for (int i = 0; i < KTFS_NUM_DIRECT_DATA_BLOCKS; ++i){
        cache_get_block(file_sys.cache, file_sys.data_blk_pos + root_inode_block[root_dir_inode_block_offset].block[i] * CACHE_BLKSZ, &dentries_ptr);
        struct ktfs_dir_entry * dentries = dentries_ptr;
        for (int j = 0; j < CACHE_BLKSZ / sizeof(struct ktfs_dir_entry); ++j){
            if (strcmp(name, dentries[j].name) == 0){
                memcpy(&fd->dentry_local, &dentries[j], sizeof(struct ktfs_dir_entry));
                fd->dentry = &fd->dentry_local;
                
                break;
            }
        }
        cache_release_block(file_sys.cache, dentries, 0);
        if (fd->dentry != NULL){
            break;
        }
    }


    // check if the file is found
    if (fd->dentry == NULL) {
        //cache_release_block(file_sys.cache, inodes, 0);
        kfree(fd);
        return -ENOENT;
    }

    // find file size
    void *inode_again = NULL;
    uint16_t index = fd->dentry->inode;
    int inode_num = index / (CACHE_BLKSZ / sizeof(struct ktfs_inode));
    int inode_offset = index % (CACHE_BLKSZ / sizeof(struct ktfs_inode));
    struct ktfs_inode * actual_inodes = inodes;
    if(inode_num != 0){
        cache_get_block(file_sys.cache, file_sys.inode_blk_pos + inode_num, &inode_again);
        actual_inodes = inode_again;
        fd->size = actual_inodes[inode_offset].size;
        cache_release_block(file_sys.cache, inode_again, 0);
    }else{
        fd->size = actual_inodes[inode_offset].size;
       // cache_release_block(file_sys.cache, inodes, 0);
    }

    // assign io interface
    ioinit0(&fd->io, &ktfs_iointf);
    struct io *smio = create_seekable_io(&fd->io);  
    *ioptr = smio;
    // add file to open file list
    if (file_sys.open_file == NULL) {
        file_sys.open_file = fd;
    } else {
        struct ktfs_file * cur = file_sys.open_file;
        while (cur->next != NULL) {
            cur = cur->next;
        }
        cur->next = fd;
    }

    return 0;
}
//==============================================================================================
// void ktfs_close(struct io* io)
// inputs: struct io * io: io returned by open
// outputs: none
// description:
//     close a file in ktfs and remove it from the open file list.
//==============================================================================================

void ktfs_close(struct io* io)
{
    if (io == NULL) {
        return;
    }
    struct ktfs_file *fd = (struct ktfs_file *)((char *)io - offsetof(struct ktfs_file, io));
    struct ktfs_file * cur = file_sys.open_file;
    struct ktfs_file * prev = NULL;
    while (cur != NULL) {
        if (cur->dentry_local.inode == fd->dentry_local.inode) {
            if (prev == NULL) {
                file_sys.open_file = cur->next;
            } else {
                prev->next = cur->next;
            }
            kfree(cur);
            break;
        }
        prev = cur;
        cur = cur->next;
    }
}


//==============================================================================================
// long ktfs_readat(struct io* io, unsigned long long pos, void * buf, long len)
// inputs: struct io * io: io for the file
//         unsigned long long pos: pos
//         void * buf: receive sotred data 
//         long len: length to read 
// outputs: long: number of bytes successfully read
//              EINVA:if input is invalid or block mapping fails
// description:
//     Reads up to `len` bytes from the file starting at position `pos`.
//==============================================================================================

long ktfs_readat(struct io* io, unsigned long long pos, void * buf, long len)
{
    struct ktfs_file *fd = (struct ktfs_file *)((char *)io - offsetof(struct ktfs_file, io));
    if (fd == NULL || buf == NULL || len <= 0) {
        return -EINVAL;
    }
    if (pos > fd->size) {
        return -EINVAL;
    }
    if (pos + len > fd->size) {
        len = fd->size - pos;
    }

    char * new_buf = (char *)buf;

    // find the inode
    uint16_t index = fd->dentry->inode;
    void * inodes = NULL;
    int inode_num = index / (CACHE_BLKSZ / sizeof(struct ktfs_inode));
    int inode_offset = index % (CACHE_BLKSZ / sizeof(struct ktfs_inode));
    cache_get_block(file_sys.cache, file_sys.inode_blk_pos + inode_num * CACHE_BLKSZ, &inodes);
    struct ktfs_inode * actual_inodes = inodes; 
    struct ktfs_inode *target_inode = &actual_inodes[inode_offset];
    cache_release_block(file_sys.cache, inodes, 0);

    // find the block number to read
    uint32_t block_num = pos / KTFS_BLKSZ;
    uint32_t block_offset = pos % KTFS_BLKSZ;

    long long bits_read = 0;
    // read the block, until reach the len
    while (bits_read < len) {
       void *block = NULL;
       if(ktfs_get_data_block(block_num, target_inode, &block) <0){
        return -EIO;
       }
        // read the data from the block
        char * actual_block = block;
        if(len - bits_read< KTFS_BLKSZ-block_offset){
            uint32_t read_len = len - bits_read;
            memcpy(new_buf+bits_read, actual_block + block_offset, read_len);
            bits_read += read_len;
            //cache_release_block(file_sys.cache, block, 0);
        // }else if (len - bits_read < KTFS_BLKSZ){
        //     uint32_t read_len = len - bits_read;
        //     memcpy(new_buf+bits_read, actual_block + block_offset, read_len);
        //     bits_read += read_len;
        //     //cache_release_block(file_sys.cache, block, 0);
        }else{
            uint32_t read_len = KTFS_BLKSZ - block_offset;
            memcpy(new_buf+bits_read, actual_block + block_offset, read_len);
            bits_read += read_len;
           // cache_release_block(file_sys.cache, block, 0);
        }
        cache_release_block(file_sys.cache, block, 0);
        block_offset = 0; 
        block_num++;
    
    }
    return bits_read;
}

//==============================================================================================
// int ktfs_create(const char* name)
// inputs: const char* name: name of the file to create
// outputs: int 0: success
//              EINVAL: if input is invalid or creation fails
// description:
//     creates a new empty file with the given name in the root directory's dentries
//     and allocate a empty inode for use
//==============================================================================================

int ktfs_create(const char* name){

    if( name == NULL || strlen(name) > KTFS_MAX_FILENAME_LEN){
        return -EINVAL;
    }

    //get directory inode
    void * inodes = NULL;
    uint32_t root_dir_inode_block_pos = file_sys.super.root_directory_inode / (CACHE_BLKSZ / sizeof(struct ktfs_inode)) * CACHE_BLKSZ; 
    uint32_t root_dir_inode_block_offset = file_sys.super.root_directory_inode % (CACHE_BLKSZ / sizeof(struct ktfs_inode));
    cache_get_block(file_sys.cache, file_sys.inode_blk_pos + root_dir_inode_block_pos, &inodes);
    cache_release_block(file_sys.cache, inodes, 0);
    struct ktfs_inode * root_inode_block = inodes;
    int inode_start = root_inode_block->size / KTFS_DENSZ;
    int dentrie_index = -1;
    int direct_index = -1;
    
    // try to find empty file dentry
    void * dentries_ptr = NULL;
    for (int i = 0; i < KTFS_NUM_DIRECT_DATA_BLOCKS; ++i){
        cache_get_block(file_sys.cache, file_sys.data_blk_pos + root_inode_block[root_dir_inode_block_offset].block[i] * CACHE_BLKSZ, &dentries_ptr);
        struct ktfs_dir_entry * dentries = dentries_ptr;
        for ( int j = 0; j < CACHE_BLKSZ / sizeof(struct ktfs_dir_entry); ++j){
            if (strcmp(name, dentries[j].name) == 0){
                cache_release_block(file_sys.cache, dentries_ptr, 0);
                return -EINVAL;
            }else if( dentries[j].name[0]=='\0'){
                dentrie_index = j;
                direct_index = i;
                break;
            }
        }
        cache_release_block(file_sys.cache, dentries_ptr, 0);
        if(dentrie_index >=0 && direct_index >=0){
            break;
        }
    }
        
    if( dentrie_index <0 && direct_index <0){
        return -EINVAL;
    }
    //try to find empty inode
    int inode_count = -1;
    struct ktfs_inode * inode_block = NULL;
    for(int i =0; i < file_sys.super.inode_block_count; i ++){
        cache_get_block(file_sys.cache, file_sys.inode_blk_pos + KTFS_BLKSZ*i, &inodes);
        inode_block = inodes;
        int j =0;
        if(i ==0){
            j = inode_start;
        }
        for(; j < KTFS_BLKSZ / sizeof(struct ktfs_inode); j ++){
            if(inode_block[j].flags == 0 && inode_block[j].size == 0){
                inode_count = (KTFS_BLKSZ / sizeof(struct ktfs_inode)) * i + j;
                memset(&inode_block[j], 0, sizeof(struct ktfs_inode));
                inode_block[j].flags = 1;
                break;
            }
        }
        //found inode, force a write to disk
        if(inode_count >=0){
            iowriteat(file_sys.vioblk,file_sys.inode_blk_pos + KTFS_BLKSZ*i , inodes, CACHE_BLKSZ);
            cache_release_block(file_sys.cache, inodes, 0);
           
            break;
        }
        cache_release_block(file_sys.cache, inodes, 0);
    }
    if(inode_count <0){
        return -EINVAL;
    }
    
    //update found dentry block with filename and inode number
    cache_get_block(file_sys.cache, file_sys.data_blk_pos + root_inode_block[root_dir_inode_block_offset].block[direct_index] * CACHE_BLKSZ, &dentries_ptr);
    struct ktfs_dir_entry * dentries = dentries_ptr;
    memcpy(dentries[dentrie_index].name, name, strlen(name));
    dentries[dentrie_index].name[KTFS_MAX_FILENAME_LEN - 1] = '\0';
    dentries[dentrie_index].inode = inode_count;
    //force a write to disk
    iowriteat(file_sys.vioblk,file_sys.data_blk_pos + root_inode_block[root_dir_inode_block_offset].block[direct_index] * CACHE_BLKSZ , dentries_ptr, CACHE_BLKSZ);
    cache_release_block(file_sys.cache, dentries_ptr, 0);

    return 0;
}


//==============================================================================================
// int ktfs_delete(const char* name)
// inputs: const char* name: name of the file to delete
// outputs: int 0: success
//              EINVAL: if input is invalid or deletion fails
// description:
//     deletes the file with the given name from the root directory's dentries 
//     and frees its data blocks in the inode
//==============================================================================================
int ktfs_delete(const char *name){
    if( name == NULL || strlen(name) > KTFS_MAX_FILENAME_LEN){
        return -EINVAL;
    }

    struct ktfs_file *cur = file_sys.open_file;
    //check if file is currently open, if it is, close it
    while (cur != NULL) {
        if (cur->dentry != NULL && strncmp(cur->dentry->name, name, KTFS_MAX_FILENAME_LEN) == 0) {
            ioclose(&cur->io);
            break;
        }
    cur = cur->next;
    }

    //find directory inode
    void * inodes = NULL;
    uint32_t root_dir_inode_block_pos = file_sys.super.root_directory_inode / (CACHE_BLKSZ / sizeof(struct ktfs_inode)) * CACHE_BLKSZ; 
    uint32_t root_dir_inode_block_offset = file_sys.super.root_directory_inode % (CACHE_BLKSZ / sizeof(struct ktfs_inode));
    cache_get_block(file_sys.cache, file_sys.inode_blk_pos + root_dir_inode_block_pos, &inodes);
    cache_release_block(file_sys.cache, inodes, 0);
    struct ktfs_inode * root_inode_block = inodes;
    int inode_num = -1;
    
    // try to find the dentry with the same name
    void * dentries_ptr = NULL;
    for (int i = 0; i < KTFS_NUM_DIRECT_DATA_BLOCKS; ++i){
        cache_get_block(file_sys.cache, file_sys.data_blk_pos + root_inode_block[root_dir_inode_block_offset].block[i] * CACHE_BLKSZ, &dentries_ptr);
        struct ktfs_dir_entry * dentries = dentries_ptr;
        for ( int j = 0; j < CACHE_BLKSZ / sizeof(struct ktfs_dir_entry); ++j){
            if (strcmp(name, dentries[j].name) == 0){ //found the corresponding dentrie, zero everything and bring the last dentry into its place, to maintain the array
                inode_num = dentries[j].inode;
                memcpy(dentries[j].name,dentries[CACHE_BLKSZ / sizeof(struct ktfs_dir_entry) -1].name, KTFS_MAX_FILENAME_LEN);
                dentries[j].inode = dentries[CACHE_BLKSZ / sizeof(struct ktfs_dir_entry) -1].inode;
                memset(dentries[CACHE_BLKSZ / sizeof(struct ktfs_dir_entry) -1].name, 0, KTFS_MAX_FILENAME_LEN);
                dentries[CACHE_BLKSZ / sizeof(struct ktfs_dir_entry) -1].inode = 0;
                iowriteat(file_sys.vioblk,file_sys.data_blk_pos + root_inode_block[root_dir_inode_block_offset].block[i] * CACHE_BLKSZ , dentries_ptr, CACHE_BLKSZ);
                cache_release_block(file_sys.cache, dentries_ptr, 0);
             
                break;
            }
        }
        if(inode_num >= 0){
            break;
        }
        cache_release_block(file_sys.cache, dentries_ptr, 0);
    }

    if(inode_num <0){
        return -EINVAL;
    }
    //find the inode of the file
    void * data_inodes = NULL;
    int inode_block = inode_num / (CACHE_BLKSZ / sizeof(struct ktfs_inode));
    int inode_offset = inode_num % (CACHE_BLKSZ / sizeof(struct ktfs_inode));
    cache_get_block(file_sys.cache, file_sys.inode_blk_pos + inode_block * CACHE_BLKSZ, &data_inodes);
    struct ktfs_inode * actual_inodes = data_inodes; 
    struct ktfs_inode *target_inode = &actual_inodes[inode_offset];
    
    int finished = 0;
    //free all indirect and dindirect themselves and direct blocks
    for(int i =0; i < KTFS_NUM_DIRECT_DATA_BLOCKS; i++){
        if(target_inode->block[i] != 0){
            ktfs_update_bitmap(target_inode->block[i],0);
        }else{
            finished = 1;
            break;
        }
    }
    //free all indirect
    if(target_inode->indirect != 0 && finished == 0){
        void * indirect_block_ptr = NULL;
        cache_get_block(file_sys.cache, file_sys.data_blk_pos + target_inode->indirect * CACHE_BLKSZ, &indirect_block_ptr);
        uint32_t * direct_blocks = indirect_block_ptr;
        for(int i =0; i < KTFS_NUM_INDIRECT_BLOCKS_COUNT; i++){
            if(direct_blocks[i] != 0){
                ktfs_update_bitmap(direct_blocks[i],0);
            }else{
                finished = 1;
                memset(direct_blocks, 0, KTFS_NUM_INDIRECT_BLOCKS_COUNT);
                break;
            }
        }
        iowriteat(file_sys.vioblk,file_sys.data_blk_pos + target_inode->indirect * CACHE_BLKSZ , indirect_block_ptr, CACHE_BLKSZ);
        cache_release_block(file_sys.cache, indirect_block_ptr, 0);
    }
    //free all double indirect index 0
    if(target_inode->dindirect[0] != 0&& finished == 0){
        void * dindirect_block_ptr = NULL;
        cache_get_block(file_sys.cache, file_sys.data_blk_pos + target_inode->dindirect[0] * CACHE_BLKSZ, &dindirect_block_ptr);
        uint32_t * indirect_blocks = dindirect_block_ptr;
        for(int i =0; i < KTFS_NUM_INDIRECT_BLOCKS_COUNT && finished ==0; i++){ //for loop for all the indirect 
            if(indirect_blocks[i]!= 0){
                void * indirect_block_ptr = NULL;
                cache_get_block(file_sys.cache, file_sys.data_blk_pos + indirect_blocks[i] * CACHE_BLKSZ, &indirect_block_ptr);
                uint32_t * direct_blocks = indirect_block_ptr;
                for(int j =0; j < KTFS_NUM_INDIRECT_BLOCKS_COUNT&& finished ==0; j++){//for loop for all the direct
                    if(direct_blocks[j] != 0){
                        ktfs_update_bitmap(direct_blocks[j],0);
                    }else{
                        memset(direct_blocks, 0, KTFS_NUM_INDIRECT_BLOCKS_COUNT);// must also clear the indirect content
                        finished = 1;
                        break;
                    }
                }
                ktfs_update_bitmap(indirect_blocks[i],0);
                iowriteat(file_sys.vioblk,file_sys.data_blk_pos + indirect_blocks[i] * CACHE_BLKSZ , indirect_block_ptr, CACHE_BLKSZ);
                cache_release_block(file_sys.cache, indirect_block_ptr, 0);
            }else{
                finished = 1;
                memset(indirect_blocks, 0, KTFS_NUM_INDIRECT_BLOCKS_COUNT); // finished, clear all the indirect pointers of doubleindrect[0]
                ktfs_update_bitmap(target_inode->dindirect[0],0);
                break;
            }
        }
        //force write back to disk
        iowriteat(file_sys.vioblk,file_sys.data_blk_pos + target_inode->dindirect[0] * CACHE_BLKSZ , dindirect_block_ptr, CACHE_BLKSZ);
        cache_release_block(file_sys.cache, dindirect_block_ptr, 0);
    
    }
    //free all double indirect[1], same logic as above
    if(target_inode->dindirect[1] != 0&& finished == 0){
        void * dindirect_block_ptr = NULL;
        cache_get_block(file_sys.cache, file_sys.data_blk_pos + target_inode->dindirect[1] * CACHE_BLKSZ, &dindirect_block_ptr);
        uint32_t * indirect_blocks = dindirect_block_ptr;
        for(int i =0; i < KTFS_NUM_INDIRECT_BLOCKS_COUNT && finished ==0; i++){
            if(indirect_blocks[i]!= 0){
                void * indirect_block_ptr = NULL;
                cache_get_block(file_sys.cache, file_sys.data_blk_pos + indirect_blocks[i] * CACHE_BLKSZ, &indirect_block_ptr);
                uint32_t * direct_blocks = indirect_block_ptr;
                for(int j =0; j < KTFS_NUM_INDIRECT_BLOCKS_COUNT&& finished ==0; j++){
                    if(direct_blocks[j] != 0){
                        ktfs_update_bitmap(direct_blocks[j],0);
                    }else{
                        memset(direct_blocks, 0, KTFS_NUM_INDIRECT_BLOCKS_COUNT);
                        finished = 1;
                        break;
                    }
                }
                ktfs_update_bitmap(indirect_blocks[i],0);
                iowriteat(file_sys.vioblk,file_sys.data_blk_pos + indirect_blocks[i] * CACHE_BLKSZ , indirect_block_ptr, CACHE_BLKSZ);
                cache_release_block(file_sys.cache, indirect_block_ptr, 0);
            }else{
                finished = 1;
                memset(indirect_blocks, 0, KTFS_NUM_INDIRECT_BLOCKS_COUNT);
                ktfs_update_bitmap(target_inode->dindirect[0],0);
                break;
            }
        }
        iowriteat(file_sys.vioblk,file_sys.data_blk_pos + target_inode->dindirect[1] * CACHE_BLKSZ , dindirect_block_ptr, CACHE_BLKSZ);
        cache_release_block(file_sys.cache, dindirect_block_ptr, 0);
    
    }

    //free the indirect and double indirect blocks 
    if(target_inode->indirect!= 0){
        ktfs_update_bitmap(target_inode->indirect,0);
    }
    if(target_inode->dindirect[0] != 0){
        ktfs_update_bitmap(target_inode->dindirect[0],0);
    }
    if(target_inode->dindirect[1] != 0){
        ktfs_update_bitmap(target_inode->dindirect[1],0);
    }
    

    target_inode->indirect = 0;
    target_inode->dindirect[0] = 0;
    target_inode->dindirect[1] = 0;
    target_inode->size = 0;
    target_inode->block[0] = 0;
    target_inode->block[1] = 0;
    target_inode->block[2] = 0;
    //force a write to disk
    iowriteat(file_sys.vioblk,file_sys.inode_blk_pos + inode_block * CACHE_BLKSZ , data_inodes, CACHE_BLKSZ);
    cache_release_block(file_sys.cache, data_inodes, 1);
    //cache_flush(file_sys.cache);
    return 0;
}


//==============================================================================================
// long ktfs_writeat(struct io* io, unsigned long long pos, const void *buf, long len)
// inputs: struct io* io: io for the file
//         unsigned long long pos: position to start writing
//         const void *buf: data to write
//         long len: length of data to write
// outputs: long: number of bytes successfully written
//              EINVAL: if input is invalid or block mapping fails
// description:
//     writes up to `len` bytes to the file starting at position `pos`. 
//     uses ktfs_get_data_block to get the pointer to the data
//==============================================================================================

long ktfs_writeat(struct io* io, unsigned long long pos, const void *buf, long len){
    struct ktfs_file *fd = (struct ktfs_file *)((char *)io - offsetof(struct ktfs_file, io));
    if (fd == NULL || buf == NULL || len <= 0) {
        return -EINVAL;
    }
    if (pos >= fd->size) {
        return -EINVAL;
    }
    if(pos+ len >= fd->size){ //truncate length if too long
        len = fd->size - pos;
    }

    //find the inode
    char * new_buf = (char *)buf;
    uint16_t index = fd->dentry->inode;
    void * inodes = NULL;
    int inode_num = index / (CACHE_BLKSZ / sizeof(struct ktfs_inode));
    int inode_offset = index % (CACHE_BLKSZ / sizeof(struct ktfs_inode));
    cache_get_block(file_sys.cache, file_sys.inode_blk_pos + inode_num * CACHE_BLKSZ, &inodes);
    struct ktfs_inode * actual_inodes = inodes; 
    struct ktfs_inode target_inode = actual_inodes[inode_offset];
    cache_release_block(file_sys.cache, inodes, 0);

    // find the block number to write
    uint32_t block_num = pos / KTFS_BLKSZ;
    uint32_t block_offset = pos % KTFS_BLKSZ;

    long long bits_wrote = 0;
    // write the block, until reach the len
    while (bits_wrote < len) {
       void *block = NULL;
       uint32_t i = ktfs_get_data_block(block_num, &target_inode, &block); //use this function to get the actual data block
       if(i <0){
        return -EIO;
       }
        // read the data from the block
        char * actual_block = block;
        if(len - bits_wrote < KTFS_BLKSZ-block_offset){
            uint32_t copy_length = len - bits_wrote;
            memcpy(actual_block + block_offset, new_buf+bits_wrote, copy_length);
            bits_wrote += copy_length;
        }else{
            uint32_t copy_length = KTFS_BLKSZ - block_offset;
            memcpy(actual_block + block_offset, new_buf+bits_wrote, copy_length);
            bits_wrote += copy_length;
           // cache_release_block(file_sys.cache, block, 0);
        }
        cache_release_block(file_sys.cache, block, 1);//releast with dirty since we wrote to it
        block_offset = 0; 
        block_num++;
    
    }

    return bits_wrote;

}

//==============================================================================================
// int ktfs_cntl(struct io *io, int cmd, void *arg)
// inputs: struct io * io: io
//         int cmd:control command 
//         void * arg: arg
// outputs: int 0:success
//              EINVAL: invalid input
//              ENOTSUP: command is unsupported
// description:
//     handles control commands.
//==============================================================================================

int ktfs_cntl(struct io *io, int cmd, void *arg)
{
    struct ktfs_file * const fd = (void*)io - 
        offsetof(struct ktfs_file, io);

    if (!fd) return -EINVAL;

    switch (cmd) {
    case IOCTL_GETEND:
		    *(uint32_t*)arg = fd->size;
        return 0;
    case IOCTL_GETBLKSZ:
        return 1;
    case IOCTL_GETPOS:
        *(uint32_t*)arg = fd->pos;
        return 0;
    case IOCTL_SETEND: //calls ktfs_add_new_block to extend length, but have to make sure the length is valid
        if(arg == NULL){
            return -EINVAL;



            
        }
        if(*(uint32_t*)arg <= fd->size){
            return -EINVAL;
        }
        return (ktfs_add_new_block(io, arg) >= 0) ? 0 : -EINVAL;

    case IOCTL_SETPOS:
        if (*(uint32_t*)arg > fd->size) {
            return -EINVAL;
        }
        fd->pos = *(uint32_t*)arg;
        return 0;
    default:
        return -ENOTSUP;
    }
    return 0;
}
//==============================================================================================
// int ktfs_flush(void)
// inputs: None
// outputs: int 0:success
//              EINVAL: if cache is NULL or flush fail
// description:
//     flushes all dirty cached blocks back to disk using the cache_flush function.
//==============================================================================================

int ktfs_flush(void)
{
    if (file_sys.cache == NULL) {
        return -EINVAL;
    }
    if (cache_flush(file_sys.cache) != 0) {
        return -EINVAL;
    }
    return 0;
}



//==============================================================================================
// uint32_t ktfs_get_data_block(uint32_t block_num, struct ktfs_inode* target_inode, void **block)
// inputs: uint32_t block_num: block number relative to file
//         struct ktfs_inode *target_inode: inode of the file
//         void **block: output buffer for block
// outputs: 0 if success, negative on error
// description:
//     gets the pointer to a data block given a file block number.
//==============================================================================================


uint32_t ktfs_get_data_block(uint32_t block_num, struct ktfs_inode* target_inode, void **block){
    // if in direct block
    if(block_num < KTFS_NUM_DIRECT_DATA_BLOCKS_COUNT){
        cache_get_block(file_sys.cache, file_sys.data_blk_pos + target_inode->block[block_num] * CACHE_BLKSZ, block);
       // cache_release_block(file_sys.cache, block, 0);
        return 0;
    // if in indirect block
    }else if (block_num < KTFS_NUM_DIRECT_DATA_BLOCKS_COUNT + KTFS_NUM_INDIRECT_BLOCKS_COUNT){
        void * indirect_block_ptr = NULL;
        cache_get_block(file_sys.cache, file_sys.data_blk_pos + target_inode->indirect * CACHE_BLKSZ, &indirect_block_ptr);
        uint32_t * direct_blocks = indirect_block_ptr;
        uint32_t blk_index = block_num - KTFS_NUM_DIRECT_DATA_BLOCKS_COUNT;
        cache_get_block(file_sys.cache, file_sys.data_blk_pos + direct_blocks[blk_index] * CACHE_BLKSZ, block);
        cache_release_block(file_sys.cache, indirect_block_ptr, 0);
       // cache_release_block(file_sys.cache, block, 0);
        return 0;
    // if in doubly indirect block
    }else if (block_num < KTFS_NUM_DIRECT_DATA_BLOCKS_COUNT + KTFS_NUM_INDIRECT_BLOCKS_COUNT + 2 * KTFS_NUM_DINDIRECT_BLOCKS_COUNT){
        // check if the blocknum is in the second dindirect block
        void * dindirect_block_ptr = NULL;
        void * indirect_block_ptr = NULL;
        if (block_num>= KTFS_NUM_DIRECT_DATA_BLOCKS_COUNT + KTFS_NUM_INDIRECT_BLOCKS_COUNT + KTFS_NUM_DINDIRECT_BLOCKS_COUNT){
            cache_get_block(file_sys.cache, file_sys.data_blk_pos + target_inode->dindirect[1] * CACHE_BLKSZ, &dindirect_block_ptr);
            uint32_t * indirect_blocks = dindirect_block_ptr;
            uint32_t indir_blk_index = (block_num - KTFS_NUM_DIRECT_DATA_BLOCKS_COUNT - KTFS_NUM_INDIRECT_BLOCKS_COUNT - KTFS_NUM_DINDIRECT_BLOCKS_COUNT) / KTFS_NUM_INDIRECT_BLOCKS_COUNT;
            cache_get_block(file_sys.cache, file_sys.data_blk_pos + indirect_blocks[indir_blk_index] * CACHE_BLKSZ, &indirect_block_ptr);
            uint32_t * direct_blocks = indirect_block_ptr;
            uint32_t blk_index = (block_num - KTFS_NUM_DIRECT_DATA_BLOCKS_COUNT - KTFS_NUM_INDIRECT_BLOCKS_COUNT - KTFS_NUM_DINDIRECT_BLOCKS_COUNT) % KTFS_NUM_INDIRECT_BLOCKS_COUNT;
            cache_get_block(file_sys.cache, file_sys.data_blk_pos + direct_blocks[blk_index] * CACHE_BLKSZ, block);
           
           // cache_release_block(file_sys.cache, block, 0);
        }else{
            cache_get_block(file_sys.cache, file_sys.data_blk_pos + target_inode->dindirect[0] * CACHE_BLKSZ, &dindirect_block_ptr);
            uint32_t * indirect_blocks = dindirect_block_ptr;
            uint32_t indir_blk_index = (block_num - KTFS_NUM_DIRECT_DATA_BLOCKS_COUNT - KTFS_NUM_INDIRECT_BLOCKS_COUNT) / KTFS_NUM_INDIRECT_BLOCKS_COUNT;
            cache_get_block(file_sys.cache, file_sys.data_blk_pos + indirect_blocks[indir_blk_index] * CACHE_BLKSZ, &indirect_block_ptr);
            uint32_t * direct_blocks = indirect_block_ptr;
            uint32_t blk_index = (block_num - KTFS_NUM_DIRECT_DATA_BLOCKS_COUNT - KTFS_NUM_INDIRECT_BLOCKS_COUNT) % KTFS_NUM_INDIRECT_BLOCKS_COUNT;
            cache_get_block(file_sys.cache, file_sys.data_blk_pos + direct_blocks[blk_index] * CACHE_BLKSZ, block);
            //  cache_release_block(file_sys.cache, block, 0);
        }
        cache_release_block(file_sys.cache, dindirect_block_ptr, 0);
        cache_release_block(file_sys.cache, indirect_block_ptr, 0);
        return 0;
    }else{
        return -EINVAL;
    }

}


//==============================================================================================
// int ktfs_add_new_block(struct io *io, void *arg)
// inputs: struct io * io: io pointer of the file
//         void *arg: new file size
// outputs: new file size or error code
// description:
//     adds new data blocks to a file to extend its size. Updates bitmap and use iowrite to persist
//     on disk immediately
//==============================================================================================


int ktfs_add_new_block(struct io *io, void *arg){
    if(io == NULL || arg == NULL){
        return -EINVAL;
    }
    struct ktfs_file * const fd = (void*)io - offsetof(struct ktfs_file, io);
    uint32_t new_pos = *(uint32_t*) arg;
    if(new_pos <= fd->size){
        return -EINVAL;
    }
    if(new_pos >= KTFS_MAX_FILE_SIZE){
        new_pos = KTFS_MAX_FILE_SIZE;
    }

    //if the length to be extended is contained in the current blocks, we only change the size
    if((fd->size / KTFS_BLKSZ) == (new_pos - 1) / KTFS_BLKSZ && fd->size!= 0){
        fd->size = new_pos;
        
        uint16_t index = fd->dentry->inode;
        void * inodes = NULL;
        int inode_num = index / (CACHE_BLKSZ / sizeof(struct ktfs_inode));
        int inode_offset = index % (CACHE_BLKSZ / sizeof(struct ktfs_inode));
        cache_get_block(file_sys.cache, file_sys.inode_blk_pos + inode_num * CACHE_BLKSZ, &inodes);
        struct ktfs_inode *actual_inodes = inodes;
        actual_inodes[inode_offset].size = new_pos;
        iowriteat(file_sys.vioblk,file_sys.inode_blk_pos + inode_num * CACHE_BLKSZ , inodes, CACHE_BLKSZ);
        cache_release_block(file_sys.cache, inodes, 0);
        return 0;
    }


    int block_needed = (new_pos + KTFS_BLKSZ - 1) / KTFS_BLKSZ - (fd->size + KTFS_BLKSZ - 1) / KTFS_BLKSZ;
    if(block_needed <= 0){
        return -EINVAL;
    }
    //calculate the current block number
    int curr_block_num = (fd->size + KTFS_BLKSZ - 1) / KTFS_BLKSZ;
    int block_fetched = 0;
    //find the inode of the file
    uint16_t index = fd->dentry->inode;
    void * inodes = NULL;
    int inode_num = index / (CACHE_BLKSZ / sizeof(struct ktfs_inode));
    int inode_offset = index % (CACHE_BLKSZ / sizeof(struct ktfs_inode));
    cache_get_block(file_sys.cache, file_sys.inode_blk_pos + inode_num * CACHE_BLKSZ, &inodes);
    struct ktfs_inode *actual_inodes = inodes; 
    struct ktfs_inode *target_inode = &actual_inodes[inode_offset];
    
    uint32_t *indirect = NULL;
    uint32_t *data_blocks = NULL;
    uint32_t *indirect_blocks=NULL;

    int finished = 0;
    //loop continues as long as there is available block in bitmap and we still need more blocks
    while(block_fetched < block_needed && finished == 0){
        int new_block = ktfs_update_bitmap(0, 1);//get empty block num
        if(new_block < 0){
            finished = 1;
            break;
        }
        //if length to be extend can be saved in direct
        if(curr_block_num < KTFS_NUM_DIRECT_DATA_BLOCKS_COUNT){
            target_inode->block[curr_block_num] = new_block;
        }else if(curr_block_num < KTFS_NUM_DIRECT_DATA_BLOCKS_COUNT + KTFS_NUM_INDIRECT_BLOCKS_COUNT){ //if length to be extend can be saved in indirect 
            if(target_inode->indirect == 0){//first check if we have block for indirect
                int indirect_block = ktfs_update_bitmap(0, 1);
                if(indirect_block < 0) {
                    finished = 1;
                    break;
                }
                target_inode->indirect = indirect_block;
            }
            void *indirect_ptr = NULL;
            cache_get_block(file_sys.cache, file_sys.data_blk_pos + target_inode->indirect * CACHE_BLKSZ, &indirect_ptr);
            indirect = (uint32_t *)indirect_ptr;
            indirect[curr_block_num - KTFS_NUM_DIRECT_DATA_BLOCKS_COUNT] = new_block;
            iowriteat(file_sys.vioblk,file_sys.data_blk_pos + target_inode->indirect * CACHE_BLKSZ , indirect_ptr, CACHE_BLKSZ);
            cache_release_block(file_sys.cache, indirect_ptr, 0);
        }else{//if length to be extend can be saved in double indirect
            int dblk_index = curr_block_num - (KTFS_NUM_DIRECT_DATA_BLOCKS_COUNT + KTFS_NUM_INDIRECT_BLOCKS_COUNT);
            int outer = dblk_index / KTFS_NUM_DINDIRECT_BLOCKS_COUNT;
            if(outer > 1){ // we only have 2 double indirect
                return -EINVAL;
            }else if(outer == 1){// if double indirect[1], we minus everything before to make it behave like double indirect[0]
                dblk_index = dblk_index - KTFS_NUM_DINDIRECT_BLOCKS_COUNT;
            }
            int indirect_index = dblk_index / KTFS_NUM_INDIRECT_BLOCKS_COUNT;
            int direct_index = dblk_index % KTFS_NUM_INDIRECT_BLOCKS_COUNT;
            if(target_inode->dindirect[outer] == 0){//first check if we have block for double indirect
                int dindirect_block = ktfs_update_bitmap(0, 1);
                if (dindirect_block < 0) {
                    finished = 1;
                    break;
                }
                target_inode->dindirect[outer] = dindirect_block;
            }
            void *dindirect_ptr = NULL;
            cache_get_block(file_sys.cache, file_sys.data_blk_pos + target_inode->dindirect[outer] * CACHE_BLKSZ, &dindirect_ptr);
            indirect_blocks = (uint32_t *)dindirect_ptr;
            if(indirect_blocks[indirect_index] == 0){//second check if we have block for indirect who is in double indirect
                int new_indirect = ktfs_update_bitmap(0, 1);
                if (new_indirect < 0) {
                    cache_release_block(file_sys.cache, dindirect_ptr, 0);
                    finished = 1;
                    break;
                }
                indirect_blocks[indirect_index] = new_indirect;
                iowriteat(file_sys.vioblk,file_sys.data_blk_pos + target_inode->dindirect[outer] * CACHE_BLKSZ , dindirect_ptr, CACHE_BLKSZ);
                cache_release_block(file_sys.cache, dindirect_ptr, 0);
            }else{
                cache_release_block(file_sys.cache, dindirect_ptr, 0);
            }
            void *indirect =NULL;
            cache_get_block(file_sys.cache, file_sys.data_blk_pos + indirect_blocks[indirect_index] * CACHE_BLKSZ, &indirect);
            data_blocks = (uint32_t *)indirect;
            data_blocks[direct_index] = new_block;
            iowriteat(file_sys.vioblk,file_sys.data_blk_pos + indirect_blocks[indirect_index] * CACHE_BLKSZ , indirect, CACHE_BLKSZ);
            cache_release_block(file_sys.cache, indirect, 0);
        }
        curr_block_num++;
        block_fetched++;
    }

    //if we were able to extend the length to the asked length, we update size and return
    if( block_fetched == block_needed){
        target_inode->size = new_pos;
        fd->size = target_inode->size;
        iowriteat(file_sys.vioblk,file_sys.inode_blk_pos + inode_num * CACHE_BLKSZ , inodes, CACHE_BLKSZ);
        cache_release_block(file_sys.cache, inodes, 0);
        return new_pos;
    }
    //if we can only get block_fetched number of blocks, update size accordingly and return
    int new_size = block_fetched * KTFS_BLKSZ; 
    target_inode->size += new_size;
    fd->size = target_inode->size;
    iowriteat(file_sys.vioblk,file_sys.inode_blk_pos + inode_num * CACHE_BLKSZ , inodes, CACHE_BLKSZ);
    cache_release_block(file_sys.cache, inodes, 0);
    return new_size;
}



//==============================================================================================
// int ktfs_update_bitmap(uint32_t block_num, int delete_or_add)
// inputs: uint32_t block_num: block to free or allocate
//         int delete_or_add: 0 to free block, 1 to allocate
// outputs: updated block number or error
// description:
//     updates the bitmap to allocate or free a block or update the bitmap to free a block
//==============================================================================================


int ktfs_update_bitmap(uint32_t block_num, int delete_or_add){
    if(delete_or_add == 0){//if we want to free a block
    
    
        block_num = 1 + file_sys.super.bitmap_block_count+file_sys.super.inode_block_count+ block_num;
        int bitmap_block_num = block_num / (KTFS_BLKSZ*8);
        int bit_index = block_num% (KTFS_BLKSZ*8);
            
        void *bitmap_block = NULL;
        cache_get_block(file_sys.cache, KTFS_BLKSZ+bitmap_block_num * KTFS_BLKSZ, &bitmap_block);
        uint8_t *bitmap = (uint8_t*)bitmap_block;
        bitmap[bit_index/8] &= ~(1<<(bit_index%8));//zero it
        iowriteat(file_sys.vioblk,KTFS_BLKSZ+bitmap_block_num * KTFS_BLKSZ , bitmap_block, CACHE_BLKSZ);//force to disk
        cache_release_block(file_sys.cache, bitmap_block, 0);
        return 0;
    }else{//if we want to find an empty block
        int finished =0;
        uint32_t num_of_blocks = 0;
        for(int i = 0; i < file_sys.super.bitmap_block_count; i++) {//loop over # of bitmap blocks
            void * bitmap_block = NULL;
            cache_get_block(file_sys.cache, KTFS_BLKSZ + i* KTFS_BLKSZ, &bitmap_block);
            uint8_t *bitmap =(uint8_t*) bitmap_block;
            for(int j = 0; j < KTFS_BLKSZ * 8 ; j++) { //loop over all bits in a bitmap block
                int byte_index = j / 8;
                int bit_offset = j % 8;
                int bit = bitmap[byte_index] >> bit_offset & 1;
                if(bit ==0){//if not used
                    num_of_blocks = j + i * (CACHE_BLKSZ * 8);
                    if(num_of_blocks < file_sys.data_blk_pos / KTFS_BLKSZ){
                        continue; 
                    }
                    num_of_blocks -= file_sys.data_blk_pos / KTFS_BLKSZ; //calculate the block's data block number
                    bitmap[byte_index] |= (1 << bit_offset); //set to one
                    iowriteat(file_sys.vioblk,KTFS_BLKSZ + i* KTFS_BLKSZ , bitmap_block, CACHE_BLKSZ);
                    cache_release_block(file_sys.cache, bitmap_block, 0);
                    finished = 1;
                    break;
                }
            }
            if(finished == 1){
                break;
            }
            cache_release_block(file_sys.cache, bitmap_block, 0);
        }
        if(finished == 0){
            return -EINVAL;
        }
        return num_of_blocks;
    }
}


