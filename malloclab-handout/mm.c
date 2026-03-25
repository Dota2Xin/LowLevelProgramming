/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include "mm.h"
#include "memlib.h"

//Run simple trace:  ./mdriver -V -f 
/*
Design Principles:
Basic design will be size-segregated explicit free lists with no footers on allocated blocks.
For the size segregation each size will get its own class up to maybe 512 bytes and then we do break ups from there. 
We'll figure out the larger size break ups.
If this works well then we're good, if we want to do more than the thing to do would be for the fixed sizes <=512 bytes
we can implement big bag of pages to deal with those although we'll see how much we need that.   

Explicit free list for <=512 bytes, we'll do an array with 32 up to 512 bytes in steps of 8 (chunk size) (so length 63 array) 
for larger byte sizes everything will be organized in a red black tree. We'll start by doing everything
in an explicit free list and go from there. 
*/

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define MINSIZE 32 //Ensures free blocks have at least 4 chunks of space to work with 1 header 1 next pointer 1 prev pointer 1 footer. 
#define WSIZE 8
#define DSIZE 8
#define SEGSIZE 488 //size of segregated list at start of file
#define SEGBASE 61
#define SIZECROSS 512
#define CHUNKSIZE (1<<12) //extend heap by this fixed amount
#define MAX(x,y) ((x)>(y)? (x): (y))
#define PACK(size, alloc) ((size)|(alloc)) // #make a header/footer 

#define GET(p) (*(unsigned long int *)(p))
#define PUT(p,val) (*(unsigned long int *)(p)=val)

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define GET_NEXT(p) (GET((char*)(p)+DSIZE))
#define GET_PREV(p) (GET((char*)(p)+2*DSIZE))

#define PUT_NEXT(p, val) (PUT((char*)(p)+DSIZE, val))
#define PUT_PREV(p, val) (PUT((char*)(p)+2*DSIZE, val))

#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp)+GET_SIZE(HDRP(bp))-WSIZE)
#define HDFTRP(p) ((char* )(p)+GET_SIZE(p)-DSIZE);

//Binary tree methods
#define GET_LEFT_CHILD(p) (GET(((char*) p)+DSIZE))
#define GET_RIGHT_CHILD(p) (GET(((char*) p)+2*DSIZE))
#define GET_PARENT(p) (GET(((char*) p)+3*DSIZE))

#define PUT_LEFT(root, child) (PUT(((char*)root)+DSIZE, child))
#define PUT_RIGHT(root, child) (PUT(((char*)root)+2*DSIZE, child))
#define PUT_PARENT(root, parent) (PUT(((char*)root)+3*DSIZE, parent))
#define PUT_COLOR(p, color) (PUT(p, PACK_COLOR(GET_SIZE(p), color, GET_ALLOC(p))))

#define GET_COLOR(p) ((GET(p) & 0x2)>>1)
#define PACK_COLOR(size, color, alloc) ((size)|(2*color)|(alloc))

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//Points to first free block in the heap. 

static char* firstFree;
static char* segregatedList;
static char* rootPointer;
static size_t extendSize=CHUNKSIZE;

void*  getLargest(void* root);
void*  getSmallest(void* root);
void*  recurse(void* root, void* lastLarger, size_t size);

char* rootMain;
/*
DESIGN IDEAS:
1. Might need a tree bit stored somewhere to check if we even have a tree when using our functions
2. If we are too fragmented then getting rid of footers on allocated blocks is almost certainly the first thing to fix,
   we have space in the 4 bit (last bit we have space actually lol) of the next block irregardless if that block is free
   or allocated as allocation is the 1 bit and color is the 2 bit (only for free). 
*/
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    //initialize the heap and the prologue+epilogue blocks
    size_t heapSize=0;
    char* heapStart=mem_heap_lo();
    char* dump=mem_sbrk(CHUNKSIZE);
    heapSize=CHUNKSIZE;
    segregatedList=heapStart;
    PUT(heapStart+SEGSIZE,PACK(DSIZE, 1));
    PUT(heapStart+SEGSIZE+DSIZE, PACK(DSIZE, 1));    
    char* heapEnd=mem_heap_hi();
    PUT(heapEnd-DSIZE, PACK(DSIZE, 1));
    
    heapSize-=SEGSIZE+3*DSIZE;
    //initialize our root node for the red black tree
    rootMain=SEGSIZE+2*DSIZE;
    PUT(rootMain, PACK_COLOR(heapSize, 0, 1));
    PUT_LEFT(rootMain, 0);
    PUT_RIGHT(rootMain, 0);
    PUT_PARENT(rootMain, 0);
    PUT(rootMain+GET_SIZE(rootMain), PACK_COLOR(heapSize, 0, 1));
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newSize;
    if (size==0) {
        return NULL;
    }
    else if ((size+SIZE_T_SIZE)<=MINSIZE) {
        newSize=MINSIZE;
    }
    else {
        newSize = ALIGN(size + SIZE_T_SIZE);
    }
    
    if (newSize <= SIZECROSS) {
        return addBlockArray(newSize);
    }

    else {
        return addBlockTree(newSize);
    }
}

/*
We start by checking all of our blocks to see if they contain anything (listPointer!=0) and then if they don't
we split up the blocks that we have already in the tree, if we have something but we're smaller then we split it up
understanding that we can't have less than 32 bytes on their own.
Once that's done you just put stuff in easily.
*/
//Need to figure out how to do footer stuff with our data structures.
void* addBlockArray(size_t size) {
    int index=size/8-4;

    char* listPointer=GET(segregatedList+index);

    //THIS LOOP IS OFF BY ONE MAYBE LOLZ
    while (listPointer==0 && index+1<SEGBASE) {
        index+=1;
        listPointer=GET(segregatedList+index);
    }

    if (index+1==SEGBASE) {
        //we don't have any location to place our stuff so we have to go to our tree and break it up
        listPointer=breakTree(size);
    }

    //list pointer gives us the location in memory that our free block is at.
    //now we check the size of the free block, if it's less than size+32 we use the entire block up
    //on the otherhand if we have more than that then we break up everything after size into its own free block.
    size_t checkSize=GET_SIZE(listPointer);

    //early return in here
    if(checkSize<size+32) {
        PUT(listPointer, PACK(checkSize, 1));
        PUT(listPointer+GET_SIZE(listPointer)-DSIZE, PACK(checkSize, 1));
        //have to add correct allocation bits at foot somehow...
        return listPointer+DSIZE;
    }

    makeFree(listPointer+size, checkSize-size);
    PUT(listPointer, PACK(size, 1));
    PUT(listPointer+GET_SIZE(listPointer)-DSIZE, PACK(size, 1));
    return listPointer+DSIZE;
}

//makes a free block of a given size at ptr and then passes it to be added to the global data structures
void makeFree(void* ptr, size_t size) {
    PUT(ptr, PACK(size, 0));
    PUT(((char*) ptr)+size-DSIZE, PACK(size, 0));
    addFree(ptr, size);
}

//adds a given block to our data structures of tree/array based on its size
void addFree(void* ptr, size_t size) {
    //doubly linked array case
    if (size<=512) {
        int index=size/8-4;
        if (GET(segregatedList+index)==0) {
            //there's no free block so we add it
            PUT(segregatedList+index, ptr);
            PUT_NEXT(ptr, 0);
            PUT_PREV(ptr, 0);
        } else {
            char* listBlock=GET(segregatedList+index);
            PUT(segregatedList+index, ptr);
            PUT_NEXT(ptr, listBlock);
            PUT_PREV(listBlock, ptr);
        }
        return;
    }
    //red black tree case
    addNode(rootMain, ptr, size);
    return;
}

void* breakTree(size_t size) {
    char* nodePointer=searchSize(rootMain, size);
    if (nodePointer==0) {
        //need to extend the heap
        //NEED TO FIX EXTEND HEAP TO RETURN PROPER POINTERS AND WHATNOTSKIS
        nodePointer=extendHeap(size);
        return nodePointer;
    }

    size_t totalSize=GET_SIZE(nodePointer);
    if(totalSize-size<32) {
        removeNode(nodePointer);
        return nodePointer;
    }
    //add a free chunk based on nodepointer excess
    return NULL;
}

void* addBlockTree(size_t size) {
    return NULL;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}



//HEAP GROWTH WILL HAVE TO REIMPLEMENT TO ADD BLOCKS TO THE TREE ONCE THAT IS SETUP
void* extendHeap(size_t requested) {
    size_t growSize=MAX(extendSize, requested);
    char* boundary=mem_sbrk(growSize);
    PUT(boundary-DSIZE, PACK(DSIZE,0)); //remove old epilogue block
    char checkPrev=GET_ALLOC(boundary-2*DSIZE);
    if (checkPrev) {
        //get the previous block which we now know is free from checkprev
        char* prevBlock=boundary-3*DSIZE-GET_SIZE(boundary-2*DSIZE);

        //make new free block+epilogue block
        size_t newSize=GET_SIZE(prevBlock)+growSize-DSIZE;
        PUT(prevBlock, PACK(newSize, 0));
        PUT(prevBlock+newSize, PACK(newSize, 0));
        PUT(boundary+growSize-DSIZE, PACK(DSIZE, 0));

        extendSize=ALIGN((size_t) round(1.3*growSize));
        
        return prevBlock;
    }
    //Previous block is allocated then we just make free block (using epilogue block from old heap up as well)
    char* newBlock=boundary-DSIZE;
    PUT(newBlock, PACK(growSize-DSIZE, 0));
    PUT(newBlock+growSize-DSIZE, PACK(growSize-DSIZE, 0));
    PUT(newBlock+growSize, PACK(DSIZE, 0));

    extendSize=ALIGN((size_t) round(1.3*growSize));

    return newBlock;
}


/////////RED BLACK TREE METHODS////////////
/*
Possible Optimizations (dig into this bag if tree appears to be slowing program down):
1. Store child state bits in the extra space of our nodes so that we don't have to do if statements or else statements to
   determine which child a node is and whatever other stuff about its uncle, we could store this in the bits after the parent.
2. ...
*/
//we also have to store parents I think. 
//we also have to store parents I think. 
void leftRotate(void* root) {
    char* temp=GET_RIGHT_CHILD(root);
    char* temp2;
    if(GET_LEFT_CHILD(temp)!= 0) {
        temp2=GET_LEFT_CHILD(temp);
        PUT_PARENT(temp2, root);
    } else {
        temp2=0;
    }
    char* parent=GET_PARENT(root);
    PUT_RIGHT(root, temp2);
    PUT_LEFT(temp, root);
    PUT_PARENT(root, temp);
    
    if (parent== 0) {
        PUT_PARENT(temp, 0);
        rootMain=temp;
        return;
    }

    PUT_PARENT(temp, parent);
    if (GET_SIZE(root)<=GET_SIZE(parent)) {
        PUT_LEFT(parent, temp);
    } else {
        PUT_RIGHT(parent, temp);
    }
}

//temp should never be null
void rightRotate(void* root) {
    char* temp=GET_LEFT_CHILD(root);
    char* temp2;
    if(GET_RIGHT_CHILD(temp)!= 0) {
        temp2=GET_RIGHT_CHILD(temp);
        PUT_PARENT(temp2, root);
    } else {
        temp2=0;
    }
    char* parent=GET_PARENT(root);
    PUT_LEFT(root, temp2);
    PUT_RIGHT(temp, root);
    PUT_PARENT(root, temp);
    
    if (parent== 0) {
        rootMain=temp;
        PUT_PARENT(temp, 0);
        return;
    }

    PUT_PARENT(temp, parent);
    if (GET_SIZE(root)<=GET_SIZE(parent)) {
        PUT_LEFT(parent, temp);
    } else {
        PUT_RIGHT(parent, temp);
    }
}

//only assumption is node1 is above node2
//update parents
void swap(void* node1, void* node2) {
    char* left1=GET_LEFT_CHILD(node1);
    char* right1=GET_RIGHT_CHILD(node1);
    char* parent1=GET_PARENT(node1);
    char* parent2=GET_PARENT(node2);
    char* left2=GET_LEFT_CHILD(node2);
    char* right2=GET_RIGHT_CHILD(node2);
    PUT_LEFT(node1, left2);
    PUT_RIGHT(node1, right2);
    PUT_LEFT(node2, left1);
    PUT_RIGHT(node2, right1);
    PUT_PARENT(node1, parent2);
    PUT_PARENT(node2, parent1);

    if(left2!=0) {
        PUT_PARENT(left2, node1);
    }
    if(right2!=0) {
        PUT_PARENT(right2, node1);
    }
    if(left1!=0) {
        PUT_PARENT(left1, node2);
    }
    if(right1!=0) {
        PUT_PARENT(right1, node2);
    }
    
    char tempColor=GET_COLOR(node1);
    PUT_COLOR(node1, GET_COLOR(node2));
    PUT_COLOR(node2, tempColor);

    if(GET_SIZE(node2)<=GET_SIZE(parent2)) {
            PUT_LEFT(parent2, node1);
        } else {
            PUT_RIGHT(parent2, node1);
        }
    if (parent1!=0) {
        if(GET_SIZE(node1)<=GET_SIZE(parent1)) {
            PUT_LEFT(parent1, node2);
        } else {
            PUT_RIGHT(parent1, node2);
        }
    } else {
        rootMain=node2;
    }
    return;
}

//for the special case swap breaks when you swap directly with your child 
void swapChild(void* parent, void* child) {
    if (GET_SIZE(child)<=GET_SIZE(parent)) {
        char* right1=GET_RIGHT_CHILD(parent);
        char* grandparent=GET_PARENT(parent);
        char* right2=GET_RIGHT_CHILD(child);
        char* left2=GET_LEFT_CHILD(child);
        char pColor=GET_COLOR(parent);

        PUT_LEFT(child, parent);
        PUT_RIGHT(child, right1);
        if(right1!=0) {
            PUT_PARENT(right1, child);
        }
        PUT_LEFT(parent, left2);
        if(left2!=0) {
            PUT_PARENT(left2, parent);
        }
        PUT_RIGHT(parent, right2);
        if(right2!=0) {
            PUT_PARENT(right2, parent);
        }
        //
        if(grandparent!=0 && GET_SIZE(parent)<=GET_SIZE(grandparent)) {
            PUT_LEFT(grandparent, child);
        } else if (grandparent!=0) {
            PUT_RIGHT(grandparent, child);
        } else {
            rootMain=child;
        }
        PUT_PARENT(child, grandparent);
        PUT_PARENT(parent, child);
        PUT_COLOR(parent, GET_COLOR(child));
        PUT_COLOR(child, pColor);

    } else {
        char* left1=GET_LEFT_CHILD(parent);
        char* grandparent=GET_PARENT(parent);
        char* right2=GET_RIGHT_CHILD(child);
        char* left2=GET_LEFT_CHILD(child);
        char pColor=GET_COLOR(parent);

        PUT_RIGHT(child, parent);
        PUT_LEFT(child, left1);
        if(left1!=0) {
            PUT_PARENT(left1, child);
        }
        PUT_LEFT(parent, left2);
        if(left2!=0) {
            PUT_PARENT(left2, child);
        }
        PUT_RIGHT(parent, right2);
        if(right2!=0) {
            PUT_PARENT(right2, child);
        }
        
        if(grandparent!=0 && GET_SIZE(parent)<=GET_SIZE(grandparent)) {
            PUT_LEFT(grandparent, child);
        } else if (grandparent!=0) {
            PUT_RIGHT(grandparent, child);
        } else {
            rootMain=child;
        }
        PUT_PARENT(child, grandparent);
        PUT_PARENT(parent, child);
        PUT_COLOR(parent, GET_COLOR(child));
        PUT_COLOR(child, pColor);
    }
    return;
}
//SHOULD HAVE A COMMAND TO PUT RED/BLACK BIT IN POINTER HEADERS FOR USE HERE
void* addNode(void* root, void* newNode, size_t size) {
    PUT(newNode, PACK_COLOR(GET_SIZE(newNode),1, GET_ALLOC(newNode) ));
    baseAdd(root, newNode, size);
    insertRecolor(newNode);
    return NULL;
}

void baseAdd(void* root, void* newNode, size_t size) {
    if (GET_SIZE(root)<size) {
        if(GET_RIGHT_CHILD(root)!=0) {
            baseAdd(GET_RIGHT_CHILD(root), newNode, size);
        } else {
            PUT_RIGHT(root, newNode);
            PUT_LEFT(newNode, 0);
            PUT_RIGHT(newNode, 0);
            PUT_PARENT(newNode, root);
        }
    }

    if(GET_SIZE(root)>=size) {
        if(GET_LEFT_CHILD(root)!=0) {
            baseAdd(GET_LEFT_CHILD(root), newNode, size);
        } else {
            PUT_LEFT(root, newNode);
            PUT_LEFT(newNode, 0);
            PUT_RIGHT(newNode, 0);
            PUT_PARENT(newNode, root);
        }
    }
}
/*
The Rules:
Every node is either red or black.
1. All null nodes are considered black.
2. A red node does not have a red child.
3. Every path from a given node to any of its leaf nodes (that is, to any descendant null node) goes through the same number of 
   black nodes.
4. (Conclusion) If a node N has exactly one child, the child must be red. If the child were black, its leaves would sit at a 
   different black depth than N's null node (which is considered black by rule 2), violating requirement 4.
*/
void insertRecolor(void* newNode) {
    char* parent=GET_PARENT(newNode);

    //root==newNode
    if (parent==0) {
        rootMain=newNode;
        PUT_COLOR(newNode, 0);
        return;
    }

    char* grandparent=GET_PARENT(parent);
    //your parent is the root, root is always black, so we return immediately.  
    if (grandparent==0) {
        return;
    }
    //need to do the no uncle case as well. 


    //general cases now
    char* uncle;
    //CHANGE TO CHECK USING SIZES LOL
    if (GET_LEFT_CHILD(grandparent)==parent) {
        uncle=GET_RIGHT_CHILD(grandparent);
    } else {
        uncle=GET_LEFT_CHILD(grandparent);
    }

    //parent is black then no work has to be done all the rules are obeyed
    if (GET_COLOR(parent)==0) {
        return;
    }

    //recolor according to rules and repeat as we go up.
    if (uncle!=0 && GET_COLOR(uncle)==1) {
        PUT_COLOR(parent, 0);
        PUT_COLOR(uncle, 0);
        PUT_COLOR(grandparent, 1);
        insertRecolor(grandparent);
        return;
    }

    //these transformations work irregardless of uncle 
    //now we know uncle is black and have to make some rotations accordingly
    //first case we are left child of parent who is left child of grandparent
    if (GET_SIZE(newNode)<=GET_SIZE(parent) && GET_SIZE(parent)<= GET_SIZE(grandparent)) {
        //rotate parent up to the right and color parent black and grandparent red
        rightRotate(grandparent);
        PUT_COLOR(parent, 0);
        PUT_COLOR(grandparent, 1);
        return;
    }

    //second case we are right child of parent who is left child of grandparent
    if(GET_SIZE(newNode)>GET_SIZE(parent) && GET_SIZE(parent)<=GET_SIZE(grandparent)) {
        //after left rotation we have same as previous case but neNode and parent are swapped.
        leftRotate(parent);
        rightRotate(grandparent);
        PUT_COLOR(newNode, 0);
        PUT_COLOR(grandparent, 1);
        return;
    }

    //third case we are right child of parent who is right child of grandparent
    if(GET_SIZE(newNode)>GET_SIZE(parent) && GET_SIZE(parent)>GET_SIZE(grandparent)) {
        leftRotate(grandparent);
        PUT_COLOR(parent, 0);
        PUT_COLOR(grandparent, 1);
        return;
    } 

    //fourth case we are left child of parent who is right child of grandparent
    //after right rotation we have same case as before but with newNode and parent swapped
    rightRotate(parent);
    leftRotate(grandparent);
    PUT_COLOR(newNode, 0);
    PUT_COLOR(grandparent, 1);
    return;
}


void removeNode(void* removeNode) {
    baseRemove(removeNode);
    deleteRecolor(removeNode);
    return;
}

void baseRemove(void* removeNode) {
    //deal with leaf case first
    if(GET_LEFT_CHILD(removeNode)==0 && GET_RIGHT_CHILD(removeNode)==0) {
        return;
    }

    if(GET_RIGHT_CHILD(removeNode)==0) {
        //deal with single child case
        if(GET_RIGHT_CHILD(GET_LEFT_CHILD(removeNode))==0 && GET_LEFT_CHILD(GET_LEFT_CHILD(removeNode))==0) {
            return;
        }
        char* swapNode=getLargest(GET_LEFT_CHILD(removeNode));
        //note that swap also swaps the color to preserve tree rules.
        if(swapNode==GET_LEFT_CHILD(removeNode)) {
            swapChild(removeNode, swapNode);
        } else {
            swap(removeNode, swapNode);
        }
        baseRemove(removeNode);
        return;
    }

    if(GET_LEFT_CHILD(removeNode)==0) {
        if(GET_RIGHT_CHILD(GET_RIGHT_CHILD(removeNode))==0 && GET_LEFT_CHILD(GET_RIGHT_CHILD(removeNode))==0) {
            return;
        }
        char* swapNode=getSmallest(GET_RIGHT_CHILD(removeNode));
        if(swapNode==GET_RIGHT_CHILD(removeNode)) {
            swapChild(removeNode, swapNode);
        } else {
            swap(removeNode, swapNode);
        }
        baseRemove(removeNode);
        return;
    }

    char* swapNode=getSmallest(GET_RIGHT_CHILD(removeNode));
    if(swapNode==GET_RIGHT_CHILD(removeNode)) {
        swapChild(removeNode, swapNode);
    } else {
        swap(removeNode, swapNode);
    }
    baseRemove(removeNode);
    return;
}

//handles the double black case, the nullCheck is for if we let a null node be a double black
void handleColoringDelete(void* colorNode, char nullCheck) {
    //we are not null
    char* sibling;
    char* parent;
    if(nullCheck==0) {
        parent=GET_PARENT(colorNode);
        if (parent==0) {
            return;
        }
        if(colorNode==GET_LEFT_CHILD(parent)) {
            sibling=GET_RIGHT_CHILD(parent);
        } else {
            sibling=GET_LEFT_CHILD(parent);
        }
    } else {
        parent=colorNode;
        //in null case the node passed in is the parent of the null node rather than the node 
        if(GET_LEFT_CHILD(colorNode)==0) {
            sibling=GET_RIGHT_CHILD(colorNode);
        } else {
            sibling=GET_LEFT_CHILD(colorNode);
        }
    }
    //now we handle every sibling color/child pair case
    if (GET_LEFT_CHILD(sibling)==0 && GET_RIGHT_CHILD(sibling)==0) {
        //by the rules of the tree you can't not have kids and be red as well with a double black case. 
        PUT_COLOR(sibling, 1);
        if(GET_COLOR(parent)==0) {
            handleColoringDelete(parent, 0);
        } else {
            PUT_COLOR(parent, 0);
        }
        return;
    }

    if(GET_COLOR(sibling)==1) {
        //if you're red you have two children and they are both black and not null by tree rules
        if (GET_SIZE(sibling)<=GET_SIZE(parent)) {
            rightRotate(parent);
        } else {
            leftRotate(parent);
        }
        PUT_COLOR(parent, 1);
        PUT_COLOR(sibling, 0);
        handleColoringDelete(parent, 1);
        return;
    }

    //check if there are two children
    if (GET_LEFT_CHILD(sibling)!=0 && GET_RIGHT_CHILD(sibling)!=0) {
        char* left=GET_LEFT_CHILD(sibling);
        char* right=GET_RIGHT_CHILD(sibling);
        //one red child case (MAYBE NEED TO DO A CoLOR BASED ON ~GET_COLOR(PARENT))
        if (GET_COLOR(left)==1) {
            if(GET_SIZE(sibling)<=GET_SIZE(parent)) {
                rightRotate(parent);
                PUT_COLOR(sibling, GET_COLOR(parent));
                PUT_COLOR(left, 0);
                PUT_COLOR(parent, 0);
                return;
            } else {
                rightRotate(sibling);
                PUT_COLOR(sibling, 1);
                PUT_COLOR(left, 0);
                leftRotate(parent);
                PUT_COLOR(left, GET_COLOR(parent));
                PUT_COLOR(sibling, 0);
                PUT_COLOR(parent, 0);
                return; 
            }
        }
        if(GET_COLOR(right)==1 ) {
            if(GET_SIZE(sibling)<=GET_SIZE(parent)) {
                leftRotate(sibling);
                PUT_COLOR(sibling, 1);
                PUT_COLOR(right, 0);
                rightRotate(parent);
                PUT_COLOR(right, GET_COLOR(parent));
                PUT_COLOR(sibling, 0);
                PUT_COLOR(parent, 0);
                return;
            } else {
                leftRotate(parent);
                PUT_COLOR(sibling, GET_COLOR(parent));
                PUT_COLOR(right, 0);
                PUT_COLOR(parent, 0);
                return;
            }
        }
        PUT_COLOR(sibling, 1);
        if(GET_COLOR(parent)==0) {
            handleColoringDelete(parent, 0);
        } else {
            PUT_COLOR(parent, 0);
        }
        return;
    }
    //now we know its one child handle left and right case
    if(GET_LEFT_CHILD(sibling)==0) {
        char* right=GET_RIGHT_CHILD(sibling);
         if(GET_SIZE(sibling)<=GET_SIZE(parent)) {
            leftRotate(sibling);
            PUT_COLOR(sibling, 1);
            PUT_COLOR(right, 0);
            rightRotate(parent);
            PUT_COLOR(right, GET_COLOR(parent));
            PUT_COLOR(sibling, 0);
            PUT_COLOR(parent, 0);
            //PUT_COLOR(sibling, 0);
            return;
        } else {
            leftRotate(parent);
            PUT_COLOR(sibling, GET_COLOR(parent));
            PUT_COLOR(right, 0);
            PUT_COLOR(parent, 0);
            return;
        }
    }

    if(GET_RIGHT_CHILD(sibling)==0) {
        char* left=GET_LEFT_CHILD(sibling);
        if(GET_SIZE(sibling)<=GET_SIZE(parent)) {
            rightRotate(parent);
            PUT_COLOR(sibling, GET_COLOR(parent));
            PUT_COLOR(left, 0);
            PUT_COLOR(parent, 0);
            return;
        } else {
            rightRotate(sibling);
            PUT_COLOR(sibling, 1);
            PUT_COLOR(left, 0);
            leftRotate(parent);
            PUT_COLOR(left, GET_COLOR(parent));
            PUT_COLOR(sibling, 0);
            PUT_COLOR(parent, 0);
            return;
        }
    }

}
//now do red-black deletion operations understanding that we either have a leaf or a node with a child and parent.
//handle the casework accordingly. 
//handles the actual deletion but not 
void deleteRecolor(void* removeNode) {
    //DEAL WITH THIS
    if(GET_PARENT(removeNode)==0) {
        char* left=GET_LEFT_CHILD(removeNode);
        char* right=GET_RIGHT_CHILD(removeNode);
        if(left!=0) {
            rootMain=left;
            PUT_PARENT(left, 0);
            PUT_COLOR(left, 0);
            return;
        } else if (right!=0) {
            rootMain=right;
            PUT_PARENT(right, 0);
            PUT_COLOR(right, 0);
            return;
        }
        return;
    }
    char* parent=GET_PARENT(removeNode);
    
    //now we get to the general case 
    //no children first
    if (GET_LEFT_CHILD(removeNode)==0 && GET_RIGHT_CHILD(removeNode)==0) {
        char* sibling;
        if(removeNode==GET_LEFT_CHILD(parent)) {
            PUT_LEFT(parent, 0);
            sibling=GET_RIGHT_CHILD(parent);
        } else {
            PUT_RIGHT(parent, 0);
            sibling=GET_LEFT_CHILD(parent);
        }
        //if we're red this is easy
        if (GET_COLOR(removeNode)==1) {
            return;
        }

        handleColoringDelete(parent, 1);
        return;
    }

    //right child only
    if(GET_LEFT_CHILD(removeNode)==0) {
        char* right=GET_RIGHT_CHILD(removeNode);
        //deal with if one of the two is red
        if(GET_COLOR(removeNode)==1 || GET_COLOR(right)==1) {
            if(removeNode==GET_LEFT_CHILD(parent)) {
                PUT_LEFT(parent, right);
                PUT_PARENT(right, parent);
                PUT_COLOR(right, 0);
            } else {
                PUT_RIGHT(parent, right);
                PUT_PARENT(right, parent);
                PUT_COLOR(right, 0);
            }
            return;
        } else {
            if(removeNode==GET_LEFT_CHILD(parent)) {
                PUT_LEFT(parent, right);
                PUT_PARENT(right, parent);
                PUT_COLOR(right, 0);
            } else {
                PUT_RIGHT(parent, right);
                PUT_PARENT(right, parent);
                PUT_COLOR(right, 0);
            }
            handleColoringDelete(right, 0);
        }

    }

    //left child only
    char* left=GET_LEFT_CHILD(removeNode);
    if(GET_COLOR(removeNode)==1 || GET_COLOR(left)==1) {
        if(removeNode==GET_LEFT_CHILD(parent)) {
            PUT_LEFT(parent, left);
            PUT_PARENT(left, parent);
            PUT_COLOR(left, 0);
        } else {
            PUT_RIGHT(parent, left);
            PUT_PARENT(left, parent);
            PUT_COLOR(left, 0);
        }
        return;
    } else {
        if(removeNode==GET_LEFT_CHILD(parent)) {
            PUT_LEFT(parent, left);
            PUT_PARENT(left, parent);
            PUT_COLOR(left, 0);
        } else {
            PUT_RIGHT(parent, left);
            PUT_PARENT(left, parent);
            PUT_COLOR(left, 0);
        }
        handleColoringDelete(left,0);
    }
    //no double child case because bst standard delete always has you do more swaps if two children.
    return;
}

void* getLargest(void* root) {
    if(GET_RIGHT_CHILD(root)==0) {
        return root;
    } else {
        return getLargest(GET_RIGHT_CHILD(root));
    }
}

void* getSmallest(void* root) {
    if(GET_LEFT_CHILD(root)==0) {
        return root;
    } else {
        return getSmallest(GET_LEFT_CHILD(root));
    }
}

//returns 0 to indicate the heap needs to be extended
//We are searching for the smallest node greater than or equal to size, how this works is you search for your node
//normally and once you hit a NULL child if you're <= you're current node just return that one but if you are
//greater than your current node then you return the last parent you went left from i.e. the last node you passed
// that was larger than you.
void* searchSize(void* root, size_t size) {
    return recurse(root, root, size);
}

void* recurse(void* root, void* lastLarger, size_t size) {
    if (size==GET_SIZE(root)) {
        return root;
    }

    if (size<GET_SIZE(root)) {
        if(GET_LEFT_CHILD(root)!=0) {
            return recurse((char*) GET_LEFT_CHILD(root), root, size);
        }
        else {
            return root;
        }
    }

    if (size>GET_SIZE(root)) {
        if(GET_RIGHT_CHILD(root)!= 0) {
            return recurse((char *) GET_RIGHT_CHILD(root), lastLarger, size);
        }
        else {
            //return 0 if heap needs to be extended  
            if (size>GET_SIZE(lastLarger)) {
                return 0;
            }
            else {
                return lastLarger;
            }
        }
    }

}










