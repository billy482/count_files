/***************************************************************************\
*                                      __     ____ __                       *
*                  _______  __ _____  / /_   / _(_) /__ ___                 *
*                 / __/ _ \/ // / _ \/ __/  / _/ / / -_|_-<                 *
*                 \__/\___/\_,_/_//_/\__/__/_//_/_/\__/___/                 *
*                                      /___/                                *
*                                                                           *
*  -----------------------------------------------------------------------  *
*  This file is a part of count_files                                       *
*                                                                           *
*  count_files is free software; you can redistribute it and/or             *
*  modify it under the terms of the GNU General Public License              *
*  as published by the Free Software Foundation; either version 3           *
*  of the License, or (at your option) any later version.                   *
*                                                                           *
*  This program is distributed in the hope that it will be useful,          *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
*  GNU General Public License for more details.                             *
*                                                                           *
*  You should have received a copy of the GNU General Public License        *
*  along with this program; if not, write to the Free Software              *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                       *
*  Boston, MA  02110-1301, USA.                                             *
*                                                                           *
*  You should have received a copy of the GNU General Public License        *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    *
*                                                                           *
*  -----------------------------------------------------------------------  *
*  Copyright (C) 2014, Clercin guillaume <clercin.guillaume@gmail.com>      *
*  Last modified: Sun, 05 Oct 2014 10:07:11 +0200                           *
\***************************************************************************/

#define _GNU_SOURCE
// getopt
#include <getopt.h>
// scandir
#include <dirent.h>
// signal
#include <signal.h>
// bool
#include <stdbool.h>
// uint
#include <stdint.h>
// asprintf, fflush, printf, snprintf
#include <stdio.h>
// free, getenv
#include <stdlib.h>
// memmove, memset, strchr, strlen
#include <string.h>
// bzero
#include <strings.h>
// ioctl
#include <sys/ioctl.h>
// lstat
#include <sys/stat.h>
// lstat, ssize_t
#include <sys/types.h>
// struct winsize
#include <termios.h>
// time
#include <time.h>
// lstat
#include <unistd.h>

#include "count-files.version"

#ifndef MAX_LINE_WIDTH
#define MAX_LINE_WIDTH 1024
#endif

struct count {
	uint64_t nb_folders;
	uint64_t nb_files;

	ssize_t total_size;
	ssize_t total_used;

	struct timespec last_update;
	int interval;
	bool one_fs;
};

static int interval = 1;
static bool one_fs = false;
static char terminal_clean_line[MAX_LINE_WIDTH + 1];
static unsigned int terminal_width = 72;

static void convert_size(ssize_t size, char * str, ssize_t str_len);
static int filter(const struct dirent * d);
static void init_clean_line(void);
static bool parse(const char * path, struct count * count);
static void process(const char * dir);
static void resize_terminal(int signal);
static void string_middle_elipsis(char * string, size_t length);
static void string_rtrim(char * str, char trim);
static int string_valid_utf8_char(const char * string);


static void convert_size(ssize_t size, char * str, ssize_t str_len) {
	unsigned short mult = 0;
	double tsize = size;

	while (tsize >= 1000 && mult < 4) {
		tsize /= 1024;
		mult++;
	}

	int fixed = 0;
	if (tsize < 0) {
		fixed = 3;
	} else if (tsize < 10) {
		fixed = 2;
	} else if (tsize < 100) {
		fixed = 1;
	}

	switch (mult) {
		case 0:
			snprintf(str, str_len, "%zd Bytes", size);
			break;
		case 1:
			snprintf(str, str_len, "%.*f KBytes", fixed, tsize);
			break;
		case 2:
			snprintf(str, str_len, "%.*f MBytes", fixed, tsize);
			break;
		case 3:
			snprintf(str, str_len, "%.*f GBytes", fixed, tsize);
			break;
		default:
			snprintf(str, str_len, "%.*f TBytes", fixed, tsize);
	}

	if (strchr(str, '.')) {
		char * ptrEnd = strchr(str, ' ');
		char * ptrBegin = ptrEnd - 1;
		while (*ptrBegin == '0')
			ptrBegin--;
		if (*ptrBegin == '.')
			ptrBegin--;

		if (ptrBegin + 1 < ptrEnd)
			memmove(ptrBegin + 1, ptrEnd, strlen(ptrEnd) + 1);
	}
}

static int filter(const struct dirent * d)  {
	if (d->d_name[0] != '.')
		return 1;

	if (d->d_name[1] == '\0')
		return 0;

	return d->d_name[1] != '.' || d->d_name[2] != '\0';
}

int main(int argc, char * argv[]) {
	enum {
		OPT_DIR      = 'd',
		OPT_HELP     = 'h',
		OPT_INTERVAL = 'i',
		OPT_ONE_FS   = 'x',
		OPT_VERSION  = 'V',
	};

	static const struct option op[] = {
		{ "directory",	    1, 0, OPT_DIR },
		{ "help",           0, 0, OPT_HELP },
		{ "interval",       1, 0, OPT_INTERVAL },
		{ "one-filesystem", 0, 0, OPT_ONE_FS },
		{ "version",        0, 0, OPT_VERSION },

		{ 0, 0, 0, 0 }
	};

	bool found = false;

	resize_terminal(0);
	signal(SIGWINCH, resize_terminal);

	int c, lo;
	do {
		c = getopt_long(argc, argv, "d:hi:Vx", op, &lo);

		int tmp_interval;
		switch (c) {
			case OPT_DIR:
				found = true;
				string_rtrim(optarg, '/');
				process(optarg);

				break;

			case OPT_HELP:
				printf("count_files: compute the number of files/folders into a directory\n");
				printf("  usage: count_files [-d <dir>|-h]\n");
				printf("    -d, --directory <dir> : count from <dir> directory\n");
				printf("    -h, --help            : show this and exit\n");
				printf("    -i, --interval <int>  : set interval between two updates of progression\n");
				printf("    -x, --one-filesystem  : skip directories on different file systems\n");
				return 0;

			case OPT_INTERVAL:
				sscanf(optarg, "%d", &tmp_interval);

				if (tmp_interval > 0)
					interval = tmp_interval;
				break;

			case OPT_ONE_FS:
				one_fs = true;
				break;

			case OPT_VERSION:
				printf("count_files: compute the number of files/folders into a directory\n");
				printf("  Version: " COUNTFILES_VERSION ", git commit: " COUNTFILES_GIT_COMMIT "\n");
				return 0;
		}
	} while (c > -1);

	for (; optind < argc; optind++) {
		found = true;
		string_rtrim(argv[optind], '/');
		process(argv[optind]);
	}

	if (!found)
		process(".");

	return 0;
}

static void init_clean_line() {
	memset(terminal_clean_line + 1, ' ', MAX_LINE_WIDTH - 1);
	terminal_clean_line[0] = '\r';
	terminal_clean_line[terminal_width] = '\r';
	terminal_clean_line[terminal_width + 1] = '\0';
}

static bool parse(const char * path, struct count * count) {
	struct stat st;
	if (lstat(path, &st))
		return false;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (now.tv_sec >= count->last_update.tv_sec + count->interval && now.tv_nsec >= count->last_update.tv_nsec) {
		static short i = 0;
		static char vals[] = { '|', '/', '-', '\\' };

		char buf[16];
		convert_size(count->total_size, buf, 16);
		float wasted = ((float) count->total_used) / count->total_size - 1;

		printf("%s", terminal_clean_line);
		int width;
		printf("nb folders: %zu, nb files: %zu, total size: %s, wasted: %.2f%%, path: %n", count->nb_folders, count->nb_files, buf, 100 * wasted, &width);

		char * ppath = strdup(path);
		string_middle_elipsis(ppath, terminal_width - width - 5);

		printf("%s [%c]", ppath, vals[i]);
		fflush(stdout);

		free(ppath);

		count->last_update = now;
		i++;
		i &= 0x3;
	}

	bool ok = true;

	if (S_ISREG(st.st_mode)) {
		count->nb_files++;
		count->total_size += st.st_size;
		count->total_used += st.st_blocks << 9;
	} else if (S_ISDIR(st.st_mode)) {
		count->nb_folders++;
		count->total_used += st.st_blocks << 9;

		struct dirent ** dl = NULL;
		int i, nb_files = scandir(path, &dl, filter, versionsort);
		for (i = 0; i < nb_files; i++) {
			if (ok) {
				char * subpath;
				asprintf(&subpath, "%s/%s", path, dl[i]->d_name);

				if (count->one_fs) {
					struct stat fst;
					lstat(subpath, &fst);

					if (st.st_dev == fst.st_dev)
						ok = parse(subpath, count);
				} else
					ok = parse(subpath, count);

				free(subpath);
			}
			free(dl[i]);
		}
		free(dl);
	}

	return ok;
}

static void process(const char * dir) {
	struct count cnt;
	bzero(&cnt, sizeof(cnt));
	cnt.interval = interval;
	cnt.one_fs = one_fs;

	parse(dir, &cnt);

	char buf_size[16], buf_used[16];
	convert_size(cnt.total_size, buf_size, 16);
	convert_size(cnt.total_used, buf_used, 16);
	float wasted = ((float) cnt.total_used) / cnt.total_size - 1;

	printf("%s", terminal_clean_line);
	printf("Folder parsed: %s\n", dir);
	printf("Nb folders: %zu, nb files: %zu\nTotal size: %s, total used space: %s, wasted: %.2f%%\n\n", cnt.nb_folders, cnt.nb_files, buf_size, buf_used, 100 * wasted);
}

static void resize_terminal(int signal __attribute__((unused))) {
	terminal_width = 72;

	static struct winsize size;
	int status = ioctl(1, TIOCGWINSZ, &size);
	if (!status) {
		terminal_width = size.ws_col > MAX_LINE_WIDTH ? MAX_LINE_WIDTH : size.ws_col;
		init_clean_line();
		return;
	}

	status = ioctl(0, TIOCGWINSZ, &size);
	if (!status) {
		terminal_width = size.ws_col > MAX_LINE_WIDTH ? MAX_LINE_WIDTH : size.ws_col;
		init_clean_line();
		return;
	}

	char * columns = getenv("COLUMNS");
	int tmp_width;
	if (columns != NULL && sscanf(columns, "%d", &tmp_width) == 1) {
		terminal_width = tmp_width > MAX_LINE_WIDTH ? MAX_LINE_WIDTH : tmp_width;
		init_clean_line();
	}
}

static void string_middle_elipsis(char * string, size_t length) {
	size_t str_length = strlen(string);
	if (str_length <= length)
		return;

	length -= 3;
	if (length < 2)
		return;

	size_t used = 0;
	char * ptrA = string;
	char * ptrB = string + str_length;
	while (used < length) {
		int char_length = string_valid_utf8_char(ptrA);
		if (char_length == 0)
			return;

		if (used + char_length > length)
			break;

		used += char_length;
		ptrA += char_length;

		int offset = 1;
		while (char_length = string_valid_utf8_char(ptrB - offset), ptrA < ptrB - offset && char_length == 0 && offset < 4)
			offset++;

		if (char_length == 0)
			return;

		if (used + char_length > length)
			break;

		used += char_length;
		ptrB -= char_length;
	}

	memmove(ptrA, "…", 3);
	memmove(ptrA + 1, ptrB, strlen(ptrB) + 1);
}

static void string_rtrim(char * str, char trim) {
	size_t length = strlen(str);

	char * ptr;
	for (ptr = str + (length - 1); *ptr == trim && ptr > str; ptr--);

	if (ptr[1] != '\0')
		ptr[1] = '\0';
}

static int string_valid_utf8_char(const char * string) {
	const unsigned char * ptr = (const unsigned char *) string;
	if ((*ptr & 0x7F) == *ptr) {
		return 1;
	} else if ((*ptr & 0xBF) == *ptr) {
		return 0;
	} else if ((*ptr & 0xDF) == *ptr) {
		ptr++;
		if ((*ptr & 0xBF) != *ptr || (*ptr & 0x80) != 0x80)
			return 0;

		return 2;
	} else if ((*ptr & 0xEF) == *ptr) {
		ptr++;
		if ((*ptr & 0xBF) != *ptr || (*ptr & 0x80) != 0x80)
			return 0;

		ptr++;
		if ((*ptr & 0xBF) != *ptr || (*ptr & 0x80) != 0x80)
			return 0;

		return 3;
	} else if ((*ptr & 0x7F) == *ptr) {
		ptr++;
		if ((*ptr & 0xBF) != *ptr || (*ptr & 0x80) != 0x80)
			return 0;

		ptr++;
		if ((*ptr & 0xBF) != *ptr || (*ptr & 0x80) != 0x80)
			return 0;

		ptr++;
		if ((*ptr & 0xBF) != *ptr || (*ptr & 0x80) != 0x80)
			return 0;

		return 4;
	} else {
		return 0;
	}
}

