typedef struct {
	FloatPrecision precision;
	union {
		F32 *valuesF32;
		F64 *valuesF64;
	};
	U64 *cols;
	U64 *rows;
	// U64 *indices;   // index of first value in each row
	U64 num_values;
	U64 total_rows; // total number of rows in matrix (non-sparse)
	U64 total_cols; // total number of cols in matrix (non-sparse)
} SparseMatrix;

typedef struct {
	FloatPrecision precision;
	union {
		F32 *valuesF32;
		F64 *valuesF64;
	};
	U64 num_values; 
} Vector;

SparseMatrix *sparse_mat_alloc(Arena *arena, FloatPrecision precision, U64 num_values) {
	SparseMatrix *m = arena_alloc(arena, sizeof(SparseMatrix));
	m->precision = precision;
	m->num_values = num_values;

	if (precision == PRECISION_F32) {
		m->valuesF32 = arena_alloc(arena, num_values * sizeof(F32));
	} else {
		assert(precision == PRECISION_F64);
		m->valuesF64 = arena_alloc(arena, num_values * sizeof(F64));
	}
	return v;
}

Vector *vec_alloc(Arena *arena, FloatPrecision precision, U64 num_values) {
	Vector *v = arena_alloc(arena, sizeof(Vector));
	v->precision = precision;
	v->num_values = num_values;

	if (precision == PRECISION_F32) {
		v->valuesF32 = arena_alloc(arena, num_values * sizeof(F32));
	} else {
		assert(precision == PRECISION_F64);
		v->valuesF64 = arena_alloc(arena, num_values * sizeof(F64));
	}
	return v;
}

Vector vec_add(Vector a, Vector b) {

}

Vector vec_sub(Vector a, Vector b) {

}

F64 vec_dot(Vector a, Vector b) {
	assert(a.precision == b.precision);
	assert(a.num_values == b.num_values);

	F64 result = 0;
	if (a.precision == PRECISION_F32) {
		for (U64 i=0; i<a.num_values; ++i) {
			result += (F64)a.valuesF32[i] * (F64)b.valuesF32[i];
		}
	} else {
		for (U64 i=0; i<a.num_values; ++i) {
			result += a.valuesF64[i] * b.valuesF64[i];
		}
	}
	return result;
}

Vector vec_scale(Arena *arena, Vector v, F64 scalar) {
	
}



Vector sparse_mat_mul_vec(SparseMatrix m, Vector v) {
	assert(m.precision == v.precision);
	assert(m.total_cols == v.num_values);

	Vector result = {0};
	result.precision = v.precision;
	result.num_values = v.num_values;

	if (result.precision == PRECISION_F32) {
		result.valuesF32 = xmalloc(result.num_values * sizeof(*result.valuesF32));
		for (U64 i=0; i<m.num_values; ++i) {
			U64 row = m.rows[i];
			U64 v_index = m.cols[i];
			result.valuesF32[row] += v.valuesF32[v_index] * m.valuesF32[i];
		}
	} else {
		assert(result.precision == PRECISION_F64);
		result.valuesF64 = xmalloc(result.num_values * sizeof(*result.valuesF64));
		for (U64 i=0; i<m.num_values; ++i) {
			U64 row = m.rows[i];
			U64 v_index = m.cols[i];
			result.valuesF64[row] += v.valuesF64[v_index] * m.valuesF64[i];
		}
	}

	return result;
}



