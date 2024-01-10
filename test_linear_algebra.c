#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#pragma warning (push, 0)
#include <windows.h>
#pragma warning (pop)

#include "common.c"
#include "sparse_linear_algebra.c"

// see https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
bool F32_equal(F32 a, F32 b, F32 max_diff) {
    F32 diff = fabsf(a - b);
    if (diff <= max_diff) return true;

    a = fabsf(a);
    b = fabsf(b);
    float largest = (b > a) ? b : a;

    return (diff <= largest * FLT_EPSILON);
}

bool F64_equal(F64 a, F64 b, F64 max_diff) {
    // Check if the numbers are really close -- needed
    // when comparing numbers near zero.
    F64 diff = fabs(a - b);
    if (diff <= max_diff) return true;

    a = fabs(a);
    b = fabs(b);
    F64 largest = (b > a) ? b : a;

    return (diff <= largest * DBL_EPSILON);
}

static bool vec_equal(Vector *a, Vector *b) {
	if (a->precision != b->precision)   return false;	
	if (a->num_values != b->num_values) return false;	

	for (U64 i=0; i < a->num_values; ++i) {
		if (a->precision == PRECISION_F32) {
			if (!F32_equal(a->valuesF32[i], b->valuesF32[i], 0.000001f))
				return false;
		} else {
			if (!F64_equal(a->valuesF64[i], b->valuesF64[i], 0.000000001))
				return false;
		}
	}

	return true;
}

// TODO(shaw): more tests, this is a simple sanity test, not extensive or
// exhaustive at all
static void test_linear_algebra(void) {
	Arena *scratch = arena_alloc();

	SparseMatrix *mat_9 = sparse_mat_alloc(scratch, PRECISION_F32, 9);
	Vector *a = vec_alloc(scratch, PRECISION_F32, 3);
	Vector *b = vec_alloc(scratch, PRECISION_F32, 3);
	Vector *actual = vec_alloc(scratch, PRECISION_F32, 3);
	Vector *expected = vec_alloc(scratch, PRECISION_F32, 3);
	F32 scalar;
	F32 epsilon = 0.000001f;

	// test vec_add
	vec_set(a, 0, 1);
	vec_set(a, 1, 2);
	vec_set(a, 2, 3);
	vec_set(b, 0, 4);
	vec_set(b, 1, 5);
	vec_set(b, 2, 6);
	vec_add(actual, a, b);
	vec_set(expected, 0, 5);
	vec_set(expected, 1, 7);
	vec_set(expected, 2, 9);
	assert(vec_equal(actual, expected));

	vec_set(a, 0, 0);
	vec_set(a, 1, 0);
	vec_set(a, 2, 0);
	vec_set(b, 0, 1);
	vec_set(b, 1, 1);
	vec_set(b, 2, 1);
	vec_add(actual, a, b);
	vec_set(expected, 0, 1);
	vec_set(expected, 1, 1);
	vec_set(expected, 2, 1);
	assert(vec_equal(actual, expected));

	vec_set(a, 0, 1919.4848);
	vec_set(a, 1, 1234.5678);
	vec_set(a, 2, 666.666);
	vec_set(b, 0, 1814.0);
	vec_set(b, 1, 619234.0983);
	vec_set(b, 2, 0.999);
	vec_add(actual, a, b);
	vec_set(expected, 0, 3733.4848);
	vec_set(expected, 1, 620468.666);
	vec_set(expected, 2, 667.665);
	assert(vec_equal(actual, expected));

	// test vec_sub
	vec_set(a, 0, 0.25);
	vec_set(a, 1, 0.5);
	vec_set(a, 2, 0.75);
	vec_set(b, 0, 3.3);
	vec_set(b, 1, 4.4);
	vec_set(b, 2, 5.5);
	vec_sub(actual, a, b);
	vec_set(expected, 0, -3.05);
	vec_set(expected, 1, -3.9);
	vec_set(expected, 2, -4.75);
	assert(vec_equal(actual, expected));

	vec_set(a, 0, 895.3);
	vec_set(a, 1, -69.23);
	vec_set(a, 2, 222.22);
	vec_set(b, 0, -4890.02);
	vec_set(b, 1, -7900.89);
	vec_set(b, 2, 0.4);
	vec_sub(actual, a, b);
	vec_set(expected, 0, 5785.32);
	vec_set(expected, 1, 7831.66);
	vec_set(expected, 2, 221.82);
	assert(vec_equal(actual, expected));

	// test vec_scale
	vec_set(a, 0, 1);
	vec_set(a, 1, 2);
	vec_set(a, 2, 3);
	vec_scale(actual, a, 16);
	vec_set(expected, 0, 16);
	vec_set(expected, 1, 32);
	vec_set(expected, 2, 48);
	assert(vec_equal(actual, expected));

	vec_set(a, 0, 6);
	vec_set(a, 1, 2.4);
	vec_set(a, 2, 18);
	vec_scale(actual, a, 0.02);
	vec_set(expected, 0, 0.12);
	vec_set(expected, 1, 0.048);
	vec_set(expected, 2, 0.36);
	assert(vec_equal(actual, expected));

	// test vec_dot
	vec_set(a, 0, 3);
	vec_set(a, 1, 6);
	vec_set(a, 2, 9);
	vec_set(b, 0, 2);
	vec_set(b, 1, 5);
	vec_set(b, 2, 7);
	scalar = (F32)vec_dot(a, b);
	assert(F32_equal(scalar, 99.0f, epsilon));

	vec_set(a, 0, -2.8);
	vec_set(a, 1, 23090);
	vec_set(a, 2, 0.01234);
	vec_set(b, 0, 69.123);
	vec_set(b, 1, -88);
	vec_set(b, 2, 4.29);
	scalar = (F32)vec_dot(a, b);
	assert(F32_equal(scalar, -2032113.491f, epsilon));

	// test mat x vec
	for (U64 i=0; i<9; ++i) {
		mat_9->valuesF32[i] = (F32)(i+1);
		mat_9->rows[i] = i / 3;
		mat_9->cols[i] = i % 3;
	}
	vec_set(a, 0, 7);
	vec_set(a, 1, 5);
	vec_set(a, 2, 13);
	sparse_mat_mul_vec(actual, mat_9, a);
	vec_set(expected, 0, 56);
	vec_set(expected, 1, 131);
	vec_set(expected, 2, 206);
	assert(vec_equal(actual, expected));


	arena_release(scratch);
	printf("test_linear_algebra: success\n");
}

// static void test_conjugate_gradients(void) {
	// Arena *scratch = arena_alloc();
	// S32 num_rows = 10000;
	// U64 num_values = num_rows + (2 * (num_rows-1));
	// SparseMatrix *m = sparse_mat_alloc(scratch, PRECISION_F32, num_values);
	// for (S32 row=0; row<num_rows; ++row) {
		// S32 diag_low = row-1;
		// S32 diag_mid = row;
		// S32 diag_high = row+1;
		// if (diag_low >= 0) {
			// sparse_mat_set(m, row, diag_low, -1.0);
		// }
		// sparse_mat_set(m, row, diag_mid, 2.1);
		// if (diag_high < num_rows) {
			// sparse_mat_set(m, row, diag_high, -1.0);
		// }
	// }
	// arena_release(scratch);
// }


int main(int argc, char **argv) {
	(void)argc; (void)argv;
	test_linear_algebra();
	// test_conjugate_gradients();
	return 0;
}

