#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
 
#include "mm.h"
#include "memlib.h"

typedef struct { char k; int id; size_t sz; } Op;
double now() { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

int main(int argc, char **argv) {
    if (argc < 2) return printf("Usage: %s <trace>\n", argv[0]), 1;
    FILE *f = fopen(argv[1], "r");
    int ids, ops, junk; 
    fscanf(f, "%d %d %d %d", &junk, &ids, &ops, &junk);
    printf("IDS %i, OPS:%i\n", ids, ops);
    Op *v = malloc(ops * sizeof(Op));
    for (int i = 0; i < ops; i++) {
        fscanf(f, " %c %d", &v[i].k, &v[i].id);
        if (v[i].k != 'f') fscanf(f, "%zu", &v[i].sz);
    }
    fclose(f);

    void **p = calloc(ids, sizeof(void*));
    size_t *s = calloc(ids, sizeof(size_t));
    size_t peak = 0, cur = 0;
    printf("HELLO\n");
    // --- Run MM ---
    mem_init(); mm_init();
    double t0 = now();
    for (int i = 0; i < ops; i++) {
        if (v[i].k == 'a') {
            p[v[i].id] = mm_malloc(v[i].sz);
            s[v[i].id] = v[i].sz; cur += v[i].sz;
        } else if (v[i].k == 'f') {
            cur -= s[v[i].id]; mm_free(p[v[i].id]);
        } else {
            cur -= s[v[i].id]; p[v[i].id] = mm_realloc(p[v[i].id], v[i].sz);
            s[v[i].id] = v[i].sz; cur += v[i].sz;
        }
        if (cur > peak) peak = cur;
    }
    double t_mm = now() - t0;
    double U = (double)peak / mem_heapsize();
    printf("Here\n");
    // --- Run Libc ---
    memset(p, 0, ids * sizeof(void*));
    t0 = now();
    for (int i = 0; i < ops; i++) {
        if (v[i].k == 'a') p[v[i].id] = malloc(v[i].sz);
        else if (v[i].k == 'f') free(p[v[i].id]);
        else p[v[i].id] = realloc(p[v[i].id], v[i].sz);
    }
    double t_libc = now() - t0;

    // --- Results ---
    double T_ratio = t_libc / t_mm; // (Ops/t_mm) / (Ops/t_libc) simplifies to this
    double P = 0.6 * U + 0.4 * fmin(1.0, T_ratio);

    printf("Utilization: %.2f%% | T_ratio: %.3f | P: %.4f\n", U*100, T_ratio, P);

    free(v); free(p); free(s);
    return 0;
}