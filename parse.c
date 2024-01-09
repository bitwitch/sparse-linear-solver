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
	SparseMatrix matrix;
	SparseVector vector;
	bool failed;
	char *error;
} ParseResult;

static char *stream;
static Token token;
static int current_line;

char *keyword_float;
char *keyword_double;
char *keyword_steepest_descent;
char *keyword_conjugate_directions;
char *keyword_conjugate_gradients;

void init_keywords(void) {
    static bool first = true;
    if (first) {
		keyword_float = str_intern("float");
		keyword_double = str_intern("double");
		keyword_steepest_descent = str_intern("steepest_descent");
		keyword_conjugate_directions = str_intern("conjugate_directions");
		keyword_conjugate_gradients = str_intern("conjugate_gradients");
	}
	first = false;
}

void next_token(void) {
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
}

static void init_parse(char *source_path, char *source) {
	init_keywords();
	stream = source;
	token.pos.filepath = str_intern(source_path);
	current_line = 1;
	next_token();
}

// used for error messages
static char *token_kind_to_str(TokenKind kind) {
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
    return str_intern(str);
}

static void expect_token(TokenKind kind) {
	if (token.kind != kind) {
		fatal("Expected token %s, got %s", 
			token_kind_to_str(kind), 
			token_kind_to_str(token.kind));
	}
	next_token();
}

static bool match_token(TokenKind kind) {
	if (token.kind == kind) {
		next_token();
		return true;
	}
	return false;
}

static inline bool is_token(TokenKind kind) {
	return token.kind == kind;
}

static inline bool is_token_name(char *name) {
	return token.kind == TOKEN_NAME && token.name == str_intern(name);
}

static void expect_token_name(char *name) {
	char *expected_name = str_intern(name);
	if (token.kind != TOKEN_NAME) {
		fatal("Expected token name '%s', got %s", 
			expected_name,
			token_kind_to_str(token.kind));
	}

	if (token.name != str_intern(name)) {
		fatal("Expected token name '%s', got '%s'", 
			expected_name,
			token.name);
	}

	next_token();
}

static S64 parse_int(void) {
	if (!is_token(TOKEN_INT)) {
		fatal("Expected integer, got %s", token_kind_to_str(token.kind));
	}
	U64 number = token.int_val;
	next_token();
	return number;
}

static F64 parse_float(void) {
	F64 number = 0;
	if (is_token(TOKEN_INT)) {
		number = (F64)token.int_val;
	} else if (is_token(TOKEN_FLOAT)) {
		number = token.float_val;
	} else {
		fatal("Expected float, got %s", token_kind_to_str(token.kind));
	}
	next_token();
	return number;
}

char *parse_name(void) {
	if (!is_token(TOKEN_NAME)) {
        fatal("Expected name, got %s", token_kind_to_str(token.kind));
	}
	char *name = token.name;
	next_token();
	return name;
}

static FloatPrecision parse_format(void) {
	FloatPrecision format = 0;
	expect_token_name("format");
	expect_token(':');
	char *name = parse_name();
	if (name == keyword_float) {
		format = PRECISION_F32;
	} else if (name == keyword_double) {
		format = PRECISION_F64;
	} else {
		fatal("expected one of [float, double], got %s", name);
	}
	return format;
}

static SolverKind parse_solver(void) {
	SolverKind solver = 0;
	expect_token_name("solver");
	expect_token(':');
	char *name = parse_name();
	if (name == keyword_conjugate_gradients) {
		solver = SOLVER_CONJUGATE_GRADIENTS;
	} else if (name == keyword_conjugate_directions) {
		solver = SOLVER_CONJUGATE_DIRECTIONS;
	} else if (name == keyword_steepest_descent) {
		solver = SOLVER_STEEPEST_DESCENT;
	} else {
		fatal("expected one of [conjugate_gradients, conjugate_directions, steepest_descent], got %s", name);
	}
	return solver;
}

static SparseMatrix parse_matrix(FloatPrecision format) {
	SparseMatrix matrix = {0};
	matrix.precision = format;

	expect_token_name("matrix");
	expect_token(':');

	matrix.total_rows = parse_int();
	matrix.total_cols = parse_int();

	Token save_token = token;
	char *save_stream = stream;
	int save_line = current_line;

	// skip over matrix to count number of values
	while (is_token(TOKEN_INT)) {
		next_token();
		next_token();
		next_token();
		++matrix.num_values;
	}

	// reset parsing to start of matrix 
	token = save_token;
	stream = save_stream;
	current_line = save_line;

	// allocate space for matrix
	if (format == PRECISION_F32) {
		matrix.valuesF32 = xmalloc(matrix.num_values * sizeof(*matrix.valuesF32));
	} else {
		matrix.valuesF64 = xmalloc(matrix.num_values * sizeof(*matrix.valuesF64));
	}
	matrix.cols = xmalloc(matrix.num_values * sizeof(*matrix.cols));
	matrix.rows = xmalloc(matrix.num_values * sizeof(*matrix.rows));
	matrix.indices = xmalloc(matrix.total_rows * sizeof(*matrix.indices));

	// parse matrix values
	U64 i = 0;
	if (format == PRECISION_F32) {
		while (is_token(TOKEN_INT)) {
			matrix.rows[i] = parse_int();
			matrix.cols[i] = parse_int();
			matrix.valuesF32[i] = (F32)parse_float();
			++i;
		}
	} else {
		assert(format == PRECISION_F64);
		while (is_token(TOKEN_INT)) {
			matrix.rows[i] = parse_int();
			matrix.cols[i] = parse_int();
			matrix.valuesF64[i] = parse_float();
			++i;
		}
	}

	return matrix;
}

static SparseVector parse_vector(FloatPrecision format) {
	SparseVector vector = {0};
	vector.precision = format;

	expect_token_name("vector");
	expect_token(':');

	vector.total_cols = parse_int();

	Token save_token = token;
	char *save_stream = stream;
	int save_line = current_line;

	// skip over vector to count number of values
	while (is_token(TOKEN_INT)) {
		next_token();
		next_token();
		next_token();
		++vector.num_values;
	}

	// reset parsing to start of vector
	token = save_token;
	stream = save_stream;
	current_line = save_line;

	// allocate space for vector
	// TODO(shaw): look at allocation strategy
	if (format == PRECISION_F32) {
		vector.valuesF32 = xmalloc(vector.num_values * sizeof(*vector.valuesF32));
	} else {
		vector.valuesF64 = xmalloc(vector.num_values * sizeof(*vector.valuesF64));
	}
	vector.cols = xmalloc(vector.num_values * sizeof(*vector.cols));

	// parse vector values
	U64 i = 0;
	if (format == PRECISION_F32) {
		while (is_token(TOKEN_INT)) {
			vector.cols[i] = parse_int();
			vector.valuesF32[i] = (F32)parse_float();
			++i;
		}
	} else {
		assert(format == PRECISION_F64);
		while (is_token(TOKEN_INT)) {
			vector.cols[i] = parse_int();
			vector.valuesF64[i] = parse_float();
			++i;
		}
	}

	return vector;
}

static ParseResult parse_input(char *file_name) {
	ParseResult result = {0};

	char *file_data;
	U64 file_size;
	if (!read_entire_file(file_name, &file_data, &file_size)) {
		result.failed = true;
		result.error = "Failed to read input file";
		return result;
	}

	init_parse(file_name, file_data);

	// TODO(shaw): allow any order for parameters in input file

	FloatPrecision format = parse_format();
	result.solver = parse_solver();
	result.matrix = parse_matrix(format);
	result.vector = parse_vector(format);

	return result;
}


