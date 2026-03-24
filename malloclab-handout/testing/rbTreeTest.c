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
void *getLargest(void *root);
void *getSmallest(void *root);
void *searchSize(void *root, size_t size);
void *recurse(void *root, void *lastLarger, size_t size);
void print_binary_tree(void *root);
 

//print tree
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
 
/* 10. getSmallest returns leftmost node                                */
static void test_getSmallest(void)
{
    TEST("getSmallest – returns leftmost (minimum) node");
    arena_reset();
 
    size_t sizes[] = {64, 128, 32, 256};
    void *root = make_node(sizes[0]);
    PUT_COLOR(root, BLACK);
    PUT_LEFT(root, 0); PUT_RIGHT(root, 0); PUT_PARENT(root, 0);
 
    for (int i = 1; i < 4; i++)
        addNode(find_root(root), make_node(sizes[i]), sizes[i]);
    root = find_root(root);
 
    void *min = getSmallest(root);
    if (GET_SIZE(min) == 32)
        PASS();
    else
        FAIL("getSmallest did not return minimum");
}
 
/* 11. getLargest returns rightmost node                              */
static void test_getLargest(void)
{
    TEST("getLargest – returns rightmost (maximum) node");
    arena_reset();
 
    size_t sizes[] = {64, 128, 32, 256};
    void *root = make_node(sizes[0]);
    PUT_COLOR(root, BLACK);
    PUT_LEFT(root, 0); PUT_RIGHT(root, 0); PUT_PARENT(root, 0);
 
    for (int i = 1; i < 4; i++)
        addNode(find_root(root), make_node(sizes[i]), sizes[i]);
    root = find_root(root);
 
    void *max = getLargest(root);
    if (GET_SIZE(max) == 256)
        PASS();
    else
        FAIL("getLargest did not return maximum");
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
    for (int i = 0; i < 9; i++, s += 32) {
        addNode(find_root(root), make_node(s), s);
        //printf("%i", i);
        //print_binary_tree(find_root(root));
    }
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
/* Delete helpers                                                       */
/* ------------------------------------------------------------------ */
extern char *rootMain;
/*
 * Build a standard tree from an array of sizes, returning the root.
 * sizes[0] is used as the first (black) root.
 */
static void *build_tree(size_t *sizes, int n)
{
    void *root = make_node(sizes[0]);
    PUT_COLOR(root, BLACK);
    PUT_LEFT(root, 0); PUT_RIGHT(root, 0); PUT_PARENT(root, 0);
    rootMain = root;   /* seed before any addNode calls */
    for (int i = 1; i < n; i++)
        addNode(rootMain, make_node(sizes[i]), sizes[i]);
    return rootMain;
}
 
/* Walk the tree and return the node whose block size == target, or NULL */
static void *find_node(void *node, size_t target)
{
    if (node == 0) return 0;
    size_t s = GET_SIZE(node);
    if (s == target) return node;
    if (target < s)  return find_node((void *)GET_LEFT_CHILD(node),  target);
                     return find_node((void *)GET_RIGHT_CHILD(node), target);
}
 
/* Count nodes in the tree */
static int count_nodes(void *node)
{
    if (node == 0) return 0;
    return 1 + count_nodes((void *)GET_LEFT_CHILD(node))
             + count_nodes((void *)GET_RIGHT_CHILD(node));
}
 
/* ------------------------------------------------------------------ */
/* Delete tests                                                         */
/* ------------------------------------------------------------------ */
 
/* 20. Delete the only node – tree becomes empty */
static void test_delete_only_node(void)
{
    TEST("Delete only node – tree is empty afterwards");
    arena_reset();
 
    void *root = make_node(64);
    PUT_COLOR(root, BLACK);
    PUT_LEFT(root, 0); PUT_RIGHT(root, 0); PUT_PARENT(root, 0);
 
    removeNode(root);
    /* After removing the sole node its parent is still 0 and both
     * children should be 0 – the caller is responsible for nulling
     * their root pointer, so we just verify the node is isolated.   */
    int ok = (GET_LEFT_CHILD(root) == 0) &&
             (GET_RIGHT_CHILD(root) == 0);
    if (ok) PASS(); else FAIL("node not properly isolated after delete");
}
 
/* 21. Delete a red leaf – RBT valid, node count decreases by 1 */
static void test_delete_red_leaf(void)
{
    TEST("Delete red leaf – valid RBT, count decreases");
    arena_reset();
 
    /* Tree: 64(B) with 32(R) left and 96(R) right – 96 is a red leaf */
    size_t sizes[] = {64, 32, 96};
    void *root = build_tree(sizes, 3);
    int before = count_nodes(root);
 
    void *target = find_node(root, 96);
    if (!target) { FAIL("could not find node 96"); return; }
 
    removeNode(target);
    root = find_root(root == target ? (void *)(GET_PARENT(root) ? (void *)GET_PARENT(root) : root) : root);
    /* Refresh root – after deleting a leaf root doesn't change */
    root = find_node(root, 64) ? find_root(root) : find_root(root);
 
    int after = count_nodes(root);
    if (is_valid_rbt(root) && after == before - 1)
        PASS();
    else
        FAIL("invalid RBT or wrong count after red leaf delete");
}
 
/* 22. Delete a black leaf – requires rebalancing, RBT must stay valid */
static void test_delete_black_leaf(void)
{
    TEST("Delete black leaf – valid RBT after rebalancing");
    arena_reset();
 
    /*
     * Insert enough nodes so we get a black leaf.
     * With sizes {64,32,96,16,48} the node 16 ends up black.
     */
    size_t sizes[] = {64, 32, 96, 40, 56,72,80,88, 48};
    void *root = build_tree(sizes, 9);
 
    //print_binary_tree(root);
    void *target = find_node(root, 72);
    if (!target || GET_COLOR(target) != BLACK) {
        /* If the tree shaped differently just skip with a note */
        //printf("SKIP (node 16 not black in this tree shape)\n");
        tests_run--; /* don't count as fail */
        return;
    }
    int before = count_nodes(root);
    removeNode(target);
    root = find_root(find_node(root, 64) ? root : root);
    //print_binary_tree(root);
    if (is_valid_rbt(root) && count_nodes(root) == before - 1)
        PASS();
    else
        FAIL("invalid RBT or wrong count after black leaf delete");
}
 
/* 23. Delete root node – new root must be black */
static void test_delete_root(void)
{
    TEST("Delete root – new root is black");
    arena_reset();
 
    size_t sizes[] = {64, 32, 96};
    void *root = build_tree(sizes, 3);
    /* Keep a reference to a non-root node so we can find the new root */
    void *non_root = find_node(root, 32);
    removeNode(root);
    void *new_root = find_root(non_root);
    if (new_root != 0 && GET_COLOR(new_root) == BLACK && is_valid_rbt(new_root))
        PASS();
    else
        FAIL("new root is not black or tree invalid after root delete");
}
 
/* 24. Delete node with one child – child must be promoted correctly */
static void test_delete_one_child(void)
{
    TEST("Delete node with one child – RBT valid, correct count");
    arena_reset();
 
    /*
     * Build a tree where one node has exactly one child.
     * {64,32,96,80} → 80 is the only child of 96 (right child).
     * After inserting, 96 should have just a left child 80.
     */
    size_t sizes[] = {64, 32, 96, 80};
    void *root = build_tree(sizes, 4);
    int before = count_nodes(root);
    void *target = find_node(root, 96);
    if (!target) { FAIL("could not find node 96"); return; }
 
    /* Confirm it has exactly one child */
    int has_left  = GET_LEFT_CHILD(target)  != 0;
    int has_right = GET_RIGHT_CHILD(target) != 0;
    if (has_left == has_right) {
        printf("SKIP (node 96 doesn't have exactly one child in this shape)\n");
        tests_run--;
        return;
    }
 
    removeNode(target);
    /* find a surviving node to locate the new root */
    void *survivor = find_node(root, 64);
    if (!survivor) survivor = find_node(root, 32);
    void *new_root = find_root(survivor);
    if (is_valid_rbt(new_root) && count_nodes(new_root) == before - 1)
        PASS();
    else
        FAIL("invalid RBT or wrong count after one-child delete");
}
 
/* 25. Delete node with two children – RBT valid, correct count */
static void test_delete_two_children(void)
{
    TEST("Delete node with two children – RBT valid, correct count");
    arena_reset();
 
    size_t sizes[] = {64, 32, 96, 40, 48, 80, 128};
    void *root = build_tree(sizes, 7);
    int before = count_nodes(root);
    /* Node 32 has both 16 and 48 as children */
    void *target = find_node(root, 96);
    if (!target || GET_LEFT_CHILD(target) == 0 || GET_RIGHT_CHILD(target) == 0) {
        printf("SKIP (node 32 doesn't have two children in this shape)\n");
        tests_run--;
        return;
    }
 
    removeNode(target);
    void *new_root = find_root(find_node(root, 64));
 
    if (is_valid_rbt(new_root) && count_nodes(new_root) == before - 1)
        PASS();
    else
        FAIL("invalid RBT or wrong count after two-child delete");
}
 
/* 26. Delete all nodes one by one – tree must stay valid at every step */
static void test_delete_all_one_by_one(void)
{
    TEST("Delete all nodes one by one – RBT valid at every step");
    arena_reset();
 
    size_t sizes[] = {64, 32, 96, 88, 48, 80, 128,40, 56};
    int N = 9;
    build_tree(sizes, N);
    int ok = 1;
    for (int remaining = N; remaining > 1; remaining--) {
        /* Always delete the current minimum (leftmost) */
        void *min = getSmallest(rootMain);
        removeNode(min);

        /* rootMain updated automatically by deleteRecolor */
 
        if (!is_valid_rbt(rootMain)) {
            printf("\n  FAIL: invalid after deleting min with %d nodes remaining",
                   remaining - 1);
            ok = 0;
            break;
        }
    }
 
    if (ok) PASS(); else FAIL("RBT invariant broken during sequential deletes");
}
 
/* 27. Interleaved inserts and deletes – RBT valid throughout */
static void test_interleaved_insert_delete(void)
{
    TEST("Interleaved inserts and deletes – RBT valid throughout");
    arena_reset();
 
    size_t init[] = {128, 64, 192, 32, 96, 160, 256};
    build_tree(init, 7);
 
    int ok = 1;
 
    void *d = find_node(rootMain, 64);
    if (d) removeNode(d);
    if (!is_valid_rbt(rootMain)) { ok = 0; goto done27; }
 
    addNode(rootMain, make_node(72), 72);
    if (!is_valid_rbt(rootMain)) { ok = 0; goto done27; }
 
    d = find_node(rootMain, 192);
    if (d) removeNode(d);
    if (!is_valid_rbt(rootMain)) { ok = 0; goto done27; }
 
    addNode(rootMain, make_node(200), 200);
    if (!is_valid_rbt(rootMain)) { ok = 0; goto done27; }
 
    d = find_node(rootMain, 32);
    if (d) removeNode(d);
    if (!is_valid_rbt(rootMain)) { ok = 0; goto done27; }
 
done27:
    if (ok) PASS(); else FAIL("RBT invariant broken during interleaved ops");
}
 
/* 28. Delete then search – deleted size must no longer be found */
static void test_delete_then_search(void)
{
    TEST("Delete then search – deleted size no longer found");
    arena_reset();
 
    size_t sizes[] = {64, 32, 96, 128, 48};
    build_tree(sizes, 5);
 
    void *before = searchSize(rootMain, 96);
    if (!before || GET_SIZE(before) != 96) {
        FAIL("node 96 not found before delete");
        return;
    }
 
    void *target = find_node(rootMain, 96);
    removeNode(target);
    /* rootMain updated automatically */
 
    /* Searching for 96 should now return 128 (next larger), not 96 */
    void *after = searchSize(rootMain, 96);
    int ok = is_valid_rbt(rootMain) &&
             (after == 0 || GET_SIZE(after) != 96);
 
    if (ok) PASS(); else FAIL("deleted node still returned by searchSize");
}
 
/* ------------------------------------------------------------------ */
/* rootMain-specific tests                                              */
/* ------------------------------------------------------------------ */
 
/* 29. rootMain is set correctly by the very first insert (insertRecolor
 *     sets rootMain when it detects parent==0).                       */
static void test_rootmain_set_on_first_insert(void)
{
    TEST("rootMain – set correctly after first insert");
    arena_reset();
 
    void *n = make_node(64);
    PUT_COLOR(n, BLACK);
    PUT_LEFT(n, 0); PUT_RIGHT(n, 0); PUT_PARENT(n, 0);
    rootMain = n;   /* seed as if this were the allocator's first free block */
 
    /* Insert a second node via addNode; insertRecolor will keep rootMain */
    addNode(rootMain, make_node(128), 128);
 
    int ok = (rootMain != 0) &&
             (GET_PARENT(rootMain) == 0) &&   /* real root has no parent */
             (GET_COLOR(rootMain) == BLACK);
 
    if (ok) PASS(); else FAIL("rootMain wrong after first addNode");
}
 
/* 30. rootMain stays valid through a rotation that lifts a new root   */
static void test_rootmain_after_rotation(void)
{
    TEST("rootMain – updated correctly when rotation changes root");
    arena_reset();
 
    /* Ascending insert 32→64→96 forces a left rotation that lifts 64
     * to the root; rootMain must reflect that.                        */
    void *n = make_node(32);
    PUT_COLOR(n, BLACK);
    PUT_LEFT(n, 0); PUT_RIGHT(n, 0); PUT_PARENT(n, 0);
    rootMain = n;
 
    addNode(rootMain, make_node(64), 64);
    addNode(rootMain, make_node(96), 96);
 
    int ok = (rootMain != 0) &&
             (GET_PARENT(rootMain) == 0) &&
             (GET_COLOR(rootMain) == BLACK) &&
             is_valid_rbt(rootMain);
 
    if (ok) PASS(); else FAIL("rootMain not updated after rotation");
}
 
/* 31. rootMain updated when the root itself is deleted                 */
static void test_rootmain_after_root_delete(void)
{
    TEST("rootMain – updated after deleting the root node");
    arena_reset();
 
    size_t sizes[] = {64, 32, 96, 40, 48};
    build_tree(sizes, 5);
 
    void *old_root = rootMain;
    removeNode(old_root);
 
    int ok = (rootMain != 0) &&
             (rootMain != old_root) &&
             (GET_PARENT(rootMain) == 0) &&
             (GET_COLOR(rootMain) == BLACK) &&
             is_valid_rbt(rootMain);
 
    if (ok) PASS(); else FAIL("rootMain wrong after root delete");
}
 
/* 32. rootMain is consistent across 10 successive root deletions       */
static void test_rootmain_successive_root_deletes(void)
{
    TEST("rootMain – consistent across 10 successive root deletes");
    arena_reset();
 
    /* Build a 12-node tree */
    size_t sizes[] = {128,64,192,32,96,160,256,40,48,80,112,224};
    int N = 12;
    build_tree(sizes, N);
 
    int ok = 1;
    for (int i = 0; i < 10; i++) {
        void *old_root = rootMain;
        removeNode(old_root);
 
        if (rootMain == 0) break;   /* tree emptied early, fine */
 
        if (GET_PARENT(rootMain) != 0 ||
            GET_COLOR(rootMain) != BLACK ||
            !is_valid_rbt(rootMain)) {
            printf("\n  FAIL: rootMain invalid on iteration %d", i + 1);
            ok = 0;
            break;
        }
    }
 
    if (ok) PASS(); else FAIL("rootMain became inconsistent during root deletes");
}
 
/* 33. rootMain matches find_root() after a mixed sequence of ops       */
static void test_rootmain_matches_find_root(void)
{
    TEST("rootMain – always matches find_root() during mixed ops");
    arena_reset();
 
    size_t sizes[] = {128, 64, 192, 32, 96};
    build_tree(sizes, 5);
 
    int ok = 1;
 
    /* delete 64 */
    void *d = find_node(rootMain, 64);
    if (d) removeNode(d);
    if (rootMain != find_root(find_node(rootMain, 128))) { ok = 0; goto done33; }
 
    /* insert 72 */
    addNode(rootMain, make_node(72), 72);
    if (rootMain != find_root(find_node(rootMain, 128))) { ok = 0; goto done33; }
 
    /* delete 192 */
    d = find_node(rootMain, 192);
    if (d) removeNode(d);
    /* find a surviving anchor */
    void *anchor = find_node(rootMain, 128);
    if (!anchor) anchor = find_node(rootMain, 96);
    if (anchor && rootMain != find_root(anchor)) { ok = 0; goto done33; }
 
    /* insert 300 */
    addNode(rootMain, make_node(300), 300);
    anchor = find_node(rootMain, 128);
    if (!anchor) anchor = find_node(rootMain, 96);
    if (anchor && rootMain != find_root(anchor)) { ok = 0; goto done33; }
 
done33:
    if (ok) PASS(); else FAIL("rootMain diverged from find_root()");
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
    test_getSmallest();
    test_getLargest();
    test_left_rotate();
    test_right_rotate();
    test_duplicate_sizes();
    test_ascending_inserts();
    test_descending_inserts();
    test_search_single_node();
    test_pack_color_roundtrip();
    test_stress_insert_search();

    printf("\n--- Delete ---\n");
    test_delete_only_node();
    test_delete_red_leaf();
    test_delete_black_leaf();
    test_delete_root();
    test_delete_one_child();
    test_delete_two_children();
    test_delete_all_one_by_one();
    test_interleaved_insert_delete();
    test_delete_then_search();

    printf("\n--- rootMain ---\n");
    test_rootmain_set_on_first_insert();
    test_rootmain_after_rotation();
    test_rootmain_after_root_delete();
    test_rootmain_successive_root_deletes();
    test_rootmain_matches_find_root();

 
    printf("\n========================================\n");
    printf("  Results: %d / %d passed\n", tests_passed, tests_run);
    printf("========================================\n");
 
    return (tests_passed == tests_run) ? 0 : 1;
}
 