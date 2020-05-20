#ifndef _DECOMPRESSOR_H_INCLUDED
#define _DECOMPRESSOR_H_INCLUDED

#include <unistd.h> // pid_t

/* spawn cmd, with all the files appended to it
 * (printf(filefmt, files_num_list[n]), with n = 0 -> n = num_files - 1),
 * put its stdout into *fd, and return its pid
 */
pid_t spawn_decompressor(int* files_num_list, int num_files, int* fd,
		const char* filefmt, const char* cmd);

#endif

