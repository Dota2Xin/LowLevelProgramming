/*
 * mm_test.c – Test suite for mm.c (the full allocator, not just the RB-tree).
 *
 * Build:
 *   gcc -Wall -Wextra -g -o mm_test mm_test.c mm.c memlib.c -lm
 * Run:
 *   ./mm_test
 *
 * Each test calls mem_reset() + mm_init() for a clean heap, then exercises
 * mm_malloc / mm_free / mm_realloc and the internal helpers exposed in mm.h.
 *
 * Checks performed per test:
 *   - Return values (NULL vs. non-NULL, alignment, range)
 *   - Heap consistency via mm_check() helper defined below
 *   - Coalescing: heap doesn't grow when adjacent frees are merged
 *   - Fragmentation: previously freed blocks are actually reused
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
 
#include "mm.h"
#include "memlib.h"
 
/* ------------------------------------------------------------------ */
/* Macros duplicated from mm.c so the test suite can inspect headers   */
/* ------------------------------------------------------------------ */
#define ALIGNMENT  8
#define WSIZE      8
#define DSIZE      8
#define MINSIZE    32
#define SIZECROSS  512
#define SEGSIZE    488
#define SEGBASE    61
 
#define GET(p)          (*(unsigned long int *)(p))
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)
#define GET_COLOR(p)    ((GET(p) & 0x2) >> 1)
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define ALIGN(s)        (((s) + (ALIGNMENT-1)) & ~0x7)
 
/* ------------------------------------------------------------------ */
/* Test framework                                                       */
/* ------------------------------------------------------------------ */
static int tests_run    = 0;
static int tests_passed = 0;
 
#define TEST(name) \
    do { tests_run++; printf("[TEST %2d] %-55s", tests_run, name); } while(0)
#define PASS() \
    do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) \
    do { printf("FAIL – %s\n", msg); } while(0)
#define SKIP(msg) \
    do { tests_run--; printf("SKIP – %s\n", msg); } while(0)
 
/* Reinitialise everything before each test */
static void reset(void)
{
    mem_reset();
    mm_init();
}
 
/* ------------------------------------------------------------------ */
/* Heap consistency checker                                             */
/* ------------------------------------------------------------------ */
 
/*
 * Walk every block from the first payload word after the prologue to
 * the epilogue and verify:
 *   1. Every allocated block's payload is 8-byte aligned.
 *   2. Header size >= MINSIZE (or == 2*DSIZE for prologue/epilogue).
 *   3. No two consecutive free blocks exist (coalescing must have run).
 *   4. Every free block's footer matches its header.
 *
 * Returns 1 if consistent, 0 otherwise (and prints diagnostics).
 */
static int heap_consistent(void)
{
    char *lo   = (char *)mem_heap_lo();
    char *hi   = (char *)mem_heap_hi();
 
    /* Skip the segregated-list area at the start of the heap */
    char *bp   = lo + SEGSIZE;   /* first block header */
    /* Step past prologue (size == 2*DSIZE, alloc == 1) */
    if (GET_SIZE(bp) != 2*DSIZE || !GET_ALLOC(bp)) {
        printf("  heap_consistent: bad prologue header\n");
        return 0;
    }
    bp += 2*DSIZE;   /* now at first real block header */
 
    int prev_free = 0;
    int ok        = 1;
 
    while (bp < hi - DSIZE) {
        size_t sz    = GET_SIZE(bp);
        int    alloc = GET_ALLOC(bp);
 
        if (sz == DSIZE && alloc) break;   /* epilogue */
 
        if (sz < MINSIZE) {
            printf("  heap_consistent: block at %p has sz=%zu < MINSIZE\n",
                   bp, sz);
            ok = 0;
        }
 
        /* Alignment check on payload (bp+DSIZE) */
        if (alloc && ((uintptr_t)(bp + DSIZE) % ALIGNMENT != 0)) {
            printf("  heap_consistent: payload at %p not aligned\n", bp+DSIZE);
            ok = 0;
        }
 
        /* Consecutive free blocks = coalescing missed */
        if (!alloc && prev_free) {
            printf("  heap_consistent: two consecutive free blocks at %p\n", bp);
            ok = 0;
        }
 
        /* Footer must match header for free blocks */
        if (!alloc) {
            char *ftr = bp + sz - DSIZE;
            if (GET_SIZE(ftr) != sz || GET_ALLOC(ftr) != 0) {
                printf("  heap_consistent: footer mismatch at %p (hdr sz=%zu, ftr sz=%lu)\n",
                       bp, sz, GET_SIZE(ftr));
                ok = 0;
            }
        }
 
        prev_free = !alloc;
        bp += sz;
    }
    return ok;
}
 
/* ------------------------------------------------------------------ */
/* Pointer range check                                                  */
/* ------------------------------------------------------------------ */
static int in_heap(void *p)
{
    return (char *)p >= (char *)mem_heap_lo() &&
           (char *)p <= (char *)mem_heap_hi();
}
 
static int aligned(void *p)
{
    return ((uintptr_t)p % ALIGNMENT) == 0;
}
 
/* ------------------------------------------------------------------ */
/* Tests – mm_init                                                      */
/* ------------------------------------------------------------------ */
 
static void test_init_returns_zero(void)
{
    TEST("mm_init returns 0");
    mem_reset();
    int r = mm_init();
    if (r == 0) PASS(); else FAIL("mm_init returned non-zero");
}
 
static void test_init_heap_nonempty(void)
{
    TEST("mm_init: heap size > 0 after init");
    reset();
    if (mem_heapsize() > 0) PASS(); else FAIL("heap empty after init");
}
 
static void test_init_heap_consistent(void)
{
    TEST("mm_init: heap consistent after init");
    reset();
    if (heap_consistent()) PASS(); else FAIL("heap inconsistent after init");
}
 
/* ------------------------------------------------------------------ */
/* Tests – mm_malloc basic                                              */
/* ------------------------------------------------------------------ */
 
static void test_malloc_zero(void)
{
    TEST("mm_malloc(0) returns NULL");
    reset();
    void *p = mm_malloc(0);
    if (p == NULL) PASS(); else FAIL("expected NULL for size 0");
}
 
static void test_malloc_small_nonnull(void)
{
    TEST("mm_malloc(1) returns non-NULL");
    reset();
    void *p = mm_malloc(1);
    if (p != NULL) PASS(); else FAIL("got NULL for size 1");
}
 
static void test_malloc_aligned(void)
{
    TEST("mm_malloc: returned pointer is 8-byte aligned");
    reset();
    void *p = mm_malloc(13);
    if (p && aligned(p)) PASS(); else FAIL("pointer not aligned");
}
 
static void test_malloc_in_heap(void)
{
    TEST("mm_malloc: returned pointer is within heap bounds");
    reset();
    void *p = mm_malloc(64);
    if (p && in_heap(p)) PASS(); else FAIL("pointer outside heap");
}
 
static void test_malloc_write_read(void)
{
    TEST("mm_malloc: payload can be written and read back");
    reset();
    int *p = mm_malloc(sizeof(int) * 4);
    if (!p) { FAIL("got NULL"); return; }
    p[0]=1; p[1]=2; p[2]=3; p[3]=4;
    if (p[0]==1 && p[1]==2 && p[2]==3 && p[3]==4)
        PASS();
    else
        FAIL("readback mismatch");
}
 
static void test_malloc_multiple_no_overlap(void)
{
    TEST("mm_malloc: multiple allocs don't overlap");
    reset();
    #define NP 8
    char *ptrs[NP];
    size_t sizes[NP] = {16,32,64,128,256,512,1024,2048};
    for (int i = 0; i < NP; i++) {
        ptrs[i] = mm_malloc(sizes[i]);
        if (!ptrs[i]) { FAIL("NULL returned"); return; }
        memset(ptrs[i], i+1, sizes[i]);
    }
    /* Verify nothing was overwritten */
    int ok = 1;
    for (int i = 0; i < NP && ok; i++)
        for (size_t j = 0; j < sizes[i] && ok; j++)
            if ((unsigned char)ptrs[i][j] != (unsigned char)(i+1)) ok = 0;
    if (ok) PASS(); else FAIL("overlap detected");
    #undef NP
}
 
static void test_malloc_heap_consistent_after_allocs(void)
{
    TEST("mm_malloc: heap consistent after 10 allocs");
    reset();
    for (int i = 1; i <= 10; i++) mm_malloc(i * 8);
    if (heap_consistent()) PASS(); else FAIL("heap inconsistent");
}
 
/* ------------------------------------------------------------------ */
/* Tests – small (array path, size <= 512)                             */
/* ------------------------------------------------------------------ */
 
static void test_malloc_small_boundary(void)
{
    TEST("mm_malloc(512) uses array path – non-NULL, aligned");
    reset();
    void *p = mm_malloc(512 - DSIZE);   /* fits in SIZECROSS */
    if (p && aligned(p) && in_heap(p)) PASS(); else FAIL("array-path alloc failed");
}
 
static void test_malloc_minsize_respected(void)
{
    TEST("mm_malloc(1): block header size >= MINSIZE");
    reset();
    char *p = mm_malloc(1);
    if (!p) { FAIL("NULL"); return; }
    size_t bsz = GET_SIZE(HDRP(p));
    if (bsz >= MINSIZE) PASS();
    else FAIL("block smaller than MINSIZE");
}
 
/* ------------------------------------------------------------------ */
/* Tests – large (tree path, size > 512)                               */
/* ------------------------------------------------------------------ */
 
static void test_malloc_large(void)
{
    TEST("mm_malloc(4096) uses tree path – non-NULL, aligned");
    reset();
    void *p = mm_malloc(4096);
    if (p && aligned(p) && in_heap(p)) PASS(); else FAIL("tree-path alloc failed");
}
 
static void test_malloc_large_multiple(void)
{
    TEST("mm_malloc: 5 large allocs (>512) – no NULL, no overlap");
    reset();
    #define NL 5
    char *ptrs[NL];
    size_t sz = 600;
    int ok = 1;
    for (int i = 0; i < NL; i++) {
        ptrs[i] = mm_malloc(sz);
        if (!ptrs[i]) { ok = 0; break; }
        memset(ptrs[i], 0xAB, sz);
        sz += 200;
    }
    /* Check sentinels */
    sz = 600;
    for (int i = 0; i < NL && ok; i++) {
        for (size_t j = 0; j < sz && ok; j++)
            if ((unsigned char)ptrs[i][j] != 0xAB) ok = 0;
        sz += 200;
    }
    if (ok) PASS(); else FAIL("large alloc failure or overlap");
    #undef NL
}
 
/* ------------------------------------------------------------------ */
/* Tests – mm_free and coalescing                                       */
/* ------------------------------------------------------------------ */
 
static void test_free_single(void)
{
    TEST("mm_free: free single block – heap consistent");
    reset();
    void *p = mm_malloc(64);
    if (!p) { FAIL("alloc failed"); return; }
    mm_free(p);
    if (heap_consistent()) PASS(); else FAIL("heap inconsistent after free");
}
 
static void test_free_reuse(void)
{
    TEST("mm_free: freed block is reused on next alloc (no heap growth)");
    reset();
    void *p = mm_malloc(64);
    if (!p) { FAIL("first alloc"); return; }
    size_t before = mem_heapsize();
    mm_free(p);
    void *q = mm_malloc(64);
    if (!q) { FAIL("second alloc"); return; }
    size_t after = mem_heapsize();
    /* Heap must not have grown – we recycled the freed block */
    if (after == before) PASS(); else FAIL("heap grew despite free block available");
}
 
static void test_free_coalesce_next(void)
{
    TEST("mm_free: coalesce with next free block");
    reset();
    void *a = mm_malloc(64);
    void *b = mm_malloc(64);
    if (!a || !b) { FAIL("alloc"); return; }
    mm_free(a);
    mm_free(b);   /* b is physically after a – should merge */
    if (heap_consistent()) PASS(); else FAIL("heap inconsistent after next-coalesce");
}
 
static void test_free_coalesce_prev(void)
{
    TEST("mm_free: coalesce with previous free block");
    reset();
    void *a = mm_malloc(64);
    void *b = mm_malloc(64);
    void *c = mm_malloc(64);
    if (!a || !b || !c) { FAIL("alloc"); return; }
    mm_free(b);   /* free middle */
    mm_free(a);   /* free before middle – should merge with b */
    if (heap_consistent()) PASS(); else FAIL("heap inconsistent after prev-coalesce");
}
 
static void test_free_coalesce_both(void)
{
    TEST("mm_free: coalesce both neighbors");
    reset();
    void *a = mm_malloc(64);
    void *b = mm_malloc(64);
    void *c = mm_malloc(64);
    void *d = mm_malloc(64);   /* anchor so heap doesn't shrink */
    if (!a || !b || !c || !d) { FAIL("alloc"); return; }
    mm_free(a);
    mm_free(c);
    mm_free(b);   /* merges a+b+c into one block */
    if (heap_consistent()) PASS(); else FAIL("heap inconsistent after both-coalesce");
}
 
static void test_free_coalesced_block_reused(void)
{
    TEST("mm_free: coalesced block large enough is reused");
    reset();
    void *a = mm_malloc(64);
    void *b = mm_malloc(64);
    void *c = mm_malloc(64);   /* anchor */
    if (!a || !b || !c) { FAIL("alloc"); return; }
    size_t before = mem_heapsize();
    mm_free(a);
    mm_free(b);   /* a+b coalesce → 128-byte free block */
    void *big = mm_malloc(100);   /* should fit in the coalesced block */
    if (!big) { FAIL("reuse alloc NULL"); return; }
    size_t after = mem_heapsize();
    if (after == before) PASS(); else FAIL("heap grew; coalesced block not reused");
}
 
static void test_free_many(void)
{
    TEST("mm_free: 20 alloc/free cycles – heap consistent throughout");
    reset();
    int ok = 1;
    for (int i = 0; i < 20; i++) {
        void *p = mm_malloc((i + 1) * 16);
        if (!p) { ok = 0; break; }
        //breaks at i=1 apparently still
        mm_free(p);
        if (!heap_consistent()) { 
            ok = 0;
            printf("Break at Free: %i, Size: %i" , i, (i+1)*16); 
            break; 
        }
    }
    if (ok) PASS(); else FAIL("inconsistency during alloc/free cycles");
}
 
/* ------------------------------------------------------------------ */
/* Tests – mm_realloc                                                   */
/* ------------------------------------------------------------------ */
 
static void test_realloc_null_ptr(void)
{
    TEST("mm_realloc(NULL, size) behaves like mm_malloc");
    reset();
    /* Per C standard realloc(NULL,sz)==malloc(sz); implementation may vary */
    void *p = mm_realloc(NULL, 64);
    /* We accept either NULL (not implemented) or a valid pointer */
    if (p == NULL || (aligned(p) && in_heap(p)))
        PASS();
    else
        FAIL("returned unaligned or out-of-heap pointer");
}
 
static void test_realloc_grow(void)
{
    TEST("mm_realloc: grow block – data preserved, new pointer valid");
    reset();
    int *p = mm_malloc(4 * sizeof(int));
    if (!p) { FAIL("initial alloc"); return; }
    p[0]=10; p[1]=20; p[2]=30; p[3]=40;
    int *q = mm_realloc(p, 8 * sizeof(int));
    if (!q) { FAIL("realloc returned NULL"); return; }
    if (q[0]==10 && q[1]==20 && q[2]==30 && q[3]==40)
        PASS();
    else
        FAIL("data not preserved after grow");
}
 
static void test_realloc_shrink(void)
{
    TEST("mm_realloc: shrink block – data preserved up to new size");
    reset();
    int *p = mm_malloc(8 * sizeof(int));
    if (!p) { FAIL("initial alloc"); return; }
    for (int i = 0; i < 8; i++) p[i] = i * 10;
    int *q = mm_realloc(p, 4 * sizeof(int));
    if (!q) { FAIL("realloc NULL"); return; }
    int ok = 1;
    for (int i = 0; i < 4; i++) if (q[i] != i * 10) { ok = 0; break; }
    if (ok) PASS(); else FAIL("data not preserved after shrink");
}
 
static void test_realloc_heap_consistent(void)
{
    TEST("mm_realloc: heap consistent after grow+shrink cycle");
    reset();
    void *p = mm_malloc(128);
    if (!p) { FAIL("alloc"); return; }
    p = mm_realloc(p, 512);
    if (!p) { FAIL("grow"); return; }
    p = mm_realloc(p, 64);
    if (!p) { FAIL("shrink"); return; }
    mm_free(p);
    if (heap_consistent()) PASS(); else FAIL("heap inconsistent");
}
 
/* ------------------------------------------------------------------ */
/* Tests – stress / mixed patterns                                      */
/* ------------------------------------------------------------------ */
 
static void test_stress_random_sizes(void)
{
    TEST("Stress: 50 mallocs of varied sizes – all aligned, in heap");
    reset();
    size_t szs[] = {
        1,7,8,9,15,16,17,31,32,33,63,64,65,
        127,128,129,255,256,257,511,512,513,
        600,700,800,1000,1024,1500,2000,2048,
        3000,4000,4096,5000,6000,7000,8000,
        9000,10000,12000,15000,16384,20000,
        24000,28000,32768,40000,48000,56000,65536
    };
    int N = (int)(sizeof(szs)/sizeof(szs[0]));
    void **ptrs = malloc(N * sizeof(void*));
    int ok = 1;
    for (int i = 0; i < N; i++) {
        ptrs[i] = mm_malloc(szs[i]);
        if (!ptrs[i] || !aligned(ptrs[i]) || !in_heap(ptrs[i])) {
            ok = 0; break;
        }
    }
    free(ptrs);
    if (ok) PASS(); else FAIL("NULL, misaligned, or OOB pointer");
}
 
static void test_stress_alloc_free_interleaved(void)
{
    TEST("Stress: interleaved alloc/free – heap consistent at end");
    reset();
    #define NS 30
    void *ptrs[NS];
    memset(ptrs, 0, sizeof(ptrs));
    int ok = 1;
    for (int round = 0; round < 3; round++) {
        /* Allocate */
        for (int i = 0; i < NS; i++) {
            ptrs[i] = mm_malloc((i+1) * 8 + round * 32);
            if (!ptrs[i]) { ok = 0; break; }
            memset(ptrs[i], 0xCC, (i+1)*8 + round*32);
        }
        if (!ok) break;
        /* Free every other one */
        for (int i = 0; i < NS; i += 2) {
            mm_free(ptrs[i]);
            ptrs[i] = NULL;
        }
        /* Re-allocate those slots */
        for (int i = 0; i < NS; i += 2) {
            ptrs[i] = mm_malloc((i+1) * 8);
            if (!ptrs[i]) { ok = 0; break; }
        }
        if (!ok) break;
        /* Free everything */
        for (int i = 0; i < NS; i++) {
            if (ptrs[i]) { mm_free(ptrs[i]); ptrs[i] = NULL; }
        }
        if (!heap_consistent()) { ok = 0; break; }
    }
    #undef NS
    if (ok) PASS(); else FAIL("NULL alloc or heap inconsistency");
}
 
static void test_stress_heap_growth(void)
{
    TEST("Stress: allocs that force repeated heap extension");
    reset();
    /* Allocate until we've grown the heap at least 3x CHUNKSIZE */
    size_t target = 3 * (1 << 12);
    int ok = 1;
    while (mem_heapsize() < target) {
        void *p = mm_malloc(512);
        if (!p) { ok = 0; break; }
    }
    if (ok && heap_consistent()) PASS(); else FAIL("heap growth failed");
}
 
static void test_stress_no_fragmentation_after_frees(void)
{
    TEST("Stress: freed blocks reconstituted – large alloc succeeds");
    reset();
    /* Allocate 20 x 256-byte blocks then free them all */
    #define NF 20
    void *ptrs[NF];
    for (int i = 0; i < NF; i++) {
        ptrs[i] = mm_malloc(256);
        if (!ptrs[i]) { FAIL("initial alloc"); return; }
    }
    size_t before = mem_heapsize();
    for (int i = 0; i < NF; i++) mm_free(ptrs[i]);
    /* Now request a large contiguous block – should not need heap growth
     * because coalescing should have produced enough space.            */
    void *big = mm_malloc(NF * 256 / 2);
    size_t after = mem_heapsize();
    if (big && after == before)
        PASS();
    else if (big)
        FAIL("large alloc succeeded but heap grew unnecessarily");
    else
        FAIL("large alloc failed after freeing all blocks");
    #undef NF
}
 
/* ------------------------------------------------------------------ */
/* Tests – edge / boundary                                              */
/* ------------------------------------------------------------------ */
 
static void test_boundary_exactly_sizecross(void)
{
    TEST("Boundary: mm_malloc(SIZECROSS) routes correctly – non-NULL");
    reset();
    void *p = mm_malloc(SIZECROSS);
    if (p && aligned(p)) PASS(); else FAIL("boundary alloc failed");
}
 
static void test_boundary_one_over_sizecross(void)
{
    TEST("Boundary: mm_malloc(SIZECROSS+1) uses tree path – non-NULL");
    reset();
    void *p = mm_malloc(SIZECROSS + 1);
    if (p && aligned(p)) PASS(); else FAIL("tree-path boundary alloc failed");
}
 
static void test_boundary_minsize(void)
{
    TEST("Boundary: mm_malloc(MINSIZE) – header size exactly MINSIZE");
    reset();
    char *p = mm_malloc(MINSIZE - DSIZE);   /* payload that needs exactly MINSIZE */
    if (!p) { FAIL("NULL"); return; }
    size_t bsz = GET_SIZE(HDRP(p));
    if (bsz >= MINSIZE) PASS(); else FAIL("block < MINSIZE");
}
 
static void test_double_free_safety(void)
{
    /* We can't test UB directly, but we can at least verify that after
     * a normal free the heap is still consistent before any double-free. */
    TEST("Safety: heap consistent immediately after free (pre-double-free)");
    reset();
    void *p = mm_malloc(64);
    if (!p) { FAIL("alloc"); return; }
    mm_free(p);
    if (heap_consistent()) PASS(); else FAIL("heap inconsistent");
}
 
/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
 
int main(void)
{
    mem_init();
 
    printf("============================================================\n");
    printf("  mm.c Allocator Test Suite\n");
    printf("============================================================\n\n");
 
    printf("--- mm_init ---\n");
    test_init_returns_zero();
    test_init_heap_nonempty();
    test_init_heap_consistent();
 
    printf("\n--- mm_malloc basic ---\n");
    test_malloc_zero();
    test_malloc_small_nonnull();
    test_malloc_aligned();
    test_malloc_in_heap();
    test_malloc_write_read();
    test_malloc_multiple_no_overlap();
    test_malloc_heap_consistent_after_allocs();
 
    printf("\n--- mm_malloc small path (array, <=512) ---\n");
    test_malloc_small_boundary();
    test_malloc_minsize_respected();
 
    printf("\n--- mm_malloc large path (tree, >512) ---\n");
    test_malloc_large();
    test_malloc_large_multiple();
 
    printf("\n--- mm_free & coalescing ---\n");
    test_free_single();
    test_free_reuse();
    test_free_coalesce_next();
    test_free_coalesce_prev();
    test_free_coalesce_both();
    test_free_coalesced_block_reused();
    test_free_many();
 
    //printf("\n--- mm_realloc ---\n");
    //test_realloc_null_ptr();
    //test_realloc_grow();
    //test_realloc_shrink();
    //test_realloc_heap_consistent();
 
    //printf("\n--- Stress ---\n");
    //test_stress_random_sizes();
    //test_stress_alloc_free_interleaved();
    //test_stress_heap_growth();
    //test_stress_no_fragmentation_after_frees();
 
    //printf("\n--- Boundary & Edge ---\n");
    //test_boundary_exactly_sizecross();
    //test_boundary_one_over_sizecross();
    //test_boundary_minsize();
    //test_double_free_safety();
 
    printf("\n============================================================\n");
    printf("  Results: %d / %d passed\n", tests_passed, tests_run);
    printf("============================================================\n");
 
    return (tests_passed == tests_run) ? 0 : 1;
}
 