/* 
 * Gao Jiang - goaj
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
    int diagonal_element = 0;
    int diagonal_index = 0;

    /*
     * deal with square-matrix
     */
    if (N == 32) {

        for (column_index = 0; column_index < N; column_index += 8) {

            for (row_index = 0; row_index < N; row_index += 8) {

                for (i = 0; i < row_index + 8; i++) {

                    for (j = 0; j < column_index + 8; j++) {

                        /*
                         * The element in the diagonal doesn't need to be transposed
                         * Find the diagonal element in each small block (possible diagonal element in the complete matrix)
                         */
                         if (i != j) {

                            B[j][i] = A[i][j];

                         } else {

                            diagonal_element = A[i][j]; // get the diagonal element
                            diagonal_index = i; // get the diagonal index

                         }

                    }

                    /*
                     * Determine whether the diagonal element in the small block is in the complete matrix
                     */
                    if (row_index == column_index) {

                        B[diagonal_index][diagonal_index] = diagonal_element;

                    }

                }
            }
        }

    } else if (N == 64) {

        for (column_index = 0; column_index < N; column_index += 4) {

            for (row_index = 0; row_index < N; row_index += 4) {

                for (i = 0; i < row_index + 4; i++ ) {

                    for (j = 0; j < column_index + 4; j++) {

                        if (i != j) {

                            B[j][i] = A[i][j];

                        } else {

                            diagonal_element = A[i][j];
                            diagonal_index = i;

                        }

                    }

                     if (row_index == column_index) {

                        B[diagonal_index][diagonal_index] = diagonal_element;

                     }
                }
            }
        }

    } else {


        for (column_index = 0; column_index < M; column_index += 18) {

            for (row_index = 0; row_index < N; row_index += 18) {

                for (i = 0; (i < row_index + 18) && (i < N); i++ ) {

                    for (j = 0; (j < column_index + 18) && (j < M); j++) {

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

