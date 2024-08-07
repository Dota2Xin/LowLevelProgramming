/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int i, j,k,l,tmp;
    int blockSize=8;
    //square matrix=simpler algorithm
    //also both divisible by 4 so we can do fourfold unrolling for this.
    int padSize=2;
    for(i=0; i<N; i+=blockSize) {
        for(j=0; j<M; j+=blockSize) {
            for(k=0; k<blockSize; k++) {
                for(l=0; l<blockSize; l++) {
                    tmp=A[(i*padSize)%N+k][j+l];
                    B[j+l][(i*padSize)%N+k]=tmp;
                }
            }
        }
    }
}

char trans_desc_block[] = "Block Transpose";
void transpose_block(int M, int N, int A[N][M], int B[M][N]) {
    int i, j,k,l;
    int blockSize=8;
    int tmp1, tmp2, tmp3 ,tmp4, tmp5, tmp6, tmp7, tmp8;
    //square matrix=simpler algorithm
    //also both divisible by 4 so we can do fourfold unrolling for this.
    //int padSize=9*blockSize;
    for(i=0; i<N; i+=blockSize) {
        for(j=0; j<M; j+=blockSize) {
            for(k=i; k<i+blockSize; k++) {
                for(l=j; l<j+blockSize; l+=8) {
                    tmp1=A[k][l];
                    tmp2=A[k][l+1];
                    tmp3=A[k][l+2];
                    tmp4=A[k][l+3];
                    tmp5=A[k][l+4];
                    tmp6=A[k][l+5];
                    tmp7=A[k][l+6];
                    tmp8=A[k][l+7];
                    B[l][k]=tmp1;
                    B[l+1][k]=tmp2;
                    B[l+2][k]=tmp3;
                    B[l+3][k]=tmp4;
                    B[l+4][k]=tmp5;
                    B[l+5][k]=tmp6;
                    B[l+6][k]=tmp7;
                    B[l+7][k]=tmp8;
                    //B[l][k]=A[k][l];
                }
            }
        }
    }
}
/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 
int min(int a, int b) {
    if (a<b) {
        return a;
    }
    else {
        return b;
    }
}
char trans_desc_diag[]="Diagonal Access Transpose";
void trans_diag(int M, int N, int A[N][M], int B[M][N]) {
    int i,j, tmp;
    for(i=0; i<2*M-1; i++) {
        int maxVal=min(i+1,2*M-i-1);
        int otherVal=i-M+1;
        for(j=0; j<maxVal;j++) {
            if(i+1>M) {
                tmp = A[otherVal+j][M-1-j];
                //printf("Val: %d, Index1:%d Index2:%d  \n",tmp, otherVal+j, M-1-j);
                //fflush(stdout);
                B[M-1-j][otherVal+j]=tmp;

            } else {
                tmp = A[j][maxVal-j-1];
                //printf("Val: %d, Index1:%d Index2:%d  \n",tmp, j, maxVal-j-1);
                //fflush(stdout);
                B[maxVal-1-j][j]=tmp;
            }
        }
    }
    //int check=is_transpose(M, N, A, B);
    //printf("%d", check);
}
/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    //registerTransFunction(trans_diag, trans_desc_diag);
    /* Register your solution function */
    registerTransFunction(transpose_block, trans_desc_block);
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
   // registerTransFunction(trans, trans_desc); 


}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

