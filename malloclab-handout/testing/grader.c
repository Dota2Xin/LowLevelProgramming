/*
 * grader.c  –  Times run_libc() and run_mm() (from trace_ops.c),
 *              then computes the weighted performance index.
 *
 * P = w*U + (1-w) * min(1, T / T_libc)    default w = 0.6
 *
 * Build (after generating trace_ops.c with gen_trace):
 *   gcc -Wall -O2 -o grader grader.c trace_ops.c mm.c memlib.c -lm
 *
 * Usage:
 *   ./grader           # uses default w=0.6
 *   ./grader 0.5       # custom weight
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
 
#include "mm.h"
#include "memlib.h"
#include "grader.h"
 
/* ── tunables ── */
#define DEFAULT_W  0.6
#define REPS       5    /* repeat each run; take the minimum time */
 
/* ── timing ── */
static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
 
/* ── grade bands ── */
static const char *letter(double P)
{
    if (P >= 0.92) return "A  (excellent)";
    if (P >= 0.80) return "B  (good)";
    if (P >= 0.68) return "C  (acceptable)";
    if (P >= 0.55) return "D  (needs improvement)";
    return             "F  (significant issues)";
}
 
/* ── bar chart helper (width = 40 chars) ── */
static void bar(const char *label, double frac, const char *suffix)
{
    int filled = (int)(frac * 40.0 + 0.5);
    if (filled > 40) filled = 40;
    printf("  %-22s [", label);
    for (int i = 0; i < 40; i++) putchar(i < filled ? '#' : ' ');
    printf("] %s\n", suffix);
}
 
int main(int argc, char *argv[])
{
    double w = DEFAULT_W;
    if (argc >= 2) {
        w = atof(argv[1]);
        if (w < 0.0 || w > 1.0) {
            fprintf(stderr, "Weight must be in [0,1]\n");
            return 1;
        }
    }
 
    mem_init();
 
    int num_ops = trace_num_ops();
    int num_ids = trace_num_ids();
 
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║          mm_malloc vs libc  –  Performance Grader   ║\n");
    printf("║      P = %.1fw·U  +  %.1f·min(1, T/T_libc)            ║\n", w, 1.0-w);
    printf("╚══════════════════════════════════════════════════════╝\n");
    printf("  Operations: %d    IDs: %d    Weight w: %.2f\n\n",
           num_ops, num_ids, w);
 
     /* ── time libc ── */
    TraceResult libc_r = {0};
    double      libc_best = 1e18;
    for (int rep = 0; rep < REPS; rep++) {
        TraceResult tmp = {0};
        double t0 = now_sec();
        run_libc(&tmp);
        double t1 = now_sec();
        if (t1 - t0 < libc_best) {
            libc_best = t1 - t0;
            libc_r    = tmp;
        }
    }
    printf("HELLO\n");
    /* ── time mm ── */
    TraceResult mm_r   = {0};
    double      mm_best = 1e18;
    for (int rep = 0; rep < REPS; rep++) {
        TraceResult tmp = {0};
        double t0 = now_sec();
        run_mm(&tmp);
        double t1 = now_sec();
        if (t1 - t0 < mm_best) {
            mm_best = t1 - t0;
            mm_r    = tmp;
        }
    }
 
 
    /* ── metrics ── */
    double T_libc = (libc_best > 0) ? num_ops / libc_best : 0;
    double T_mm   = (mm_best   > 0) ? num_ops / mm_best   : 0;
    double T_ratio = (T_libc > 0) ? T_mm / T_libc : 0;
 
    /* Utilisation: peak payload / peak heap committed */
    double U = (mm_r.heap_size > 0)
             ? (double)mm_r.peak_payload / (double)mm_r.heap_size
             : 0.0;
    if (U > 1.0) U = 1.0;
 
    double P = w * U + (1.0 - w) * fmin(1.0, T_ratio);
 
    /* ── results table ── */
    printf("  ┌──────────────────────────┬────────────────┬────────────────┐\n");
    printf("  │ Metric                   │   mm_malloc    │  libc malloc   │\n");
    printf("  ├──────────────────────────┼────────────────┼────────────────┤\n");
    printf("  │ Best time (s)            │ %14.6f │ %14.6f │\n",
           mm_best, libc_best);
    printf("  │ Throughput  (Kops/s)     │ %14.1f │ %14.1f │\n",
           T_mm / 1e3, T_libc / 1e3);
    printf("  │ Peak payload (KB)        │ %14.1f │ %14.1f │\n",
           mm_r.peak_payload / 1024.0, libc_r.peak_payload / 1024.0);
    printf("  │ Peak heap   (KB)         │ %14.1f │    (unmeasured) │\n",
           mm_r.heap_size / 1024.0);
    printf("  │ Errors (unexpected NULL) │ %14d │ %14d │\n",
           mm_r.errors, libc_r.errors);
    printf("  └──────────────────────────┴────────────────┴────────────────┘\n\n");
 
    /* ── visual bars ── */
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f%%", U * 100.0);
        bar("Utilisation  U", U, buf);
    }
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.3f  (T/T_libc)", T_ratio);
        bar("Throughput ratio", fmin(1.0, T_ratio), buf);
    }
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "P = %.4f", P);
        bar("Performance index P", P, buf);
    }
    printf("\n");
 
    /* ── component breakdown ── */
    printf("  Utilisation component   :  %.2f * %.4f = %.4f\n",
           w, U, w * U);
    printf("  Throughput component    :  %.2f * %.4f = %.4f\n",
           1.0 - w, fmin(1.0, T_ratio), (1.0 - w) * fmin(1.0, T_ratio));
    printf("  ────────────────────────────────────────────────\n");
    printf("  Performance index     P = %.4f\n\n", P);
 
    /* ── throughput commentary ── */
    if (T_ratio >= 1.0)
        printf("  ✓ Throughput: faster than libc  (%.2fx)\n", T_ratio);
    else if (T_ratio >= 0.5)
        printf("  ~ Throughput: within 2x of libc  (%.2fx libc speed)\n", T_ratio);
    else
        printf("  ✗ Throughput: %.2fx libc speed – may want to profile hot paths\n",
               T_ratio);
 
    if (U >= 0.85)
        printf("  ✓ Utilisation: good (%.1f%% of heap used for payload)\n", U*100.0);
    else if (U >= 0.65)
        printf("  ~ Utilisation: moderate (%.1f%%) – fragmentation likely culprit\n",
               U * 100.0);
    else
        printf("  ✗ Utilisation: low (%.1f%%) – significant internal or external"
               " fragmentation\n", U * 100.0);
 
    if (mm_r.errors > 0)
        printf("\n  ⚠  WARNING: mm_malloc returned NULL %d time(s) unexpectedly.\n"
               "     Throughput and utilisation numbers are unreliable.\n",
               mm_r.errors);
 
    /* ── final grade ── */
    printf("\n");
    printf("  ╔══════════════════════════════════════════╗\n");
    printf("  ║  Performance index  P  =  %-6.4f        ║\n", P);
    printf("  ║  Grade : %-30s  ║\n", letter(P));
    printf("  ╚══════════════════════════════════════════╝\n\n");
 
    return (mm_r.errors > 0) ? 1 : 0;
}