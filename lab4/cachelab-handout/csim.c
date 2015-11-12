/**
 * csim.c - Cache Lab Task 1, Cache Simulator
 * Name: Liruoyang Yu
 * Andrew Id: liruoyay
 */
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "cachelab.h"

/**
 * Abstraction of a cache line in a paticular set.
 */
typedef struct line {
    long tag;
    struct line* next;
    struct line* prev;
} LINE;

/**
 * Abstraction of a cache set.
 * Maintain a double-linked list so that
 * from head to tail are most recently used to least recently used.
 */
typedef struct set {
    int cur_line_num;
    LINE* lru_head;
    LINE* lru_tail;
} SET;

// inputs
int s, E, b;
bool verbose;
char* tracefile;

// derived infomation
int set_num;
int line_num;
int block_size;

// simulating the cache
SET* cache;

// counters
int hit = 0;
int miss = 0;
int eviction = 0;

/**
 * Handle Runtime errors and terminate the program.
 * param: msg Message to be printed out to stdout.
 */
void handleError(char* msg) {
    printf("%s", msg);
    printf("\n");
    exit(EXIT_FAILURE);
}

/**
 * init the cache
 */
void init() {
    cache = malloc(set_num * sizeof(SET));
    if (!cache) {
        handleError("Stack overflow.");
    }
    int i;
    for (i = 0; i < set_num; i++) {
        cache[i].cur_line_num = 0;
        cache[i].lru_head = malloc(sizeof(LINE));
        cache[i].lru_tail = malloc(sizeof(LINE));
        cache[i].lru_head->next = cache[i].lru_tail;
        cache[i].lru_head->prev = NULL;
        cache[i].lru_tail->next = NULL;
        cache[i].lru_tail->prev = cache[i].lru_head;
    }
}

/**
 * Cut one item from the double-linked list
 * param: item  the node to be cut off
 */
void detach(LINE* item) {
    item->prev->next = item->next;
    item->next->prev = item->prev;
}

/**
 * Insert one item after the the prev_item into the double-linked list
 * param: item  the node to be inserted into the list
 * param: prev_item  the node in the list after which the item will be inserted
 */
void insertAfter(LINE* item, LINE* prev_item) {
    item->prev = prev_item;
    item->next = prev_item->next;
    prev_item->next->prev = item;
    prev_item->next = item;
}

/**
 * Check hit or miss
 * param: tag  the tag portion
 * param: set_idx  the set portion
 * return: the verbose result string
 */
char* check(long tag, int set_idx) {
    // pointer to the corresponding set
    SET* set = cache + set_idx;
    LINE* cur_line = set->lru_head->next;
    
    // iterate over the lines
    while (cur_line != set->lru_tail) {
        // hit
        if (cur_line->tag == tag) {
            hit++;
            // pull current line next to head
            detach(cur_line);
            insertAfter(cur_line, set->lru_head);
            return "hit";
        }
        cur_line = cur_line->next;
    }
    
    // missed
    miss++;
    
    // insert a new line
    LINE* new_line = malloc(sizeof(LINE));
    new_line->tag = tag;
    insertAfter(new_line, set->lru_head);
    
    // need eviction
    if (set->cur_line_num == line_num) {
        LINE* evict = set->lru_tail->prev;
        detach(evict);
        free(evict);
        // increment eviction count
        eviction++;
        return "miss eviction";
    }
    
    // increment the number of current lines in the set
    set->cur_line_num++;
    return "miss";
}

/**
 * Run the simulation.
 */
void run() {
    init();
    char msg[128];
    // util constants
    char space = 0x20;
    int offset_mask = block_size - 1;
    int set_mask = set_num - 1;
    // file parsing vars
    int buf_size = 64;
    char buf[buf_size];
    FILE* f = fopen(tracefile, "r");
    // L S M
    char opt;
    long addr;
    // ignorable
    int size;
    // parsed info
    long tag;
    int set, offset;
    
    while (fgets(buf, buf_size, f)) {
        if (buf[0] != space) {
            continue;
        }
        // address is in hex string
        sscanf(buf, " %c %lx,%d\n", &opt, &addr, &size);
        // parse
        offset = addr & offset_mask;
        set = (addr >> b) & set_mask;
        tag = (addr - offset - set) >> (s + b);
        
        switch (opt) {
            case 'L':
            case 'S':
                // do once
                strcpy(msg, check(tag, set));
                break;
            case 'M':
                // do twice
                strcpy(msg, check(tag, set));
                strcat(strcat(msg, " "), check(tag, set));
                break;
            default:
                sprintf(msg, "Unknown Operation: %c", opt);
                handleError(msg);
        }
        // verbose
        if (verbose) {
            // write address as hex string
            printf("%c %lx,%d %s", opt, addr, size, msg);
            printf("\n");
        }
    }
    // clean up
    fclose(f);
    int i;
    LINE* tmp;
    for (i = 0; i < set_num; i++) {
        LINE* line = (cache + i)->lru_head;
        while (line) {
            tmp = line;
            line = line->next;
            free(tmp);
        }
    }
    free(cache);
}


/**
 * Entry point of the program
 */
int main(int argc, char** argv) {
    int opt;
    char* t;
    char msg[128];
    
    // at least s E b t should be present
    if (argc < 4) {
        handleError("Usage: ./csim [-hv] -s <s> -E <E> -b <b> -t <tracefile>");
    }
    while((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (opt) {
            case 'h':
                printf("Usage: ./csim [-hv] -s <s> -E <E> -b <b> -t <tracefile>");
                exit(EXIT_SUCCESS);
            case 'v':
                verbose = true;
                break;
            case 's':
			    s = atoi(optarg);
                if (!s) {
                    handleError("-s must be a positive number");
                }
                set_num = 1 << s;
                break;
            case 'E':
                E = atoi(optarg);
                if (!E) {
                    handleError("-E must be a positive number");
                }
                line_num = E;
                break;
            case 'b':
                b = atoi(optarg);
                if (!b) {
                    handleError("-b must be a positive number");
                }
                block_size = 1 << b;
                break;
            case 't':
                t = optarg;
                struct stat buf;
                if (stat(t, &buf) == -1) {
                    handleError("File invaid.");
                }
                tracefile = t;
                break;
            default:
                sprintf(msg, "Invalid opt: %c", opt);
                handleError(msg);
        }
    }
    run();
    printSummary(hit, miss, eviction);
    return 0;
}
