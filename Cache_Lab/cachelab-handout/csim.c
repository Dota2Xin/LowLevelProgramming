#include "cachelab.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>


int power(int base, int exp) {
    int store=1;
    for (int i=0; i<exp; i++) {
        store=store*base;
    }
    return store;
}

struct line {
    int valid;
    long tag;
    char *block;
};

struct set {
    //lruIndex= Least Recently Used Line Index
    int lruIndex;
    struct line *lines;
    int *leastRecentlyUsed;
};

struct cache {
    int s;
    int b;
    long E;
    int tagLen;
    struct set *sets;
};

struct cacheUpdate {
    int hits;
    int misses;
    int evictions
};

void updateLeastRecentlyUsed(int *leastRecentlyUsed, int mostRecentlyUsed, long len) {
    int storeIndex=0;
    for (int i=0; i<len; i++) {
        if (leastRecentlyUsed[i]==mostRecentlyUsed) {
            storeIndex=i;
            break;
        }
    }
    for(int i=0; i<len-1;i++) {
        if(i>=storeIndex) {
            leastRecentlyUsed[i]=leastRecentlyUsed[i+1];
        }
    }
    leastRecentlyUsed[len-1]=mostRecentlyUsed;
    return;
}

struct cache* makeCache(int s, int b, long E) {
    struct cache mainCache;
    mainCache.s=s;
    mainCache.b=b;
    mainCache.E=E;
    struct set setList[s];
    for(int i=0; i<s; i++) {
        struct set currentSet;
        struct line lineList[E];
        int leastReuArr[E];
        for(int j=0; j<E; i++) {
            leastReuArr[i]=i;
            struct line currentLine;
            currentLine.valid=0;
            currentLine.tag=0;
            char byteList[b];
            char *bytePtr=&byteList;
            currentLine.block=bytePtr;
            lineList[j]=currentLine;
        }
        currentSet.leastRecentlyUsed=&leastReuArr;
        struct line *linesPtr=&lineList;
        currentSet.lines=linesPtr;
        currentSet.lruIndex=0;
        setList[i]=currentSet;
    }

    mainCache.tagLen=64-(b+s);
    struct set *setPtr=&setList;
    mainCache.sets=setPtr;
    struct cache *cachePointer;
    cachePointer=&mainCache;
    return cachePointer;
}

void simulateLoad(struct cache *mainCache, unsigned long address, struct cacheUpdate *cacheValues) {
    unsigned long tagVal=0;
    unsigned long setIndex=0;
    unsigned long bytes=0;
    int s=mainCache->s;
    int b=mainCache->b;
    long E=mainCache->E;

    //break up address into different parts with some bit manip
    unsigned long bitStore=power(2,b)-1;
    bytes=address & bitStore;
    bitStore=(power(2,s)-1)<<b;
    setIndex= (address & bitStore)>>b;
    tagVal=(address-bytes-setIndex)>>(b+s);

    //look for matching tag
    struct set currentSet=mainCache->sets[setIndex];
    for(int i=0; i<E; i++) {
        struct line currentLine=currentSet.lines[i];
        long currentTag=currentLine.tag;
        int valid=currentLine.valid;
        if (currentTag==tagVal && valid==1) {
            cacheValues->hits+=1;
            updateLeastRecentlyUsed(currentSet.leastRecentlyUsed, i, E);
            return;
        }
    }

    //if tag not found see if we have valid cache lines open in the set and put it in
    for(int i=0; i<E; i++) {
        struct line currentLine=currentSet.lines[i];
        long currentTag=currentLine.tag;
        int valid=currentLine.valid;
        if (valid==0) {
           currentSet.lines[i].valid=1;
           currentSet.lines[i].tag=tagVal;
           cacheValues->misses+=1;
           updateLeastRecentlyUsed(currentSet.leastRecentlyUsed, i, E);
           return;
        }
    }

    //evict and update the least recently used value
    cacheValues->misses+=1;
    cacheValues->evictions+=1;
    int indexToEvict=currentSet.leastRecentlyUsed[0];
    currentSet.lines[indexToEvict].tag=tagVal;
    updateLeastRecentlyUsed(currentSet.leastRecentlyUsed, indexToEvict, E);
    return;


}

void simulateStore(struct cache *mainCache,unsigned long address, struct cacheUpdate *cacheValues) {
    simulateLoad(mainCache, address, cacheValues);
}

void simulateModify(struct cache *mainCache, unsigned long address, struct cacheUpdate *cacheValues) {
    simulateLoad(mainCache, address, cacheValues);
    simulateLoad(mainCache, address, cacheValues);
}

void simulateCache() {
    return;
} 

int main(int argc, char *argv[])
{
    char verbose=0;
    char help=0;
    int flags, opt;
    char *inputFile;
    int s, b;
    long E;
    while((opt=getopt(argc,argv,"hvs:E:b:t:")) !=-1) {
        switch(opt) {
            case 'h':
                help=1;
                break;
            case 'v':
                verbose=1;
                break;
            case 's':
                s=atoi(optarg);
                break;
            case 'E':
                E=atoi(optarg);
                break;
            case 'b':
                b=atoi(optarg);
                break;
            case 't':
                inputFile=atoi(optarg);
                break;
        }
    }
    printSummary(0, 0, 0);
    return 0;
}
