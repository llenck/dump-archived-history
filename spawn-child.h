#ifndef _SPAWN_CHILD_H_INCLUDED_
#define _SPAWN_CHILD_H_INCLUDED_

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#define WANT_STDIN 1
#define WANT_STDOUT 2
#define WANT_STDERR 4

// spawn a child process, optionally creating pipes for its STDIN / STDOUT / STDERR
// fds and put them in "int* fds" in that order (fds can be smaller than 3 ints if
// less are requested)
// Remember to close(2) your pipes after you're done with them!
int spawn_child(const char* file, char * const* arg, int* fds, uint8_t wants_fds);

#endif
