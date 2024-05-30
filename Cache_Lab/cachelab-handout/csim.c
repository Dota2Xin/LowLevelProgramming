#include "cachelab.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>


unsigned long power(int base, int exp) {
    unsigned long store=1;
    for (int i=0; i<exp; i++) {
        store=store*base;
    }
    return store;
}

struct line {
    int valid;
    long tag;
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
    int E;
    int tagLen;
    struct set *sets;
};

struct cacheUpdate {
    int hits;
    int misses;
    int evictions;
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

struct cache* makeCache(int s, int b, int E) {
    struct cache *mainCache=(struct cache *)malloc(sizeof(struct cache));
    mainCache->s=s;
    mainCache->b=b;
    mainCache->E=E;
    int maxSets=1;
    for(int k=0; k<s; k++) {
        maxSets=maxSets*2;
    } 
    mainCache->sets=(struct set *)malloc(maxSets*sizeof(struct set));
    //struct set setList[s];
    for(int i=0; i<maxSets; i++) {
        mainCache->sets[i].lines=(struct line *)malloc(E*sizeof(struct line));
        mainCache->sets[i].leastRecentlyUsed=(int *)malloc(E*sizeof(int));
        //struct line lineList[E];
        //int leastReuArr[E];
        for(int j=0; j<E; j++) {
            //leastReuArr[i]=i;
            mainCache->sets[i].lines[j].valid=0;
            mainCache->sets[i].lines[j].tag=0;
            mainCache->sets[i].leastRecentlyUsed[j]=j;
            //lineList[j]=currentLine;
        }
        //currentSet.leastRecentlyUsed=&leastReuArr;
        //struct line *linesPtr=&lineList;
        //currentSet.lines=linesPtr;
        mainCache->sets[i].lruIndex=0;
        //setList[i]=currentSet;
    }

    mainCache->tagLen=64-(b+s);
    //struct set *setPtr=&setList;
    //mainCache.sets=setPtr;
    //struct cache *cachePointer;
    //cachePointer=&mainCache;
    return mainCache;
}

void simulateLoad(struct cache *mainCache, unsigned long address, struct cacheUpdate *cacheValues) {
    unsigned long tagVal=0;
    unsigned long setIndex=0;
    unsigned long bytes=0;
    int s=mainCache->s;
    int b=mainCache->b;
    int E=mainCache->E;

    //break up address into different parts with some bit manip
    unsigned long bitStore=power(2,b)-1;
    bytes=address & bitStore;
    bitStore=(power(2,s)-1)<<b;
    setIndex= (address & bitStore)>>b;
    tagVal=(address-bytes-setIndex)>>(b+s);

    //look for matching tag
    for(int i=0; i<E; i++) {
        long currentTag=mainCache->sets[setIndex].lines[i].tag;
        int valid=mainCache->sets[setIndex].lines[i].valid;
        if (currentTag==tagVal && valid==1) {
            cacheValues->hits+=1;
            updateLeastRecentlyUsed(mainCache->sets[setIndex].leastRecentlyUsed, i, E);
            return;
        }
    }
    //if tag not found see if we have valid cache lines open in the set and put it in
    for(int i=0; i<E; i++) {
        //long currentTag=currentLine.tag;
        int valid=mainCache->sets[setIndex].lines[i].valid;
        if (valid==0) {
           mainCache->sets[setIndex].lines[i].valid=1;
           mainCache->sets[setIndex].lines[i].tag=tagVal;
           cacheValues->misses+=1;
           updateLeastRecentlyUsed(mainCache->sets[setIndex].leastRecentlyUsed, i, E);
           return;
        }
    }

    //evict and update the least recently used value
    cacheValues->misses+=1;
    cacheValues->evictions+=1;
    printf("Here it is %d", mainCache->sets[setIndex].leastRecentlyUsed[0]);
    int indexToEvict=mainCache->sets[setIndex].leastRecentlyUsed[0];
    mainCache->sets[setIndex].lines[indexToEvict].tag=tagVal;
    updateLeastRecentlyUsed(mainCache->sets[setIndex].leastRecentlyUsed, indexToEvict, E);
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

void freeCache(struct cache *mainCache) {
    for(int i=0; i<mainCache->s; i++) {
        free(mainCache->sets[i].lines);
        free(mainCache->sets[i].leastRecentlyUsed);
    }
    free(mainCache->sets);
    free(mainCache);
    return;
}

int main(int argc, char *argv[])
{
    //handles file inputs
    char verbose=0;
    char help=0;
    int opt;
    char *inputFile;
    int s, b;
    int E;
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
                inputFile=optarg;
                break;
            case '?':
                fprintf(stderr, "Unknown error occurred\n");
                return 1;
            default:
                fprintf(stderr, "Unknown error occurred\n");
                return 1;
        }
    }
    if(help==1) {
        printf("You asked for help ddx");
    }
    if(verbose==1) {
        printf("this is more verbose than it would have been had you not selected that option...");
    }

    //open file, check if its real, and store amount of lines
    FILE *file2=fopen(inputFile, "r");

    if(file2==NULL) {
        perror("Error Opening File");
        return EXIT_FAILURE;
    }

    char currentString[40];
    int count=0;

    while(fgets(currentString,40,file2)) {
        count++;     
    }

    fclose(file2);
    //reopen file and parse for info
    FILE *file=fopen(inputFile, "r");
    //Type of operation to be performed, 1=load, 2=write, 3=modify, 4=none
    int operations[count];
    long addresses[count];
    int i=0;
    while(fgets(currentString, 40, file)) {
        if(currentString[0]=='I') {
            operations[i]=4;
        }
        else if(currentString[1]=='L') {
            operations[i]=1;
        }
        else if(currentString[1]=='S') {
            operations[i]=2;
        }
        else if(currentString[1]=='M') {
            operations[i]=3;
        }
        int j=3;
        while(currentString[j]!=',') {
            j++;
        }
        char subString[j-3];
        j=3;
        while(currentString[j]!=',') {
            subString[j-3]=currentString[j];
            j++;
        }
        char **endptr=NULL;
        long address=strtol(subString,endptr,16);
        addresses[i]=address;
        i++;
    }
    //make and simulate the cache with instructions from file
    struct cache *mainCache=makeCache(s, b, E);
    struct cacheUpdate storeUpdate;
    storeUpdate.hits=0;
    storeUpdate.misses=0;
    storeUpdate.evictions=0;
    for(int k=0; k<count; k++) {
        if(operations[k]==1) {
            simulateLoad(mainCache, addresses[k], &storeUpdate);
        }
        else if (operations[k]==2) {
            simulateStore(mainCache, addresses[k], &storeUpdate);
        }
        else if (operations[k]==3) {
            simulateModify(mainCache, addresses[k], &storeUpdate);
        }

    }
    freeCache(mainCache);
    printSummary(storeUpdate.hits, storeUpdate.misses, storeUpdate.evictions);
    return 0;
}
