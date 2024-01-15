static void vec_zero_no_branch(Vector *v) {
	memset(v->valuesF32, 0, v->num_values * sizeof(*v->valuesF32));
}

static Vector *vec_alloc_no_branch(Arena *arena, FloatPrecision precision, U64 num_values) {
	Vector *v = arena_push_n(arena, Vector, 1);
	v->precision = precision;
	v->num_values = num_values;

	v->valuesF32 = arena_push_n(arena, F32, num_values);

	return v;
}

static Vector *vec_copy_no_branch(Arena *arena, Vector *v) {
	Vector *result = vec_alloc_no_branch(arena, v->precision, v->num_values);
	memcpy(result->valuesF32, v->valuesF32, v->num_values * sizeof(*v->valuesF32));
	return result;
}


static void vec_sub_no_branch(Vector *result, Vector *a, Vector *b) {
	check_vector_arguments("vec_sub_no_branch", result, a, b);
	for (U64 i=0; i < a->num_values; ++i) {
		result->valuesF32[i] = a->valuesF32[i] - b->valuesF32[i];
	}
}

static void vec_add_no_branch(Vector *result, Vector *a, Vector *b) {
	check_vector_arguments("vec_add_no_branch", result, a, b);
	for (U64 i=0; i < a->num_values; ++i) {
		result->valuesF32[i] = a->valuesF32[i] + b->valuesF32[i];
	}
}

static void sparse_mat_mul_vec_no_branch(Vector *result, SparseMatrix *m, Vector *v) {
	if (m->precision != v->precision || v->precision != result->precision) {
		fatal("sparse_mat_mul_vec: arguments have different float precision");
	}
	if (v->num_values != result->num_values) {
		fatal("sparse_mat_mul_vec: vector arguments have different sizes: result=%llu, v=%llu",
			result->num_values, v->num_values);
	}

	// NOTE(shaw): result and v could alias, and since result needs to be
	// zeroed before accumulating values this means we have to allocate a
	// temporary vector to accumulate values into and then copy them out to
	// result at the end
	ArenaTemp scratch = scratch_begin(NULL, 0);
	Vector *tmp = vec_alloc_no_branch(scratch.arena, result->precision, result->num_values);

	for (U64 i=0; i<m->num_values; ++i) {
		U64 row = m->rows[i];
		U64 v_index = m->cols[i];

		assert(v_index < v->num_values);
		assert(row < result->num_values);

		tmp->valuesF32[row] += v->valuesF32[v_index] * m->valuesF32[i];
	}

	memcpy(result->valuesF32, tmp->valuesF32, sizeof(F32) * tmp->num_values);

	scratch_end(scratch);
}

static F64 vec_dot_no_branch(Vector *a, Vector *b) {
	if (a->precision != b->precision) {
		fatal("vec_dot_no_branch: vector arguments have different float precision");
	}
	if (a->num_values != b->num_values) {
		fatal("vec_dot_no_branch: vector arguments have different sizes: a=%llu, b=%llu",
			a->num_values, b->num_values);
	}

	F64 result = 0;
	for (U64 i=0; i < a->num_values; ++i) {
		result += (F64)a->valuesF32[i] * (F64)b->valuesF32[i];
	}

	return result;
}

static void vec_scale_no_branch(Vector *result, Vector *v, F64 scalar) {
	if (result->precision != v->precision) {
		fatal("vec_scale: vector arguments have different float precision");
	}
	if (result->num_values != v->num_values) {
		fatal("vec_scale: vector arguments have different sizes: result=%llu, v=%llu",
			result->num_values, v->num_values);
	}

	for (U64 i=0; i < v->num_values; ++i) {
		result->valuesF32[i] = v->valuesF32[i] * (F32)scalar;
	}
}


static bool solver_no_branch(SparseMatrix *A, Vector *b, Vector *result) {
	FloatPrecision precision = result->precision;
	U64 vec_size = b->num_values;

	ArenaTemp scratch = scratch_begin(NULL, 0);

	// initial guess for solution, start at zero
	vec_zero_no_branch(result);

	// residual = b - A * x
	Vector *residual = vec_alloc_no_branch(scratch.arena, precision, vec_size);
	sparse_mat_mul_vec_no_branch(residual, A, result);
	vec_sub_no_branch(residual, b, residual);

	Vector *search_dir = vec_copy_no_branch(scratch.arena, residual);
	F64 delta = vec_dot_no_branch(residual, residual);
	// F64 tolerance = 0.000001 * delta;
	F64 tolerance = 0.000001;

	U64 pos = arena_pos(scratch.arena);

	U64 i_max = 1000;
	U64 i = 0;
	for (; i < i_max && delta > tolerance; ++i) {
		// q = A * search_dir
		Vector *q = vec_alloc_no_branch(scratch.arena, precision, vec_size);
		sparse_mat_mul_vec_no_branch(q, A, search_dir);

		F64 step_amount = delta / (vec_dot_no_branch(search_dir, q));

		Vector *tmp = vec_alloc_no_branch(scratch.arena, precision, vec_size);
		
		// result = result + step_amount * search_dir
		vec_scale_no_branch(tmp, search_dir, step_amount);
		vec_add_no_branch(result, result, tmp);

		if (((i+1) % ITERATIONS_BEFORE_RESIDUAL_RECOMPUTE) == 0) {
			// residual = b - A * x
			sparse_mat_mul_vec_no_branch(tmp, A, result);
			vec_sub_no_branch(residual, b, tmp);

		} else {
			// residual = residual - step_amount * q
			vec_scale_no_branch(tmp, q, step_amount);
			vec_sub_no_branch(residual, residual, tmp);
		}

		F64 delta_old = delta;
		delta = vec_dot_no_branch(residual, residual);
		F64 beta = delta / delta_old;

		// search_dir = residual + beta * search_dir
		vec_scale_no_branch(tmp, search_dir, beta);
		vec_add_no_branch(search_dir, residual, tmp);

		arena_pop_to(scratch.arena, pos);
	}

	scratch_end(scratch);

	return result;
}
