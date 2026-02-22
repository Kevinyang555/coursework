#include <string.h>
#include <assert.h>
#include "cache.h"
#include "io.h" 
#include "error.h"
#include "heap.h"
#include "string.h"
#include "riscv.h"
#include "intr.h"
#include "memory.h"
#include "thread.h"

#define CACHE_BLOCKS 64

struct cache_block {
    unsigned long long pos;         // Position in backing device
    int dirty;                      // CACHE_CLEAN or CACHE_DIRTY
    long count;                     // Reference count or usage flag
    char block[CACHE_BLKSZ];        // Pointer to cached block
    struct lock cnm;
    struct cache_block *next;
};

// Definition of the cache structure
struct cache {
    struct io *bkgio;              // Backing I/O device
    struct cache_block *block_list;
    int blkcnt;  
};
//==================================================================================================
// int create_cache(struct io * bkgio, struct cache ** cptr)
// inputs:
//     struct io * bkgio:pointer to io.
//     struct cache ** cptr:pointer to a cache pointer.
// outputs:
//     int: 0 :success
//          -EINVAL :invalid input
// description:
//     allocates and initializes a cache structure.
//==================================================================================================

int create_cache(struct io * bkgio, struct cache ** cptr) {
    // Implementation stub
    if (bkgio == NULL || cptr == NULL)
        return -EINVAL;

    struct cache *c = kcalloc(1, sizeof(struct cache));
    c->bkgio = ioaddref(bkgio);  
    c->blkcnt = 0;

    c->block_list = NULL;
    *cptr = c;

    return 0;
}
//==================================================================================================
// int cache_get_block(struct cache * cache, unsigned long long pos, void ** pptr)
// inputs:
//     struct cache * cache:pointer to cache structure.
//     unsigned long long pos:position in the device.
//     void ** pptr:output pointer.
// outputs:
//     int: 0: sucess
//          -EINVAL: invalid input or pos is not aligned
// description:
//     retrieves a block from cache based on the input position. 
//==================================================================================================

int cache_get_block(struct cache * cache, unsigned long long pos, void ** pptr) {
    // Implementation stub
    if( cache == NULL){
        return -EINVAL;
    }
    if(pos <= 0){
        return -EINVAL;
    }

    if(pos % CACHE_BLKSZ != 0){
        return -EINVAL;
    }

    struct cache_block *curr = cache->block_list; 
    if(curr != NULL){
        while(curr != NULL){
            if(curr->pos == pos) {
               lock_acquire(&curr->cnm);
                curr->count++;
                *pptr = curr->block;
                return 0;
            }
            curr = curr->next;
        }
    }

    curr = cache->block_list;
    if(cache->blkcnt >= 64 && curr != NULL){
        int min = curr->count;
        struct cache_block *evict = curr;
        while(curr != NULL){
            if(curr->count <= min && curr->cnm.tid == -1){
                min = evict->count;
                evict = curr;
            }
            curr = curr->next;
        }
       
        lock_acquire(&evict->cnm);
        if(evict->dirty == CACHE_DIRTY){
            iowriteat(cache->bkgio, evict->pos, evict->block, CACHE_BLKSZ);
        }
        evict->pos = pos;
        evict->count =1;
        int read = ioreadat(cache->bkgio, pos, evict->block, CACHE_BLKSZ);
        if(read <=0){
            lock_release(&evict->cnm);
            evict->count--;
            return -EINVAL;
        }
        *pptr = evict->block;
        return 0;

    }else{
        struct cache_block *mem_block = kcalloc(1, sizeof(struct cache_block));
        lock_init(&mem_block->cnm);
        lock_acquire(&mem_block->cnm);
        if(curr == NULL){
            cache->block_list = mem_block;
        }else{
            while(curr->next!=NULL){
                curr = curr->next;
            }
            curr->next = mem_block;
        }
        mem_block-> pos = pos;
        mem_block->count = 1;
        int read = ioreadat(cache->bkgio, pos, mem_block->block, CACHE_BLKSZ);

        if(read <= 0){
            return -EINVAL;
        }
        *pptr = mem_block-> block;
        cache->blkcnt++;
    }
    return 0;
}
//==================================================================================================
// void cache_release_block(struct cache * cache, void * pblk, int dirty)
// inputs:
//     struct cache * cache:pointer to the cache.
//     void * pblk:pointer to a block.
//     int dirty:indicates whether the block has been modified.
// Outputs:none
// description:
//     releases the lock on the block previously acquired. 
//==================================================================================================

extern void cache_release_block(struct cache * cache, void * pblk, int dirty){
    struct cache_block *curr = cache->block_list;

    // woohoo
    while(curr !=NULL){
        if (curr->block == pblk){
            if (dirty == CACHE_DIRTY){
                curr->dirty = CACHE_DIRTY;
            }
            lock_release(&curr->cnm);
            return;
        }
        curr = curr->next;
    }
}
//==================================================================================================
// int cache_flush(struct cache * cache)
// inputs:
//     struct cache * cache:pointer to the cache.
// outputs:
//     int: 0:success
//          -EINVAL:invalid input
// description:
//     writes all dirty blocks in the cache back to the device, mark as clean.
//==================================================================================================

extern int cache_flush(struct cache * cache){
    if (cache == NULL)
        return -EINVAL;

    struct cache_block *curr = cache->block_list;
    while(curr !=NULL){
        if (curr->dirty == CACHE_DIRTY){
            // Write the block back to the backing device
            int ret = iowriteat(cache->bkgio, curr->pos, curr->block, CACHE_BLKSZ);
            if (ret < 0)
                return -EINVAL;
            curr->dirty = CACHE_CLEAN; // Mark as clean after writing
        }
        curr = curr->next;
    }
    //curr = cache->block_list;
    // while(curr != NULL){
    //     struct cache_block *prev = curr;
    //     curr = curr->next;
    //     kfree(prev);
    // }
    // // Flush the cache
    // kfree(cache);
    return 0;
}