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
#define MINSIZE 16
#define HEAPEXTSIZE  (BLKSIZE * 64)  /* Extend heap by this amount (bytes) */  
#define SEGLISTNUM 12
#define LOWER_BOUND_OF_SEGLIST 5

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

/* helper macros */
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define PACK(size, alloc)  ((size) | (alloc))
#define PUT(p, v) (*(unsigned int *)(p) = (v))
#define GET(p) (*(unsigned int *)(p))
// #define GET_P(p) (*(char **)(p))
// #define PUT_P(p, addr) (*(char **)(p) = (addr))
#define GET_P(p) (*(int *)(p))
#define PUT_P(p, addr) (*(int *)(p) = (addr))

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

#define PTR_TO_OFFSET(p) ((int) (!(p) ? 0 : (char *)(p) - heap_hd_p))
#define OFFSET_TO_PTR(off) ((off) ? (heap_hd_p + (off)) : NULL)

// #define NEXT_FR_P(p) (GET_P(p))
// #define PREV_FR_P(p) (GET_P((char *)(p) + BLKSIZE))
// #define PUT_NEXT_FR_P(p, addr) (*(char **)(p) = (addr))
// #define PUT_PREV_FR_P(p, addr) (*((char **)(p) + 1) = (char *)(addr))
#define NEXT_FR_P(p) (OFFSET_TO_PTR(GET_P(p)))
#define PREV_FR_P(p) (OFFSET_TO_PTR(GET_P((char *)(p) + WSIZE)))
#define PUT_NEXT_FR_P(p, addr) (PUT_P(p, PTR_TO_OFFSET(addr)))
#define PUT_PREV_FR_P(p, addr) (PUT_P((char *)(p) + WSIZE, PTR_TO_OFFSET(addr)))

#define PUT_HD(p, v) (PUT(HD_P(p), (v)))
#define PUT_FT(p, v) (PUT(FT_P(p), (v)))

#define SAME_LIST(s1, s2) (((s1) ^ (s2)) <= (s1) && ((s1) ^ (s2)) <= (s2))


/* helper functions */
static inline void *extend_heap();
//static void insert_free_blk(void *p);
static inline int in_heap(const void *p);
void mm_checkheap(int lineno);
static inline char **get_seglist_p(void* p);
static inline char **get_seglist_p_by_sz(size_t sz);
static inline void rm_from_freelist(void* p);
static inline void add_to_freelist(void* p);
static inline void mv_link(void *from, void *to);

/* global variables */
static char *heap_hd_p = 0;
static long int heapsize = 0;
static char *firstblk;
static char **seglist;
static char **firstseg = 0;

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
    heap_hd_p = 0;
    heapsize = 0;
    firstblk = NULL;
    firstseg = 0;
    mem_reset_brk();
    /* Create the initial empty heap */
    if ((heap_hd_p = mem_sbrk(HEAPEXTSIZE)) == (void *) -1) 
        return -1;
    memset(heap_hd_p, 0, HEAPEXTSIZE);
    seglist = (char **)heap_hd_p;
    // padding. before this point is for seg list pointers
    heap_hd_p += BLKSIZE * SEGLISTNUM;
    // prologue
    PUT(heap_hd_p, PACK(0, 1));
    // head pointer to the data part
    char* real_p = heap_hd_p + BLKSIZE;
    // effective heap size
    heapsize += HEAPEXTSIZE - BLKSIZE * SEGLISTNUM;
    PUT_HD(real_p, PACK(heapsize - BLKSIZE, 0));
    PUT_FT(real_p, PACK(heapsize - BLKSIZE, 0));
    // epilogue
    PUT_HD(NEXT_P(real_p), PACK(0, 1));
    add_to_freelist(real_p);

    firstblk = real_p;
    dbg_printf("data heap starts from %p\n", heap_hd_p);
    dbg_printf("data blk starts from %p\n", firstblk);
    return 0;
}

/*
 * malloc
 */
void *malloc (size_t size) {
    dbg_printf("malloc %d\n", (int)size);
    dbg_printf("heapsize %d\n", (int)heapsize);
    // increment by 1 block to account for the head and footer
    size += BLKSIZE;
    size = MAX(ALIGN(size), MINSIZE);
    size_t sz = 0;
    char *curptr = NULL;
    char **seg_p = MAX(get_seglist_p_by_sz(size), firstseg);
    dbg_printf("allocating block size %d to list %p\n", (int)size, seg_p);
    for (; seg_p < (char **)heap_hd_p && !curptr; seg_p++) {
        dbg_printf("checking list %p\n", seg_p);
        curptr = *seg_p;
        while (curptr) {
            sz = (size_t) SIZE(curptr);
            if (sz >= size) {
                dbg_printf("found block size %d\n", (int)sz);
                break;
            }
            curptr = NEXT_FR_P(curptr);
        }
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
        if (SAME_LIST(sz, newsize)) {
            mv_link(curptr, newptr);
        }
        else {
            rm_from_freelist(curptr);
            add_to_freelist(newptr);
        }
        sz = size;
    }
    else { /* don't need splitting */
        rm_from_freelist(curptr);
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
    char *prev;
    char *next;
    // allo | free | allo
    if (IS_PRE_AL(p) && IS_NEXT_AL(p)) {
        dbg_printf("free case 1\n");
        freeptr = p;
        newsize = SIZE(p);
        // add the free blk to the free list
        PUT_HD(freeptr, PACK(newsize, 0));
        PUT_FT(freeptr, PACK(newsize, 0));
        add_to_freelist(freeptr);
    }
    // free | free | free
    else if (!IS_PRE_AL(p) && !IS_NEXT_AL(p)) {
        dbg_printf("free case 2\n");
        prev = PREV_P(p);
        next = NEXT_P(p);
        freeptr = prev;
        size_t prevsz = SIZE(prev);
        newsize = prevsz + size + SIZE(next);
        PUT_HD(freeptr, PACK(newsize, 0));
        PUT_FT(freeptr, PACK(newsize, 0));
        if (SAME_LIST(prevsz, newsize)) {
            rm_from_freelist(next);
        }
        else if (SAME_LIST(SIZE(next), newsize)) {
            rm_from_freelist(prev);
            mv_link(next, prev);
        }
        else {
            rm_from_freelist(prev);
            rm_from_freelist(next);
            add_to_freelist(freeptr);
        }
    }
    // free | free | allo
    else if (!IS_PRE_AL(p)) {
        dbg_printf("free case 3\n");
        prev = PREV_P(p);
        // concat the 2 parts
        freeptr = prev;
        size_t prevsz = SIZE(prev);
        newsize = prevsz + size;
        PUT_HD(freeptr, PACK(newsize, 0));
        PUT_FT(freeptr, PACK(newsize, 0));
        if (!SAME_LIST(prevsz, newsize)) {
            rm_from_freelist(prev);
            add_to_freelist(freeptr);
        }
    }
    // allo | free | free
    else {
        dbg_printf("free case 4\n");
        next = NEXT_P(p);
        freeptr = p;
        newsize = size + SIZE(next);
        PUT_HD(freeptr, PACK(newsize, 0));
        PUT_FT(freeptr, PACK(newsize, 0));
        if (!SAME_LIST(SIZE(next), newsize)) {
            rm_from_freelist(next);
            add_to_freelist(freeptr);
        }
        else {
            mv_link(next, freeptr);
        }
    }
    // PUT_HD(freeptr, PACK(newsize, 0));
    // PUT_FT(freeptr, PACK(newsize, 0));
    
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
static inline int in_heap(const void *p) {
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
    int i;
    for (i = 0; i < SEGLISTNUM; i++) {
        curptr = *(seglist + i);
        while (curptr) {
            if (get_seglist_p(curptr) != seglist + i) {
                printf("block %p size %d in wrong free list size %d. ",
                    curptr, SIZE(curptr), 1 << (LOWER_BOUND_OF_SEGLIST + i));
                printf("Called from %d line.\n", lineno);
                exit(1);
            }
            if (IS_AL(curptr)) {
                printf("free blk not free. Called from %d line.\n", lineno);
                exit(1);
            }
            freecnt_2++;
            printf("free list %p, size %d\n", curptr, SIZE(curptr));
            if (freecnt_2 > freecnt_1) {
                printf("dead loop. Called from %d line.\n", lineno);
                exit(1);
            }
            curptr = NEXT_FR_P(curptr);
        }
    }
    if (freecnt_1 != freecnt_2) {
        printf("in-order: %li; list: %li\n", freecnt_1, freecnt_2);
        printf("different counts of free blocks. Called from %d line.\n", lineno);
        exit(1);
    }
}

static inline void *extend_heap(size_t size) {
    void* p;
    if ((p = mem_sbrk(size)) == (void *) -1) {
        return NULL;
    }
    memset(p, 0, size);
    PUT_HD(p, PACK(size, 0));
    PUT_FT(p, PACK(size, 0));
    // new epilogue block
    PUT_HD(NEXT_P(p), PACK(0, 1));
    return p;
}

static inline char **get_seglist_p(void *p) {
    return get_seglist_p_by_sz(SIZE(p));
}

static inline char **get_seglist_p_by_sz(size_t sz) {
    sz >>= LOWER_BOUND_OF_SEGLIST;
    int idx = 0;
    while (idx < SEGLISTNUM - 1 && sz > 0) {
        idx++;
        sz >>= 1;
    }
    return seglist + idx;
}

static inline void rm_from_freelist(void *p) {
    char *prev = PREV_FR_P(p);
    // p is not in a seglist (newly created block)
    if (!prev) {
        return;
    }
    char *next = NEXT_FR_P(p);
    // previous block is the head of the seg list
    if (prev < heap_hd_p) {
        *((char **)prev) = next;
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

static inline void add_to_freelist(void *p) {
    char **seg_hd = get_seglist_p(p);
    firstseg = !firstseg ? seg_hd : MIN(firstseg, seg_hd);
    char *next = *seg_hd;
    // insert p as the first block of the seglist
    *seg_hd = p;
    PUT_PREV_FR_P(p, seg_hd);
    // point p to the next block
    PUT_NEXT_FR_P(p, next);
    if (next) {
        PUT_PREV_FR_P(next, p);
    }
    dbg_printf("added %p size %d to list %p\n", p, SIZE(p), seg_hd);
}

static inline void mv_link(void *from, void *to) {
    /* in the middle of the list */
    if (PREV_FR_P(from) > heap_hd_p) {
        PUT_NEXT_FR_P(PREV_FR_P(from), to);
        PUT_PREV_FR_P(to, PREV_FR_P(from));
    }
    /* head of the list */
    else {
        char **seg_hd = (char **)PREV_FR_P(from);
        *seg_hd = to;
        PUT_PREV_FR_P(to, seg_hd);
    }
    PUT_NEXT_FR_P(to, NEXT_FR_P(from));
    if (NEXT_FR_P(from)) {
        PUT_PREV_FR_P(NEXT_FR_P(from), to);
    }
}
