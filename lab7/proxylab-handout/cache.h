#include <semaphore.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include "debug.h"

/* The cache result struct */
typedef struct {
    void *val;                  /* pointer to the acutal cached value */
    size_t size;                /* size of the cache entry */
} c_res_t;


/* The cache node struct */
typedef struct c_node {
    struct c_node *next;        /* hash table next */
    struct c_node *prev;        /* hash table prev */
    struct c_node *lru_next;    /* lru list next */
    struct c_node *lru_prev;    /* lru list prev */
    char *key;                  /* the cache key */
    void *val;                  /* pointer to the acutal cached value */
    size_t size;                /* size of the cache entry */
} c_node_t;


/* The cache struct */
typedef struct {
    size_t cap;                 /* capacity of the cache */
    volatile size_t size;       /* current size of the cache */
    int rowlen;                 /* hash table row number */
    c_node_t *lru_h;            /* head of the lru list */
    c_node_t *lru_t;            /* tail of the lru list */
    c_node_t **cache;           /* actual cache (hash table) */
    sem_t mutex;                /* readcnt lock */
    sem_t wlock;                /* write lock */
    volatile int readcnt;       /* number of readers */
} cache_t;


cache_t *init_cache(int);
void free_cache(cache_t *);
int put(cache_t *, char *, void *, size_t);
c_res_t *get(cache_t *, char *);
