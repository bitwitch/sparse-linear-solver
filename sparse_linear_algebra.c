typedef struct {
	FloatPrecision precision;
	union {
		F32 *valuesF32;
		F64 *valuesF64;
	};
	U64 *cols;
	U64 *rows;
	U64 num_values;
} SparseMatrix;

typedef struct {
	FloatPrecision precision;
	union {
		F32 *valuesF32;
		F64 *valuesF64;
	};
	U64 num_values; 
} Vector;

static void vec_print(Vector *v) {
	printf("{ ");
	for (U64 i=0; i<v->num_values; ++i) {
		if (v->precision == PRECISION_F32) {
			printf("%g ", v->valuesF32[i]);
		} else {
			assert(v->precision == PRECISION_F64);
			printf("%g ", v->valuesF64[i]);
		}
	}
	printf("}\n");
}

static Vector *vec_alloc(Arena *arena, FloatPrecision precision, U64 num_values) {
	Vector *v = arena_push_n(arena, Vector, 1);
	v->precision = precision;
	v->num_values = num_values;

	if (precision == PRECISION_F32) {
		v->valuesF32 = arena_push_n(arena, F32, num_values);
	} else {
		assert(precision == PRECISION_F64);
		v->valuesF64 = arena_push_n(arena, F64, num_values);
	}
	return v;
}

static Vector *vec_copy(Arena *arena, Vector *v) {
	Vector *result = vec_alloc(arena, v->precision, v->num_values);
	if (v->precision == PRECISION_F32) {
		memcpy(result->valuesF32, v->valuesF32, v->num_values * sizeof(*v->valuesF32));
	} else {
		assert(v->precision == PRECISION_F64);
		memcpy(result->valuesF64, v->valuesF64, v->num_values * sizeof(*v->valuesF64));
	}
	return result;
}

static void vec_zero(Vector *v) {
	if (v->precision == PRECISION_F32) {
		memset(v->valuesF32, 0, v->num_values * sizeof(*v->valuesF32));
	} else {
		assert(v->precision == PRECISION_F64);
		memset(v->valuesF64, 0, v->num_values * sizeof(*v->valuesF64));
	}
}

static void vec_set(Vector *v, U64 index, F64 value) {
	if (index >= v->num_values) {
		fatal("vec_set: index (%llu) is greater than the size of the vector (%llu)", index, v->num_values);
	}

	if (v->precision == PRECISION_F32) {
		v->valuesF32[index] = (F32)value;
	} else {
		assert(v->precision == PRECISION_F64);
		v->valuesF64[index] = value;
	}
}

static void check_vector_arguments(char *prefix, Vector *result, Vector *a, Vector *b) {
	if (a->precision != b->precision || a->precision != result->precision) {
		fatal("%s: vector arguments have different float precision", prefix);
	}
	if (a->num_values != b->num_values || a->num_values != result->num_values) {
		fatal("%s: vector arguments have different sizes: result=%llu, a=%llu, b=%llu",
			prefix, result->num_values, a->num_values, b->num_values);
	}
}

static void vec_add(Vector *result, Vector *a, Vector *b) {
	check_vector_arguments("vec_add", result, a, b);
	for (U64 i=0; i < a->num_values; ++i) {
		if (a->precision == PRECISION_F32) {
			result->valuesF32[i] = a->valuesF32[i] + b->valuesF32[i];
		} else {
			assert(a->precision == PRECISION_F64);
			result->valuesF64[i] = a->valuesF64[i] + b->valuesF64[i];
		}
	}
}

static void vec_sub(Vector *result, Vector *a, Vector *b) {
	check_vector_arguments("vec_sub", result, a, b);
	for (U64 i=0; i < a->num_values; ++i) {
		if (a->precision == PRECISION_F32) {
			result->valuesF32[i] = a->valuesF32[i] - b->valuesF32[i];
		} else {
			assert(a->precision == PRECISION_F64);
			result->valuesF64[i] = a->valuesF64[i] - b->valuesF64[i];
		}
	}
}

static F64 vec_dot(Vector *a, Vector *b) {
	if (a->precision != b->precision) {
		fatal("vec_dot: vector arguments have different float precision");
	}
	if (a->num_values != b->num_values) {
		fatal("vec_dot: vector arguments have different sizes: a=%llu, b=%llu",
			a->num_values, b->num_values);
	}

	F64 result = 0;
	for (U64 i=0; i < a->num_values; ++i) {
		if (a->precision == PRECISION_F32) {
			result += (F64)a->valuesF32[i] * (F64)b->valuesF32[i];
		} else {
			assert(a->precision == PRECISION_F64);
			result += a->valuesF64[i] * b->valuesF64[i];
		}
	}

	return result;
}

static void vec_scale(Vector *result, Vector *v, F64 scalar) {
	if (result->precision != v->precision) {
		fatal("vec_scale: vector arguments have different float precision");
	}
	if (result->num_values != v->num_values) {
		fatal("vec_scale: vector arguments have different sizes: result=%llu, v=%llu",
			result->num_values, v->num_values);
	}

	for (U64 i=0; i < v->num_values; ++i) {
		if (v->precision == PRECISION_F32) {
			result->valuesF32[i] = v->valuesF32[i] * (F32)scalar;
		} else {
			assert(v->precision == PRECISION_F64);
			result->valuesF64[i] = v->valuesF64[i] * scalar;
		}
	}
}

static SparseMatrix *sparse_mat_alloc(Arena *arena, FloatPrecision precision, U64 num_values) {
	SparseMatrix *m = arena_push_n(arena, SparseMatrix, 1);
	m->precision = precision;
	m->num_values = num_values;
	m->cols = arena_push_n(arena, U64, num_values);
	m->rows = arena_push_n(arena, U64, num_values);

	if (precision == PRECISION_F32) {
		m->valuesF32 = arena_push_n(arena, F32, num_values);
	} else {
		assert(precision == PRECISION_F64);
		m->valuesF64 = arena_push_n(arena, F64, num_values);
	}

	return m;
}

static void sparse_mat_mul_vec(Vector *result, SparseMatrix *m, Vector *v) {
	if (m->precision != v->precision || v->precision != result->precision) {
		fatal("sparse_mat_mul_vec: arguments have different float precision");
	}
	if (v->num_values != result->num_values) {
		fatal("sparse_mat_mul_vec: vector arguments have different sizes: result=%llu, v=%llu",
			result->num_values, v->num_values);
	}

	// NOTE(shaw): result and v could alias and values are accumulated into
	// result and assume it starts zeroed. this means we have to allocate a
	// temporary vector to accumulate values into and then copy them out to
	// result at the end
	ArenaTemp scratch = scratch_begin(NULL, 0);
	Vector *tmp = vec_alloc(scratch.arena, result->precision, result->num_values);

	for (U64 i=0; i<m->num_values; ++i) {
		U64 row = m->rows[i];
		U64 v_index = m->cols[i];

		assert(v_index < v->num_values);
		assert(row < result->num_values);

		if (result->precision == PRECISION_F32) {
			tmp->valuesF32[row] += v->valuesF32[v_index] * m->valuesF32[i];
		} else {
			tmp->valuesF64[row] += v->valuesF64[v_index] * m->valuesF64[i];
		}
	}

	if (result->precision == PRECISION_F32) {
		memcpy(result->valuesF32, tmp->valuesF32, sizeof(F32) * tmp->num_values);
	} else {
		assert(result->precision == PRECISION_F64);
		memcpy(result->valuesF64, tmp->valuesF64, sizeof(F64) * tmp->num_values);
	}

	scratch_end(scratch);
}


