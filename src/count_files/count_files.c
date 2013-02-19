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
		OPT_DIR = 'd',
	};

	static const struct option op[] = {
		{ "directory",	1, 0, OPT_DIR },

		{ 0, 0, 0, 0 }
	};

	static struct count cnt;
	static char buf_size[16], buf_used[16];

	bool found = false;

	int c, lo;
	do {
		c = getopt_long(argc, argv, "d:", op, &lo);

		switch (c) {
			case 'd':
				found = true;

				bzero(&cnt, sizeof(cnt));

				parse(optarg, &cnt);

				convert_size(cnt.total_size, buf_size, 16);
				convert_size(cnt.total_used, buf_used, 16);

				printf("\r                                                                                             \r");
				printf("Folders parsed: %s\n", optarg);
				printf("Nb folders: %llu, nb files: %llu\nTotal size: %s, total used space: %s\n\n", cnt.nb_folders, cnt.nb_files, buf_size, buf_used);

				break;
		}
	} while (c > -1);

	if (!found) {
		bzero(&cnt, sizeof(cnt));

		parse(".", &cnt);

		convert_size(cnt.total_size, buf_size, 16);
		convert_size(cnt.total_used, buf_used, 16);

		printf("\r                                                                                             \r");
		printf("Folders parsed: current directory\n");
		printf("Nb folders: %llu, nb files: %llu\nTotal size: %s, total used space: %s\n\n", cnt.nb_folders, cnt.nb_files, buf_size, buf_used);
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

		printf("\rnb folders: %llu, nb files: %llu, total size: %zd [%c]", count->nb_folders, count->nb_files, count->total_size, vals[i]);
		fflush(stdout);

		count->last_update = now;
		i++;
		i &= 0x3;
	}

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
			char * subpath;
			asprintf(&subpath, "%s/%s", path, dl[i]->d_name);

			parse(subpath, count);

			free(subpath);
			free(dl[i]);
		}
		free(dl);
	}
}

