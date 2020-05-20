#include "spawn-child.h"

int spawn_child(const char* file, char * const* arg, int* fds, uint8_t wants_fds) {
	// create an ipc pipe for every pipe wanted
	int fd_cnt = (wants_fds & WANT_STDIN) +
				 (wants_fds & WANT_STDOUT) / WANT_STDOUT +
				 (wants_fds & WANT_STDERR) / WANT_STDERR;

	int pipes[fd_cnt * 2];

	for (int i = 0; i < fd_cnt; i++) {
		if (pipe(pipes + i * 2) != 0) {
			// if pipe() fails, close already opened pipes
			for (int j = 0; j < i * 2; j++) {
				close(pipes[j]);
			}
			return -1;
		}
	}

	pid_t pid = fork();
	if (pid == -1) {
		// fork failed; close all pipes
		for (int i = 0; i < fd_cnt * 2; i++) {
			close(pipes[i]);
		}
		return -1;
	}
	else if (pid != 0) {
		// in parent process, close pipes that aren't needed, and copy wanted fds over
		if (wants_fds & WANT_STDIN) {
			fds[0] = pipes[1];
			close(pipes[0]);
		}
		if (wants_fds & WANT_STDOUT) {
			int stdout_offs = (wants_fds & WANT_STDIN) / WANT_STDIN;
			fds[stdout_offs] = pipes[stdout_offs * 2];
			close(pipes[stdout_offs * 2 + 1]);
		}
		if (wants_fds & WANT_STDERR) {
			int stderr_offs = (wants_fds & WANT_STDIN) / WANT_STDIN +
							  (wants_fds & WANT_STDOUT) / WANT_STDOUT;
			fds[stderr_offs] = pipes[stderr_offs * 2];
			close(pipes[stderr_offs * 2 + 1]);
		}
		return pid;
	}
	else {
		// in child process, close pipes that aren't needed and replace std*
		if (wants_fds & WANT_STDIN) {
			close(pipes[1]);
			dup2(pipes[0], STDIN_FILENO);
		}
		if (wants_fds & WANT_STDOUT) {
			int stdout_offs = (wants_fds & WANT_STDIN) / WANT_STDIN;
			dup2(pipes[stdout_offs * 2 + 1], STDOUT_FILENO);
			close(pipes[stdout_offs * 2]);
		}
		if (wants_fds & WANT_STDERR) {
			int stderr_offs = (wants_fds & WANT_STDIN) / WANT_STDIN +
							  (wants_fds & WANT_STDOUT) / WANT_STDOUT;
			dup2(pipes[stderr_offs * 2 + 1], STDERR_FILENO);
			close(pipes[stderr_offs * 2]);
		}

		// after that, replace current process with new one
		execvp(file, arg);

		// exit if execvp failed (if it works we won't get here)
		exit(1);
	}
}

