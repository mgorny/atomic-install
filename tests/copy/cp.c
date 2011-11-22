/* atomic-install -- file copying tests
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"
#include "copy.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_STDINT_H
#	include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
#	include <inttypes.h>
#endif
#ifndef PRIuMAX
#	define PRIuMAX "llu"
#endif
#ifndef PRIxMAX
#	define PRIxMAX "llx"
#endif

enum test_codes {
	T_REGULAR = 'r',
	T_EMPTY = 'e'
};

static int create_input(const char *path, int fill) {
	FILE *f = fopen(path, "wb");
	int ret = 1;

	if (!f)
		return 0;
	if (fill) {
		/* XXX */
	}

	fclose(f);
	return ret;
}

static void print_diff(const char *output_prefix, const char *msg,
		uintmax_t left, uintmax_t right)
{
	fprintf(stderr, "[%s] %s (%" PRIuMAX " vs %" PRIuMAX ")\n",
			output_prefix, msg, left, right);
}

static void print_diff_x(const char *output_prefix, const char *msg,
		uintmax_t left, uintmax_t right)
{
	fprintf(stderr, "[%s] %s (%" PRIxMAX " vs %" PRIxMAX ")\n",
			output_prefix, msg, left, right);
}


static int compare_files(const char *inp, const char *out, const char *output_prefix) {
	struct stat st_in, st_out;
	int ret = 0;

	if (lstat(inp, &st_in)) {
		perror("lstat(INPUT) failed");
		return 2;
	}
	if (lstat(out, &st_out)) {
		perror("lstat(OUTPUT) failed");
		return 2;
	}

	if (st_in.st_size != st_out.st_size) {
		print_diff(output_prefix, "Size differs",
				st_in.st_size, st_out.st_size);
		ret = 1;
	}

	if (st_in.st_size > 0) {
		/* XXX */
		ret = 77;
	}

	if (st_in.st_mode != st_out.st_mode) {
		print_diff_x(output_prefix, "Mode differs",
				st_in.st_mode, st_out.st_mode);
		ret = 1;
	}

	if (st_in.st_uid != st_out.st_uid) {
		print_diff(output_prefix, "UID differs",
				st_in.st_uid, st_out.st_uid);
		ret = 1;
	}

	if (st_in.st_gid != st_out.st_gid) {
		print_diff(output_prefix, "GID differs",
				st_in.st_gid, st_out.st_gid);
		ret = 1;
	}

	if (st_in.st_mtime != st_out.st_mtime) {
		print_diff(output_prefix, "mtime (in seconds) differs",
				st_in.st_mtime, st_out.st_mtime);
		ret = 1;
	}

	return ret;
}

int main(int argc, char *argv[]) {
	const char *code = argv[1];
	const char *slash = strrchr(code, '/');

	int ret;

	/* stupid automake! */
	if (slash)
		code = slash + 1;

	switch (code[0]) {
		case T_REGULAR:
			return 77;
		case T_EMPTY:
			if (!create_input(INPUT_FILE, 0)) {
				perror("Input creation failed");
				return 2;
			}
			break;
		default:
			fprintf(stderr, "Invalid arg: [%s]\n", code);
			return 3;
	}

	ret = ai_cp_a(INPUT_FILE, OUTPUT_FILE);
	if (ret) {
		fprintf(stderr, "[%s] Copying failed: %s\n",
				code, strerror(errno));
		return 1;
	}

	return compare_files(INPUT_FILE, OUTPUT_FILE, code);
}
