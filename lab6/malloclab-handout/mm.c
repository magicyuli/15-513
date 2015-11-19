/*
 * mm.c
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
//#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
# define checkheap(...) mm_checkheap(__VA_ARGS__)
#else
# define dbg_printf(...)
# define checkheap(...)
#endif

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
#define WSIZE 4
#define BLKSIZE 8
#define MINSIZE 24
#define HEAPEXTSIZE  (1<<10)  /* Extend heap by this amount (bytes) */  

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

/* helper macros */
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define PACK(size, alloc)  ((size) | (alloc)) 
#define PUT(p, v) (*(unsigned int *)(p) = (v))
#define GET(p) (*(unsigned int *)(p))
#define GET_P(p) (*(char **)(p))
#define PUT_P(p, addr) (*(char **)(p) = (addr))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define HD_P(p) ((char *)(p) - WSIZE)
#define SIZE(p) (GET(HD_P(p)) & ~0x7)
#define FT_P(p) ((char *)(p) + SIZE(p) - BLKSIZE)
#define PREV_FT_P(p) ((char *)(p) - BLKSIZE)
#define NEXT_P(p) ((char *)(p) + SIZE(p))
#define PREV_P(p) ((char *)(p) - GET_SIZE(PREV_FT_P(p)))

#define IS_AL(p) (GET(HD_P(p)) & 0x1)
#define IS_PRE_AL(p) (IS_AL(PREV_P(p)))
#define IS_NEXT_AL(p) (IS_AL(NEXT_P(p)))

#define NEXT_FR_P(p) (GET_P(p))
#define PREV_FR_P(p) (GET_P((char *)(p) + BLKSIZE))
#define PUT_NEXT_FR_P(p, addr) (*(char **)(p) = (addr))
#define PUT_PREV_FR_P(p, addr) (*((char **)(p) + 1) = (addr))

#define PUT_HD(p, v) (PUT(HD_P(p), (v)))
#define PUT_FT(p, v) (PUT(FT_P(p), (v)))

/* helper functions */
static void *extend_heap();
static void insert_free_blk(void *p);
static int in_heap(const void *p);
void mm_checkheap(int lineno);
//static void set_blk_size(void* p, long heapsize);

/* global variables */
static char *heap_hd_p = 0;
static long int heapsize = 0;
static char *firstfree = 0;
static char *firstblk;

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
    heap_hd_p = 0;
    heapsize = 0;
    firstfree = 0;
    firstblk = NULL;
    mem_reset_brk();
    //mem_deinit();
    /* Create the initial empty heap */
    if ((heap_hd_p = mem_sbrk(HEAPEXTSIZE)) == (void *) -1) 
        return -1;
    PUT(heap_hd_p, 1);                          /* Alignment padding */
    heapsize += HEAPEXTSIZE;
    char* real_p = heap_hd_p + BLKSIZE;
    PUT_HD(real_p, PACK(heapsize - 8, 0));
    PUT_FT(real_p, PACK(heapsize - 8, 0));
    insert_free_blk(real_p);
    firstblk = real_p;
    return 0;
}

/*
 * malloc
 */
void *malloc (size_t size) {
    dbg_printf("malloc %d\n", (int)size);
    dbg_printf("heapsize %d\n", (int)heapsize);
    char* curptr = firstfree;
    // increment by 1 block to account for the head and footer
    size += BLKSIZE;
    size = MAX(ALIGN(size), MINSIZE);
    size_t sz;
    while (curptr) {
        sz = (size_t) SIZE(curptr);
        if (sz >= size) {
            dbg_printf("found block size %d\n", (int)sz);
            break;
        }
        curptr = NEXT_FR_P(curptr);
    }
    if (!curptr) {
        if ((curptr = (char *)extend_heap(MAX(size, HEAPEXTSIZE))) == (char *)-1) {
            return NULL;
        }
        sz = SIZE(curptr);
        heapsize += sz;
        dbg_printf("extended heap by %d\n", (int)sz);
    }
    if (sz >= size + MINSIZE) { /* need splitting */
        
        // set up new free blk
        char *newptr = curptr + size;
        size_t newsize = sz - size;
        PUT_HD(newptr, PACK(newsize, 0));
        PUT_FT(newptr, PACK(newsize, 0));
        // maintain the free list
        if (PREV_FR_P(curptr)) { /* in the middle of the list */
            PUT_NEXT_FR_P(PREV_FR_P(curptr), newptr);
            PUT_PREV_FR_P(newptr, PREV_FR_P(curptr));
        }
        else { /* head of the list */    
            firstfree = newptr;
            PUT_PREV_FR_P(newptr, NULL);
        }
        PUT_NEXT_FR_P(newptr, NEXT_FR_P(curptr));
        if (NEXT_FR_P(curptr)) {
            PUT_PREV_FR_P(NEXT_FR_P(curptr), newptr);
        }
        sz = size;
    }
    else { /* don't need splitting */
        if (PREV_FR_P(curptr)) { /* in the middle of the list */
            PUT_NEXT_FR_P(PREV_FR_P(curptr), NEXT_FR_P(curptr));
            if (NEXT_FR_P(curptr)) {
                PUT_PREV_FR_P(NEXT_FR_P(curptr), PREV_FR_P(curptr));
            }
        }
        else { /* head of the list */    
            firstfree = NEXT_FR_P(curptr);
            if (NEXT_FR_P(curptr)) {
                PUT_PREV_FR_P(NEXT_FR_P(curptr), NULL);
            }
        }
    }
    // update the size and allocation status
    PUT_HD(curptr, PACK(sz, 1));
    PUT_FT(curptr, PACK(sz, 1));
    checkheap(__LINE__);
    dbg_printf("return %p\n", curptr);
    return (void *) curptr;
}

/*
 * free
 */
void free (void *p) {
    dbg_printf("free %p\n", p);
    if(!p || !in_heap(p)) return;
    size_t size = SIZE(p);
    char *freeptr;
    size_t newsize;
    char *prevp;
    char *nextp;
    // allo | free | allo
    if (IS_PRE_AL(p) && IS_NEXT_AL(p)) {
        dbg_printf("free case 1\n");
        newsize = SIZE(p);
        freeptr = p;
        // add the free blk to the free list
        insert_free_blk(p);
    }
    // free | free | free
    else if (!IS_PRE_AL(p) && !IS_NEXT_AL(p)) {
        dbg_printf("free case 2\n");
        prevp = PREV_P(p);
        nextp = NEXT_P(p);
        // remove next free from the list
        if (PREV_FR_P(nextp)) {
            PUT_NEXT_FR_P(PREV_FR_P(nextp), NEXT_FR_P(nextp));
        }
        else {
            firstfree = NEXT_FR_P(nextp);
        }
        if (NEXT_FR_P(nextp)) {
            PUT_PREV_FR_P(NEXT_FR_P(nextp), PREV_FR_P(nextp));
        }
        // concat the 3 parts
        freeptr = prevp;
        newsize = SIZE(prevp) + size + SIZE(nextp);
    }
    // free | free | allo
    else if (!IS_PRE_AL(p)) {
        dbg_printf("free case 3\n");
        prevp = PREV_P(p);
        // concat the 2 parts
        freeptr = prevp;
        newsize = SIZE(prevp) + size;
    }
    // allo | free | free
    else {
        dbg_printf("free case 4\n");
        nextp = NEXT_P(p);
        // move links from next to current
        if (PREV_FR_P(nextp)) { /* next is in the middle of the list */
            PUT_NEXT_FR_P(PREV_FR_P(nextp), p);
            PUT_PREV_FR_P(p, PREV_FR_P(nextp));
        }
        else { /* next is the head of the list */
            firstfree = p;
            PUT_PREV_FR_P(p, NULL);
        }
        PUT_NEXT_FR_P(p, NEXT_FR_P(nextp));
        if (NEXT_FR_P(nextp)) {
            PUT_PREV_FR_P(NEXT_FR_P(nextp), p);
        }
        freeptr = p;
        newsize = size + SIZE(nextp);
    }
    PUT_HD(freeptr, PACK(newsize, 0));
    PUT_FT(freeptr, PACK(newsize, 0));
    //dbg_printf("got here. %d\n", __LINE__);
    checkheap(__LINE__);
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {
    dbg_printf("realloc %p %d\n", oldptr, (int)size);
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        free(oldptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(oldptr == NULL) {
        return malloc(size);
    }

    newptr = malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }

    /* Copy the old data. */
    oldsize = SIZE(oldptr);
    if(size < oldsize) oldsize = size;
    memcpy(newptr, oldptr, oldsize);

    /* Free the old block. */
    free(oldptr);

    return newptr;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc(size_t nmemb, size_t size) {
    size_t bytes = nmemb * size;
    dbg_printf("calloc %d\n", (int)bytes);
    void *p = malloc(bytes);
    memset(p, 0, bytes);

    return p;
}


/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
//static int aligned(const void *p) {
    //return (size_t)ALIGN(p) == (size_t)p;
//}

/*
 * mm_checkheap
 */
void mm_checkheap(int lineno) {
    char* curptr = firstblk;
    long freecnt_1 = 0;
    long freecnt_2 = 0;
    // walk through the heap
    while (curptr && in_heap(curptr) && SIZE(curptr) > 0) {
        if (!IS_AL(curptr)) {
            freecnt_1++;
            //printf("%p\n", curptr);
        }
        curptr = NEXT_P(curptr);
    }
    // walk through the free list
    curptr = firstfree;
    while (curptr) {
        if (IS_AL(curptr)) {
            printf("free blk not free. Called from %d line.\n", lineno);
            exit(1);
        }
        freecnt_2++;
        printf("free list %p\n", curptr);
        if (freecnt_2 > freecnt_1) {
            printf("dead loop. Called from %d line.\n", lineno);
            exit(1);
        }
        curptr = NEXT_FR_P(curptr);
    }
    if (freecnt_1 != freecnt_2) {
        printf("in-order: %li; list: %li\n", freecnt_1, freecnt_2);
        printf("different counts of free blocks. Called from %d line.\n", lineno);
        exit(1);
    }
}

static void *extend_heap(size_t size) {
    void* p;
    if ((p = mem_sbrk(size)) == (void*) -1) {
        return NULL;
    }
    PUT_HD(p, PACK(size, 0));
    PUT_FT(p, PACK(size, 0));
    PUT_HD(NEXT_P(p), PACK(0, 1));
    insert_free_blk(p);
    return p;
}

static void insert_free_blk(void* p) {
    PUT_NEXT_FR_P(p, firstfree);
    if (firstfree) {
        PUT_PREV_FR_P(firstfree, p);
    }
    firstfree = p;
    PUT_PREV_FR_P(p, NULL);
}
