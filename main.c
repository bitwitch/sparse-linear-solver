#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.c"
#include "sparse_linear_algebra.c"
#include "parse.c"

#define ITERATIONS_BEFORE_RESIDUAL_RECOMPUTE 1000

static void sparse_vector_print(SparseVector v) {
	(void) v;
	assert(0 && "not implemented");
}

static SparseVector solve_steepest_descent(SparseMatrix A, SparseVector b) {
	(void)A; (void)b;
	SparseVector result = {0};
	assert(0 && "not implemented");
	return result;
}

static SparseVector solve_conjugate_directions(SparseMatrix A, SparseVector b) {
	(void)A; (void)b;
	SparseVector result = {0};
	assert(0 && "not implemented");
	return result;
}

static SparseVector solve_conjugate_gradients(SparseMatrix A, SparseVector b) {
	// (void)A; (void)b;
	// SparseVector result = {0};
	// assert(0 && "not implemented");

	SparseVector result = {0};

	SparseVector risidual = sparse_vec_sub(b, sparse_mat_mul_vec(A, result));
	SparseVector search_dir = residual;

	F64 delta = sparse_vec_dot(risidual, risidual);
	F64 tolerance = 0.01 * delta;

	U64 i_max = 1000;
	for (U64 i = 0; i < i_max && delta > tolerance; ++i) {
		// TODO(shaw): think about allocation strategy in here as vectors will
		// need to be allocated in this loop. scratch arena cleared each iteration?

		SparseVec q = sparse_mat_mul_vec(A, search_dir);
		F64 step_amount = delta / (sparse_vec_dot(search_dir, q));
		result = sparse_vec_add(result, sparse_vec_scale(search_dir, step_amount));
		if ((i % ITERATIONS_BEFORE_RESIDUAL_RECOMPUTE) == 0) {
			risidual = sparse_vec_sub(b, sparse_mat_mul_vec(A, x));
		} else {
			risidual = sparse_vec_sub(residual, sparse_vec_scale(q, step_amount));
		}
		F64 delta_old = delta;
		delta = sparse_vec_dot(risidual, risidual);
		F64 beta = delta_new / delta_old;
		search_dir = sparse_vec_add(residual, sparse_vec_scale(search_dir, beta));
	}

	return result;
}

static SparseVector solve(SolverKind kind, SparseMatrix A, SparseVector v) {
	switch (kind) {
		case SOLVER_STEEPEST_DESCENT:     return solve_steepest_descent(A, v);
		case SOLVER_CONJUGATE_DIRECTIONS: return solve_conjugate_directions(A, v);
		case SOLVER_CONJUGATE_GRADIENTS:  return solve_conjugate_gradients(A, v);
		default: assert(0); return (SparseVector){0};
	}
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: %s [FILENAME]\n", argv[0]);
		exit(1);
	}
	char *filename = argv[1];
	
	ParseResult parse_result = parse_input(filename);
	if (parse_result.failed) {
		fatal("Failed to parse input file: %s\n", parse_result.error);
	}

	SparseVector solution = solve(parse_result.solver, parse_result.matrix, parse_result.vector);

	// SparseMatrix matrix = {0};
	// SparseVector vector = {0};
	// SparseVector solution = solve(SOLVER_CONJUGATE_GRADIENTS, matrix, vector);

	sparse_vector_print(solution);

	return 0;
}


