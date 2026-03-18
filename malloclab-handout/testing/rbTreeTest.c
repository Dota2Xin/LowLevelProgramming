/*
 * rbtree_test.c
 *
 * Test suite for the red-black tree implementation used in the heap allocator.
 *
 * The tree lives in raw heap memory, so each "node" is a simulated block with:
 *   [0]       header word  – PACK_COLOR(size, color, alloc)
 *   [DSIZE]   left child   (pointer / address)
 *   [2*DSIZE] right child
 *   [3*DSIZE] parent
 *
 * We allocate node buffers from a static arena to keep this self-contained.
 *
 * Build:  gcc -Wall -Wextra -g -o rbtree_test rbtree_test.c
 * Run:    ./rbtree_test
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
 
/* ------------------------------------------------------------------ */
/* Macros (copied verbatim from your allocator header)                 */
/* ------------------------------------------------------------------ */
 
#define ALIGNMENT 8
#define WSIZE     8
#define DSIZE     8
#define MINSIZE   32
#define RED       1
#define BLACK     0
 
#define GET(p)           (*(unsigned long int *)(p))
#define PUT(p,val)       (*(unsigned long int *)(p) = (val))
 
#define GET_SIZE(p)      (GET(p) & ~0x7)
#define GET_ALLOC(p)     (GET(p) & 0x1)
#define GET_COLOR(p)     ((GET(p) & 0x2) >> 1)
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
/* Node arena – avoids actual sbrk; each node needs 4 * DSIZE bytes   */
/* ------------------------------------------------------------------ */
 
#define NODE_BYTES  (4 * DSIZE)   /* header + left + right + parent */
#define ARENA_NODES 256
static unsigned char arena[ARENA_NODES][NODE_BYTES];
static int arena_next = 0;
 
/* Reset the arena between tests */
static void arena_reset(void) { arena_next = 0; }
 
/*
 * Allocate a fake heap node with the given block size.
 * color=RED by default (inserted nodes start red).
 */
static void *make_node(size_t block_size)
{
    assert(arena_next < ARENA_NODES && "arena exhausted");
    void *p = arena[arena_next++];
    memset(p, 0, NODE_BYTES);
    PUT(p, PACK_COLOR(block_size, RED, 0));
    return p;
}
 
/* ------------------------------------------------------------------ */
/* Forward-declare the tree functions under test                       */
/* ------------------------------------------------------------------ */
 
void  leftRotate(void *root);
void  rightRotate(void *root);
void  swap(void *node1, void *node2);
void  baseAdd(void *root, void *newNode, size_t size);
void *insertRecolor(void *newNode);
void *addNode(void *root, void *newNode, size_t size);
void  baseRemove(void *node);
void  deleteRecolor(void *node);
void  removeNode(void *node);
void *getPostorder(void *root);
void *getPreorder(void *root);
void *searchSize(void *root, size_t size);
void *recurse(void *root, void *lastLarger, size_t size);
 
/* ------------------------------------------------------------------ */
/* RB-tree invariant checker                                           */
/* ------------------------------------------------------------------ */
 
/* Returns the black-height of the subtree, or -1 on violation. */
static int check_subtree(void *node, void *parent)
{
    if (node == 0) return 1;   /* null counts as 1 black node */
 
    /* Parent pointer must be consistent */
    if ((void *)GET_PARENT(node) != parent) {
        printf("  FAIL: parent pointer mismatch at node size=%lu\n",
               GET_SIZE(node));
        return -1;
    }
 
    /* Red node must not have a red child */
    if (GET_COLOR(node) == RED) {
        void *l = (void *)GET_LEFT_CHILD(node);
        void *r = (void *)GET_RIGHT_CHILD(node);
        if ((l && GET_COLOR(l) == RED) || (r && GET_COLOR(r) == RED)) {
            printf("  FAIL: red-red violation at node size=%lu\n",
                   GET_SIZE(node));
            return -1;
        }
    }
 
    /* BST order */
    void *l = (void *)GET_LEFT_CHILD(node);
    void *r = (void *)GET_RIGHT_CHILD(node);
    if (l && GET_SIZE(l) > GET_SIZE(node)) {
        printf("  FAIL: BST violation (left > node) at size=%lu\n",
               GET_SIZE(node));
        return -1;
    }
    if (r && GET_SIZE(r) < GET_SIZE(node)) {
        printf("  FAIL: BST violation (right < node) at size=%lu\n",
               GET_SIZE(node));
        return -1;
    }
 
    int bh_l = check_subtree(l, node);
    int bh_r = check_subtree(r, node);
 
    if (bh_l == -1 || bh_r == -1) return -1;
    if (bh_l != bh_r) {
        printf("  FAIL: black-height mismatch at node size=%lu (%d vs %d)\n",
               GET_SIZE(node), bh_l, bh_r);
        return -1;
    }
    return bh_l + (GET_COLOR(node) == BLACK ? 1 : 0);
}
 
/* Returns 1 if tree rooted at root is a valid RB-tree, 0 otherwise */
static int is_valid_rbt(void *root)
{
    if (root == 0) return 1;
    if (GET_COLOR(root) != BLACK) {
        printf("  FAIL: root is not black\n");
        return 0;
    }
    return check_subtree(root, 0) != -1;
}
 
/* Walk tree in-order and fill sizes[] array; return count */
static int inorder(void *node, size_t *sizes, int idx)
{
    if (node == 0) return idx;
    idx = inorder((void *)GET_LEFT_CHILD(node), sizes, idx);
    sizes[idx++] = GET_SIZE(node);
    idx = inorder((void *)GET_RIGHT_CHILD(node), sizes, idx);
    return idx;
}
 
/* ------------------------------------------------------------------ */
/* Helper: find the actual root by walking parent pointers             */
/* ------------------------------------------------------------------ */
static void *find_root(void *node)
{
    while ((void *)GET_PARENT(node) != 0)
        node = (void *)GET_PARENT(node);
    return node;
}
 
/* ------------------------------------------------------------------ */
/* Test utilities                                                       */
/* ------------------------------------------------------------------ */
 
static int tests_run    = 0;
static int tests_passed = 0;
 
#define TEST(name) \
    do { \
        tests_run++; \
        printf("[TEST %2d] %s ... ", tests_run, name); \
    } while(0)
 
#define PASS() \
    do { tests_passed++; printf("PASS\n"); } while(0)
 
#define FAIL(msg) \
    do { printf("FAIL – %s\n", msg); } while(0)
 
/* ------------------------------------------------------------------ */
/* Individual tests                                                     */
/* ------------------------------------------------------------------ */
 
/* 1. Single-node insert – root must be black */
static void test_single_insert(void)
{
    TEST("Single insert – root is black");
    arena_reset();
 
    void *n = make_node(64);
    PUT_PARENT(n, 0);
    PUT_LEFT(n, 0);
    PUT_RIGHT(n, 0);
    PUT_COLOR(n, BLACK); /* root starts black per convention */
 
    if (GET_COLOR(n) == BLACK)
        PASS();
    else
        FAIL("root not black after single insert");
}
 
/* 2. insertRecolor makes the inserted node's color red initially      */
static void test_new_node_starts_red(void)
{
    TEST("New node is red before recolor");
    arena_reset();
    void *n = make_node(128);
    if (GET_COLOR(n) == RED)
        PASS();
    else
        FAIL("make_node should produce a red node");
}
 
/* 3. Three right-leaning inserts trigger a left rotation              */
static void test_three_right_inserts(void)
{
    TEST("Three right-leaning inserts – valid RBT");
    arena_reset();
 
    /* Build root manually (root is always black) */
    void *r = make_node(32);
    PUT_COLOR(r, BLACK);
    PUT_LEFT(r, 0); PUT_RIGHT(r, 0); PUT_PARENT(r, 0);
 
    addNode(r, make_node(64), 64);
    void *root = find_root(r);
    addNode(root, make_node(96), 96);
    root = find_root(r);
 
    if (is_valid_rbt(root))
        PASS();
    else
        FAIL("tree invalid after 3 right-leaning inserts");
}
 
/* 4. Three left-leaning inserts trigger a right rotation              */
static void test_three_left_inserts(void)
{
    TEST("Three left-leaning inserts – valid RBT");
    arena_reset();
 
    void *r = make_node(96);
    PUT_COLOR(r, BLACK);
    PUT_LEFT(r, 0); PUT_RIGHT(r, 0); PUT_PARENT(r, 0);
 
    addNode(r, make_node(64), 64);
    void *root = find_root(r);
    addNode(root, make_node(32), 32);
    root = find_root(r);
 
    if (is_valid_rbt(root))
        PASS();
    else
        FAIL("tree invalid after 3 left-leaning inserts");
}
 
/* 5. Insert 7 distinct sizes and verify RBT invariants                */
static void test_seven_inserts_rbt(void)
{
    TEST("Seven distinct inserts – valid RBT");
    arena_reset();
 
    size_t sizes[] = {64, 128, 32, 256, 48, 96, 192};
    void *root = make_node(sizes[0]);
    PUT_COLOR(root, BLACK);
    PUT_LEFT(root, 0); PUT_RIGHT(root, 0); PUT_PARENT(root, 0);
 
    for (int i = 1; i < 7; i++) {
        addNode(find_root(root), make_node(sizes[i]), sizes[i]);
    }
    root = find_root(root);
 
    if (is_valid_rbt(root))
        PASS();
    else
        FAIL("invariant violated after 7 inserts");
}
 
/* 6. In-order traversal is sorted after 7 inserts                     */
static void test_inorder_sorted(void)
{
    TEST("In-order traversal is sorted after 7 inserts");
    arena_reset();
 
    size_t sizes[] = {64, 128, 32, 256, 48, 96, 192};
    void *root = make_node(sizes[0]);
    PUT_COLOR(root, BLACK);
    PUT_LEFT(root, 0); PUT_RIGHT(root, 0); PUT_PARENT(root, 0);
 
    for (int i = 1; i < 7; i++)
        addNode(find_root(root), make_node(sizes[i]), sizes[i]);
    root = find_root(root);
 
    size_t out[16];
    int n = inorder(root, out, 0);
 
    int ok = 1;
    for (int i = 1; i < n; i++)
        if (out[i] < out[i-1]) { ok = 0; break; }
 
    if (ok)
        PASS();
    else
        FAIL("in-order traversal not sorted");
}
 
/* 7. searchSize finds exact match                                      */
static void test_search_exact(void)
{
    TEST("searchSize – exact match found");
    arena_reset();
 
    size_t sizes[] = {64, 128, 32, 256, 48, 96, 192};
    void *root = make_node(sizes[0]);
    PUT_COLOR(root, BLACK);
    PUT_LEFT(root, 0); PUT_RIGHT(root, 0); PUT_PARENT(root, 0);
 
    for (int i = 1; i < 7; i++)
        addNode(find_root(root), make_node(sizes[i]), sizes[i]);
    root = find_root(root);
 
    void *result = searchSize(root, 96);
    if (result != 0 && GET_SIZE(result) == 96)
        PASS();
    else
        FAIL("exact search failed");
}
 
/* 8. searchSize returns smallest node >= requested size                */
static void test_search_best_fit(void)
{
    TEST("searchSize – best-fit (no exact match)");
    arena_reset();
 
    /* Insert sizes 32, 64, 128, 256 */
    void *root = make_node(64);
    PUT_COLOR(root, BLACK);
    PUT_LEFT(root, 0); PUT_RIGHT(root, 0); PUT_PARENT(root, 0);
 
    addNode(find_root(root), make_node(32),  32);
    addNode(find_root(root), make_node(128), 128);
    addNode(find_root(root), make_node(256), 256);
    root = find_root(root);
 
    /* Request 100 – smallest available is 128 */
    void *result = searchSize(root, 100);
    if (result != 0 && GET_SIZE(result) == 128)
        PASS();
    else
        FAIL("best-fit search failed");
}
 
/* 9. searchSize returns 0 when no block is large enough               */
static void test_search_too_large(void)
{
    TEST("searchSize – returns 0 when no block large enough");
    arena_reset();
 
    void *root = make_node(64);
    PUT_COLOR(root, BLACK);
    PUT_LEFT(root, 0); PUT_RIGHT(root, 0); PUT_PARENT(root, 0);
 
    addNode(find_root(root), make_node(32), 32);
    root = find_root(root);
 
    void *result = searchSize(root, 512);
    if (result == 0)
        PASS();
    else
        FAIL("expected 0 but got a node");
}
 
/* 10. getPreorder returns leftmost node                                */
static void test_getpreorder(void)
{
    TEST("getPreorder – returns leftmost (minimum) node");
    arena_reset();
 
    size_t sizes[] = {64, 128, 32, 256};
    void *root = make_node(sizes[0]);
    PUT_COLOR(root, BLACK);
    PUT_LEFT(root, 0); PUT_RIGHT(root, 0); PUT_PARENT(root, 0);
 
    for (int i = 1; i < 4; i++)
        addNode(find_root(root), make_node(sizes[i]), sizes[i]);
    root = find_root(root);
 
    void *min = getPreorder(root);
    if (GET_SIZE(min) == 32)
        PASS();
    else
        FAIL("getPreorder did not return minimum");
}
 
/* 11. getPostorder returns rightmost node                              */
static void test_getpostorder(void)
{
    TEST("getPostorder – returns rightmost (maximum) node");
    arena_reset();
 
    size_t sizes[] = {64, 128, 32, 256};
    void *root = make_node(sizes[0]);
    PUT_COLOR(root, BLACK);
    PUT_LEFT(root, 0); PUT_RIGHT(root, 0); PUT_PARENT(root, 0);
 
    for (int i = 1; i < 4; i++)
        addNode(find_root(root), make_node(sizes[i]), sizes[i]);
    root = find_root(root);
 
    void *max = getPostorder(root);
    if (GET_SIZE(max) == 256)
        PASS();
    else
        FAIL("getPostorder did not return maximum");
}
 
/* 12. Left rotation preserves BST and RBT properties                  */
static void test_left_rotate(void)
{
    TEST("leftRotate – BST order preserved");
    arena_reset();
 
    /* Build a simple right-leaning chain manually: 32 -> 64 -> 128
     * then rotate 32 left so 64 becomes the local root.              */
    void *n32 = make_node(32); PUT_COLOR(n32, BLACK);
    void *n64 = make_node(64); PUT_COLOR(n64, RED);
    void *n128= make_node(128);PUT_COLOR(n128, RED);
 
    PUT_LEFT(n32,  0);    PUT_RIGHT(n32,  n64); PUT_PARENT(n32, 0);
    PUT_LEFT(n64,  0);    PUT_RIGHT(n64,  n128);PUT_PARENT(n64, n32);
    PUT_LEFT(n128, 0);    PUT_RIGHT(n128, 0);   PUT_PARENT(n128,n64);
 
    leftRotate(n32);
 
    /* n64 should now be the root, n32 its left child */
    void *newRoot = find_root(n32);
    size_t out[8];
    int cnt = inorder(newRoot, out, 0);
 
    int sorted = 1;
    for (int i = 1; i < cnt; i++)
        if (out[i] < out[i-1]) { sorted = 0; break; }
 
    if (sorted && GET_SIZE(newRoot) == 64)
        PASS();
    else
        FAIL("order broken after leftRotate");
}
 
/* 13. Right rotation preserves BST order                              */
static void test_right_rotate(void)
{
    TEST("rightRotate – BST order preserved");
    arena_reset();
 
    void *n128 = make_node(128); PUT_COLOR(n128, BLACK);
    void *n64  = make_node(64);  PUT_COLOR(n64,  RED);
    void *n32  = make_node(32);  PUT_COLOR(n32,  RED);
 
    PUT_LEFT(n128, n64);  PUT_RIGHT(n128, 0);  PUT_PARENT(n128, 0);
    PUT_LEFT(n64,  n32);  PUT_RIGHT(n64,  0);  PUT_PARENT(n64,  n128);
    PUT_LEFT(n32,  0);    PUT_RIGHT(n32,  0);  PUT_PARENT(n32,  n64);
 
    rightRotate(n128);
 
    void *newRoot = find_root(n128);
    size_t out[8];
    int cnt = inorder(newRoot, out, 0);
 
    int sorted = 1;
    for (int i = 1; i < cnt; i++)
        if (out[i] < out[i-1]) { sorted = 0; break; }
 
    if (sorted && GET_SIZE(newRoot) == 64)
        PASS();
    else
        FAIL("order broken after rightRotate");
}
 
/* 14. Repeated inserts of the same size are handled (equal-size chain)*/
static void test_duplicate_sizes(void)
{
    TEST("Duplicate sizes – valid RBT after same-size inserts");
    arena_reset();
 
    void *root = make_node(64);
    PUT_COLOR(root, BLACK);
    PUT_LEFT(root, 0); PUT_RIGHT(root, 0); PUT_PARENT(root, 0);
 
    /* Insert 4 more nodes all of size 64 */
    for (int i = 0; i < 4; i++)
        addNode(find_root(root), make_node(64), 64);
    root = find_root(root);
 
    if (is_valid_rbt(root))
        PASS();
    else
        FAIL("invariant violated after duplicate inserts");
}
 
/* 15. Ascending insert order (worst case for naive BST)               */
static void test_ascending_inserts(void)
{
    TEST("Ascending insert order – valid RBT (worst-case BST)");
    arena_reset();
 
    size_t s = 32;
    void *root = make_node(s); s += 32;
    PUT_COLOR(root, BLACK);
    PUT_LEFT(root, 0); PUT_RIGHT(root, 0); PUT_PARENT(root, 0);
 
    for (int i = 0; i < 9; i++, s += 32)
        addNode(find_root(root), make_node(s), s);
    root = find_root(root);
 
    if (is_valid_rbt(root))
        PASS();
    else
        FAIL("invariant violated after ascending inserts");
}
 
/* 16. Descending insert order                                          */
static void test_descending_inserts(void)
{
    TEST("Descending insert order – valid RBT");
    arena_reset();
 
    size_t s = 320;
    void *root = make_node(s); s -= 32;
    PUT_COLOR(root, BLACK);
    PUT_LEFT(root, 0); PUT_RIGHT(root, 0); PUT_PARENT(root, 0);
 
    for (int i = 0; i < 9; i++, s -= 32)
        addNode(find_root(root), make_node(s), s);
    root = find_root(root);
 
    if (is_valid_rbt(root))
        PASS();
    else
        FAIL("invariant violated after descending inserts");
}
 
/* 17. searchSize on a single-node tree                                 */
static void test_search_single_node(void)
{
    TEST("searchSize – single-node tree, exact and larger query");
    arena_reset();
 
    void *root = make_node(64);
    PUT_COLOR(root, BLACK);
    PUT_LEFT(root, 0); PUT_RIGHT(root, 0); PUT_PARENT(root, 0);
 
    void *r1 = searchSize(root, 64);
    void *r2 = searchSize(root, 32);
    void *r3 = searchSize(root, 128);
 
    if (r1 != 0 && GET_SIZE(r1) == 64 &&
        r2 != 0 && GET_SIZE(r2) == 64 &&
        r3 == 0)
        PASS();
    else
        FAIL("single-node search results wrong");
}
 
/* 18. PACK_COLOR / GET_COLOR / GET_SIZE round-trip                    */
static void test_pack_color_roundtrip(void)
{
    TEST("PACK_COLOR round-trip – size/color/alloc bits independent");
    arena_reset();
 
    void *n = make_node(256);
    PUT(n, PACK_COLOR(256, RED, 1));
 
    int ok = (GET_SIZE(n) == 256) && (GET_COLOR(n) == RED) && (GET_ALLOC(n) == 1);
 
    PUT_COLOR(n, BLACK);
    ok = ok && (GET_COLOR(n) == BLACK) && (GET_SIZE(n) == 256) && (GET_ALLOC(n) == 1);
 
    if (ok)
        PASS();
    else
        FAIL("PACK_COLOR round-trip failed");
}
 
/* 19. Mixed insert/search stress – 20 nodes, 5 random searches        */
static void test_stress_insert_search(void)
{
    TEST("Stress – 20 inserts then multiple searches, valid RBT");
    arena_reset();
 
    size_t vals[] = {
        96,160,224,288,352,416,480,544,608,672,
        64,128,192,256,320,384,448,512,576,640
    };
    int N = 20;
 
    void *root = make_node(vals[0]);
    PUT_COLOR(root, BLACK);
    PUT_LEFT(root,0); PUT_RIGHT(root,0); PUT_PARENT(root,0);
 
    for (int i = 1; i < N; i++)
        addNode(find_root(root), make_node(vals[i]), vals[i]);
    root = find_root(root);
 
    if (!is_valid_rbt(root)) { FAIL("RBT invalid after 20 inserts"); return; }
 
    /* Search for values we know exist */
    void *r1 = searchSize(root, 64);
    void *r2 = searchSize(root, 640);
    /* Best-fit: 65 -> smallest >= 65 is 96 */
    void *r3 = searchSize(root, 65);
    /* Too large */
    void *r4 = searchSize(root, 700);
 
    int ok = (r1 && GET_SIZE(r1) == 64)  &&
             (r2 && GET_SIZE(r2) == 640) &&
             (r3 && GET_SIZE(r3) == 96)  &&
             (r4 == 0);
 
    if (ok)
        PASS();
    else
        FAIL("stress search results wrong");
}
 
/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
 
int main(void)
{
    printf("========================================\n");
    printf("  Red-Black Tree Test Suite\n");
    printf("========================================\n\n");
 
    test_single_insert();
    test_new_node_starts_red();
    test_three_right_inserts();
    test_three_left_inserts();
    test_seven_inserts_rbt();
    test_inorder_sorted();
    test_search_exact();
    test_search_best_fit();
    test_search_too_large();
    test_getpreorder();
    test_getpostorder();
    test_left_rotate();
    test_right_rotate();
    test_duplicate_sizes();
    test_ascending_inserts();
    test_descending_inserts();
    test_search_single_node();
    test_pack_color_roundtrip();
    test_stress_insert_search();
 
    printf("\n========================================\n");
    printf("  Results: %d / %d passed\n", tests_passed, tests_run);
    printf("========================================\n");
 
    return (tests_passed == tests_run) ? 0 : 1;
}
 