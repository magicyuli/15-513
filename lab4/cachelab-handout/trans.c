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
#include "contracts.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);
void transpose32(int M, int N, int A[N][M], int B[M][N]);
void transpose64(int M, int N, int A[N][M], int B[M][N]);
void transpose67x61(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    REQUIRES(M > 0);
    REQUIRES(N > 0);


    
    if (M == N && M == 32) {
        transpose32(M, N, A, B);   
    }
    else if (M == N && M == 64) {
        transpose64(M, N, A, B);   
    }
    else if (M == 61 && N == 67) {
        transpose67x61(M, N, A, B);
    }
    ENSURES(is_transpose(M, N, A, B));
}

/**
 * Transpose function for 67x61 matrix
 */
void transpose67x61(int M, int N, int A[N][M], int B[M][N]) {
    // ith row of A, jth column of A, kth sub-row, lth sub-column
    int i, j, k, l;

    // Transpose 8x8 blocks column by column
    for (i = 0; i < N; i += 8) {
        for (j=0; j < M; j += 8) {
            for (l = j; l < j + 8 && l < M; l++) {
                for (k = i; k < i + 8 && k < N; k++) {
                    B[l][k] = A[k][l];
                }
            }
        }
    }
}

/**
 * Transpose function for 64x64 matrix
 */
void transpose64(int M, int N, int A[N][M], int B[M][N]) {
    // ith row of A, jth column of A, kth block
    int i, j, k;
    // block coordinates
    int blk_x;
    int blk_y;
    // temporary values
    int v1, v2, v3, v4;
    // temporary diagonal index and value
    int diag_tmp;
    int diag_idx;

    // Use 8x8 blocks, and process the 4 4x4 sub-blocks in them separately.
    // We'll call them TL(top-left), TR(top-right), 
    // BL(bottom-left), BR(bottom-right).
    for (k = 0; k < (M / 8) * (N / 8); k++) {
        blk_x = k % (M / 8);
        blk_y = k / (M / 8);
        // transpose TL of A into TL of B, and at the same time put the 
        // transpose of TR of A into TR of B, where the transpose of BL of A 
        // belong.
        for (i = 0; i < 4; i++) {
            // Load one row of TR of A into registers.
            j = 4;
            v1 = A[blk_y * 8 + i][blk_x * 8 + j];
            j++;
            v2 = A[blk_y * 8 + i][blk_x * 8 + j];
            j++;
            v3 = A[blk_y * 8 + i][blk_x * 8 + j];
            j++;
            v4 = A[blk_y * 8 + i][blk_x * 8 + j];
            
            // Transpose one row of TL of A and put into TL of B.
            // Avoid assigning diagonal values during transpose.
            diag_idx = -1;
            for (j = 0; j < 4; j++) {
                if (blk_y * 8 + i == blk_x * 8 + j) {
                    diag_idx = blk_y * 8 + i;
                    diag_tmp = A[blk_y * 8 + i][blk_x * 8 + j];
                    continue;
                }
                B[blk_x * 8 + j][blk_y * 8 + i] = 
                    A[blk_y * 8 + i][blk_x * 8 + j];
            }
            if (diag_idx > -1) {
                B[diag_idx][diag_idx] = diag_tmp;
            }
            
            // Transpose one row of TR of A and put into TR of B.
            j = 0;
            B[blk_x * 8 + j][blk_y * 8 + i + 4] = v1;
            j++;
            B[blk_x * 8 + j][blk_y * 8 + i + 4] = v2;
            j++;
            B[blk_x * 8 + j][blk_y * 8 + i + 4] = v3;
            j++;
            B[blk_x * 8 + j][blk_y * 8 + i + 4] = v4;
        }
        
        // Transpose the BL of A and put into TR B, and at the same
        // time relocate TR of B into BL of B.
        for (j = 0; j < 4; j++) {
            // Store one row of TR of B into registers.
            i = 4;
            v1 = B[blk_x * 8 + j][blk_y * 8 + i];
            i++;
            v2 = B[blk_x * 8 + j][blk_y * 8 + i];
            i++;
            v3 = B[blk_x * 8 + j][blk_y * 8 + i];
            i++;
            v4 = B[blk_x * 8 + j][blk_y * 8 + i];
            
            // Transpose one column of BL of A and put into TR of B
            for (i = 4; i < 8; i++) {
                B[blk_x * 8 + j][blk_y * 8 + i] = 
                    A[blk_y * 8 + i][blk_x * 8 + j];
            }
            
            // Put the row of BL of B in its place
            i = 0;
            B[blk_x * 8 + j + 4][blk_y * 8 + i] = v1;
            i++;
            B[blk_x * 8 + j + 4][blk_y * 8 + i] = v2;
            i++;
            B[blk_x * 8 + j + 4][blk_y * 8 + i] = v3;
            i++;
            B[blk_x * 8 + j + 4][blk_y * 8 + i] = v4;
        }
        
        // Transpose BR of A and put into BR of B
        // Avoid assigning diagonal values during transpose.
        for (i = 4; i < 8; i++) {
            diag_idx = -1;
            for (j = 4; j < 8; j++) {
                if (blk_y * 8 + i == blk_x * 8 + j) {
                    diag_idx = blk_y * 8 + i;
                    diag_tmp = A[blk_y * 8 + i][blk_x * 8 + j];
                    continue;
                }
                B[blk_x * 8 + j][blk_y * 8 + i] = 
                    A[blk_y * 8 + i][blk_x * 8 + j];
            }
            if (diag_idx > -1) {
                B[diag_idx][diag_idx] = diag_tmp;
            }
        }
    }
}

/**
 * Transpose function for 32x32 matrix
 */
void transpose32(int M, int N, int A[N][M], int B[M][N]) {
    // ith row of A, jth column of A, kth block
    int i, j, k;
    // block coordinates
    int blk_x;
    int blk_y;
    // temporary diagonal index and value
    int diag_tmp;
    int diag_idx;
    
    // Transpose 8x8 blocks row by row
    for (k = 0; k < (M / 8) * (N / 8); k++) {
        blk_x = k % (M / 8);
        blk_y = k / (M / 8);
        for (i = 0; i < 8; i++) {
            diag_idx = -1;
            // Avoid assigning diagonal values during transpose
            for (j = 0; j < 8; j++) {
                if (blk_x * 8 + j == blk_y * 8 + i) {
                    diag_tmp = A[blk_y * 8 + i][blk_x * 8 + j];
                    diag_idx = blk_x * 8 + j;
                    continue;
                }
                B[blk_x * 8 + j][blk_y * 8 + i] 
                    = A[blk_y * 8 + i][blk_x * 8 + j];
            }
            if (diag_idx > -1) {
                B[diag_idx][diag_idx] = diag_tmp;
            }
        }
    }
}
/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;
    return;
    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

    ENSURES(is_transpose(M, N, A, B));
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
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

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

