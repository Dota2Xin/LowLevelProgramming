#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <math.h>


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

/* single word (4) or double word (8) al    ignment */
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

#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp)+GET_SIZE(HDRP(bp))-WSIZE)

//Binary tree methods
#define LEFT_CHILD(p) (GET(((char*) p)+DSIZE))
#define RIGHT_CHILD(p) (GET(((char*) p)+2*DSIZE))
#define GET_COLOR(p) ((GET(p) & 0x2)>>1)
#define PACK_COLOR(size, color, alloc) ((size)|(2*color)|(alloc))

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

int main() {
    unsigned long size=1;

    printf("Sisze of %u\n", sizeof(size));
    return 0;
}

/*
size_t a=PACK_COLOR(32, 1, 0);
    size_t* p=&a;

    size_t size=GET_SIZE(p);
    char color=GET_COLOR(p);
    char alloc=GET_ALLOC(p);

    printf("size: %u\n", size);
    printf("Color: %i\n", color);
    printf("Alloc: %i\n", alloc);

    a=PACK_COLOR(ALIGN(531), 1, 1);
    p=&a;

    size=GET_SIZE(p);
    color=GET_COLOR(p);
    alloc=GET_ALLOC(p);

    printf("size: %u\n", size);
    printf("color: %i\n", color);
    printf("Alloc: %i\n", alloc);

    a=PACK_COLOR(ALIGN(5), 0, 0);
    p=&a;

    size=GET_SIZE(p);
    color=GET_COLOR(p);
    alloc=GET_ALLOC(p);

    printf("size: %u\n", size);
    printf("color: %i\n", color);
    printf("Alloc: %i\n", alloc);

    a=PACK_COLOR(ALIGN(64), 0, 1);
    p=&a;

    size=GET_SIZE(p);
    color=GET_COLOR(p);
    alloc=GET_ALLOC(p);

    printf("size: %u\n", size);
    printf("color: %i\n", color);
    printf("Alloc: %i\n", alloc);
    return 0;
*/