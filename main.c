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
#include "parse.c"

#define ITERATIONS_BEFORE_RESIDUAL_RECOMPUTE 1000

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

static bool solve_conjugate_gradients(SparseMatrix *A, Vector *b, Vector *result) {
	FloatPrecision precision = result->precision;
	U64 vec_size = b->num_values;

	Arena *scratch = arena_alloc();

	vec_zero(result);

	// residual = b - A * x
	Vector *residual = vec_alloc(scratch, precision, vec_size);
	sparse_mat_mul_vec(residual, A, result);
	vec_sub(residual, b, residual);

	Vector *search_dir = vec_copy(scratch, residual);
	F64 delta = vec_dot(residual, residual);
	F64 tolerance = 0.0001 * delta;

	U64 pos = arena_pos(scratch);

	U64 i_max = 1000;
	U64 i = 0;
	for (; i < i_max && delta > tolerance; ++i) {
		// q = A * search_dir
		Vector *q = vec_alloc(scratch, precision, vec_size);
		sparse_mat_mul_vec(q, A, search_dir);

		F64 step_amount = delta / (vec_dot(search_dir, q));

		Vector *tmp = vec_alloc(scratch, precision, vec_size);
		
		// result = result + step_amount * search_dir
		vec_scale(tmp, search_dir, step_amount);
		vec_add(result, result, tmp);

		if (((i+1) % ITERATIONS_BEFORE_RESIDUAL_RECOMPUTE) == 0) {
			// r = b - A * x
			sparse_mat_mul_vec(tmp, A, result);
			vec_sub(residual, b, tmp);

		} else {
			// residual = residual - step_amount * q
			vec_scale(tmp, q, step_amount);
			vec_sub(residual, residual, tmp);
		}

		F64 delta_old = delta;
		delta = vec_dot(residual, residual);
		F64 beta = delta / delta_old;

		// search_dir = residual + beta * search_dir
		vec_scale(tmp, search_dir, beta);
		vec_add(search_dir, residual, tmp);

		printf("iteration %llu: ", i);
		vec_print(result);

		arena_pop_to(scratch, pos);
	}
	arena_release(scratch);

	printf("Solver Diagnostics:\n");
	printf("\t%llu iterations\n", i);
	if (delta <= tolerance) {
		printf("\tconverged\n");
	} else {
		printf("did not converge\n");
	}

	return result;
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

	if (!solve(parse_result.solver, parse_result.matrix, parse_result.vector, solution)) {
		fatal("Solver failed\n");
	}

	vec_print(solution);

	return 0;
}


