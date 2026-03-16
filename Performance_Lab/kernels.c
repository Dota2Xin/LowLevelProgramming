/********************************************************
 * Kernels to be optimized for the CS:APP Performance Lab
 ********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "defs.h"

/* 
 * Please fill in the following team struct 
 */
team_t team = {
    "Ws Only",              /* Team name */

    "IDRK WUSSUP",     /* First member full name */
    "ddx@ucsb.edu",  /* First member email address */

    "",                   /* Second member full name (leave blank if none) */
    ""                    /* Second member email addr (leave blank if none) */
};

/***************
 * ROTATE KERNEL
 ***************/

/******************************************************
 * Your different versions of the rotate kernel go here
 ******************************************************/

/* 
 * naive_rotate - The naive baseline version of rotate 
 */

char naive_rotate_descr[] = "naive_rotate: Naive baseline implementation";
void naive_rotate(int dim, pixel *src, pixel *dst) 
{
    int i, j;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++)
	    dst[RIDX(dim-1-j, i, dim)] = src[RIDX(i, j, dim)];
}

/* 
 * rotate - Your current working version of rotate
 * IMPORTANT: This is the version you will be graded on
 */
//#define RIDX(i,j,n) ((i)*(n)+(j))
//Loop unrolls aren't doing anything ddx
char rotate_descr[] = "rotate: Current working version";
void rotate(int dim, pixel *src, pixel *dst) 
{
    int i=0;
    int j=0;
    int startRid=0;
    int nextRid=0;
    int preCalc=dim*dim;

    for (i = 0; i < dim; i+=2) {
        startRid=i*dim;
	    for (j = 0; j < dim; j+=4) {
            nextRid=preCalc-dim*(1+j)+i;
	        dst[nextRid] = src[startRid+j];
            dst[nextRid-dim]=src[startRid+j+1];
            dst[nextRid-2*dim]=src[startRid+j+2];
            dst[nextRid-3*dim]=src[startRid+j+3];
        }
        startRid+=dim;
        for (j = 0; j < dim; j+=4) {
            nextRid=preCalc-dim*(1+j)+i+1;
	        dst[nextRid] = src[startRid+j];
            dst[nextRid-dim]=src[startRid+j+1];
            dst[nextRid-2*dim]=src[startRid+j+2];
            dst[nextRid-3*dim]=src[startRid+j+3];
        }
    }
}

/*********************************************************************
 * register_rotate_functions - Register all of your different versions
 *     of the rotate kernel with the driver by calling the
 *     add_rotate_function() for each test function. When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_rotate_functions() 
{
    add_rotate_function(&naive_rotate, naive_rotate_descr);   
    add_rotate_function(&rotate, rotate_descr);   
    /* ... Register additional test functions here */
}


/***************
 * SMOOTH KERNEL
 **************/

/***************************************************************
 * Various typedefs and helper functions for the smooth function
 * You may modify these any way you like.
 **************************************************************/

/* A struct used to compute averaged pixel value */
typedef struct {
    int red;
    int green;
    int blue;
    int num;
} pixel_sum;

/* Compute min and max of two integers, respectively */
static int min(int a, int b) { return (a < b ? a : b); }
static int max(int a, int b) { return (a > b ? a : b); }

/* 
 * initialize_pixel_sum - Initializes all fields of sum to 0 
 */
static void initialize_pixel_sum(pixel_sum *sum) 
{
    sum->red = sum->green = sum->blue = 0;
    sum->num = 0;
    return;
}

/* 
 * accumulate_sum - Accumulates field values of p in corresponding 
 * fields of sum 
 */
static void accumulate_sum(pixel_sum *sum, pixel p) 
{
    sum->red += (int) p.red;
    sum->green += (int) p.green;
    sum->blue += (int) p.blue;
    sum->num++;
    return;
}

/* 
 * assign_sum_to_pixel - Computes averaged pixel value in current_pixel 
 */
static void assign_sum_to_pixel(pixel *current_pixel, pixel_sum sum) 
{
    current_pixel->red = (unsigned short) (sum.red/sum.num);
    current_pixel->green = (unsigned short) (sum.green/sum.num);
    current_pixel->blue = (unsigned short) (sum.blue/sum.num);
    return;
}

/* 
 * avg - Returns averaged pixel value at (i,j) 
 */
static pixel avg(int dim, int i, int j, pixel *src) 
{
    int ii, jj;
    pixel_sum sum;
    pixel current_pixel;

    initialize_pixel_sum(&sum);
    for(ii = max(i-1, 0); ii <= min(i+1, dim-1); ii++) {
	    for(jj = max(j-1, 0); jj <= min(j+1, dim-1); jj++) {
	        accumulate_sum(&sum, src[RIDX(ii, jj, dim)]);
        }
    }

    assign_sum_to_pixel(&current_pixel, sum);
    return current_pixel;
}

static pixel myAvg(int dim, int i, int j, pixel *src) 
{
    int ii, jj;
    pixel_sum sum;
    pixel current_pixel;
    int minI=min(i+1,dim-1);
    int minJ=min(j+1,dim-1);
    int maxI=max(i-1,0);
    int maxJ=max(j-1,0);
    initialize_pixel_sum(&sum);
    for(ii = maxI; ii <= minI; ii++) {
	    for(jj =maxJ; jj <= minJ; jj++) {
	        accumulate_sum(&sum, src[RIDX(ii, jj, dim)]);
        }
    }

    assign_sum_to_pixel(&current_pixel, sum);
    return current_pixel;
}

void updateSum(int dim, int i, int j, pixel *src, pixel_sum *sum) {
    sum->red += (int) (src[RIDX(i,j,dim)].red+src[RIDX(i+1,j,dim)].red+src[RIDX(i-1,j,dim)].red);
    sum->blue += (int) (src[RIDX(i,j,dim)].blue+src[RIDX(i+1,j,dim)].blue+src[RIDX(i-1,j,dim)].blue);
    sum->green +=(int) (src[RIDX(i,j,dim)].green+src[RIDX(i+1,j,dim)].green+src[RIDX(i-1,j,dim)].green);
    sum->num+=3;
    return;
}

void makeValue(pixel *p, pixel_sum sum1, pixel_sum sum2, pixel_sum sum3) {
    p->red = (unsigned short) ((sum1.red+sum2.red+sum3.red)/(sum1.num+sum2.num+sum3.num));
    p->blue = (unsigned short) ((sum1.blue+sum2.blue+sum3.blue)/(sum1.num+sum2.num+sum3.num));
    p->green = (unsigned short) ((sum1.green+sum2.green+sum3.green)/(sum1.num+sum2.num+sum3.num));
    return;
}
/******************************************************
 * Your different versions of the smooth kernel go here
 ******************************************************/

/*
 * naive_smooth - The naive baseline version of smooth 
 */
char naive_smooth_descr[] = "naive_smooth: Naive baseline implementation";
void naive_smooth(int dim, pixel *src, pixel *dst) 
{
    int i, j;

    for (i = 0; i < dim; i++) {
	    for (j = 0; j < dim; j++) {
	        dst[RIDX(i, j, dim)] = avg(dim, i, j, src);
        }
    }
}


/*
 * smooth - Your current working version of smooth. 
 * IMPORTANT: This is the version you will be graded on
 */
char smooth_descr[] = "smooth: Current working version";
void smooth(int dim, pixel *src, pixel *dst) 
{
    int i, j;
    for(j=0; j<dim; j++) {
        dst[RIDX(0,j,dim)]=avg(dim,0,j,src);
    }
    for(j=0; j<dim; j++) {
        dst[RIDX(dim-1,j,dim)]=avg(dim,dim-1,j,src);
    }
    for(i=1; i<dim-1; i++) {
        dst[RIDX(i,0,dim)]=avg(dim,i,0,src);
    }
    for(i=1; i<dim-1; i++) {
        dst[RIDX(i, dim-1,dim)]=avg(dim,i,dim-1,src);
    }
    pixel_sum left;
    pixel_sum on;
    pixel_sum right;
    pixel_sum temp;
    initialize_pixel_sum(&left);
    initialize_pixel_sum(&right);
    initialize_pixel_sum(&on);
    initialize_pixel_sum(&temp);
    pixel storePixel;
    for (i = 1; i < dim-1; i++) {
        initialize_pixel_sum(&left);
        initialize_pixel_sum(&right);
        initialize_pixel_sum(&on);
        updateSum(dim, i, 0, src, &left);
        updateSum(dim, i, 1, src, &on);
        updateSum(dim, i, 2, src, &right);
        makeValue(&storePixel, left,on,right);
        dst[RIDX(i,1,dim)]=storePixel;
	    for (j = 2; j < dim-1; j++) {
            initialize_pixel_sum(&temp);
            pixel currentPixel;
            updateSum(dim,i,j+1,src,&temp);
            makeValue(&currentPixel,on,right,temp);
	        dst[RIDX(i, j, dim)] = currentPixel;
            left=on;
            on=right;
            right=temp;
        }
    }
}
//idea for smooth, do running count of stuff above and below and in your row and then essentially move along and scan and whatnot. 
//So we do sum over top and then would add and subtract neighbor values, basically instead of doing a new set of 9 computations for
//each pixel we'll just update the convolved value with 6 new values. 

/********************************************************************* 
 * register_smooth_functions - Register all of your different versions
 *     of the smooth kernel with the driver by calling the
 *     add_smooth_function() for each test function.  When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_smooth_functions() {
    add_smooth_function(&smooth, smooth_descr);
    add_smooth_function(&naive_smooth, naive_smooth_descr);
    /* ... Register additional test functions here */
}

