/**
 * mm.c
 * 
 * The implementation is based on segregated list.
 * 
 * The heads of the seg lists are stored at the beginning of the heap.
 * 
 * There are 12 seg lists. The size lower bound for list i is 
 * 2^(5 + i) bytes.
 * 
 * Each list head is augmented with the max size in that list. So,
 * when searching for free blocks, it can skip the list where the max
 * size is smaller than the target size.
 * 
 * Malloc searches the lists and use the first fit. Free does coalescing
 * and put the block into the seg list based on the size. A approximate
 * ascending order of the lists are maintained.
 * 
 * Each data block is preceeded by a 4-byte header and followed by a 4-byte
 * footer, which recording the size and the allocation status of the block 
 * to facilitate the lookup. These two pieces of information are packed 
 * into a int. As the blocks are all 8-byte aligned, so the low 3 bits of
 * the size are always 0. Thus, we can use the last bit as the allocation
 * status bit.
 * 
 * Name: Liruoyang Yu
 * AndrewId: liruoyay
 * 
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

typedef unsigned long long int_p;

//#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
# define checkheap(...) mm_checkheap(__VA_ARGS__)
#else
# define dbg_printf(...)
# define checkheap(...)
#endif

/* for running the test program */
/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* word size */
#define WSIZE 4
#define BLKSIZE 8
/* min block size */
#define MINSIZE 16
/* number of seg lists */
#define SEGLISTNUM 12
/* initial heap size (bytes) */
#define HEAPINITSIZE  (BLKSIZE * 128)
/* extend heap by this amount (bytes) */
#define HEAPEXTSIZE  (BLKSIZE * 64)
/* blocks smaller that 2^5 bytes are in the first seg list */
#define LOWER_BOUND_OF_SEGLIST 5

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

/**********************
 * start helper macros 
 **********************/
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* get the pointer packed in the low 32 bits of l */
#define LOWHALFP(l) (!(l) ? 0 : \
            heap_hd_p + ((int_p)(l) & (((int_p)1 << 32) - 1)))
/* get the value packed in the high 32 bits of l */
#define HIGHHALF(l) (((int_p)(l) >> 32) & (((int_p)1 << 32) - 1))
/* pack two values into one */
#define PACK(size, alloc)  ((size) | (alloc))
/* pack two values into a 64-bit number */
#define PACK64(high, low) (((int_p)(high) << 32) | (int_p)(low))

/* put the value as unsigned int at position p */
#define PUT(p, v) (*(unsigned int *)(p) = (v))
/* get the value stored at position p as unsigned int */
#define GET(p) (*(unsigned int *)(p))
/* put the value as signed int at position p */
#define PUT_I(p, v) (*(int *)(p) = (v))
/* get the value stored at position p as signed int */
#define GET_I(p) (*(int *)(p))

/* get the size packed at p */
#define GET_SIZE(p) (GET(p) & ~0x7)
/* get the header pointer given block pointer */
#define HD_P(p) ((char *)(p) - WSIZE)
/* get the size of block pointed by p */
#define SIZE(p) (GET(HD_P(p)) & ~0x7)
/* get the footer pointer given block pointer */
#define FT_P(p) ((char *)(p) + SIZE(p) - BLKSIZE)
/* get the footer pointer of the previous block given block pointer */
#define PREV_FT_P(p) ((char *)(p) - BLKSIZE)
/* get the pointer of the next block given block pointer */
#define NEXT_P(p) ((char *)(p) + SIZE(p))
/* get the pointer of the previous block given block pointer */
#define PREV_P(p) ((char *)(p) - GET_SIZE(PREV_FT_P(p)))

/* if the block is allocated */
#define IS_AL(p) (GET(HD_P(p)) & 0x1)
/* if the previous block is allocated */
#define IS_PRE_AL(p) (IS_AL(PREV_P(p)))
/* if the next block is allocated */
#define IS_NEXT_AL(p) (IS_AL(NEXT_P(p)))

/* convert offset to pointer */
#define PTR_TO_OFFSET(p) ((int) (!(p) ? 0 : (char *)(p) - heap_hd_p))
/* convert pointer to offset */
#define OFFSET_TO_PTR(off) ((off) ? (heap_hd_p + (off)) : NULL)

/* get the next free block in the seg list */
#define NEXT_FR_P(p) (OFFSET_TO_PTR(GET_I(p)))
/* get the previous free block in the seg list */
#define PREV_FR_P(p) (OFFSET_TO_PTR(GET_I((char *)(p) + WSIZE)))
/* set the next free block in the seg list */
#define PUT_NEXT_FR_P(p, ad) (PUT_I(p, PTR_TO_OFFSET(ad)))
/* set the previous free block in the seg list */
#define PUT_PREV_FR_P(p, ad) (PUT_I((char *)(p) + WSIZE, PTR_TO_OFFSET(ad)))

/* set the header value */
#define PUT_HD(p, v) (PUT(HD_P(p), (v)))
/* set the footer value */
#define PUT_FT(p, v) (PUT(FT_P(p), (v)))

/* if the two blocks with size s1 and s2 in the same seg list */
#define SAME_LIST(s1, s2) (((s1) ^ (s2)) <= (s1) && ((s1) ^ (s2)) <= (s2))
/**********************
 * end helper macros 
 **********************/

/*************************
 * begin helper functions 
 *************************/
void mm_checkheap(int lineno);
static inline void *extend_heap();
static inline int in_heap(const void *p);
static inline size_t split(char *p, size_t size);
static inline size_t compute_size(size_t size);
static inline char **get_seglist_p(void* p);
static inline char **get_seglist_p_by_sz(size_t sz);
static inline void rm_from_freelist(void* p);
static inline void add_to_freelist(void* p);
static inline void mv_link(void *from, void *to);
static inline char *new_seg_head(size_t sz, void *p);
/*************************
 * end helper functions 
 *************************/

/*************************
 * begin global variables 
 *************************/
/* pointer to the start of the data area of the heap */
static char *heap_hd_p = 0;
static long int heapsize = 0;
/* pointer to the first data block */
static char *firstblk;
/* seg list head array */
static char **seglist;
/*************************
 * end global variables 
 *************************/

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
    /* reset heap and globals */
    heap_hd_p = 0;
    heapsize = 0;
    firstblk = NULL;
    mem_reset_brk();
    
    /* Create the initial empty heap */
    if ((heap_hd_p = mem_sbrk(HEAPINITSIZE)) == (void *) -1) 
        return -1;
    /* init seg lists */
    memset(heap_hd_p, 0, BLKSIZE * SEGLISTNUM);
    
    /* init seg list */
    seglist = (char **)heap_hd_p;
    
    /* area before heap_hd_p is for seg list pointers */
    heap_hd_p += BLKSIZE * SEGLISTNUM;
    
    /* prologue block */
    PUT(heap_hd_p, PACK(0, 1));
    /* head pointer to the data part */
    firstblk = heap_hd_p + BLKSIZE;
    /* effective heap size */
    heapsize += HEAPINITSIZE - BLKSIZE * SEGLISTNUM;
    /* init the first free block */
    PUT_HD(firstblk, PACK(heapsize - BLKSIZE, 0));
    PUT_FT(firstblk, PACK(heapsize - BLKSIZE, 0));
    /* epilogue block */
    PUT_HD(NEXT_P(firstblk), PACK(0, 1));

    /* init first free blk */
    PUT_NEXT_FR_P(firstblk, NULL);
    PUT_PREV_FR_P(firstblk, NULL);
    add_to_freelist(firstblk);

    dbg_printf("data heap starts from %p\n", heap_hd_p);
    dbg_printf("data blk starts from %p\n", firstblk);
    return 0;
}

/*
 * malloc
 */
void *malloc(size_t size) {
    dbg_printf("malloc %d\n", (int)size);
    dbg_printf("heapsize %d\n", (int)heapsize);
    
    size = compute_size(size);
    /* size of fit block */
    size_t sz = 0;
    char *curptr = NULL;

    int i;
    for (i = 0; i < SEGLISTNUM && !curptr; i++) {
        /* max size in the list smaller than the target size */
        if (HIGHHALF(seglist[i]) < size) {
            continue;
        }
        dbg_printf("checking list %p\n", seglist + i);
        /* first element in this seg list */
        curptr = LOWHALFP(seglist[i]);
        while (curptr) {
            sz = (size_t) SIZE(curptr);
            if (sz >= size) {
                dbg_printf("found block size %d\n", (int)sz);
                break;
            }
            curptr = NEXT_FR_P(curptr);
        }
    }
    /* haven't found fit block, extend the heap */
    if (!curptr) {
        if ((curptr = (char *)extend_heap(MAX(size, HEAPEXTSIZE)))
            == (char *) -1) {
            return NULL;
        }
        sz = SIZE(curptr);
        heapsize += sz;
        dbg_printf("extended heap by %d\n", (int)sz);
    }
    
    sz = split(curptr, size);
    
    /* update the size and allocation status */
    PUT_HD(curptr, PACK(sz, 1));
    PUT_FT(curptr, PACK(sz, 1));
    
    checkheap(__LINE__);
    dbg_printf("return %p\n", curptr);
    return (void *) curptr;
}

/*
 * free
 */
void free(void *p) {
    dbg_printf("free %p\n", p);
    if(!p || !in_heap(p)) return;
    size_t size = SIZE(p);
    /* pointer to the resulting coalesced free block */
    char *freeptr;
    /* size of the resulting coalesced free block */
    size_t newsize;
    /* contiguous blocks */
    char *prev;
    char *next;
    
    /***********************
     * start coalesce 
     ***********************/
    /* allo | free | allo */
    if (IS_PRE_AL(p) && IS_NEXT_AL(p)) {
        dbg_printf("free case 1\n");
        freeptr = p;
        newsize = SIZE(p);
        /* just add the free blk to the free list */
        PUT_HD(freeptr, PACK(newsize, 0));
        PUT_FT(freeptr, PACK(newsize, 0));
        add_to_freelist(freeptr);
    }
    /* free | free | free */
    else if (!IS_PRE_AL(p) && !IS_NEXT_AL(p)) {
        dbg_printf("free case 2\n");
        prev = PREV_P(p);
        next = NEXT_P(p);
        freeptr = prev;
        size_t prevsz = SIZE(prev);
        /* concat the 3 parts */
        newsize = prevsz + size + SIZE(next);
        PUT_HD(freeptr, PACK(newsize, 0));
        PUT_FT(freeptr, PACK(newsize, 0));
        
        /* new block in the same seg list as previous block */
        if (SAME_LIST(prevsz, newsize)) {
            rm_from_freelist(next);
        }
        /* new block in the same seg list as next block */
        else if (SAME_LIST(SIZE(next), newsize)) {
            rm_from_freelist(prev);
            mv_link(next, prev);
        }
        /* new block in a different block */
        else {
            rm_from_freelist(prev);
            rm_from_freelist(next);
            add_to_freelist(freeptr);
        }
    }
    /* free | free | allo */
    else if (!IS_PRE_AL(p)) {
        dbg_printf("free case 3\n");
        prev = PREV_P(p);
        /* concat the 2 parts */
        freeptr = prev;
        size_t prevsz = SIZE(prev);
        newsize = prevsz + size;
        PUT_HD(freeptr, PACK(newsize, 0));
        PUT_FT(freeptr, PACK(newsize, 0));
        
        /* new block in the same seg list as the previous block */
        if (!SAME_LIST(prevsz, newsize)) {
            rm_from_freelist(prev);
            add_to_freelist(freeptr);
        }
    }
    /* allo | free | free */
    else {
        dbg_printf("free case 4\n");
        next = NEXT_P(p);
        freeptr = p;
        newsize = size + SIZE(next);
        PUT_HD(freeptr, PACK(newsize, 0));
        PUT_FT(freeptr, PACK(newsize, 0));
        
        /* new block in the same seg list as the next block */
        if (!SAME_LIST(SIZE(next), newsize)) {
            rm_from_freelist(next);
            add_to_freelist(freeptr);
        }
        /* same list, just move links */
        else {
            mv_link(next, freeptr);
        }
    }
    /***********************
     * end coalesce 
     ***********************/
    checkheap(__LINE__);
}

/*
 * realloc
 */
void *realloc(void *oldptr, size_t size) {
    dbg_printf("realloc %p %d\n", oldptr, (int)size);
    size_t newsize;
    size_t oldsize;
    void *newptr;

    /* if size == 0 then this is just free */
    if(size == 0) {
        free(oldptr);
        dbg_printf("realloc size 0. return 0.\n");
        return 0;
    }

    /* if oldptr is NULL, then this is just malloc */
    if(oldptr == NULL) {
        dbg_printf("realloc is just malloc.\n");
        return malloc(size);
    }

    oldsize = SIZE(oldptr);
    newsize = compute_size(size);

    if (oldsize == newsize) {
        dbg_printf("newsize == oldsize. return %p\n", oldptr);
        return oldptr;
    }

    /* shrink the payload */
    if (oldsize > newsize) {
        dbg_printf("newsize > oldsize. ");
        if (oldsize - MINSIZE < newsize) {
            dbg_printf("return %p\n", oldptr);
            newptr = oldptr;
        }
        else {
            /* update the size and allocation status */
            PUT_HD(oldptr, PACK(newsize, 1));
            PUT_FT(oldptr, PACK(newsize, 1));
            
            char* freeptr = NEXT_P(oldptr);
            PUT_HD(freeptr, PACK(oldsize - newsize, 0));
            PUT_FT(freeptr, PACK(oldsize - newsize, 0));
            add_to_freelist(freeptr);
            
            dbg_printf("added %p to free list. return %p\n", freeptr, oldptr);
            newptr = oldptr;
        }
    }
    /* extend the payload */
    else {
        char *next = NEXT_P(oldptr);
        size_t extsize = MAX(newsize - oldsize, MINSIZE);
        /* can be expand directly */
        if (!IS_AL(next) && SIZE(next) >= extsize) {
            newsize = oldsize + split(next, extsize);
            /* update the size and allocation status */
            PUT_HD(oldptr, PACK(newsize, 1));
            PUT_FT(oldptr, PACK(newsize, 1));
            dbg_printf("expanded. oldsize %d, newsize %d\n", 
                (int)oldsize, (int)newsize);
            dbg_printf("return %p, size %d\n", oldptr, (int)newsize);
            newptr = oldptr;
        }
        /* need to malloc new space */
        else {
            newptr = malloc(size);

            /* if realloc fails the original block is left untouched */
            if(!newptr) {
                return 0;
            }

            /* copy the old data */
            
            if(size < oldsize) oldsize = size;
            memcpy(newptr, oldptr, oldsize);

            /* free the old block. */
            free(oldptr);
            dbg_printf("malloc and copy. return %p\n", newptr);
            newptr = newptr;
        }
    }
    checkheap(__LINE__);
    return newptr;
}

/*
 * calloc - malloc with all bytes set to 0.
 */
void *calloc(size_t nmemb, size_t size) {
    size_t bytes = nmemb * size;
    dbg_printf("calloc %d\n", (int)bytes);
    void *p = malloc(bytes);
    memset(p, 0, bytes);

    return p;
}

/*
 * check whether the pointer is in the heap.
 */
static inline int in_heap(const void *p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Convert arbitrary size to the size that 
 * includes header footer and is aligned
 */
static inline size_t compute_size(size_t size) {
    /* increment by 1 block to account for the header and footer */
    size += BLKSIZE;
    /* align the size */
    size = MAX(ALIGN(size), MINSIZE);
    return size;
}

/*
 * split a free block based on the requested size
 */
static inline size_t split(char *curptr, size_t size) {
    size_t sz = SIZE(curptr);
    /* need splitting */
    if (sz >= size + MINSIZE) {
        /* set up new free blk */
        char *newptr = curptr + size;
        size_t newsize = sz - size;
        PUT_HD(newptr, PACK(newsize, 0));
        PUT_FT(newptr, PACK(newsize, 0));
        /* the split free block is in the same seg list */
        if (SAME_LIST(sz, newsize)) {
            /* just move the links */
            mv_link(curptr, newptr);
        }
        else {
            /* move the block to another list */
            rm_from_freelist(curptr);
            add_to_freelist(newptr);
        }
        sz = size;
    }
    /* don't need splitting */
    else {
        rm_from_freelist(curptr);
    }
    return sz;
}

/*
 * heap consistency check routine for debugging
 */
void mm_checkheap(int lineno) {
    char* curptr = firstblk;
    long freecnt_1 = 0;
    long freecnt_2 = 0;
    
    /* walk through the heap */
    while (curptr && in_heap(curptr) && SIZE(curptr) > 0) {
        if (!IS_AL(curptr)) {
            freecnt_1++;
        }
        curptr = NEXT_P(curptr);
    }
    
    /* walk through the free list */
    int i;
    for (i = 0; i < SEGLISTNUM; i++) {
        curptr = LOWHALFP(*(seglist + i));
        while (curptr) {
            /* free block in the wrong seg list */
            if (get_seglist_p(curptr) != seglist + i) {
                printf("block %p size %d in wrong free list size %d. ",
                    curptr, SIZE(curptr), 1 << (LOWER_BOUND_OF_SEGLIST + i));
                printf("Line %d.\n", lineno);
                exit(1);
            }
            /* allocated block in the seg list */
            if (IS_AL(curptr)) {
                printf("free blk not free. Line %d.\n", lineno);
                exit(1);
            }
            freecnt_2++;
            printf("free list %p, size %d\n", curptr, SIZE(curptr));
            /* potential loop happened in the linked list */
            if (freecnt_2 > freecnt_1) {
                printf("inf loop. Line %d.\n", lineno);
                exit(1);
            }
            curptr = NEXT_FR_P(curptr);
        }
    }
    /* counts are not consistent */
    if (freecnt_1 != freecnt_2) {
        printf("in-order count: %li; list count: %li\n", freecnt_1, freecnt_2);
        printf("different counts of free blks. Line %d.\n", lineno);
        exit(1);
    }
}

/*
 * Extend the heap
 */
static inline void *extend_heap(size_t size) {
    void* p;
    if ((p = mem_sbrk(size)) == (void *) -1) {
        return NULL;
    }
    /* init new block */
    PUT_HD(p, PACK(size, 0));
    PUT_FT(p, PACK(size, 0));
    /* new epilogue block */
    PUT_HD(NEXT_P(p), PACK(0, 1));
    PUT_NEXT_FR_P(p, NULL);
    PUT_PREV_FR_P(p, NULL);
    return p;
}

/*
 * Get the head of the seg list p belongs to
 */
static inline char **get_seglist_p(void *p) {
    return get_seglist_p_by_sz(SIZE(p));
}

/*
 * Get the head of the seg list the block with size sz belongs to
 */
static inline char **get_seglist_p_by_sz(size_t sz) {
    sz >>= LOWER_BOUND_OF_SEGLIST;
    int idx = 0;
    while (idx < SEGLISTNUM - 1 && sz > 0) {
        idx++;
        sz >>= 1;
    }
    return seglist + idx;
}

/*
 * Remove p from the seg list
 */
static inline void rm_from_freelist(void *p) {
    char *prev = PREV_FR_P(p);
    // p is not in a seglist (newly created block)
    if (!prev) {
        return;
    }
    char *next = NEXT_FR_P(p);
    // previous block is the head of the seg list
    if (prev < heap_hd_p) {
        char **seg_hd = (char **)prev;
        if (!next) {
            *seg_hd = 0;
        }
        else {
            *seg_hd = new_seg_head(HIGHHALF(*seg_hd), next);
        }
    }
    // previous block is regular free block
    else {
        PUT_NEXT_FR_P(prev, next);
    }
    // point the previous pointer of the next block to the previous block
    if (next) {
        PUT_PREV_FR_P(next, prev);
    }
}

/*
 * Add p to the seg list. 
 * Try maintaining the ascending order of the list,
 * by ensuring each block size is no more than 8 bigger than its next.
 */
static inline void add_to_freelist(void *p) {
    char **seg_hd = get_seglist_p(p);
    char *first = LOWHALFP(*seg_hd);
    char *next = first;
    char *prev = (char *)seg_hd;
    
    /* take care of the max size and first block */
    size_t psz = SIZE(p);
    /* empty seglist */
    if (!first) {
        *seg_hd = (char *)new_seg_head(psz, p);
    }
    /* p is the smallest */
    else if (psz <= SIZE(first) + BLKSIZE) {
        *seg_hd = (char *)new_seg_head(HIGHHALF(*seg_hd), p);
    }
    /* p is not the smallest */
    else {
        if (psz > HIGHHALF(*seg_hd)) {
            *seg_hd = (char *)new_seg_head(psz, first);
        }
        while (next && psz > SIZE(next) + BLKSIZE) {
            prev = next;
            next = NEXT_FR_P(next);
        }
    }
    // point p to the next block
    PUT_NEXT_FR_P(p, next);
    PUT_PREV_FR_P(p, prev);
    if ((char **)prev != seg_hd) {
        PUT_NEXT_FR_P(prev, p);
        PUT_PREV_FR_P(p, prev);
    }
    if (next) {
        PUT_PREV_FR_P(next, p);
    }
    dbg_printf("added %p size %d to list %p\n", p, SIZE(p), seg_hd);
}

/*
 * Move the links on "from" to "to"
 */
static inline void mv_link(void *from, void *to) {
    /* in the middle of the list */
    dbg_printf("moving link from %p to %p\n", from, to);
    char *fromprev = PREV_FR_P(from);
    char *fromnext = NEXT_FR_P(from);
    
    if (fromprev > heap_hd_p) {
        PUT_NEXT_FR_P(fromprev, to);
        PUT_PREV_FR_P(to, fromprev);
    }
    /* head of the list */
    else {
        char **seg_hd = (char **)fromprev;
        size_t maxsz = MAX(HIGHHALF(*seg_hd), SIZE(to));
        *seg_hd = new_seg_head(maxsz, to);
        PUT_PREV_FR_P(to, seg_hd);
    }
    PUT_NEXT_FR_P(to, fromnext);
    if (fromnext) {
        PUT_PREV_FR_P(fromnext, to);
    }
}

/*
 * Compute the head of the seg list given the max size and the 
 * block to be added at the head
 */
static inline char *new_seg_head(size_t sz, void *p) {
    return (char *)PACK64(sz, (char *)p - heap_hd_p);
}
