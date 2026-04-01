/*
 * grade.c - Trace replay testing framework for mm.c (Static Memory Version)
 *
 * Build:
 * gcc -Wall -Wextra -g -o grade grade.c mm.c memlib.c -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "mm.h"
#include "memlib.h"

#define ALIGNMENT 8
#define MAX_OPS 150000
#define MAX_IDS 150000

/* ------------------------------------------------------------------ */
/* Test framework                                                     */
/* ------------------------------------------------------------------ */
static int tests_run    = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { tests_run++; printf("[TEST %2d] %-55s", tests_run, name); } while(0)
#define PASS() \
    do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) \
    do { printf("FAIL - %s\n", msg); } while(0)

static void reset(void)
{
    mem_reset();
    mm_init();
}

/* ------------------------------------------------------------------ */
/* Pointer diagnostics                                                */
/* ------------------------------------------------------------------ */
static int in_heap(const void *p)
{
    return (char *)p >= (char *)mem_heap_lo() &&
           (char *)p <= (char *)mem_heap_hi();
}

static int aligned(const void *p)
{
    return ((uintptr_t)p % ALIGNMENT) == 0;
}

/* ------------------------------------------------------------------ */
/* Static Grader State (Bypasses the Libc Heap entirely)              */
/* ------------------------------------------------------------------ */
static char   trace_kinds[MAX_OPS];
static int    trace_ids[MAX_OPS];
static size_t trace_sizes[MAX_OPS];
static void* ptrs[MAX_IDS];

static int num_ids = 0;
static int num_ops = 0;

static int load_trace(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { 
        perror(path); 
        return 0; 
    }

    int junk;
    /* Header: junk, num_ids, num_ops, junk */
    if (fscanf(f, "%d %d %d %d", &junk, &num_ids, &num_ops, &junk) != 4) {
        fclose(f);
        return 0;
    }

    if (num_ops > MAX_OPS || num_ids > MAX_IDS) {
        printf("Trace exceeds static buffer limits.\n");
        fclose(f);
        return 0;
    }

    for (int i = 0; i < num_ops; i++) {
        fscanf(f, " %c %d", &trace_kinds[i], &trace_ids[i]);
        if (trace_kinds[i] == 'a' || trace_kinds[i] == 'r') {
            fscanf(f, "%zu", &trace_sizes[i]);
        } else {
            trace_sizes[i] = 0;
        }
    }

    fclose(f);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Trace Execution Runner                                             */
/* ------------------------------------------------------------------ */
static void run_trace_test(const char *filename)
{
    char test_name[256];
    snprintf(test_name, sizeof(test_name), "Trace Replay: %s", filename);
    
    if (strlen(test_name) > 50) {
        strcpy(test_name + 47, "...");
    }
    
    TEST(test_name);

    if (!load_trace(filename)) {
        FAIL("Failed to parse trace file");
        return;
    }

    /* Wipe our static pointer array clean before the run */
    memset(ptrs, 0, sizeof(ptrs));
    
    reset();

    int ok = 1;
    char fail_msg[128];

    for (int i = 0; i < num_ops; i++) {
        char kind = trace_kinds[i];
        int id = trace_ids[i];
        size_t size = trace_sizes[i];
        
        if (kind == 'a') {
            ptrs[id] = mm_malloc(size);
            
            if (size > 0 && !ptrs[id]) {
                snprintf(fail_msg, sizeof(fail_msg), "Op %d (Alloc): returned NULL", i);
                ok = 0; break;
            }
            if (size > 0 && !aligned(ptrs[id])) {
                snprintf(fail_msg, sizeof(fail_msg), "Op %d (Alloc): pointer not aligned", i);
                ok = 0; break;
            }
            if (size > 0 && !in_heap(ptrs[id])) {
                snprintf(fail_msg, sizeof(fail_msg), "Op %d (Alloc): pointer out of bounds", i);
                ok = 0; break;
            }

        } else if (kind == 'f') {
            mm_free(ptrs[id]);
            ptrs[id] = NULL;
            
        } else if (kind == 'r') {
            ptrs[id] = mm_realloc(ptrs[id], size);
            
            if (size > 0 && !ptrs[id]) {
                snprintf(fail_msg, sizeof(fail_msg), "Op %d (Realloc): returned NULL", i);
                ok = 0; break;
            }
            if (size > 0 && !aligned(ptrs[id])) {
                snprintf(fail_msg, sizeof(fail_msg), "Op %d (Realloc): pointer not aligned", i);
                ok = 0; break;
            }
            if (size > 0 && !in_heap(ptrs[id])) {
                snprintf(fail_msg, sizeof(fail_msg), "Op %d (Realloc): pointer out of bounds", i);
                ok = 0; break;
            }
        }
    }

    if (ok) {
        PASS();
    } else {
        FAIL(fail_msg);
    }
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <trace_file_1.rep> [trace_file_2.rep ...]\n", argv[0]);
        return 1;
    }

    mem_init();

    printf("============================================================\n");
    printf("  mm.c Trace Execution Test Suite (Static Safe)\n");
    printf("============================================================\n\n");

    for (int i = 1; i < argc; i++) {
        run_trace_test(argv[i]);
    }

    printf("\n============================================================\n");
    printf("  Results: %d / %d traces passed\n", tests_passed, tests_run);
    printf("============================================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}