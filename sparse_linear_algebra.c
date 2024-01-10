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
		memcpy(result->valuesF64, v->valuesF64, v->num_values * sizeof(*v->valuesF64));
	}
	return result;
}

static void vec_zero(Vector *v) {
	if (v->precision == PRECISION_F32) {
		memset(v->valuesF32, 0, v->num_values * sizeof(*v->valuesF32));
	} else {
		memset(v->valuesF64, 0, v->num_values * sizeof(*v->valuesF64));
	}
}

static void vec_set(Vector *v, U64 index, F64 value) {
	if (v->precision == PRECISION_F32) {
		v->valuesF32[index] = (F32)value;
	} else {
		v->valuesF64[index] = value;
	}
}

static void vec_add(Vector *result, Vector *a, Vector *b) {
	for (U64 i=0; i < a->num_values; ++i) {
		if (a->precision == PRECISION_F32) {
			result->valuesF32[i] = a->valuesF32[i] + b->valuesF32[i];
		} else {
			result->valuesF64[i] = a->valuesF64[i] + b->valuesF64[i];
		}
	}
}

static void vec_sub(Vector *result, Vector *a, Vector *b) {
	for (U64 i=0; i < a->num_values; ++i) {
		if (a->precision == PRECISION_F32) {
			result->valuesF32[i] = a->valuesF32[i] - b->valuesF32[i];
		} else {
			result->valuesF64[i] = a->valuesF64[i] - b->valuesF64[i];
		}
	}
}

static F64 vec_dot(Vector *a, Vector *b) {
	assert(a->precision == b->precision);
	assert(a->num_values == b->num_values);

	F64 result = 0;
	for (U64 i=0; i < a->num_values; ++i) {
		if (a->precision == PRECISION_F32) {
			result += (F64)a->valuesF32[i] * (F64)b->valuesF32[i];
		} else {
			result += a->valuesF64[i] * b->valuesF64[i];
		}
	}

	return result;
}

static void vec_scale(Vector *result, Vector *v, F64 scalar) {
	for (U64 i=0; i < v->num_values; ++i) {
		if (v->precision == PRECISION_F32) {
			result->valuesF32[i] = v->valuesF32[i] * (F32)scalar;
		} else {
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

// TODO(shaw): handle case where v and result alias
// since you can't reliably know when this is true, will have to always copy
// result into a tmp vector, perform computations then copy data back to result
static void sparse_mat_mul_vec(Vector *result, SparseMatrix *m, Vector *v) {
	assert(m->precision == v->precision);
	assert(m->precision == result->precision);
	assert(v->num_values == result->num_values);

	vec_zero(result);

	for (U64 i=0; i<m->num_values; ++i) {
		U64 row = m->rows[i];
		U64 v_index = m->cols[i];

		assert(v_index < v->num_values);
		assert(row < result->num_values);

		if (result->precision == PRECISION_F32) {
			result->valuesF32[row] += v->valuesF32[v_index] * m->valuesF32[i];
		} else {
			result->valuesF64[row] += v->valuesF64[v_index] * m->valuesF64[i];
		}
	}
}


