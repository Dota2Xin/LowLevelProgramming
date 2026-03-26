/*
 * memlib.c – Simulated heap for testing mm.c without the real mdriver.
 *
 * Backed by a single static buffer. mem_reset() wipes it between tests
 * so each test starts with a clean slate.
 */
 
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include "memlib.h"
 
#define MAX_HEAP (1 << 23)   /* 8 MB – plenty for unit tests */
 
static char   heap_buf[MAX_HEAP];
static size_t heap_used = 0;
 
void mem_init(void)  { mem_reset(); }
void mem_reset(void) { heap_used = 0; memset(heap_buf, 0, MAX_HEAP); }
 
void *mem_sbrk(int incr)
{
    if (incr <= 0) {
        fprintf(stderr, "mem_sbrk: incr must be > 0 (got %d)\n", incr);
        return (void *)-1;
    }
    if (heap_used + (size_t)incr > MAX_HEAP) {
        fprintf(stderr, "mem_sbrk: heap exhausted\n");
        return (void *)-1;
    }
    void *old_brk = heap_buf + heap_used;
    heap_used += (size_t)incr;
    return old_brk;
}
 
void  *mem_heap_lo(void)   { return heap_buf; }
void  *mem_heap_hi(void)   { return heap_buf + heap_used - 1; }
size_t mem_heapsize(void)  { return heap_used; }
size_t mem_pagesize(void)  { return 4096; }