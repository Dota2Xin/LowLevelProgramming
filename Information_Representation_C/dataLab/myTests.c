//Developers are clowns so lets make it ourselves

#include "bits.c"
#include "tests.c"
#include <stdio.h>
void testBitXor() {
    int status=0;
    for(int i=0; i<=10; i++) {
        for(int j=0;j<=10; j++) {
            int test1=bitXor(i,j);
            int test2=i^j;
            if (test1!=test2){
                printf("Test Bit Failed, Expected: %d, Recieved: %d \n",test2,test1);
                status=1;
            }
        }
    }
    if (status==0) {
        printf("Great Success!!!");
    }
}

void testTMin() {
    int minNum=tmin();
    if (minNum==0x80000000) {
        printf("Worked\n");
    }
    else {
        printf("Error, Expected %d, Got: %d \n", 0x80000000, minNum);
    }
}

void testTMax() {
    int minNum=isTmax(0x7FFFFFFF);
    int maxNum=isTmax(67);
    if (minNum==1 && maxNum==0) {
        printf("Worked\n");
    }
    else {
        printf("Error, Expected %d, Got: %d \n", 1, minNum);
        printf("Error, Expected:%d, Got: %d\n",0,maxNum);
    }
}

void testIsAscii() {
    int success=1;
    for(int i=0; i<=31; i++) {
        int store1=isAsciiDigit(1<<i);
        int store2=test_isAsciiDigit(1<<i);
        if (store1!=store2) {
            printf("Error, Input: %d, Output: %d\n", 1<<i, store1);
            success=0;
        }
    }

    for(int i=47; i<=58; i++) {
        int store1=isAsciiDigit(i);
        int store2=test_isAsciiDigit(i);
        if (store1!=store2) {
            printf("Error, Input: %d, Output: %d\n", i, store1);
            success=0;
        }
    }
    if (success==1) { 
        printf("Worked\n");
    }
}

void testConditional() {
    int success=1;
    for (int x=-1; x<=3; x++) {
        for (int y=4; y<=10;y++) {
            for (int z=4; z<=8;z++) {
                int store1=conditional(x,y,z);
                int store2=test_conditional(x,y,z);
                if (store1!=store2) {
                    printf("Error: Expected: %d, Got: %d, Inputs: %d,%d,%d\n", store1, store2,x,y,z);
                    success=0;
                }
            }
        }
    }
    if (success==1) {
        printf("WORKED\n");
    }
}

void testLeq() {
    int success=1;
    for (int x=0; x<=31; x++) {
        for (int y=0; y<=31;y++) {
                int store1=isLessOrEqual(1<<x,1<<y);
                int store2=test_isLessOrEqual(1<<x,1<<y);
                if (store1!=store2) {
                    printf("Error: Expected: %d, Got: %d, Inputs: %d,%d,\n", store2, store1,1<<x,1<<y);
                    success=0;
                }
        }
    }
    if (success==1) {
        printf("WORKED\n");
    }
}
int main(){
    //testBitXor();
    //testTMin();
    //testTMax();
    //testIsAscii();
    //testConditional();
    testLeq();
    return 0;
}
