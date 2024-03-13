#include <stdio.h>

int main() {
    //This is code for the first homework assignment thingy
    int a, b;
    printf("Value of a: \n");
    scanf("%d",&a);
    printf("Value of b:\n");
    scanf("%d", &b);

    int c=0;

    while(b>=a) {
        b=b-a;
        c=c+1;
    }

    printf("Integer Division Result: %d\n", c);
    printf("Remainder: %d\n", b);
    return 0;
}