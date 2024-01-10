#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define ARRAY_COUNT(a) sizeof(a)/sizeof(*(a))
#define IS_POW2(x) (((x) != 0) && ((x) & ((x)-1)) == 0)

#define ALIGN_DOWN(n, a) ((n) & ~((a) - 1))
#define ALIGN_UP(n, a) ALIGN_DOWN((n) + (a) - 1, (a))
#define ALIGN_DOWN_PTR(p, a) ((void *)ALIGN_DOWN((uintptr_t)(p), (a)))
#define ALIGN_UP_PTR(p, a) ((void *)ALIGN_UP((uintptr_t)(p), (a)))

#define KILOBYTE (1024)
#define MEGABYTE (1024 * KILOBYTE)
#define GIGABYTE (1024 * MEGABYTE)

typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
typedef int8_t  S8;
typedef int16_t S16;
typedef int32_t S32;
typedef int64_t S64;
typedef float F32;
typedef double F64;

typedef enum {
	PRECISION_NONE,
	PRECISION_F32,
	PRECISION_F64,
} FloatPrecision;

typedef enum {
	SOLVER_NONE,
	SOLVER_STEEPEST_DESCENT,
	SOLVER_CONJUGATE_DIRECTIONS,
	SOLVER_CONJUGATE_GRADIENTS,
} SolverKind;

// ---------------------------------------------------------------------------
// Helper Utilities
// ---------------------------------------------------------------------------
void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        perror("malloc");
        exit(1);
    }
    return ptr;
}

void *xcalloc(size_t num_items, size_t item_size) {
    void *ptr = calloc(num_items, item_size);
    if (ptr == NULL) {
        perror("calloc");
        exit(1);
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    void *result = realloc(ptr, size);
    if (result == NULL) {
        perror("recalloc");
        exit(1);
    }
    return result;
}

void fatal(char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("FATAL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    exit(1);
}

// ---------------------------------------------------------------------------
// Stretchy Buffers, a la sean barrett
// ---------------------------------------------------------------------------

typedef struct {
	size_t len;
	size_t cap;
	char buf[]; // flexible array member
} BUF_Header;


// get the metadata of the array which is stored before the actual buffer in memory
#define buf__header(b) ((BUF_Header*)((char*)b - offsetof(BUF_Header, buf)))
// checks if n new elements will fit in the array
#define buf__fits(b, n) (buf_lenu(b) + (n) <= buf_cap(b)) 
// if n new elements will not fit in the array, grow the array by reallocating 
#define buf__fit(b, n) (buf__fits(b, n) ? 0 : ((b) = buf__grow((b), buf_lenu(b) + (n), sizeof(*(b)))))

#define BUF(x) x // annotates that x is a stretchy buffer
#define buf_len(b)  ((b) ? (int32_t)buf__header(b)->len : 0)
#define buf_lenu(b) ((b) ?          buf__header(b)->len : 0)
#define buf_set_len(b, l) buf__header(b)->len = (l)
#define buf_cap(b) ((b) ? buf__header(b)->cap : 0)
#define buf_end(b) ((b) + buf_lenu(b))
#define buf_push(b, ...) (buf__fit(b, 1), (b)[buf__header(b)->len++] = (__VA_ARGS__))
#define buf_free(b) ((b) ? (free(buf__header(b)), (b) = NULL) : 0)
#define buf_printf(b, ...) ((b) = buf__printf((b), __VA_ARGS__))

void *buf__grow(void *buf, size_t new_len, size_t elem_size) {
	size_t new_cap = MAX(1 + 2*buf_cap(buf), new_len);
	assert(new_len <= new_cap);
	size_t new_size = offsetof(BUF_Header, buf) + new_cap*elem_size;

	BUF_Header *new_header;
	if (buf) {
		new_header = xrealloc(buf__header(buf), new_size);
	} else {
		new_header = xmalloc(new_size);
		new_header->len = 0;
	}
	new_header->cap = new_cap;
	return new_header->buf;
}

// ---------------------------------------------------------------------------
// Arena Allocator
// ---------------------------------------------------------------------------

#define ARENA_RESERVE_SIZE (64LLU * GIGABYTE)

typedef struct {
	U64 cursor;
	U64 cap;
	U64 committed;
	U64 page_size;
} Arena;

Arena *arena_alloc(void) {
	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	U64 page_size = system_info.dwPageSize;
	
	Arena *reserved = VirtualAlloc(0, ARENA_RESERVE_SIZE, MEM_RESERVE, PAGE_NOACCESS);
	if (!reserved) return NULL;

	Arena *arena = VirtualAlloc(reserved, page_size, MEM_COMMIT, PAGE_READWRITE);
	if (!arena) {
		VirtualFree(reserved, 0, MEM_RELEASE);
		return NULL;
	}

	arena->cursor = sizeof(Arena);
	arena->cap = ARENA_RESERVE_SIZE;
	arena->committed = page_size;
	arena->page_size = page_size;

	return arena;
}

U64 arena_pos(Arena *arena) {
	return arena->cursor;
}

void arena_pop_to(Arena *arena, U64 pos) {
	U64 to_decommit = arena->committed - MAX(pos, arena->page_size);
	VirtualFree((U8*)arena + arena->page_size, to_decommit, MEM_DECOMMIT);
	arena->committed -= to_decommit;
	arena->cursor = MAX(pos, sizeof(Arena));
}

void arena_clear(Arena *arena) {
	U64 to_decommit = arena->committed - arena->page_size;
	VirtualFree((U8*)arena + arena->page_size, to_decommit, MEM_DECOMMIT);
	arena->committed -= to_decommit;
	arena->cursor = sizeof(Arena);
}

void arena_release(Arena *arena) {
	if (arena) {
		VirtualFree(arena, 0, MEM_RELEASE);
	}
}

void *arena_push(Arena *arena, U64 size, U64 alignment, bool zero) {

	void *start = (U8*)arena + arena->cursor;
	void *start_aligned = ALIGN_UP_PTR(start, alignment);
	U64 pad_bytes = (U64)start_aligned - (U64)start;
	size += pad_bytes;

	// commit more memory if needed
	if (arena->cursor + size >= arena->committed) {
		U64 to_commit = ALIGN_UP(arena->cursor + size, arena->page_size);
		void *ok = VirtualAlloc(arena, to_commit, MEM_COMMIT, PAGE_READWRITE);
		assert(ok);
		arena->committed = to_commit;
	}

	if (zero) {
		memset((U8*)arena + arena->cursor, 0, size);
	}

	arena->cursor += size;
	return start_aligned;
}
#define arena_push_n(arena, type, count) (type*)(arena_push(arena, sizeof(type)*count, _Alignof(type), true))
#define arena_push_n_no_zero(arena, type, count) (type*)(arena_push(arena, sizeof(type)*count, _Alignof(type), false))

// ---------------------------------------------------------------------------
// Hash Map
// ---------------------------------------------------------------------------

typedef struct {
	void **keys;
	void **vals;
	size_t len;
	size_t cap;
} Map;

uint64_t uint64_hash(uint64_t x) {
	x ^= (x * 0xff51afd7ed558ccdull) >> 32;
	return x;
}

uint64_t ptr_hash(void *ptr) {
	return uint64_hash((uintptr_t)ptr);
}

uint64_t str_hash_range(char *start, char *end) {
	uint64_t fnv_init = 0xcbf29ce484222325ull;
	uint64_t fnv_prime = 0x00000100000001B3ull;
	uint64_t hash = fnv_init;
	while (start != end) {
		hash ^= *start++;
		hash *= fnv_prime;
		hash ^= hash >> 32; // additional mixing
	}
	return hash;
}

void *map_get(Map *map, void *key) {
	if (map->len == 0) {
		return NULL;
	}
	assert(map->len < map->cap);
	size_t i = (size_t)ptr_hash(key);

	for (;;) {
		i &= map->cap - 1; // power of two masking
		if (map->keys[i] == NULL) 
			return NULL;
		if (map->keys[i] == key)
			return map->vals[i];
		++i;
	}
}

void map_put(Map *map, void *key, void *val);

void map_grow(Map *map, size_t new_cap) {
	new_cap = MAX(16, new_cap);
	assert(IS_POW2(new_cap));
	Map new_map = {
		.keys = xcalloc(new_cap, sizeof(void*)),
		.vals = xmalloc(new_cap * sizeof(void*)),
		.cap = new_cap,
	};

	for (size_t i = 0; i < map->cap; ++i) {
		if (map->keys[i]) {
			map_put(&new_map, map->keys[i], map->vals[i]);
		}
	}

	free(map->keys);
	free(map->vals);

	*map = new_map;
}

void map_put(Map *map, void *key, void *val) {
	assert(key && val);
	// TODO(shaw): currently enforcing less than 50% capacity, tweak this to be
	// less extreme/conservative
	if (2*map->len >= map->cap) {
		map_grow(map, 2*map->cap);
	}
	assert(2*map->len < map->cap); 

	size_t i = (size_t)ptr_hash(key);

	for (;;) {
		i &= map->cap - 1;
		if (map->keys[i] == NULL) { 
			map->keys[i] = key;
			map->vals[i] = val;
			++map->len;
			return;
		}
		if (map->keys[i] == key) {
			map->vals[i] = val;
			return;
		}
		++i;
	}

}

void map_clear(Map *map) {
	free(map->keys);
	free(map->vals);
	memset(map, 0, sizeof(*map));
}

void map_test(void) {
	Map map = {0};
	enum { N = 1024 * 1024 };
	for (size_t i=0; i<N; ++i) {
		map_put(&map, (void*)(i+1), (void*)(i+2));
	}
	for (size_t i=0; i<N; ++i) {
		assert(map_get(&map, (void*)(i+1)) == (void*)(i+2));
	}
}


// ---------------------------------------------------------------------------
// String Interning
// ---------------------------------------------------------------------------
typedef struct InternStr InternStr;
struct InternStr {
    size_t len;
	InternStr *next;
    char str[];
};

static Arena *intern_arena;
static Map interns;

void init_str_intern(void) {
	static bool first = true;
	if (first) {
		intern_arena = arena_alloc();
		first = false;
	}
}

char *str_intern_range(char *start, char *end) {
	size_t len = end - start;
	uint64_t hash = str_hash_range(start, end);
	void *key = (void*)(uintptr_t)(hash ? hash : 1);

	// check if string is already interned
	InternStr *intern = map_get(&interns, key);
	// NOTE: in almost all cases this loop should only execute a single time
	for (InternStr *it = intern; it; it = it->next) {
		if (it->len == len && strncmp(it->str, start, len) == 0) {
			return it->str;
		} 
	}

	InternStr *new_intern = arena_push(intern_arena, offsetof(InternStr, str) + len + 1, 1, true);
	new_intern->len = len;
	new_intern->next = intern;
	memcpy(new_intern->str, start, len);
	new_intern->str[len] = 0;
	map_put(&interns, key, new_intern);
	return new_intern->str;
}

char *str_intern(char *str) {
    return str_intern_range(str, str + strlen(str));
}


// ---------------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------------
bool read_entire_file(Arena *arena, char *file_path, char **file_data, U64 *out_size) {
	FILE *f = fopen(file_path, "rb");
	if (!f) {
		return false;
	}

	struct __stat64 stat = {0};
	_stat64(file_path, &stat);
	U64 file_size = stat.st_size;

	*out_size = file_size + 1;
	*file_data = arena_push_n(arena, char, *out_size);
	if (!*file_data) {
		fclose(f);
		return false;
	}

	U64 bytes_read = fread(*file_data, 1, file_size, f);
	if (bytes_read < file_size && !feof(f)) {
		fclose(f);
		return false;
	}

	(*file_data)[bytes_read] = 0; // add null terminator
	fclose(f);

	return true;
}

