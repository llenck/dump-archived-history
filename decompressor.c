#define _GNU_SOURCE // for asprintf

#include "decompressor.h"

#include <stdio.h>
#include <unistd.h>

#include "spawn-child.h"

pid_t spawn_decompressor(int* files_num_list, int num_files, int* fd,
		const char* filefmt, __attribute__((unused))const char* cmd)
{
	// build argument list for xz
	char* unzip_args[num_files + 5];
	unzip_args[0] = "xz";
	unzip_args[1] = "-c";
	unzip_args[2] = "-d";
	unzip_args[3] = "-qq";
	unzip_args[num_files + 4] = NULL;
	int offs = 4;

	for (int i = 0; i < num_files; i++) {
		int ret = asprintf(&unzip_args[offs], filefmt, files_num_list[i]);

		// instant eof if we couldn't provide a name
		if (ret == -1) unzip_args[offs] = "/dev/null";

		offs++;
	}

	pid_t unzip_pid = spawn_child(unzip_args[0], unzip_args, fd, WANT_STDOUT);

	for (int i = 4; i < num_files + 4; i++) {
		free(unzip_args[i]);
	}

	return unzip_pid;
}

