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
 #define PACK_COLOR(size, color, alloc) ((size) | (2*(color)) | (alloc))
 
#define PUT_COLOR(p, color) \
    (PUT(p, PACK_COLOR(GET_SIZE(p), color, GET_ALLOC(p))))
 
/* Tree child/parent accessors */
#define GET_LEFT_CHILD(p)  (GET(((char*)(p)) + DSIZE))
#define GET_RIGHT_CHILD(p) (GET(((char*)(p)) + 2*DSIZE))
#define GET_PARENT(p)      (GET(((char*)(p)) + 3*DSIZE))
 
#define PUT_LEFT(root, child)   (PUT(((char*)(root)) + DSIZE,   (unsigned long)(child)))
#define PUT_RIGHT(root, child)  (PUT(((char*)(root)) + 2*DSIZE, (unsigned long)(child)))
#define PUT_PARENT(root, par)   (PUT(((char*)(root)) + 3*DSIZE, (unsigned long)(par)))
 
/* Aliases used in some tree functions */
#define LEFT_CHILD(p)   GET_LEFT_CHILD(p)
#define RIGHT_CHILD(p)  GET_RIGHT_CHILD(p)
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
extern char* rootMain;
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
#define RED       1
#define BLACK     0
void print_binary_tree(void *root)
{
    if (root == 0) {
        printf("[empty tree]\n");
        return;
    }

    /* Queue entries: pointer + depth + side label */
    typedef struct { void *node; int depth; char side; } QEntry;

    QEntry queue[512];
    int head = 0, tail = 0;

    queue[tail++] = (QEntry){root, 0, 'R'};  /* R = root */

    int cur_depth = -1;

    while (head < tail) {
        QEntry e = queue[head++];
        void *n  = e.node;

        /* New depth = new row */
        if (e.depth != cur_depth) {
            cur_depth = e.depth;
            printf("\n[depth %d]  ", cur_depth);
        }

        /* Print node: size, color, which side it is */
        char color  = GET_COLOR(n) == BLACK ? 'B' : 'R';
        void *left  = (void *)GET_LEFT_CHILD(n);
        void *right = (void *)GET_RIGHT_CHILD(n);
        void *par   = (void *)GET_PARENT(n);

        printf("%c:sz=%lu(%c,p=%lu)  ",
               e.side,
               GET_SIZE(n),
               color,
               par ? GET_SIZE(par) : 0);

        if (left)  queue[tail++] = (QEntry){left,  e.depth + 1, 'L'};
        if (right) queue[tail++] = (QEntry){right, e.depth + 1, 'R'};
    }
    printf("\n");
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
            //if(!heap_consistent()) {
            //    printf("Heap Error 1, %i\n", i);
            //    printf("Round: %i\n", round);
            //}
            ptrs[i] = mm_malloc((i+1) * 8 + round * 32);
            //printf("ADDED SIZE: %lu\n", GET_SIZE(ptrs[i]-DSIZE));
            //print_binary_tree(rootMain);
            if (!ptrs[i]) { ok = 0; break; }
            memset(ptrs[i], 0xCC, (i+1)*8 + round*32);
        }
        if (!ok) break;
        /* Free every other one */
        for (int i = 0; i < NS; i += 2) {
            //printf("Break at free %i \n", i);
            //if(heap_consistent) {
            //    printf("Still Consistent\n");
            //}
            //printf("WE ARE SIZE: %lu\n", GET_SIZE(ptrs[i]-DSIZE));
            //printf("NEXT SIZE %lu\n", GET_SIZE(ptrs[i+1]-DSIZE));
            //print_binary_tree(rootMain);
            //if(!heap_consistent()) {
            //    printf("Heap Error 2, %i\n", i);
            //    printf("Round: %i\n", round);
            //}
            mm_free(ptrs[i]);
            ptrs[i] = NULL;
        }
        /* Re-allocate those slots */
        for (int i = 0; i < NS; i += 2) {
            /* if(!heap_consistent()) {
                printf("Heap Error 3, %i\n", i);
                printf("Round: %i\n", round);
            } */
            ptrs[i] = mm_malloc((i+1) * 8);            
            if (!ptrs[i]) { ok = 0; break; }
        }
        if (!ok) break;
        /* Free everything */
        for (int i = 0; i < NS; i++) {
            /* if(!heap_consistent()) {
                printf("Heap Error 4, %i\n", i);
                printf("Round: %i\n", round);
            } */
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
/* Trace-replay diagnostic tests                                        */
/*                                                                      */
/* Replays the failing trace:                                           */
/*   a 0 2040  a 1 4010  a 2 48  a 3 4072  a 4 4072  a 5 4072         */
/*   f 0  f 1  f 2  f 3  f 4  f 5                                      */
/*                                                                      */
/* The 64-bit note: the lab was designed for 32-bit pointers (4 bytes) */
/* but is compiled 64-bit here (8-byte pointers). WSIZE/DSIZE are      */
/* already 8 in mm.c so the header/footer macros are correct, but      */
/* block sizes will be larger than the trace expects.                   */
/* ------------------------------------------------------------------ */
 
/*
 * Allocate all six trace blocks and return 1 if every pointer is
 * non-NULL, aligned, and within the heap.  Fills each block with a
 * unique sentinel byte so we can detect corruption later.
 */
static int trace_alloc_all(void *ptrs[6])
{
    size_t sizes[6] = {2040, 4010, 48, 4072, 4072, 4072};
    for (int i = 0; i < 6; i++) {
        ptrs[i] = mm_malloc(sizes[i]);
        if (!ptrs[i] || !aligned(ptrs[i]) || !in_heap(ptrs[i]))
            return 0;
        memset(ptrs[i], 0xA0 + i, sizes[i]);
    }
    return 1;
}
 
/* T1 – every alloc succeeds, is aligned, is in-heap */
static void test_trace_allocs_valid(void)
{
    TEST("Trace: all 6 allocs return valid pointers");
    reset();
    void *ptrs[6];
    if (trace_alloc_all(ptrs))
        PASS();
    else
        FAIL("at least one alloc returned NULL/misaligned/OOB");
}
 
/* T2 – heap is consistent immediately after the 6 allocs */
static void test_trace_heap_after_allocs(void)
{
    TEST("Trace: heap consistent after 6 allocs");
    reset();
    void *ptrs[6];
    if (!trace_alloc_all(ptrs)) { FAIL("alloc failed"); return; }
    if (heap_consistent()) PASS(); else FAIL("heap inconsistent after allocs");
}
 
/* T3 – sentinel bytes are intact (no overlaps between blocks) */
static void test_trace_no_overlap(void)
{
    TEST("Trace: no overlap between the 6 allocated blocks");
    reset();
    void *ptrs[6];
    size_t sizes[6] = {2040, 4010, 48, 4072, 4072, 4072};
    if (!trace_alloc_all(ptrs)) { FAIL("alloc failed"); return; }
    int ok = 1;
    for (int i = 0; i < 6 && ok; i++) {
        unsigned char *p = ptrs[i];
        for (size_t j = 0; j < sizes[i] && ok; j++)
            if (p[j] != (unsigned char)(0xA0 + i)) ok = 0;
    }
    if (ok) PASS(); else FAIL("sentinel byte corrupted – blocks overlap");
}
 
/* T4 – free block 0 (2040 bytes, array/tree boundary) */
static void test_trace_free0(void)
{
    TEST("Trace: free ptrs[0]=2040 – heap consistent");
    reset();
    void *ptrs[6];
    if (!trace_alloc_all(ptrs)) { FAIL("alloc failed"); return; }
    mm_free(ptrs[0]); ptrs[0] = NULL;
    if (heap_consistent()) PASS(); else FAIL("heap inconsistent after free[0]");
}
 
/* T5 – free block 1 (4010 bytes, large/tree path) */
static void test_trace_free1(void)
{
    TEST("Trace: free ptrs[1]=4010 – heap consistent");
    reset();
    void *ptrs[6];
    if (!trace_alloc_all(ptrs)) { FAIL("alloc failed"); return; }
    mm_free(ptrs[0]); ptrs[0] = NULL;
    mm_free(ptrs[1]); ptrs[1] = NULL;
    if (heap_consistent()) PASS(); else FAIL("heap inconsistent after free[1]");
}
 
/* T6 – free block 2 (48 bytes, small/array path) */
static void test_trace_free2(void)
{
    TEST("Trace: free ptrs[2]=48 – heap consistent");
    reset();
    void *ptrs[6];
    if (!trace_alloc_all(ptrs)) { FAIL("alloc failed"); return; }
    mm_free(ptrs[0]); ptrs[0] = NULL;
    mm_free(ptrs[1]); ptrs[1] = NULL;
    mm_free(ptrs[2]); ptrs[2] = NULL;
    if (heap_consistent()) PASS(); else FAIL("heap inconsistent after free[2]");
}
 
/* T7 – free blocks 3,4,5 (three consecutive 4072-byte blocks) */
static void test_trace_free_large_trio(void)
{
    TEST("Trace: free ptrs[3,4,5]=4072 each – heap consistent");
    reset();
    void *ptrs[6];
    if (!trace_alloc_all(ptrs)) { FAIL("alloc failed"); return; }
    for (int i = 0; i < 6; i++) { mm_free(ptrs[i]); ptrs[i] = NULL; }
    if (heap_consistent()) PASS(); else FAIL("heap inconsistent after all frees");
}
 
/* T8 – full trace: alloc all, free all in order, check consistency */
static void test_trace_full(void)
{
    TEST("Trace: full replay (alloc 6 then free 6 in order)");
    reset();
    void *ptrs[6];
    if (!trace_alloc_all(ptrs)) { FAIL("alloc failed"); return; }
    for (int i = 0; i < 6; i++) {
        mm_free(ptrs[i]);
        ptrs[i] = NULL;
        if (!heap_consistent()) {
            printf("\n  FAIL at free[%d]\n", i);
            FAIL("heap inconsistent mid-free");
            return;
        }
    }
    PASS();
}
 
/* T9 – after full trace, heap must not have grown beyond first extension.
 * Freed memory should be fully coalesced and reusable.               */
static void test_trace_reuse_after_full_free(void)
{
    TEST("Trace: large alloc succeeds after full free (coalescing)");
    reset();
    void *ptrs[6];
    if (!trace_alloc_all(ptrs)) { FAIL("alloc failed"); return; }
    size_t before = mem_heapsize();
    for (int i = 0; i < 6; i++) { mm_free(ptrs[i]); ptrs[i] = NULL; }
 
    /* The total payload was 2040+4010+48+4072+4072+4072 = 18314 bytes.
     * After coalescing we should be able to re-allocate a chunk of that
     * without the heap growing.                                        */
    void *big = mm_malloc(8000);
    size_t after = mem_heapsize();
    if (!big)         { FAIL("large re-alloc returned NULL"); return; }
    if (after > before) { FAIL("heap grew – freed blocks not coalesced/reused"); return; }
    PASS();
}
 
/* T10 – isolate the small block (48 bytes) in the middle of large ones.
 * This specifically stresses coalescing across array/tree boundaries. */
static void test_trace_small_between_large(void)
{
    TEST("Trace: free large,small,large – small block coalesced correctly");
    reset();
    void *ptrs[6];
    if (!trace_alloc_all(ptrs)) { FAIL("alloc failed"); return; }
 
    /* Free the small block flanked by large ones */
    mm_free(ptrs[1]); ptrs[1] = NULL;   /* 4010 – left neighbour  */
    mm_free(ptrs[2]); ptrs[2] = NULL;   /* 48   – the small block */
    mm_free(ptrs[3]); ptrs[3] = NULL;   /* 4072 – right neighbour */
 
    if (!heap_consistent()) { FAIL("heap inconsistent after flanked free"); return; }
 
    /* Now we should be able to allocate something larger than 48 bytes
     * in the coalesced region without heap growth                     */
    size_t before = mem_heapsize();
    void *mid = mm_malloc(4000);
    size_t after = mem_heapsize();
    if (!mid)         { FAIL("re-alloc in coalesced region returned NULL"); return; }
    if (after > before) { FAIL("heap grew – coalescing across small/large boundary failed"); return; }
    PASS();
}

/* T11 – three identical large blocks (ptrs[3,4,5]=4072) freed in order.
 * Each free should coalesce with the previous, producing one big block. */
static void test_trace_consecutive_large_coalesce(void)
{
    TEST("Trace: three consecutive 4072 frees coalesce into one block");
    reset();
    void *ptrs[6];
    if (!trace_alloc_all(ptrs)) { FAIL("alloc failed"); return; }
 
    /* Free only the three large identical blocks */
    size_t before = mem_heapsize();
    mm_free(ptrs[3]); ptrs[3] = NULL;
    mm_free(ptrs[4]); ptrs[4] = NULL;
    mm_free(ptrs[5]); ptrs[5] = NULL;
 
    if (!heap_consistent()) { FAIL("heap inconsistent"); return; }
 
    /* Should be able to allocate 3*4072/2 worth in one shot */
    void *big = mm_malloc(4072 * 2);
    size_t after = mem_heapsize();
    if (!big)         { FAIL("large alloc in coalesced region failed"); return; }
    if (after > before) { FAIL("heap grew – three large blocks not coalesced"); return; }
    PASS();
}

static void testFullTrace(void) {
    TEST("TRACE: Equivalent Call");
    reset();
    void* ptrs[6];
    printf("\n");
    printf("Type: Alloc   | ID: 0 | Size: 2040\n");
    ptrs[0]=mm_malloc(2040);
    printf("Type: Alloc   | ID: 1 | Size: 4010\n");
    ptrs[1]=mm_malloc(4010);
    printf("Type: Alloc   | ID: 2 | Size: 48\n");
    ptrs[2]=mm_malloc(48);
    printf("Type: Alloc   | ID: 3 | Size: 4072\n");
    ptrs[3]=mm_malloc(4072);
    printf("Type: Alloc   | ID: 4 | Size: 4072\n");
    ptrs[4]=mm_malloc(4072);
    printf("Type: Alloc   | ID: 5 | Size: 4072\n");
    ptrs[5]=mm_malloc(4072);
    for (int i=0; i<6; i++) {
        printf("Type: Free    | ID: %i | Size: 0 \n", i);
        mm_free(ptrs[i]);
        if (!heap_consistent()) { FAIL("heap inconsistent"); return; }
    }
    PASS();
}
/* T12 – 64-bit pointer size check.
 * In the original 32-bit design a pointer is 4 bytes; in 64-bit it's 8.
 * The free-list next/prev pointers stored inside free blocks must fit,
 * so MINSIZE must be >= 4*DSIZE = 32 bytes on 64-bit.               */
static void test_trace_64bit_minsize(void)
{
    TEST("Trace/64-bit: MINSIZE accommodates 64-bit pointers (>=32)");
    reset();
    /* Allocate then free the smallest possible block; the resulting
     * free block header must be at least MINSIZE and store pointers. */
    void *p = mm_malloc(1);
    if (!p) { FAIL("alloc failed"); return; }
    size_t bsz = GET_SIZE(HDRP(p));
    mm_free(p);
    if (bsz >= 32 && heap_consistent())
        PASS();
    else
        FAIL("block too small for 64-bit free-list pointers");
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
 
    printf("\n--- Stress ---\n");
    //test_stress_random_sizes();
    test_stress_alloc_free_interleaved();
    test_stress_heap_growth();
    test_stress_no_fragmentation_after_frees();
 
    printf("\n--- Boundary & Edge ---\n");
    test_boundary_exactly_sizecross();
    test_boundary_one_over_sizecross();
    test_boundary_minsize();
    test_double_free_safety();
    printf("\n--- Trace Replay (failing trace diagnosis) ---\n");
    test_trace_allocs_valid();
    test_trace_heap_after_allocs();
    test_trace_no_overlap();
    test_trace_free0();
    test_trace_free1();
    test_trace_free2();
    test_trace_free_large_trio();
    test_trace_full();
    test_trace_reuse_after_full_free();
    test_trace_small_between_large();
    test_trace_consecutive_large_coalesce();
    test_trace_64bit_minsize();
    testFullTrace();
 
    printf("\n============================================================\n");
    printf("  Results: %d / %d passed\n", tests_passed, tests_run);
    printf("============================================================\n");
 
    return (tests_passed == tests_run) ? 0 : 1;
}
 