#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memlib.h"

/* Global pointers for the heap */
static char *mem_heap;      /* Points to first byte of heap */
static char *mem_brk;       /* Points to last byte of heap plus 1 */
static char *mem_max_addr;  /* Max legal heap address plus 1 */

/* This is the memory allocated by grade_sandbox.c */
extern char *sandbox_memory;
extern size_t sandbox_size;

void mem_init(void) {
    if (sandbox_memory == NULL) {
        fprintf(stderr, "mem_init: sandbox_memory is NULL. Initialize grader first.\n");
        return;
    }
    mem_heap = sandbox_memory;
    mem_brk = sandbox_memory;
    mem_max_addr = sandbox_memory + sandbox_size;
}

void mem_reset(void) {
    mem_brk = mem_heap;
    // Wipe memory to catch use-after-free bugs
    memset(mem_heap, 0, sandbox_size); 
}

void *mem_sbrk(int incr) {
    char *old_brk = mem_brk;

    if (incr < 0) {
        fprintf(stderr, "mem_sbrk: negative increment not supported\n");
        return (void *)-1;
    }
    if ((mem_brk + incr) > mem_max_addr) {
        fprintf(stderr, "mem_sbrk: ran out of memory (Heap limit %zu bytes)\n", sandbox_size);
        return (void *)-1;
    }

    mem_brk += incr;
    return (void *)old_brk;
}

void *mem_heap_lo(void)  { return (void *)mem_heap; }
void *mem_heap_hi(void)  { return (void *)(mem_brk - 1); }
size_t mem_heapsize(void) { return (size_t)(mem_brk - mem_heap); }
size_t mem_pagesize(void) { return 4096; }