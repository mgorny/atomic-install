/* atomic-install
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_STDINT_H
#	include <stdint.h>
#endif

#include <unistd.h>
#include <getopt.h>
#include <signal.h>

#include "lib/journal.h"
#include "lib/merge.h"

static const struct option opts[] = {
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },

	{ "no-replace", no_argument, NULL, 'n' },
	{ "onestep", no_argument, NULL, '1' },
	{ "resume", no_argument, NULL, 'r' },
	{ "rollback", no_argument, NULL, 'R' },
	{ "verbose", no_argument, NULL, 'v' },
	{ 0, 0, 0, 0 }
};

static void print_help(const char *argv0) {
	printf("Usage: %s [options] journal-file source dest\n"
"\n"
"Options:\n"
"    --help, -h          this help message\n"
"    --version, -V       print program version\n"
"\n"
"    --no-replace, -n    terminate before the replacement step\n"
"    --onestep, -1       perform a smallest step possible\n"
"    --resume, -r        resume existing merge, do not try creating new one\n"
"    --rollback, -R      roll existing merge back\n"
"    --verbose, -v       report progress verbosely\n"
"", argv0);
}

static void print_progress(const char *path, unsigned long int megs, unsigned long int size) {
	if (megs == 0)
		printf(">>> %s\n", path);
}

struct loop_data {
	ai_journal_t j;

	const char *source;
	const char *dest;
	const char *journal_file;

	int rollback;
	int noreplace;
	int verbose;
	int onestep;
};

struct loop_data main_data;

static int loop(struct loop_data *d) {
	int ret;

	while (1) {
		const uint32_t flags = ai_journal_get_flags(d->j);

		if (flags & AI_MERGE_ROLLBACK_STARTED || d->rollback) {
			/* Proceed with rollback. */
			if (flags & AI_MERGE_REPLACED) {
				printf("! Replacement complete, rollback impossible.\n");
				ret = 1;
				break;
			} else if (flags & AI_MERGE_BACKED_OLD_UP) {
				printf("* Rolling back replacement...\n");
				ret = ai_merge_rollback_replace(d->dest, d->j);
				if (ret) {
					printf("* Replacement rollback failed: %s\n", strerror(ret));
					break;
				}
			} else {
				printf("* Rolling back old backup...\n");
				ret = ai_merge_rollback_old(d->dest, d->j);
				if (ret) {
					printf("* Old rollback failed: %s\n", strerror(ret));
					break;
				}
			}
			printf("* Rolling back new copying...\n");
			ret = ai_merge_rollback_new(d->dest, d->j);
			if (ret)
				printf("* New rollback failed: %s\n", strerror(ret));
			else {
				printf("* Rollback successful.\n");
				if (unlink(d->journal_file))
					printf("Journal removal failed: %s\n", strerror(errno));
			}
			break;
		} else if (flags & AI_MERGE_REPLACED) {
			printf("* Post-merge clean up...\n");
			ret = ai_merge_cleanup(d->dest, d->j);
			if (ret)
				printf("Cleanup failed: %s\n", strerror(ret));
			else {
				printf("* Install done.\n");
				if (unlink(d->journal_file))
					printf("Journal removal failed: %s\n", strerror(errno));
			}
			break;
		} else if (flags & AI_MERGE_BACKED_OLD_UP && flags & AI_MERGE_COPIED_NEW) {
			if (d->noreplace)
				break;
			printf("* Replacing files...\n");
			ret = ai_merge_replace(d->dest, d->j);
			if (ret) {
				printf("Replacement failed: %s\n", strerror(ret));
				d->rollback = 1;
			}
		} else if (flags & AI_MERGE_COPIED_NEW) {
			printf("* Backing up existing files...\n");
			ret = ai_merge_backup_old(d->dest, d->j);
			if (ret) {
				printf("Backing old up failed: %s\n", strerror(ret));
				break;
			}
		} else {
			printf("* Copying new files...\n");
			ret = ai_merge_copy_new(d->source, d->dest, d->j,
					d->verbose ? print_progress : NULL);
			if (ret) {
				printf("Copying new failed: %s\n", strerror(ret));
				break;
			}
		}

		if (d->onestep)
			break;
	}

	return ret;
}

static void term_handler(int sig) {
	main_data.rollback = 1;
	main_data.onestep = 1;
	loop(&main_data);
	exit(1);
}

int main(int argc, char *argv[]) {
	int opt;
	int ret, ret2;

	struct sigaction sa;

	int resume = 0;

	while ((opt = getopt_long(argc, argv, "hV1nrRv", opts, NULL)) != -1) {
		switch (opt) {
			case '1':
				main_data.onestep = 1;
				break;
			case 'n':
				main_data.noreplace = 1;
				break;
			case 'r':
				resume = 1;
				break;
			case 'R':
				main_data.rollback = 1;
				break;
			case 'v':
				main_data.verbose = 1;
				break;
			case 'V':
				printf("%s\n", PACKAGE_STRING);
				return 0;
			case 0:
				break;
			default:
				print_help(argv[0]);
				return 0;
		}
	}

	if (argc - optind < 3) {
		printf("Synopsis: atomic-install journal source dest\n");
		return 0;
	}

	main_data.journal_file = argv[optind];
	main_data.source = argv[optind + 1];
	main_data.dest = argv[optind + 2];

	/* Try to open.
	 * If it doesn't exist, try to create and then open. */
	ret = ai_journal_open(main_data.journal_file, &main_data.j);
	if (!ret)
		printf("* Journal file open, %s.\n",
				main_data.rollback ? "rolling back" : "resuming");
	else if (ret == ENOENT && !resume && !main_data.rollback) {
		printf("* Journal not found, creating...\n");

		ret = ai_journal_create(main_data.journal_file, main_data.source);
		if (ret) {
			printf("Journal creation failed: %s\n", strerror(ret));
			return ret;
		}

		ret = ai_journal_open(main_data.journal_file, &main_data.j);
	}
	if (ret) {
		printf("Journal open failed: %s\n", strerror(ret));
		return ret;
	}

	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);

	sa.sa_handler = &term_handler;
	sigaddset(&sa.sa_mask, SIGINT);
	sigaddset(&sa.sa_mask, SIGTERM);
	sigaddset(&sa.sa_mask, SIGHUP);

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);

	ret = loop(&main_data);

	ret2 = ai_journal_close(main_data.j);
	if (ret2)
		printf("Journal close failed: %s\n", strerror(ret));

	return ret || ret2;
}
