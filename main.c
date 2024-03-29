#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.c"
#include "sparse_linear_algebra.c"
#include "parse.c"
#include "solver.c"

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: %s [FILENAME]\n", argv[0]);
		exit(1);
	}
	char *filename = argv[1];

	profile_begin();

	init_scratch();
	ArenaTemp scratch = scratch_begin(NULL, 0);
	
	ParseResult parse_result = parse_input(scratch.arena, filename);

	Vector *solution = vec_alloc(scratch.arena, parse_result.vector->precision, parse_result.vector->num_values);
	if (!solve(parse_result.solver, parse_result.matrix, parse_result.vector, solution)) {
		fatal("Solver did not to converge to a solution\n");
	}

	vec_print(solution);

	scratch_end(scratch);

	profile_end();
	return 0;
}

PROFILE_TRANSLATION_UNIT_END;
