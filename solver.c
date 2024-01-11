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

	ArenaTemp scratch = scratch_begin(NULL, 0);

	vec_zero(result);

	// residual = b - A * x
	Vector *residual = vec_alloc(scratch.arena, precision, vec_size);
	sparse_mat_mul_vec(residual, A, result);
	vec_sub(residual, b, residual);

	Vector *search_dir = vec_copy(scratch.arena, residual);
	F64 delta = vec_dot(residual, residual);
	// F64 tolerance = 0.000001 * delta;
	F64 tolerance = 0.000001;

	U64 pos = arena_pos(scratch.arena);

	U64 i_max = 1000;
	U64 i = 0;
	for (; i < i_max && delta > tolerance; ++i) {
		// q = A * search_dir
		Vector *q = vec_alloc(scratch.arena, precision, vec_size);
		sparse_mat_mul_vec(q, A, search_dir);

		F64 step_amount = delta / (vec_dot(search_dir, q));

		Vector *tmp = vec_alloc(scratch.arena, precision, vec_size);
		
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

		// printf("iteration %llu: ", i);
		// vec_print(result);

		arena_pop_to(scratch.arena, pos);
	}

	scratch_end(scratch);

#ifdef DIAGNOSTICS
	printf("Solver Diagnostics:\n");
	printf("\t%llu iterations\n", i);
	if (delta <= tolerance) {
		printf("\tconverged\n");
	} else {
		printf("did not converge\n");
	}
#endif

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


