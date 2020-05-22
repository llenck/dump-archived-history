#define _GNU_SOURCE // for asprintf

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#ifdef _BENCHMARK
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include "decompressor.h"
#include "enumerate-histfiles.h"

#define strequ(a, b) (!strcmp(a, b))

/* read from fd and dump the history as the history command would've done it
 * (with formatted time and line number)
 */
int dump_history(int fd, struct tm* last_time, const char* timefmt) {
	// a FILE makes looping over the lines much easier than a plain fd
	FILE* f = fdopen(fd, "r");
	if (!f) {
		return -1;
	}

	// getline hopefully is fast enough with a buffer that shouldn't need realloc-ing
	char* line = malloc(256);
	size_t line_len = 256; // initial length of line, might be realloc'd by getline
	int line_counter = 1;
	char formatted_time[64]; // TODO variable length maybe?
	formatted_time[0] = timefmt? ' ' : '\0'; // empty string if theres no timefmt

	while (getline(&line, &line_len, f) != -1) {
		// if the line starts with "#[0-9]" its probably a line to indicate the time
		// of the next command
		if (line[0] == '#' && line[1] >= '0' && line[1] <= '9') {
			time_t last_time_unix = strtoll(&line[1], NULL, 10);
			localtime_r(&last_time_unix, last_time);
		}
		// otherwise, it is probably a command that we should output with the
		// last known time
		else {
			if (timefmt) {
				// +1 because the first char one is a space
				//
				// fun fact, the next line is >50% of this programs runtime
				// according to valgrind --tool=callgrind
				size_t ret = strftime(formatted_time + 1, 63, timefmt, last_time);
				// in case strftime failed, empty formatted_time
				// (except for [0], which is a space)
				if (ret == 0) formatted_time[1] = '\0';
			}

			// ok even if timefmt is NULL bc we set formatted_time[0] to '\0' beforehand
			printf("%5d %s%s", line_counter++, formatted_time, line);
		}
	}

	// cleanup
	free(line);
	fclose(f);

	return 0;
}

int main(int argc, char** argv) {
	struct option long_options[] = {
		{"help", no_argument, NULL, 'h' },
		{"history-dir", required_argument, NULL, 'd' },
		{"name-format", required_argument, NULL, 'f' },
		{"extract-command", required_argument, NULL, 'x' },
		{NULL, 0, NULL, 0}
	};

	char* history_dir = NULL, * name_format = NULL, * extract_command = NULL;
	int allocated_dir = 0;

	int c;
	int option_index = 0;
	while ((c = getopt_long(argc, argv, "hd:f:x:", long_options, &option_index)) != -1) {
		switch(c) {
			case 'd':
				history_dir = optarg;
				break;

			case 'f':
				name_format = optarg;
				break;

			case 'x':
				extract_command = optarg;
				break;

			case 'h':
			case '?':
			default:
				fprintf(stderr, "TODO: usage");
				exit(0);
				break;
		}
	}

	if (!history_dir) {
		// try to use everything from HISTFILE except the file name
		const char* env_histfile = getenv("HISTFILE");
		if (env_histfile) {
			const char* last_slash = strrchr(env_histfile, '/');
			history_dir = malloc(last_slash - env_histfile + 1);
			if (!history_dir) {
				fprintf(stderr, "Error: not enough memory!\n");
				return 1;
			}

			memcpy(history_dir, env_histfile, last_slash - env_histfile);
			history_dir[last_slash - env_histfile] = '\0';
			allocated_dir = 1;
		}
		else {
			// try to use HOME/.bash_history/
			const char* env_home = getenv("HOME");
			if (!env_home) {
				fprintf(stderr, "Couldn't figure out directory for the history files\n");
				return 1;
			}
			const char* after_home = "/.bash_history";
			int after_home_len = strlen(after_home), env_home_len = strlen(env_home);

			history_dir = malloc(after_home_len + env_home_len + 1);
			if (!history_dir) {
				fprintf(stderr, "Error: not enough memory!\n");
				return 1;
			}

			memcpy(history_dir, env_home, env_home_len);
			// +1 for null byte
			memcpy(history_dir + env_home_len, after_home, after_home_len + 1);
			allocated_dir = 1;
		}
	}
	if (!name_format) {
		name_format = "history.%d.xz";
	}
	if (!extract_command) {
		const char* history_extension = strrchr(name_format, '.');
		if (!history_extension) {
			extract_command = "cat";
		}
		else {
			history_extension += 1;

			if (strequ(history_extension, "xz"))
				extract_command = "xz -qq -d -c";
			else if (strequ(history_extension, "gz"))
				extract_command = "gzip -q -d -c";
			else if (strequ(history_extension, "lz4"))
				extract_command = "lz4 -qq -d -c";
			else if (strequ(history_extension, "bz2"))
				extract_command = "bzip2 -q -d -c -k";
			else
				extract_command = "cat";
		}
	}

	int* files_num_list;
	int num_files = enumerate_histfiles(&files_num_list, history_dir, name_format);
	if (num_files <= 0) {
		fprintf(stderr, "Couldn't find enumerate archived history files");
		return 1;
	}

	char* full_format;
	int ret = asprintf(&full_format, "%s/%s", history_dir, name_format);
	if (ret == -1) {
		return 1;
	}

	// in case some dates are missing, use the modification data of the oldest file
	// (but only if HISTTIMEFORMAT exists)
	struct tm earliest_time;
	char* timefmt = getenv("HISTTIMEFORMAT");

	if (timefmt && *timefmt != '\0') {
		char* oldest_file;
		struct stat stats;

		int ret = asprintf(&oldest_file, full_format, files_num_list[0]);
		if (ret == -1) {
			fprintf(stderr, "Error: not enough memory!\n");
			return 1;
		}

		stat(oldest_file, &stats);
		localtime_r(&stats.st_mtime, &earliest_time);
		free(oldest_file);
	}

	// use the info about files to spawn the decompressor
	int unzip_fd;
	pid_t unzip_pid = spawn_decompressor(files_num_list, num_files,
		&unzip_fd, full_format, extract_command);

	// free stuff before checking the pid so we don't leak memory if the command failed
	free(files_num_list);
	free(full_format);
	if (allocated_dir) free(history_dir);

	if (unzip_pid == -1) {
		perror("Failed to spawn decompressor");
		return 1;
	}

	// this closes unzip_fd for us because fclose apparently does that even
	// if the FILE was created with fdopen
	dump_history(unzip_fd, &earliest_time, timefmt);

	// reap unzip_pid bc we don't want to leave any zombie processes
	waitpid(unzip_pid, NULL, 0);

#ifdef _BENCHMARK
	struct rusage usage;
	ret = getrusage(RUSAGE_SELF, &usage);
	if (ret == -1) return 0; // fail silently

	fprintf(stderr, "Total user time: %ld.%06lds\n",
		usage.ru_utime.tv_sec, usage.ru_utime.tv_usec);
	fprintf(stderr, "Total kernel time: %ld.%06lds\n",
		usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);
#endif

	return 0;
}

