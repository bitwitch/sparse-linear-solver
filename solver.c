#define MAX_ITERATIONS 1000
#define ITERATIONS_BEFORE_RESIDUAL_RECOMPUTE 50
#define TOLERANCE 0.000001

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

// see: https://www.cs.cmu.edu/~quake-papers/painless-conjugate-gradient.pdf
// page 50 for algorithm reference
//
// result and b must be distinct vectors
static bool solve_conjugate_gradients(SparseMatrix *A, Vector *b, Vector *result) {
	PROFILE_FUNCTION_BEGIN;
	FloatPrecision precision = result->precision;
	U64 vec_size = b->num_values;

	ArenaTemp scratch = scratch_begin(NULL, 0);

	// initial guess for solution, start at zero
	vec_zero(result);

	Vector *residual = vec_alloc(scratch.arena, precision, vec_size);
	sparse_mat_mul_vec(residual, A, result);
	vec_sub(residual, b, residual);

	Vector *search_dir = vec_copy(scratch.arena, residual);
	F64 delta = vec_dot(residual, residual);

	U64 pos = arena_pos(scratch.arena);

	U64 i;
	for (i = 0; i < MAX_ITERATIONS && delta > TOLERANCE; ++i) {
		Vector *q = vec_alloc(scratch.arena, precision, vec_size);
		sparse_mat_mul_vec(q, A, search_dir);

		F64 step_amount = delta / (vec_dot(search_dir, q));

		Vector *tmp = vec_alloc(scratch.arena, precision, vec_size);
		
		vec_scale(tmp, search_dir, step_amount);
		vec_add(result, result, tmp);

		if (((i+1) % ITERATIONS_BEFORE_RESIDUAL_RECOMPUTE) == 0) {
			sparse_mat_mul_vec(tmp, A, result);
			vec_sub(residual, b, tmp);
		} else {
			vec_scale(tmp, q, step_amount);
			vec_sub(residual, residual, tmp);
		}

		F64 delta_old = delta;
		delta = vec_dot(residual, residual);
		F64 beta = delta / delta_old;

		vec_scale(tmp, search_dir, beta);
		vec_add(search_dir, residual, tmp);

		arena_pop_to(scratch.arena, pos);
	}

	scratch_end(scratch);

#ifdef DIAGNOSTICS
	printf("Solver Diagnostics:\n");
	printf("\t%llu iterations\n", i);
	if (delta <= TOLERANCE) {
		printf("\tconverged\n");
	} else {
		printf("did not converge\n");
	}
#endif

	PROFILE_FUNCTION_END;
	return delta <= TOLERANCE;
}

// executes the solver specified by kind and places the solution into result 
// result and b must be distinct vectors
static bool solve(SolverKind kind, SparseMatrix *A, Vector *v, Vector *result) {
	PROFILE_FUNCTION_BEGIN;
	bool success = false;
	switch (kind) {
		case SOLVER_STEEPEST_DESCENT:     success = solve_steepest_descent(A, v, result);     break;
		case SOLVER_CONJUGATE_DIRECTIONS: success = solve_conjugate_directions(A, v, result); break;
		case SOLVER_CONJUGATE_GRADIENTS:  success = solve_conjugate_gradients(A, v, result);  break;
		default:
			fatal("solve: unknown solver kind (enum value = %d)", kind);
			break;
	}
	PROFILE_FUNCTION_END;
	return success;
}


