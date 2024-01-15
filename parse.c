typedef struct {
	char *filepath;
	int line;
} SourcePos;

typedef enum {
	TOKEN_EOF,
	// 
	// reserve space for ascii literal characters
	//
	TOKEN_INT = 128,
	TOKEN_FLOAT,
	TOKEN_NAME,
} TokenKind;

typedef struct {
	TokenKind kind;
	SourcePos pos;
    char *start, *end;
	union {
		S64 int_val;
		F64 float_val;
        char *name;
	};
} Token;

typedef struct {
	SolverKind solver;
	SparseMatrix *matrix;
	Vector *vector;
	Vector *solution;
	// bool failed;
	// char *error;
} ParseResult;

static char *stream;
static Token token;
static int current_line;

static char *keyword_format;
static char *keyword_float;
static char *keyword_double;
static char *keyword_solver;
static char *keyword_steepest_descent;
static char *keyword_conjugate_directions;
static char *keyword_conjugate_gradients;
static char *keyword_matrix;
static char *keyword_vector;
static char *keyword_solution;

static void parse_error(char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "%s:%d: parse error: ", token.pos.filepath, token.pos.line);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

static void init_keywords(void) {
	PROFILE_FUNCTION_BEGIN;
	keyword_format = str_intern("format");
	keyword_float = str_intern("float");
	keyword_double = str_intern("double");
	keyword_solver = str_intern("solver");
	keyword_steepest_descent = str_intern("steepest_descent");
	keyword_conjugate_directions = str_intern("conjugate_directions");
	keyword_conjugate_gradients = str_intern("conjugate_gradients");
	keyword_matrix = str_intern("matrix");
	keyword_vector = str_intern("vector");
	keyword_solution = str_intern("solution");
	PROFILE_FUNCTION_END;
}

static void next_token(void) {
	PROFILE_FUNCTION_BEGIN;
repeat:
	token.start = stream;
	token.pos.line = current_line;
	if (*stream == 0) { 
		token.kind = TOKEN_EOF;
		return;
	}
	switch (*stream) {
		case ' ': case '\r': case '\n': case '\t': case '\v': {
			while (isspace(*stream)) {
				if (*stream == '\n') ++current_line;
				++stream;
			}
            goto repeat;
            break;
        }

		case '-':
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9': {
			if (*stream == '-') ++stream;
            while (isdigit(*stream)) ++stream;
            char c = *stream;
            stream = token.start;
            if (c == '.' || tolower(c) == 'e') {
				token.kind = TOKEN_FLOAT;
				token.float_val = strtod(stream, &stream);
			} else {
				token.kind = TOKEN_INT;
				token.int_val = strtoll(stream, &stream, 10);
			}
            break;
		}

		case 'a': case 'b': case 'c': case 'd': case 'e':
		case 'f': case 'g': case 'h': case 'i': case 'j':
		case 'k': case 'l': case 'm': case 'n': case 'o':
		case 'p': case 'q': case 'r': case 's': case 't':
		case 'u': case 'v': case 'w': case 'x': case 'y':
		case 'z': case 'A': case 'B': case 'C': case 'D':
		case 'E': case 'F': case 'G': case 'H': case 'I':
		case 'J': case 'K': case 'L': case 'M': case 'N':
		case 'O': case 'P': case 'Q': case 'R': case 'S':
		case 'T': case 'U': case 'V': case 'W': case 'X':
		case 'Y': case 'Z': case '_': {
            while (isalnum(*stream) || *stream == '_')
                ++stream;
            token.kind = TOKEN_NAME;
            token.name = str_intern_range(token.start, stream);
			break;
		}

		default: {
			token.kind = *stream;
			++stream;
			break;
        }
	}
    token.end = stream;
	PROFILE_FUNCTION_END;
}

static void init_parse(char *source_path, char *source) {
	PROFILE_FUNCTION_BEGIN;
	init_str_intern();
	init_keywords();
	stream = source;
	token.pos.filepath = str_intern(source_path);
	current_line = 1;
	next_token();
	PROFILE_FUNCTION_END;
}

// used for error messages
static char *token_kind_to_str(TokenKind kind) {
	PROFILE_FUNCTION_BEGIN;
	char str[64] = {0};
	switch (kind) {
    case TOKEN_INT:    sprintf(str, "integer"); break;
    case TOKEN_FLOAT:  sprintf(str, "float");   break;
    case TOKEN_NAME:   sprintf(str, "name");    break;
	default:
        if (kind < 128 && isprint(kind)) {
            sprintf(str, "%c", kind);
        } else {
            sprintf(str, "<ASCII %d>", kind);
        }
        break;
    }
	PROFILE_FUNCTION_END;
    return str_intern(str);
}

static void expect_token(TokenKind kind) {
	PROFILE_FUNCTION_BEGIN;
	if (token.kind != kind) {
		parse_error("expected token %s, got %s", 
			token_kind_to_str(kind), 
			token_kind_to_str(token.kind));
	}
	next_token();
	PROFILE_FUNCTION_END;
}

static bool match_token(TokenKind kind) {
	if (token.kind == kind) {
		next_token();
		return true;
	}
	return false;
}

static bool is_token(TokenKind kind) {
	return token.kind == kind;
}

static bool is_token_name(char *name) {
	return token.kind == TOKEN_NAME && token.name == str_intern(name);
}

static void expect_keyword(char *keyword) {
	PROFILE_FUNCTION_BEGIN;
	if (token.kind != TOKEN_NAME) {
		parse_error("expected keyword '%s', got %s", keyword, token_kind_to_str(token.kind));
	}

	if (token.name != keyword) {
		parse_error("expected keyword '%s', got '%s'", keyword, token.name);
	}
	next_token();
	PROFILE_FUNCTION_END;
}

static S64 parse_int(void) {
	PROFILE_FUNCTION_BEGIN;
	if (!is_token(TOKEN_INT)) {
		parse_error("expected integer, got %s", token_kind_to_str(token.kind));
	}
	U64 number = token.int_val;
	next_token();
	PROFILE_FUNCTION_END;
	return number;
}

static F64 parse_float(void) {
	PROFILE_FUNCTION_BEGIN;
	F64 number = 0;
	if (is_token(TOKEN_INT)) {
		number = (F64)token.int_val;
	} else if (is_token(TOKEN_FLOAT)) {
		number = token.float_val;
	} else {
		parse_error("expected float, got %s", token_kind_to_str(token.kind));
	}
	next_token();
	PROFILE_FUNCTION_END;
	return number;
}

static char *parse_name(void) {
	PROFILE_FUNCTION_BEGIN;
	if (!is_token(TOKEN_NAME)) {
        parse_error("expected name, got %s", token_kind_to_str(token.kind));
	}
	char *name = token.name;
	next_token();
	PROFILE_FUNCTION_END;
	return name;
}

static FloatPrecision parse_format(void) {
	PROFILE_FUNCTION_BEGIN;
	FloatPrecision format = 0;
	expect_keyword(keyword_format);
	expect_token(':');
	char *name = parse_name();
	if (name == keyword_float) {
		format = PRECISION_F32;
	} else if (name == keyword_double) {
		format = PRECISION_F64;
	} else {
		parse_error("expected one of [float, double], got %s", name);
	}
	PROFILE_FUNCTION_END;
	return format;
}

static SolverKind parse_solver(void) {
	PROFILE_FUNCTION_BEGIN;
	SolverKind solver = 0;
	expect_keyword(keyword_solver);
	expect_token(':');
	char *name = parse_name();
	if (name == keyword_conjugate_gradients) {
		solver = SOLVER_CONJUGATE_GRADIENTS;
	} else if (name == keyword_conjugate_directions) {
		solver = SOLVER_CONJUGATE_DIRECTIONS;
	} else if (name == keyword_steepest_descent) {
		solver = SOLVER_STEEPEST_DESCENT;
	} else {
		parse_error("expected one of [conjugate_gradients, conjugate_directions, steepest_descent], got %s", name);
	}
	PROFILE_FUNCTION_END;
	return solver;
}

static SparseMatrix *parse_matrix(Arena *arena, FloatPrecision format) {
	PROFILE_FUNCTION_BEGIN;
	expect_keyword(keyword_matrix);
	expect_token(':');
	U64 num_values = parse_int();

	SparseMatrix *matrix = sparse_mat_alloc(arena, format, num_values);

	// parse matrix values
	for (U64 i=0; is_token(TOKEN_INT); ++i) {
		if (i >= num_values) {
			parse_error("expected %llu non-zero values in sparse matrix, but more were encountered", num_values);
		}

		matrix->rows[i] = parse_int();
		matrix->cols[i] = parse_int();

		if (format == PRECISION_F32) {
			matrix->valuesF32[i] = (F32)parse_float();
		} else {
			assert(format == PRECISION_F64);
			matrix->valuesF64[i] = parse_float();
		}
	}

	PROFILE_FUNCTION_END;
	return matrix;
}

static Vector *parse_vector(Arena *arena, char *keyword, FloatPrecision format) {
	PROFILE_FUNCTION_BEGIN;
	expect_keyword(keyword);
	expect_token(':');
	U64 num_values = parse_int();

	Vector *vector = vec_alloc(arena, format, num_values);

	// parse vector values
	for (U64 i=0; is_token(TOKEN_INT) || is_token(TOKEN_FLOAT); ++i) {
		if (i >= num_values) {
			parse_error("expected %llu values in vector, but more were encountered", num_values);
		}

		if (format == PRECISION_F32) {
			vector->valuesF32[i] = (F32)parse_float();
		} else {
			assert(format == PRECISION_F64);
			vector->valuesF64[i] = parse_float();
		}
	}

	PROFILE_FUNCTION_END;
	return vector;
}

static ParseResult parse_input(Arena *arena, char *file_name) {
	PROFILE_FUNCTION_BEGIN;
	ParseResult result = {0};

	char *file_data;
	U64 file_size;
	if (!read_entire_file(arena, file_name, &file_data, &file_size)) {
		fatal("Failed to read input file %s", file_name);
	}

	init_parse(file_name, file_data);

	// TODO(shaw): allow any order for parameters in input file,
	// format will always have to come before matrix and vector though

	FloatPrecision format = parse_format();
	result.solver = parse_solver();
	result.matrix = parse_matrix(arena, format);
	result.vector = parse_vector(arena, keyword_vector, format);

	// optionally parse a solution vector (useful for writing tests)
	if (is_token(TOKEN_NAME) && token.name == keyword_solution) {
		result.solution = parse_vector(arena, keyword_solution, format);
	}

	PROFILE_FUNCTION_END;
	return result;
}

