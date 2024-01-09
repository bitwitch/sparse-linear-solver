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

#pragma warning (push, 0)
#include <windows.h>
#pragma warning (pop)

#include "common.c"
#include "sparse_linear_algebra.c"
#include "parse.c"

#define ITERATIONS_BEFORE_RESIDUAL_RECOMPUTE 1000

static void vec_print(Vector *v) {
	(void) v;
	assert(0 && "not implemented");
}

static bool solve_steepest_descent(SparseMatrix *A, Vector *b, Vector *result) {
	(void)A; (void)b; (void)result;
	assert(0 && "not implemented");
	return false;
}

static bool solve_conjugate_directions(SparseMatrix *A, Vector *b, Vector *result) {
	(void)A; (void)b; (void)result;
	assert(0 && "not implemented");
	return false;
}

// static SparseVector solve_conjugate_gradients(SparseMatrix A, SparseVector b) {
	// // 1
	// SparseVector result = {0};

	// // 2, 3 
	// SparseVector risidual = sparse_vec_sub(b, sparse_mat_mul_vec(A, result));

	// // 4
	// SparseVector search_dir = residual;

	// F64 delta = sparse_vec_dot(risidual, risidual);
	// F64 tolerance = 0.01 * delta;

	// U64 i_max = 1000;
	// for (U64 i = 0; i < i_max && delta > tolerance; ++i) {
		// // TODO(shaw): think about allocation strategy in here as vectors will
		// // need to be allocated in this loop. scratch arena cleared each iteration?

		// // 5
		// SparseVec q = sparse_mat_mul_vec(A, search_dir);
		// F64 step_amount = delta / (sparse_vec_dot(search_dir, q));
		// // 6, 7
		// result = sparse_vec_add(result, sparse_vec_scale(search_dir, step_amount));
		// // 8, 9
		// if ((i % ITERATIONS_BEFORE_RESIDUAL_RECOMPUTE) == 0) {
			// risidual = sparse_vec_sub(b, sparse_mat_mul_vec(A, x));
		// } else {
			// risidual = sparse_vec_sub(residual, sparse_vec_scale(q, step_amount));
		// }
		// F64 delta_old = delta;
		// delta = sparse_vec_dot(risidual, risidual);
		// F64 beta = delta_new / delta_old;
		// // 10, 11
		// search_dir = sparse_vec_add(residual, sparse_vec_scale(search_dir, beta));
	// }

	// return result;
// }

static bool solve_conjugate_gradients(SparseMatrix *A, Vector *b, Vector *result) {
	(void)A; (void)b; (void)result;
	assert(0 && "not implemented");
	return false;

	// U64 vec_size = b.num_values;

	// Arena scratch = arena_make_scratch();

	// // r = b - A * x
	// Vector residual = vec_alloc(scratch, vec_size);
	// sparse_mat_mul_vec(&residual, A, result);
	// vec_sub(&residual, b, residual);

	// Vector search_dir = vec_copy(scratch, residual);
	// F64 delta = vec_dot(residual, residual);
	// F64 tolerance = 0.01 * delta;

	// U64 pos = arena_pos(scratch);

	// U64 i_max = 1000;
	// for (U64 i = 0; i < i_max && delta > tolerance; ++i) {
		// // q = A * search_dir
		// Vector q = vec_alloc(scratch, vec_size);
		// sparse_mat_mul_vec(&q, A, search_dir);

		// F64 step_amount = delta / (vec_dot(search_dir, q));

		// Vector tmp = vec_alloc(scratch, vec_size);
		
		// // result = result + step_amount * search_dir
		// vec_scale(&tmp, search_dir, step_amount);
		// vec_add(&result, result, tmp);

		// if ((i % ITERATIONS_BEFORE_RESIDUAL_RECOMPUTE) == 0) {
			// // r = b - A * x
			// sparse_mat_mul_vec(&tmp, A, result);
			// vec_sub(&residual, b, tmp);

		// } else {
			// // r = r - step_amount * q
			// vec_scale(&tmp, q, step_amount);
			// vec_sub(&residual, residual, tmp);
		// }

		// F64 delta_old = delta;
		// delta = vec_dot(risidual, risidual);
		// F64 beta = delta_new / delta_old;

		// // search_dir = residual + beta * search_dir
		// sparse_vec_scale(&tmp, search_dir, beta);
		// vec_add(&search_dir, residual, tmp);

		// arena_pop_to(scratch, pos);
	// }
	// arena_clear(scratch);

	// return result;
}

bool solve(SolverKind kind, SparseMatrix *A, Vector *v, Vector *result) {
	switch (kind) {
		case SOLVER_STEEPEST_DESCENT:     return solve_steepest_descent(A, v, result);
		case SOLVER_CONJUGATE_DIRECTIONS: return solve_conjugate_directions(A, v, result);
		case SOLVER_CONJUGATE_GRADIENTS:  return solve_conjugate_gradients(A, v, result);
		default: assert(0); return false;
	}
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: %s [FILENAME]\n", argv[0]);
		exit(1);
	}
	char *filename = argv[1];

	Arena *arena = arena_alloc();
	
	ParseResult parse_result = parse_input(arena, filename);
	if (parse_result.failed) {
		fatal("Failed to parse input file: %s\n", parse_result.error);
	}

	Vector *solution = vec_alloc(arena, parse_result.vector->precision, parse_result.vector->num_values);
	(void)solution;

	// if (!solve(parse_result.solver, parse_result.matrix, parse_result.vector, solution)) {
		// fatal("Solver failed\n");
	// }

	// vec_print(solution);

	return 0;
}


