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

int main(){
    //testBitXor();
    //testTMin();
    testTMax();
    return 0;
}
