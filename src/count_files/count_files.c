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
*  Copyright (C) 2013, Clercin guillaume <clercin.guillaume@gmail.com>      *
*  Last modified: Tue, 05 Mar 2013 22:01:38 +0100                           *
\***************************************************************************/

#define _GNU_SOURCE
// getopt
#include <getopt.h>
// scandir
#include <dirent.h>
// bool
#include <stdbool.h>
// uint
#include <stdint.h>
// asprintf, fflush, printf, snprintf
#include <stdio.h>
// free
#include <stdlib.h>
// memmove, strchr, strlen
#include <string.h>
// bzero
#include <strings.h>
// lstat
#include <sys/stat.h>
// lstat, ssize_t
#include <sys/types.h>
// time
#include <time.h>
// lstat
#include <unistd.h>

struct count {
	uint64_t nb_folders;
	uint64_t nb_files;

	ssize_t total_size;
	ssize_t total_used;

	time_t last_update;
};

static void convert_size(ssize_t size, char * str, ssize_t str_len);
static int filter(const struct dirent * d);
static bool parse(const char * path, struct count * count);


static void convert_size(ssize_t size, char * str, ssize_t str_len) {
	unsigned short mult = 0;
	double tsize = size;

	while (tsize >= 1024 && mult < 4) {
		tsize /= 1024;
		mult++;
	}

	switch (mult) {
		case 0:
			snprintf(str, str_len, "%zd Bytes", size);
			break;
		case 1:
			snprintf(str, str_len, "%.1f KBytes", tsize);
			break;
		case 2:
			snprintf(str, str_len, "%.2f MBytes", tsize);
			break;
		case 3:
			snprintf(str, str_len, "%.3f GBytes", tsize);
			break;
		default:
			snprintf(str, str_len, "%.4f TBytes", tsize);
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
		OPT_DIR  = 'd',
		OPT_HELP = 'h',
	};

	static const struct option op[] = {
		{ "directory",	1, 0, OPT_DIR },
		{ "help",       0, 0, OPT_HELP },

		{ 0, 0, 0, 0 }
	};

	static struct count cnt;
	static char buf_size[16], buf_used[16];

	bool found = false;

	int c, lo;
	do {
		c = getopt_long(argc, argv, "d:h", op, &lo);

		switch (c) {
			case OPT_DIR:
				found = true;

				bzero(&cnt, sizeof(cnt));

				parse(optarg, &cnt);

				convert_size(cnt.total_size, buf_size, 16);
				convert_size(cnt.total_used, buf_used, 16);

				printf("\r                                                                                             \r");
				printf("Folders parsed: %s\n", optarg);
				printf("Nb folders: %zu, nb files: %zu\nTotal size: %s, total used space: %s\n\n", cnt.nb_folders, cnt.nb_files, buf_size, buf_used);

				break;

			case OPT_HELP:
				printf("count_files: compute the number of files/folders into a directory\n");
				printf("  usage: count_files [-d <dir>|-h]\n");
				printf("    -d, --directory <dir> : count from <dir> directory\n");
				printf("    -h, --help            : show this and exit\n");
				return 0;
		}
	} while (c > -1);

	if (!found) {
		bzero(&cnt, sizeof(cnt));

		parse(".", &cnt);

		convert_size(cnt.total_size, buf_size, 16);
		convert_size(cnt.total_used, buf_used, 16);

		printf("\r                                                                                             \r");
		printf("Folders parsed: current directory\n");
		printf("Nb folders: %zu, nb files: %zu\nTotal size: %s, total used space: %s\n\n", cnt.nb_folders, cnt.nb_files, buf_size, buf_used);
	}

	return 0;
}

static bool parse(const char * path, struct count * count) {
	struct stat st;
	if (lstat(path, &st))
		return false;

	time_t now = time(NULL);
	if (now != count->last_update) {
		static short i = 0;
		static char vals[] = { '|', '/', '-', '\\' };

		char buf[16];
		convert_size(count->total_size, buf, 16);

		printf("\rnb folders: %zu, nb files: %zu, total size: %s [%c]", count->nb_folders, count->nb_files, buf, vals[i]);
		fflush(stdout);

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
		for (i = 0; i < nb_files && ok; i++) {
			char * subpath;
			asprintf(&subpath, "%s/%s", path, dl[i]->d_name);

			ok = parse(subpath, count);

			free(subpath);
			free(dl[i]);
		}
		free(dl);
	}

	return ok;
}

