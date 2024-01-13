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
// OS Specific Functions
// ---------------------------------------------------------------------------
#if _WIN32
#pragma warning (push, 0)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#pragma warning (pop)

U64 os_timer_freq(void) {
	LARGE_INTEGER Freq;
	QueryPerformanceFrequency(&Freq);
	return Freq.QuadPart;
}

U64 os_read_timer(void) {
	LARGE_INTEGER Value;
	QueryPerformanceCounter(&Value);
	return Value.QuadPart;
}

U64 os_file_size(char *filepath) {
	struct __stat64 stat = {0};
	_stat64(filepath, &stat);
	return stat.st_size;
}

U32 os_get_page_size(void) {
	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	return system_info.dwPageSize;
}

void *os_memory_reserve(U64 size) {
	return VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS);
}

void *os_memory_commit(void *addr, U64 size) {
	return VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE);
}

bool os_memory_decommit(void *addr, U64 size) {
	return VirtualFree(addr, size, MEM_DECOMMIT);
}

bool os_memory_release(void *addr, U64 size) {
	(void) size; // unused on windows
	return VirtualFree(addr, 0, MEM_RELEASE);
}

#else
#error "This operating system is currently not supported."
#endif

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
#define ARENA_RESERVE_SIZE (8LLU * GIGABYTE)

typedef struct {
	U64 pos;
	U64 cap;
	U64 committed;
	U64 page_size;
} Arena;

typedef struct {
	Arena *arena;
	U64 pos;
} ArenaTemp;

Arena *arena_alloc(void) {
	U64 page_size = os_get_page_size();
	
	Arena *reserved = os_memory_reserve(ARENA_RESERVE_SIZE);
	if (!reserved) return NULL;

	Arena *arena = os_memory_commit(reserved, page_size);
	if (!arena) {
		os_memory_release(reserved, 0);
		return NULL;
	}

	arena->pos = sizeof(Arena);
	arena->cap = ARENA_RESERVE_SIZE;
	arena->committed = page_size;
	arena->page_size = page_size;

	return arena;
}

U64 arena_pos(Arena *arena) {
	return arena->pos;
}

void arena_pop_to(Arena *arena, U64 pos) {
	U64 pos_aligned_to_page_size = ALIGN_UP(pos, arena->page_size);
	U64 to_decommit = arena->committed - pos_aligned_to_page_size;
	if (to_decommit > 0) {
		os_memory_decommit((U8*)arena + pos_aligned_to_page_size, to_decommit);
		arena->committed -= to_decommit;
	}
	arena->pos = MAX(pos, sizeof(Arena));
}

void arena_clear(Arena *arena) {
	arena_pop_to(arena, sizeof(Arena));
}

void arena_release(Arena *arena) {
	if (arena) {
		os_memory_release(arena, 0);
	}
}

void *arena_push(Arena *arena, U64 size, U64 alignment, bool zero) {
	void *start = (U8*)arena + arena->pos;
	void *start_aligned = ALIGN_UP_PTR(start, alignment);
	U64 pad_bytes = (U64)start_aligned - (U64)start;
	size += pad_bytes;

	// commit more memory if needed
	if (arena->pos + size >= arena->committed) {
		U64 to_commit = ALIGN_UP(arena->pos + size, arena->page_size);
		void *ok = os_memory_commit(arena, to_commit);
		assert(ok);
		arena->committed = to_commit;
	}

	if (zero) {
		memset((U8*)arena + arena->pos, 0, size);
	}

	arena->pos += size;
	return start_aligned;
}
#define arena_push_n(arena, type, count) (type*)(arena_push(arena, sizeof(type)*count, _Alignof(type), true))
#define arena_push_n_no_zero(arena, type, count) (type*)(arena_push(arena, sizeof(type)*count, _Alignof(type), false))

// TODO(shaw): in a multi-threaded environment there should be thread local
// scratch arenas
static Arena *global_scratch_arenas[2];

void init_scratch(void) {
	for (U64 i=0; i<ARRAY_COUNT(global_scratch_arenas); ++i) {
		global_scratch_arenas[i] = arena_alloc();
	}
}

ArenaTemp scratch_begin(Arena **conflicts, U64 conflict_count) {
	ArenaTemp scratch = {0};
	for (U64 j=0; j<ARRAY_COUNT(global_scratch_arenas); ++j) {
		bool scratch_conflicts = false;
		for (U64 i=0; i<conflict_count; ++i) {
			if (conflicts[i] == global_scratch_arenas[j]) {
				scratch_conflicts = true;
				break;
			}	
		}
		if (!scratch_conflicts) {
			scratch.arena = global_scratch_arenas[j];
			scratch.pos = scratch.arena->pos;
			break;
		}
	}
	return scratch;
}

void scratch_end(ArenaTemp scratch) {
	arena_pop_to(scratch.arena, scratch.pos);
}

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
	enum { n = 1024 * 1024 };
	for (size_t i=0; i<n; ++i) {
		map_put(&map, (void*)(i+1), (void*)(i+2));
	}
	for (size_t i=0; i<n; ++i) {
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

	U64 file_size = os_file_size(file_path);

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

// ---------------------------------------------------------------------------
// Profiling
// ---------------------------------------------------------------------------
U64 read_cpu_timer(void) {
	return __rdtsc();
}

U64 estimate_cpu_freq(void) {
	U64 wait_time_ms = 100;
	U64 os_freq = os_timer_freq();
	U64 os_ticks_during_wait_time = os_freq * wait_time_ms / 1000;
	U64 os_elapsed = 0;
	U64 os_start = os_read_timer();
	U64 cpu_start = read_cpu_timer();

	while (os_elapsed < os_ticks_during_wait_time) {
		os_elapsed = os_read_timer() - os_start;
	}

	U64 cpu_freq = (read_cpu_timer() - cpu_start) * os_freq / os_elapsed;

	return cpu_freq;
}

typedef struct {
	char *name;
	U64 count;
	U64 ticks_exclusive; // without children
	U64 ticks_inclusive; // with children
	U64 processed_byte_count;
} ProfileBlock;

ProfileBlock profile_blocks[4096];
U64 profile_start; 
U64 current_profile_block_index;

#ifdef PROFILE

// NOTE(shaw): This macro is not guarded with typical do-while because it relies on 
// variables declared inside it being accessible in the associated PROFILE_BLOCK_END.
// This means that you cannot have nested profile blocks in the same scope.
// However, in most cases you either already have separate scopes, or you
// should trivially be able to open a new scope {}.
#define PROFILE_BLOCK_BEGIN(block_name) \
	char *__block_name = block_name; \
	U64 __block_index = __COUNTER__ + 1; \
	ProfileBlock *__block = &profile_blocks[__block_index]; \
	U64 __top_level_sum = __block->ticks_inclusive; \
	U64 __parent_index = current_profile_block_index; \
	current_profile_block_index = __block_index; \
	U64 __block_start = read_cpu_timer(); \

#define PROFILE_BLOCK_END_THROUGHPUT(byte_count) do { \
	U64 elapsed = read_cpu_timer() - __block_start; \
	__block->name = __block_name; \
	++__block->count; \
	__block->ticks_inclusive = __top_level_sum + elapsed; \
	__block->ticks_exclusive += elapsed; \
	__block->processed_byte_count += byte_count; \
	profile_blocks[__parent_index].ticks_exclusive -= elapsed; \
	current_profile_block_index = __parent_index; \
} while(0)

#define PROFILE_BLOCK_END      PROFILE_BLOCK_END_THROUGHPUT(0)
#define PROFILE_FUNCTION_BEGIN PROFILE_BLOCK_BEGIN(__func__)
#define PROFILE_FUNCTION_END   PROFILE_BLOCK_END

#define PROFILE_TRANSLATION_UNIT_END static_assert(ARRAY_COUNT(profile_blocks) > __COUNTER__, "Too many profile blocks")

#else 

#define PROFILE_BLOCK_BEGIN(...)
#define PROFILE_BLOCK_END
#define PROFILE_FUNCTION_BEGIN
#define PROFILE_FUNCTION_END
#define PROFILE_TRANSLATION_UNIT_END

#endif // PROFILE

void profile_begin(void) {
	profile_start = read_cpu_timer();
}

void profile_end(void) {
	U64 total_ticks = read_cpu_timer() - profile_start;
	assert(total_ticks);
	U64 cpu_freq = estimate_cpu_freq();
	assert(cpu_freq);
	F64 total_ms = 1000 * (total_ticks / (F64)cpu_freq);

	printf("\nTotal time: %f ms %llu ticks (cpu freq %llu)\n", total_ms, total_ticks, cpu_freq);

	for (int i=0; i<ARRAY_COUNT(profile_blocks); ++i) {
		ProfileBlock block = profile_blocks[i];
		if (!block.ticks_inclusive) continue;

		F64 pct_exclusive = 100 * (block.ticks_exclusive / (F64)total_ticks);
		printf("\t%s[%llu]: %llu (%.2f%%", block.name, block.count, block.ticks_exclusive, pct_exclusive);

		if (block.ticks_exclusive != block.ticks_inclusive) {
			F64 pct_inclusive = 100 * (block.ticks_inclusive / (F64)total_ticks);
			printf(", %.2f%% w/children", pct_inclusive);
		} 

		printf(")");

		if (block.processed_byte_count) {
			F64 megabytes = block.processed_byte_count / (F64)(1024*1024);
			F64 gigabytes_per_second = (megabytes / 1024) / (block.ticks_inclusive / (F64)cpu_freq);
			printf(" %.3fmb at %.2fgb/s", megabytes, gigabytes_per_second);
		}

		printf("\n");
	}
}



