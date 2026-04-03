/*
 * gen_trace.c  –  Converts a .rep trace into a self-contained C file.
 *
 * The output contains two functions:
 *   void run_libc(TraceResult *r)  – replays with libc malloc/free/realloc
 *   void run_mm(TraceResult *r)    – replays with mm_malloc/mm_free/mm_realloc
 *
 * All pointer bookkeeping uses a fixed-size stack array so there is zero
 * heap allocation outside of the allocator under test.
 *
 * Build:   gcc -Wall -O2 -o gen_trace gen_trace.c
 * Usage:   ./gen_trace trace.rep > trace_ops.c
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 
int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s trace.rep > trace_ops.c\n", argv[0]);
        return 1;
    }
 
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 1; }
 
    /* ── parse header ── */
    int suggest_heap, num_ids, num_ops, weight_line;
    if (fscanf(f, "%d\n%d\n%d\n%d\n",
               &suggest_heap, &num_ids, &num_ops, &weight_line) != 4) {
        fprintf(stderr, "Bad trace header\n");
        return 1;
    }
 
    /* ── read all ops into memory ── */
    typedef struct { char kind; int id; size_t size; } Op;
    Op *ops = calloc(num_ops, sizeof(Op));
    for (int i = 0; i < num_ops; i++) {
        char k; int id; size_t sz = 0;
        if (fscanf(f, " %c %d", &k, &id) != 2) {
            fprintf(stderr, "Parse error at op %d\n", i); return 1;
        }
        if (k == 'a' || k == 'r') fscanf(f, " %zu", &sz);
        ops[i].kind = k; ops[i].id = id; ops[i].size = sz;
    }
    fclose(f);
 
    /* ── emit the generated C file ── */
    printf("/*\n"
           " * trace_ops.c  –  AUTO-GENERATED from %s\n"
           " * Do not edit by hand.  Regenerate with gen_trace.\n"
           " *\n"
           " * Build together with grader.c, mm.c, memlib.c:\n"
           " *   gcc -Wall -O2 -o grader grader.c trace_ops.c mm.c memlib.c -lm\n"
           " */\n\n", argv[1]);
 
    printf("#include <stdlib.h>\n"
           "#include <string.h>\n"
           "#include \"mm.h\"\n"
           "#include \"memlib.h\"\n"
           "#include \"grader.h\"\n\n");
 
    printf("#define TRACE_NUM_IDS  %d\n", num_ids);
    printf("#define TRACE_NUM_OPS  %d\n\n", num_ops);
 
    /* ── run_libc ── */
    printf("void run_libc(TraceResult *r)\n{\n");
    printf("    void   *ptrs[TRACE_NUM_IDS];\n");
    printf("    size_t  sizes[TRACE_NUM_IDS];\n");
    printf("    memset(ptrs,  0, sizeof(ptrs));\n");
    printf("    memset(sizes, 0, sizeof(sizes));\n");
    printf("    size_t cur_payload = 0;\n");
    printf("    r->peak_payload = 0;\n");
    printf("    r->errors       = 0;\n\n");
 
    for (int i = 0; i < num_ops; i++) {
        Op *op = &ops[i];
        switch (op->kind) {
        case 'a':
            printf("    ptrs[%d] = malloc(%zu);\n", op->id, op->size);
            printf("    if (!ptrs[%d]) { r->errors++; }\n", op->id);
            printf("    else { sizes[%d] = %zu; cur_payload += %zu;"
                   " if (cur_payload > r->peak_payload) r->peak_payload = cur_payload; }\n",
                   op->id, op->size, op->size);
            break;
        case 'f':
            printf("    if (ptrs[%d]) { cur_payload -= sizes[%d];"
                   " free(ptrs[%d]); ptrs[%d] = NULL; sizes[%d] = 0; }\n",
                   op->id, op->id, op->id, op->id, op->id);
            break;
        case 'r':
            printf("    if (ptrs[%d]) cur_payload -= sizes[%d];\n", op->id, op->id);
            printf("    ptrs[%d] = realloc(ptrs[%d], %zu);\n", op->id, op->id, op->size);
            printf("    if (!ptrs[%d] && %zu > 0) { r->errors++; }\n", op->id, op->size);
            printf("    else { sizes[%d] = %zu; cur_payload += %zu;"
                   " if (cur_payload > r->peak_payload) r->peak_payload = cur_payload; }\n",
                   op->id, op->size, op->size);
            break;
        }
    }
 
    /* clean up any still-live pointers */
    printf("\n    /* clean up live allocations */\n");
    for (int i = 0; i < num_ids; i++)
        printf("    if (ptrs[%d]) { free(ptrs[%d]); ptrs[%d] = NULL; }\n", i, i, i);
    printf("}\n\n");
 
    /* ── run_mm ── */
    printf("void run_mm(TraceResult *r)\n{\n");
    printf("    mem_reset();\n");
    printf("    mm_init();\n");
    printf("    void   *ptrs[TRACE_NUM_IDS];\n");
    printf("    size_t  sizes[TRACE_NUM_IDS];\n");
    printf("    memset(ptrs,  0, sizeof(ptrs));\n");
    printf("    memset(sizes, 0, sizeof(sizes));\n");
    printf("    size_t cur_payload = 0;\n");
    printf("    r->peak_payload = 0;\n");
    printf("    r->errors       = 0;\n\n");
 
    for (int i = 0; i < num_ops; i++) {
        Op *op = &ops[i];
        switch (op->kind) {
        case 'a':
            printf("    ptrs[%d] = mm_malloc(%zu);\n", op->id, op->size);
            printf("    if (!ptrs[%d]) { r->errors++; }\n", op->id);
            printf("    else { sizes[%d] = %zu; cur_payload += %zu;"
                   " if (cur_payload > r->peak_payload) r->peak_payload = cur_payload; }\n",
                   op->id, op->size, op->size);
            break;
        case 'f':
            printf("    if (ptrs[%d]) { cur_payload -= sizes[%d];"
                   " mm_free(ptrs[%d]); ptrs[%d] = NULL; sizes[%d] = 0; }\n",
                   op->id, op->id, op->id, op->id, op->id);
            break;
        case 'r':
            printf("    if (ptrs[%d]) cur_payload -= sizes[%d];\n", op->id, op->id);
            printf("    ptrs[%d] = mm_realloc(ptrs[%d], %zu);\n", op->id, op->id, op->size);
            printf("    if (!ptrs[%d] && %zu > 0) { r->errors++; }\n", op->id, op->size);
            printf("    else { sizes[%d] = %zu; cur_payload += %zu;"
                   " if (cur_payload > r->peak_payload) r->peak_payload = cur_payload; }\n",
                   op->id, op->size, op->size);
            break;
        }
    }
 
    printf("\n    r->heap_size = mem_heapsize();\n");
    printf("}\n\n");
 
    /* ── metadata accessor ── */
    printf("int trace_num_ops(void)  { return TRACE_NUM_OPS; }\n");
    printf("int trace_num_ids(void)  { return TRACE_NUM_IDS; }\n");
 
    free(ops);
    return 0;
}