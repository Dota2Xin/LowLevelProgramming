#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"

#define MAX_IDS 20000
#define SANDBOX_CAPACITY (64 * 1024 * 1024) // 64 MB

char *sandbox_memory = NULL;
size_t sandbox_size = SANDBOX_CAPACITY;

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <trace_file>\n", argv[0]);
        return 1;
    }

    // 1. Allocate the sandbox with 16-byte alignment
    if (posix_memalign((void**)&sandbox_memory, 16, sandbox_size) != 0) {
        perror("Failed to allocate sandbox");
        return 1;
    }

    // 2. Initialize our simulated memory system
    mem_init();

    // 3. Load Trace
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("Trace open failed"); return 1; }

    int junk, num_ids, num_ops;
    fscanf(f, "%d %d %d %d", &junk, &num_ids, &num_ops, &junk);

    // 4. Setup trackers
    void **ptrs = calloc(num_ids + 1, sizeof(void *));
    size_t *sizes = calloc(num_ids + 1, sizeof(size_t));

    // 5. Initialize the user's allocator
    if (mm_init() < 0) {
        printf("mm_init failed!\n");
        return 1;
    }

    // 6. Run the ops
    for (int i = 0; i < num_ops; i++) {
        char type;
        int id;
        size_t size;
        fscanf(f, " %c %d", &type, &id);

        if (type == 'a') {
            fscanf(f, "%zu", &size);
            ptrs[id] = mm_malloc(size);
        } else if (type == 'f') {
            mm_free(ptrs[id]);
        } else if (type == 'r') {
            fscanf(f, "%zu", &size);
            ptrs[id] = mm_realloc(ptrs[id], size);
        }
    }

    printf("Finished trace %s successfully.\n", argv[1]);

    fclose(f);
    free(ptrs);
    free(sizes);
    free(sandbox_memory);
    return 0;
}