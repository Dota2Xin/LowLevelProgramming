/*
 * perf_grade.c  –  Performance grader for mm.c
 *
 * Replays a trace file against both your allocator (mm_malloc/mm_free)
 * and libc malloc/free, measures throughput and space utilisation for
 * each, then computes the weighted performance index:
 *
 *   P = w*U + (1-w) * min(1, T / T_libc)      (w = 0.6 by default)
 *
 * Trace file format (same as mdriver):
 *   Line 1:  suggested heap size  (ignored here)
 *   Line 2:  number of ids        (max simultaneous live allocations)
 *   Line 3:  number of operations
 *   Line 4:  weight               (ignored here)
 *   Then one operation per line:
 *     a <id> <size>   – allocate <size> bytes, remember pointer at slot <id>
 *     f <id>          – free the pointer stored in slot <id>
 *     r <id> <size>   – realloc pointer at slot <id> to <size> bytes
 *
 * Build:
 *   gcc -Wall -O2 -o perf_grade perf_grade.c mm.c memlib.c -lm
 *
 * Usage:
 *   ./perf_grade trace.rep              # run one trace
 *   ./perf_grade trace1.rep trace2.rep  # run several traces, average P
 *   ./perf_grade -w 0.5 trace.rep       # custom weight
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
 
#include "mm.h"
#include "memlib.h"
 
/* ------------------------------------------------------------------ */
/* Tunables                                                             */
/* ------------------------------------------------------------------ */
#define DEFAULT_W      0.6      /* utilisation weight                  */
#define MAX_IDS        65536    /* max simultaneous live allocations    */
#define LIBC_REPS      3        /* repeat libc run this many times and
                                   take the minimum (reduce noise)     */
#define MM_REPS        3        /* same for your allocator             */
 
/* ------------------------------------------------------------------ */
/* Trace representation                                                 */
/* ------------------------------------------------------------------ */
typedef enum { OP_ALLOC, OP_FREE, OP_REALLOC } OpKind;
 
typedef struct {
    OpKind kind;
    int    id;
    size_t size;   /* used by ALLOC and REALLOC */
} Op;
 
typedef struct {
    int   num_ids;
    int   num_ops;
    Op   *ops;
} Trace;

void print_op(Op *op) {
    const char *type_str;
    
    switch (op->kind) {
        case OP_ALLOC:   type_str = "ALLOC";   break;
        case OP_FREE:    type_str = "FREE ";   break;
        case OP_REALLOC: type_str = "REALLOC"; break;
        default:         type_str = "UNKNOWN"; break;
    }

    /* * %-7s  : Left-aligns the type string (up to 7 chars)
     * %4d   : Pads the ID to 4 digits for alignment
     * %zu   : The correct format specifier for size_t
     */
    //printf("Type: %-7s | ID: %4d | Size: %zu\n", 
    //        type_str, op->id, op->size);
}
 
/* ------------------------------------------------------------------ */
/* Timing                                                               */
/* ------------------------------------------------------------------ */
static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
 
/* ------------------------------------------------------------------ */
/* Trace I/O                                                            */
/* ------------------------------------------------------------------ */
static Trace *load_trace(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return NULL; }
 
    Trace *t = calloc(1, sizeof(Trace));
 
    /* Header lines */
    int suggest_heap, weight_line;
    if (fscanf(f, "%d\n%d\n%d\n%d\n",
               &suggest_heap, &t->num_ids, &t->num_ops, &weight_line) != 4) {
        fprintf(stderr, "Bad trace header in %s\n", path);
        fclose(f); free(t); return NULL;
    }
 
    t->ops = calloc(t->num_ops, sizeof(Op));
    for (int i = 0; i < t->num_ops; i++) {
        char kind;
        int  id;
        size_t sz = 0;
        if (fscanf(f, " %c %d", &kind, &id) != 2) {
            fprintf(stderr, "Parse error at op %d in %s\n", i, path);
            fclose(f); free(t->ops); free(t); return NULL;
        }
        if (kind == 'a' || kind == 'r') {
            if (fscanf(f, " %zu", &sz) != 1) {
                fprintf(stderr, "Missing size at op %d in %s\n", i, path);
                fclose(f); free(t->ops); free(t); return NULL;
            }
        }
        t->ops[i].id   = id;
        t->ops[i].size = sz;
        switch (kind) {
            case 'a': t->ops[i].kind = OP_ALLOC;   break;
            case 'f': t->ops[i].kind = OP_FREE;     break;
            case 'r': t->ops[i].kind = OP_REALLOC;  break;
            default:
                fprintf(stderr, "Unknown op '%c' at line %d\n", kind, i+5);
                fclose(f); free(t->ops); free(t); return NULL;
        }
    }
    fclose(f);
    return t;
}
 
static void free_trace(Trace *t)
{
    if (t) { free(t->ops); free(t); }
}
 
/* ------------------------------------------------------------------ */
/* Run against libc malloc                                              */
/* ------------------------------------------------------------------ */
typedef struct {
    double elapsed_sec;
    int    errors;          /* NULL returns that shouldn't be NULL  */
    size_t total_requested; /* sum of all requested payload bytes   */
    size_t peak_in_use;     /* peak sum of live payload sizes       */
} RunResult;
 
static RunResult run_libc(const Trace *t)
{
    RunResult r = {0};
    void   **ptrs   = calloc(t->num_ids, sizeof(void *));
    size_t  *sizes  = calloc(t->num_ids, sizeof(size_t));
    size_t   in_use = 0;
 
    double t0 = now_sec();
 
    for (int i = 0; i < t->num_ops; i++) {
        Op *op = &t->ops[i];
        switch (op->kind) {
        case OP_ALLOC:
            ptrs[op->id]  = malloc(op->size);
            sizes[op->id] = op->size;
            if (!ptrs[op->id] && op->size > 0) { r.errors++; break; }
            in_use += op->size;
            r.total_requested += op->size;
            if (in_use > r.peak_in_use) r.peak_in_use = in_use;
            break;
        case OP_FREE:
            if (ptrs[op->id]) {
                in_use -= sizes[op->id];
                free(ptrs[op->id]);
                ptrs[op->id]  = NULL;
                sizes[op->id] = 0;
            }
            break;
        case OP_REALLOC:
            in_use -= sizes[op->id];
            ptrs[op->id]  = realloc(ptrs[op->id], op->size);
            sizes[op->id] = op->size;
            if (!ptrs[op->id] && op->size > 0) { r.errors++; break; }
            in_use += op->size;
            r.total_requested += op->size;
            if (in_use > r.peak_in_use) r.peak_in_use = in_use;
            break;
        }
    }
 
    r.elapsed_sec = now_sec() - t0;
 
    /* Clean up anything still live */
    for (int i = 0; i < t->num_ids; i++) if (ptrs[i]) free(ptrs[i]);
    free(ptrs); free(sizes);
    return r;
}
 
/* ------------------------------------------------------------------ */
/* Run against mm_malloc                                                */
/* ------------------------------------------------------------------ */
static RunResult run_mm(const Trace *t)
{
    RunResult r = {0};
 
    mem_reset();
    if (mm_init() != 0) {
        fprintf(stderr, "mm_init() failed\n");
        r.errors = 1;
        return r;
    }
 
    void   **ptrs  = calloc(t->num_ids, sizeof(void *));
    size_t  *sizes = calloc(t->num_ids, sizeof(size_t));
    size_t   in_use = 0;
 
    double t0 = now_sec();
 
    for (int i = 0; i < t->num_ops; i++) {
        Op *op = &t->ops[i];
        print_op(op); 
        switch (op->kind) {
        case OP_ALLOC:
            ptrs[op->id]  = mm_malloc(op->size);
            sizes[op->id] = op->size;
            if (!ptrs[op->id] && op->size > 0) { r.errors++; break; }
            in_use += op->size;
            r.total_requested += op->size;
            if (in_use > r.peak_in_use) r.peak_in_use = in_use;
            break;
        case OP_FREE:
            if (ptrs[op->id]) {
                in_use -= sizes[op->id];
                mm_free(ptrs[op->id]);
                ptrs[op->id]  = NULL;
                sizes[op->id] = 0;
            }
            break;
        case OP_REALLOC:
            in_use -= sizes[op->id];
            ptrs[op->id]  = mm_realloc(ptrs[op->id], op->size);
            sizes[op->id] = op->size;
            if (!ptrs[op->id] && op->size > 0) { r.errors++; break; }
            in_use += op->size;
            r.total_requested += op->size;
            if (in_use > r.peak_in_use) r.peak_in_use = in_use;
            break;
        }
    }
 
    r.elapsed_sec = now_sec() - t0;
 
    free(ptrs); free(sizes);
    return r;
}
 
/* ------------------------------------------------------------------ */
/* Grade one trace                                                      */
/* ------------------------------------------------------------------ */
static void grade_trace(const char *path, double w)
{
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("Trace: %s\n", path);
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
 
    Trace *t = load_trace(path);
    if (!t) return;
 
    /* ── libc: take best of LIBC_REPS runs ── */
    RunResult libc_best = {.elapsed_sec = 1e18};
    for (int rep = 0; rep < LIBC_REPS; rep++) {
        RunResult r = run_libc(t);
        if (r.elapsed_sec < libc_best.elapsed_sec) libc_best = r;
    }
 
    /* ── mm: take best of MM_REPS runs ── */
    RunResult mm_best = {.elapsed_sec = 1e18};
    for (int rep = 0; rep < MM_REPS; rep++) {
        RunResult r = run_mm(t);
        if (r.elapsed_sec < mm_best.elapsed_sec) mm_best = r;
    }
 
    /* ── Metrics ── */
 
    /* Throughput = ops / second */
    double T_libc = (libc_best.elapsed_sec > 0)
                  ? t->num_ops / libc_best.elapsed_sec : 0;
    double T_mm   = (mm_best.elapsed_sec > 0)
                  ? t->num_ops / mm_best.elapsed_sec   : 0;
 
    /* Space utilisation = peak payload / peak heap size.
     * We use mem_heapsize() after the mm run as a proxy for peak heap –
     * this slightly over-counts (heap never shrinks) but matches mdriver. */
    mem_reset(); mm_init();
    /* re-run once more just to get the final heap size */
    void **ptrs2  = calloc(t->num_ids, sizeof(void *));
    size_t *sz2   = calloc(t->num_ids, sizeof(size_t));
    size_t peak_payload = 0, cur_payload = 0;
    for (int i = 0; i < t->num_ops; i++) {
        Op *op = &t->ops[i];
        if (op->kind == OP_ALLOC) {
            ptrs2[op->id] = mm_malloc(op->size);
            sz2[op->id]   = op->size;
            cur_payload  += op->size;
            if (cur_payload > peak_payload) peak_payload = cur_payload;
        } else if (op->kind == OP_FREE && ptrs2[op->id]) {
            cur_payload  -= sz2[op->id];
            mm_free(ptrs2[op->id]);
            ptrs2[op->id] = NULL; sz2[op->id] = 0;
        } else if (op->kind == OP_REALLOC) {
            cur_payload  -= sz2[op->id];
            ptrs2[op->id] = mm_realloc(ptrs2[op->id], op->size);
            sz2[op->id]   = op->size;
            cur_payload  += op->size;
            if (cur_payload > peak_payload) peak_payload = cur_payload;
        }
    }
    size_t heap_size = mem_heapsize();
    free(ptrs2); free(sz2);
 
    double U = (heap_size > 0)
             ? (double)peak_payload / (double)heap_size
             : 0.0;
    if (U > 1.0) U = 1.0;   /* cap at 1 (can happen due to overhead) */
 
    double throughput_ratio = (T_libc > 0) ? T_mm / T_libc : 0.0;
    double P = w * U + (1.0 - w) * fmin(1.0, throughput_ratio);
 
    /* ── Report ── */
    printf("  Operations  : %d\n", t->num_ops);
    printf("  Errors      : mm=%d  libc=%d\n", mm_best.errors, libc_best.errors);
    printf("\n");
 
    printf("  ┌─────────────────────┬──────────────┬──────────────┐\n");
    printf("  │ Metric              │   mm_malloc  │  libc malloc │\n");
    printf("  ├─────────────────────┼──────────────┼──────────────┤\n");
    printf("  │ Time (s)            │ %12.6f │ %12.6f │\n",
           mm_best.elapsed_sec, libc_best.elapsed_sec);
    printf("  │ Throughput (Kop/s)  │ %12.1f │ %12.1f │\n",
           T_mm / 1000.0, T_libc / 1000.0);
    printf("  │ Peak payload (KB)   │ %12.1f │ %12.1f │\n",
           peak_payload / 1024.0, libc_best.peak_in_use / 1024.0);
    printf("  │ Peak heap (KB)      │ %12.1f │     (n/a)    │\n",
           heap_size / 1024.0);
    printf("  └─────────────────────┴──────────────┴──────────────┘\n");
 
    printf("\n");
    printf("  Space utilisation  U  = %.1f%%\n", U * 100.0);
    printf("  Throughput ratio T/Tlibc = %.3f  (%s)\n",
           throughput_ratio,
           throughput_ratio >= 1.0 ? "faster than libc" :
           throughput_ratio >= 0.5 ? "within 2x of libc" : "needs work");
    printf("  Weight w               = %.2f\n", w);
    printf("\n");
 
    /* Letter grade */
    const char *grade;
    if      (P >= 0.92) grade = "A  (excellent)";
    else if (P >= 0.80) grade = "B  (good)";
    else if (P >= 0.68) grade = "C  (acceptable)";
    else if (P >= 0.55) grade = "D  (needs improvement)";
    else                grade = "F  (significant issues)";
 
    printf("  ╔══════════════════════════════════════╗\n");
    printf("  ║  Performance index  P = %.4f        ║\n", P);
    printf("  ║  Grade: %-28s  ║\n", grade);
    printf("  ╚══════════════════════════════════════╝\n\n");
 
    if (mm_best.errors > 0)
        printf("  ⚠  WARNING: mm_malloc returned %d unexpected NULL(s)\n\n",
               mm_best.errors);
 
    free_trace(t);
}
 
/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    double w = DEFAULT_W;
    int    first_trace = 1;
 
    /* Parse optional -w flag */
    if (argc >= 3 && strcmp(argv[1], "-w") == 0) {
        w = atof(argv[2]);
        if (w < 0.0 || w > 1.0) {
            fprintf(stderr, "Weight must be between 0 and 1\n");
            return 1;
        }
        first_trace = 3;
    }
 
    if (argc <= first_trace) {
        fprintf(stderr,
            "Usage: %s [-w weight] trace.rep [trace2.rep ...]\n\n"
            "Trace format (mdriver compatible):\n"
            "  Line 1: suggested heap size\n"
            "  Line 2: number of ids\n"
            "  Line 3: number of operations\n"
            "  Line 4: weight (ignored)\n"
            "  Then: 'a <id> <size>'  'f <id>'  'r <id> <size>'\n",
            argv[0]);
        return 1;
    }
 
    mem_init();
 
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║       mm_malloc Performance Grader               ║\n");
    printf("║  P = %.1fw·U + %.1f·min(1, T/T_libc)              ║\n", w, 1.0-w);
    printf("╚══════════════════════════════════════════════════╝\n\n");
 
    int    num_traces = argc - first_trace;
    double total_P    = 0.0;
 
    for (int i = first_trace; i < argc; i++) {
        grade_trace(argv[i], w);
 
        /* Re-run quietly to get P for the average */
        Trace *t = load_trace(argv[i]);
        if (!t) continue;
 
        RunResult libc_r = {.elapsed_sec = 1e18};
        for (int r = 0; r < LIBC_REPS; r++) {
            RunResult rr = run_libc(t);
            if (rr.elapsed_sec < libc_r.elapsed_sec) libc_r = rr;
        }
        RunResult mm_r = {.elapsed_sec = 1e18};
        for (int r = 0; r < MM_REPS; r++) {
            RunResult rr = run_mm(t);
            if (rr.elapsed_sec < mm_r.elapsed_sec) mm_r = rr;
        }
 
        double T_libc = t->num_ops / libc_r.elapsed_sec;
        double T_mm   = t->num_ops / mm_r.elapsed_sec;
 
        mem_reset(); mm_init();
        void **p2 = calloc(t->num_ids, sizeof(void*));
        size_t *s2 = calloc(t->num_ids, sizeof(size_t));
        size_t pk = 0, cu = 0;
        for (int j = 0; j < t->num_ops; j++) {
            Op *op = &t->ops[j];
            if (op->kind == OP_ALLOC) {
                p2[op->id] = mm_malloc(op->size); s2[op->id] = op->size;
                cu += op->size; if (cu > pk) pk = cu;
            } else if (op->kind == OP_FREE && p2[op->id]) {
                cu -= s2[op->id]; mm_free(p2[op->id]); p2[op->id]=NULL; s2[op->id]=0;
            } else if (op->kind == OP_REALLOC) {
                cu -= s2[op->id];
                p2[op->id] = mm_realloc(p2[op->id], op->size); s2[op->id] = op->size;
                cu += op->size; if (cu > pk) pk = cu;
            }
        }
        double U = fmin(1.0, (mem_heapsize() > 0) ? (double)pk / mem_heapsize() : 0.0);
        free(p2); free(s2);
 
        total_P += w * U + (1.0 - w) * fmin(1.0, T_mm / T_libc);
        free_trace(t);
    }
 
    if (num_traces > 1) {
        double avg_P = total_P / num_traces;
        const char *grade =
            avg_P >= 0.92 ? "A  (excellent)"      :
            avg_P >= 0.80 ? "B  (good)"           :
            avg_P >= 0.68 ? "C  (acceptable)"     :
            avg_P >= 0.55 ? "D  (needs work)"     : "F";
        printf("══════════════════════════════════════════════════\n");
        printf("  OVERALL  (%d traces)\n", num_traces);
        printf("  Average P = %.4f   Grade: %s\n", avg_P, grade);
        printf("══════════════════════════════════════════════════\n\n");
    }
 
    return 0;
}