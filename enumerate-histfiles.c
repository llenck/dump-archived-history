#include "enumerate-histfiles.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

/* create a list of all the history files on the system, allocating space as needed
 * *history_num_list should be free'd by the caller
 *
 * returns: the number of files found (length of the list)
 */
int enumerate_histfiles(int** history_num_list, const char* history_dir,
		const char* name_format) {
    DIR* dir = opendir(history_dir);

	// bit field that indicates whether a history file exists,
	// which we populate by listing the directory and setting a bit to 1
	// for every file of the format given. That way, by going through the
	// list in reverse order we get all the file names in descending order
	//
	// this sort is of course horrible, because it requires at least O(n) space,
	// but that should be ok with such few files and it should take basically no time
	// at all which is what I'm after
	uint8_t history_exists[512] = { 0 };
	int highest_num = -1;

	if (dir == NULL) {
		return 0;
	}

	int num_files = 0;

	struct dirent* elem;
	while ((elem = readdir(dir)) != NULL) {
		if (elem->d_type != DT_REG) continue;

		int num;
		int ret = sscanf(elem->d_name, name_format, &num);
		if (ret != 1 || num >= 512 * 8) continue;

		num_files++;
		if (num > highest_num) highest_num = num;
		history_exists[num / 8] |= 1 << (num % 8);
	}
	closedir(dir);

	// loop through found files in reverse to fill history_num_list with the
	// numbers of the files sorted in descending order
	*history_num_list = malloc(num_files * sizeof(**history_num_list));
	int offs = 0;
	for (int i = highest_num; i >= 0; i--) {
		if (history_exists[i / 8] & (1 << i % 8)) {
			(*history_num_list)[offs++] = i;
		}
	}

	return num_files;
}
