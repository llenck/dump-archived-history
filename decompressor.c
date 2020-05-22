#define _GNU_SOURCE // for asprintf

#include "decompressor.h"

#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include "spawn-child.h"

pid_t spawn_decompressor(int* files_num_list, int num_files, int* fd,
		const char* filefmt, const char* cmd)
{
	pid_t ret = 0;

	int offs = 0;
	int current_args_len = 10;
	int num_args_before_files = -1;

	const char* placeholder_file = "/dev/null";
	char** unzip_args = malloc(current_args_len * sizeof(*unzip_args));
	if (unzip_args == NULL) return (pid_t)-1;

	char* cmd_writable = strdup(cmd);
	const char* cmd_delims = " \t\n";

	// first split cmd_writable using strtok, using offs as
	// the writing offset in unzip_args, and current_args_len
	// as the total capacity in unzip_args
	char* saveptr;
	char* arg = strtok_r(cmd_writable, cmd_delims, &saveptr);
	if (arg == NULL) {
		ret = -1;
		goto cleanup_no_files;
	}

	do {
		// check if we need to realloc unzip_args
		offs++;
		if (offs >= current_args_len) {
			current_args_len += 5;
			char** tmp = realloc(unzip_args, current_args_len * sizeof(*unzip_args));
			if (tmp == NULL) {
				ret = -1;
				goto cleanup_no_files;
			}

			unzip_args = tmp;
		}

		unzip_args[offs - 1] = arg;
	} while ((arg = strtok_r(NULL, cmd_delims, &saveptr)) != NULL);

	num_args_before_files = offs;

	// offs has the number of args so far, and the +1 is for the final NULL pointer
	char** tmp = realloc(unzip_args, (offs + num_files + 1) * sizeof(*unzip_args));
	if (tmp == NULL) {
		ret = -1;
		goto cleanup_no_files;
	}
	unzip_args = tmp;

	// append all the file names to the arguments
	for (int i = 0; i < num_files; i++) {
		int ret = asprintf(&unzip_args[offs], filefmt, files_num_list[i]);

		// instant eof if we couldn't provide a name
		// casting to char* should be ok if we couldn't provide a name
		if (ret == -1) unzip_args[offs] = (char*)placeholder_file;

		offs++;
	}

	// spawn_child wants the last argument marked with NULL
	unzip_args[offs] = NULL;

	ret = spawn_child(unzip_args[0], unzip_args, fd, WANT_STDOUT);

	for (int i = num_args_before_files; i <= num_files + num_args_before_files; i++) {
		// skip if we put "/dev/null" into it
		if (unzip_args[i] == placeholder_file) continue;

		free(unzip_args[i]);
	}

cleanup_no_files:
	free(unzip_args);
	free(cmd_writable);

	return ret;
}

