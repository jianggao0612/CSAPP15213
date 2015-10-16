/* 
 * Gao Jiang - gaoj
 *
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

    int i, j, row_index, column_index;
    int diagonal_index = 0, diagonal_element = 0;
    
    int a0, a1, a2, a3, a4, a5, a6, a7;

    
    if (N == 32) {

        /*
         *  When N = M = 32, the variance between two rows in the matrix varies in 3 bits, which results in 8 different sets, thus a 8 * 8 sub-block is efficient
         */
        for (column_index = 0; column_index < N; column_index += 8) {

            for (row_index = 0; row_index < N; row_index += 8) {

                /*
                 * Since A and B share the same cache set address, if we don't store the values of A, then when we access B, the row of B will override the cache of A
                 * Use the 8 local variables to store each row of A and let B occupy the cache
                 */
                for(i = row_index; i < row_index + 8; i++) {

                    a0 = A[i][column_index + 0];
                    a1 = A[i][column_index + 1];
                    a2 = A[i][column_index + 2];
                    a3 = A[i][column_index + 3];
                    a4 = A[i][column_index + 4];
                    a5 = A[i][column_index + 5];
                    a6 = A[i][column_index + 6];
                    a7 = A[i][column_index + 7];

                    B[column_index + 0][i] = a0;
                    B[column_index + 1][i] = a1;
                    B[column_index + 2][i] = a2;
                    B[column_index + 3][i] = a3;
                    B[column_index + 4][i] = a4;
                    B[column_index + 5][i] = a5;
                    B[column_index + 6][i] = a6;
                    B[column_index + 7][i] = a7;
                
                }

            }
        }

    } else if (N == 64) {

        /*
         * When N = 64, the address variance between two rows in the matrix varies only in 2 bits in the set bits, 8 * 8 sub-blocks will not be efficient
         * If we only use 4 * 4 sub-blocks, we will lose the cache advantages by 4 bytes, since we have 8 blocks in each line
         * So we maintain 8 * 8 sub-blocks and do the transformation in 4 * 4 inner sub-matrix in the 8 * 8 sub blocks 
         */
        for (column_index = 0; column_index < N; column_index += 8) {

            for (row_index = 0; row_index < N; row_index += 8) {

                /*
                 * For the first 4 * 4 matrix in the upper-left, do normal transpose
                 * For the second 4 * 4 matrix in the upper-right, do transpose but store the transposed matrix still on the upper-right of B(not the intended bottom-left)
                 */
                for (i = row_index; i < row_index + 4; i++ ) {

                    a0 = A[i][column_index + 0];
                    a1 = A[i][column_index + 1];
                    a2 = A[i][column_index + 2];
                    a3 = A[i][column_index + 3];
                    a4 = A[i][column_index + 4];
                    a5 = A[i][column_index + 5];
                    a6 = A[i][column_index + 6];
                    a7 = A[i][column_index + 7];

                    B[column_index + 0][i] = a0;
                    B[column_index + 1][i] = a1;
                    B[column_index + 2][i] = a2;
                    B[column_index + 3][i] = a3;
                    B[column_index + 0][i + 4] = a4;
                    B[column_index + 1][i + 4] = a5;
                    B[column_index + 2][i + 4] = a6;
                    B[column_index + 3][i + 4] = a7;

                }

                /*
                 * Deal with the second 4 * 4 matrix and third 4 * 4 matrix together
                 * Since A has already in cache, do line access of A won't cause new misses
                 * Access B in row will reduce misses
                 */
                for (i = 0; i < 4; i++) {

                    a0 = A[row_index + 4][column_index + i];
                    a1 = A[row_index + 5][column_index + i];
                    a2 = A[row_index + 6][column_index + i];
                    a3 = A[row_index + 7][column_index + i];
                    
                    a4 = B[column_index + i][row_index + 4];
                    a5 = B[column_index + i][row_index + 5];
                    a6 = B[column_index + i][row_index + 6];
                    a7 = B[column_index + i][row_index + 7];

                    B[column_index + i][row_index + 4] = a0;
                    B[column_index + i][row_index + 5] = a1;
                    B[column_index + i][row_index + 6] = a2;
                    B[column_index + i][row_index + 7] = a3;

                    B[column_index + 4 + i][row_index + 0] = a4;
                    B[column_index + 4 + i][row_index + 1] = a5;
                    B[column_index + 4 + i][row_index + 2] = a6;
                    B[column_index + 4 + i][row_index + 3] = a7;

                }

                /*
                 * Deal with the fourth bottom-right 4 * 4 matrix
                 */
                for (i = row_index + 4; i < row_index + 8; i++) {

                    B[column_index + 4][i] = A[i][column_index + 4];
                    B[column_index + 5][i] = A[i][column_index + 5];
                    B[column_index + 6][i] = A[i][column_index + 6];
                    B[column_index + 7][i] = A[i][column_index + 7];
                }

            }
        }

    } else {

        /*
         * For rectangle matrix, after experienmental trial, larger sub-blocks works efficiently
         */
        for (column_index = 0; column_index < M; column_index += 18) {

            for (row_index = 0; row_index < N; row_index += 18) {

                for (i = row_index; (i < row_index + 18) && (i < N); i++ ) {

                    for (j = column_index; (j < column_index + 18) && (j < M); j++) {

                        /*
                         * The element in the "diagonal-ish" still doesn't need to be transposed even in a rectangle
                         * Find the diagonal element in each small block (possible "diagonal-ish" element in the complete matrix)
                         */
                        if (i != j) {

                            B[j][i] = A[i][j];

                        } else {

                            diagonal_element = A[i][j];
                            diagonal_index = i;

                        }

                    }

                    /*
                     * Put the diagnal element to matrix B in the "diagonal-ish" place
                     */
                     if (row_index == column_index) {

                        B[diagonal_index][diagonal_index] = diagonal_element;

                     }
                }
            }
        }

    }

    ENSURES(is_transpose(M, N, A, B));
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

