//
// Created by Turox on 7/16/2018.
//

#ifndef PP_FINAL_MATRIX_H
#define PP_FINAL_MATRIX_H

/**
 * Creates a copy matrix a, determined by row and col
 * Returns an pointer to a new matrix
 * @param a
 * @param col_size
 * @param row
 * @param col
 * @return
 */
int *extract(const int *a, int col_size, int row, int col);


/**
 * Creates a copy matrix a, determined by row and col
 * Returns an pointer to a new matrix, zerofills if exceeds original matrix's capacity
 * @param a
 * @param col_size
 * @param row
 * @param col
 * @return
 */
int *extract_zf(const int *a, int offset_i, int offset_j, int row_size, int col_size, int row, int col);

/**
 * Prints an matrix
 * @param a
 * @param col_size
 * @param row
 * @param col
 */
void print(int *a, int col_size, int row, int col);

#endif //PP_FINAL_MATRIX_H
