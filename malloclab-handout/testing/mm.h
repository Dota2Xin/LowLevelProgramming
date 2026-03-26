#ifndef MM_H
#define MM_H
 
#include <stddef.h>
 
/* Team struct required by the original framework */
typedef struct {
    char *teamname;
    char *name1;
    char *email1;
    char *name2;
    char *email2;
} team_t;
 
extern team_t team;
int   mm_init(void);
void *mm_malloc(size_t size);
void  mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);
 
/* Internal helpers exposed for testing */
void  *addBlockArray(size_t size);
void  *addBlockTree(size_t size);
void  *breakTree(size_t size);
void   makeFree(void *ptr, size_t size);
void   addFree(void *ptr, size_t size);
void  *coalesce(void *ptr);
void  *extendHeap(size_t requested);
 
/* Red-black tree */
extern char *rootMain;
void  *addNode(void *root, void *newNode, size_t size);
void   baseAdd(void *root, void *newNode, size_t size);
void   insertRecolor(void *newNode);
void   removeNode(void *node);
void   baseRemove(void *node);
void   deleteRecolor(void *node);
void   handleColoringDelete(void *colorNode, char nullCheck);
void   leftRotate(void *root);
void   rightRotate(void *root);
void   swap(void *node1, void *node2);
void   swapChild(void *parent, void *child);
void  *getLargest(void *root);
void  *getSmallest(void *root);
void  *searchSize(void *root, size_t size);
void  *recurse(void *root, void *lastLarger, size_t size);
 
#endif