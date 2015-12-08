/**
 * This file implements a LRU cache.
 * The implementation is doubly linked hash table,
 * i.e. hash table is used to quickly put and get
 * the cached objects, and all objects in the cache
 * form a doubly linked list, where the objects are
 * ordered by access time descentantly.
 * 
 * The put and get operations are thread safe.
 * Put and put, put and get, are mutually exclusive,
 * while get and get can happen together.
 * 
 * 
 * Liruoyang YU
 * liruoyay
 */

#include "cache.h"

#define HASH_PRIME 31   /* for hashing */

/*
 * P operation
 */
static inline int P(sem_t *sem) {
    return sem_wait(sem);
}

/*
 * V operation
 */
static inline int V(sem_t *sem) {
    return sem_post(sem);
}

/*
 * Compute hashcode.
 */
static inline int hash(char *s) {
    int len = strlen(s);
    int p = HASH_PRIME;
    int res = 0;
    int i;
    for (i = 0; i < len; i++) {
        res += p * s[i];
        p *= p;
    }
    /* can be negtive when overflowing */
    if (res < 0) {
        res *= -1;
    }
    return res;
}

/*
 * Find the hash table slot give the key and the 
 * hash table row number.
 */
static inline int find_slot(char *key, int rowlen) {
    /* hashcode of the key */
    int hashcode = hash(key);
    /* the slot to put the cache */
    return hashcode % rowlen;
}

/*
 * Insert a node at the first position of the lru list.
 */
static inline void insert_lru(cache_t *csh, c_node_t *new) {
    csh->lru_h->lru_next->lru_prev = new;
    new->lru_next = csh->lru_h->lru_next;
    csh->lru_h->lru_next = new;
    new->lru_prev = csh->lru_h;
}

/*
 * Remove a node from the lru list.
 */
static inline void remove_lru(c_node_t *r) {
    r->lru_prev->lru_next = r->lru_next;
    r->lru_next->lru_prev = r->lru_prev;
}

/*
 * Evict a node from the last position of the lru list.
 */
static inline void evict(cache_t *csh) {
    c_node_t *e = csh->lru_t->lru_prev;
    int slot;
    
    /* remove from lru list */
    remove_lru(e);
    
    /* remove from the hash table */
    if (e->prev) {
        e->prev->next = e->next;
        e->next->prev = e->prev;
    }
    else {
        slot = find_slot(e->key, csh->rowlen);
        csh->cache[slot] = e->next;
        e->next->prev = NULL;
    }
    
    /* clean up */
    csh->size -= e->size;
    free(e->key);
    free(e->val);
    free(e);
}

/*
 * Init a cache instance.
 */
cache_t *init_cache(int cap) {
    int failed = 0;
    
    cache_t *csh = (cache_t *) malloc(sizeof(cache_t));
    csh->cap = cap;
    csh->size = 0;
    csh->rowlen = (cap + 1023) / 1024;
    csh->lru_h = (c_node_t *) malloc(sizeof(c_node_t));
    csh->lru_t = (c_node_t *) malloc(sizeof(c_node_t));
    csh->cache = (c_node_t **) calloc((size_t)csh->rowlen, sizeof(c_node_t *));
    csh->readcnt = 0;
    
    /* malloc failded */
    if (!csh || !csh->lru_h || !csh->lru_t || !csh->cache) {
        perror("Malloc");
        failed = 1;
    }
    
    /* init mutex */
    if (sem_init(&csh->mutex, 0, 1) < 0) {
        perror("Init mutex");
        failed = 1;
    }
    
    /* init write lock */
    if (sem_init(&csh->wlock, 0, 1) < 0) {
        perror("Init wlock");
        failed = 1;
    }
    
    /* abort */
    if (failed) {
        /* clean up */
        free_cache(csh);
        return NULL;
    }
    
    /* init lru head */
    csh->lru_h->next = NULL;
    csh->lru_h->prev = NULL;
    csh->lru_h->lru_next = csh->lru_t;
    csh->lru_h->lru_prev = NULL;
    csh->lru_h->key = NULL;
    csh->lru_h->val = NULL;
    csh->lru_h->size = 0;
    /* init lru tail */
    csh->lru_t->next = NULL;
    csh->lru_t->prev = NULL;
    csh->lru_t->lru_next = NULL;
    csh->lru_t->lru_prev = csh->lru_h;
    csh->lru_t->key = NULL;
    csh->lru_t->val = NULL;
    csh->lru_t->size = 0;
    
    return csh;
}

/*
 * Free a cache instance.
 */
void free_cache(cache_t * csh) {
    if (csh) {
        if (csh->lru_h) {
            free(csh->lru_h);
        }
        if (csh->lru_t) {
            free(csh->lru_t);
        }
        /* free all the cache nodes */
        if (csh->cache) {
            c_node_t *cur;
            c_node_t *tmp;
            int len = csh->rowlen;
            int i;
            
            for (i = 0; i < len; i++) {
                cur = csh->cache[i];
                while (cur) {
                    tmp = cur->next;
                    free(cur->key);
                    free(cur->val);
                    free(cur);
                    cur = tmp;
                }
            }
            free(csh->cache);
        }
        /* destroy semaphores */
        if (sem_destroy(&csh->mutex) < 0) {
            perror("Destroy mutex");
        }
        if (sem_destroy(&csh->wlock) < 0) {
            perror("Destroy wlock");
        }
        
        free(csh);
    }
}

/*
 * Put a cache entry key:val into the cache *csh.
 * val should be malloced.
 */
int put(cache_t *csh, char *key, void *val, size_t size) {
    if (size > csh->cap) {
        return -1;
    }
    
    int slot = find_slot(key, csh->rowlen);
    c_node_t *first;
    
    /* new node */
    c_node_t *new = (c_node_t *) malloc(sizeof(c_node_t));
    if (!new) {
        perror("Put cache - malloc");
        return -1;
    }
    new->prev = NULL;
    new->key = (char *) malloc(strlen(key) + 1);
    strcpy(new->key, key);
    dbg_printf("Putting key: %s\n", new->key);
    new->val = val;
    new->size = size;
    
    /* acquire the write lock */
    if (P(&csh->wlock) < 0) {
        perror("Put cache - lock");
        return -1;
    }
    
    /************************* 
     * start critical section 
     *************************/
    /* check size */
    while (csh->size + size > csh->cap) {
        evict(csh);
    }
    
    first = csh->cache[slot];
    
    /* insert into hash table */
    if (first) {
        new->next = first;
        first->prev = new;
    }
    else {
        new->next = NULL;
    }
    csh->cache[slot] = new;
    
    /* maintain lru list */
    insert_lru(csh, new);
    
    /************************* 
     * end critical section 
     *************************/
     
    /* release the write lock */
    if (V(&csh->wlock) < 0) {
        perror("Put cache - unlock");
    }
    
    return 0;
}

/*
 * Read a cache entry identified by key from the cache *csh.
 */
c_res_t *get(cache_t * csh, char *key) {
    c_res_t *res = NULL;
    dbg_printf("Getting key: %s\n", key);
    
    /* acquire the readcnt lock */
    if (P(&csh->mutex) < 0) {
        perror("Get cache in - lock readcnt");
        return NULL;
    }
    
    /* first reader in */
    if (csh->readcnt == 0) {
        /* acquire the write lock */
        if (P(&csh->wlock) < 0) {
            perror("Get cache in - lock write");
            return NULL;
        }
    }
    
    /* else: the write lock has been held by another reader */
    
    csh->readcnt++;
    
    /* release the readcnt lock */
    if (V(&csh->mutex) < 0) {
        perror("Get cache in - unlock readcnt");
        return NULL;
    }
    
    /*****************
     * start reading 
     *****************/
    int slot = find_slot(key, csh->rowlen);
    
    /* find the cache node */
    c_node_t *cur = csh->cache[slot];
    while (cur) {
        if (strcmp(cur->key, key)) {
            cur = cur->next;
            continue;
        }
        
        /* hit */
        /* maintain the lru list */
        remove_lru(cur);
        insert_lru(csh, cur);
        if ((res = malloc(sizeof(c_res_t))) == NULL) {
            return NULL;
        }
        res->val = cur->val;
        res->size = cur->size;
        
        break;
    }
     
    /*****************
     * end reading 
     *****************/
    
    /* acquire the readcnt lock */
    if (P(&csh->mutex) < 0) {
        perror("Get cache out - lock readcnt");
        return NULL;
    }
    
    /* last reader out */
    if (csh->readcnt == 1) {
        /* release the write lock */
        if (V(&csh->wlock) < 0) {
            perror("Get cache out - unlock write");
            return NULL;
        }
    }
    
    /* else: the write lock has been held by another reader */
    
    csh->readcnt--;
    
    /* release the readcnt lock */
    if (V(&csh->mutex) < 0) {
        perror("Get cache out - unlock readcnt");
        return NULL;
    }
    
    return res;
}
