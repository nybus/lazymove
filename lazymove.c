#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <sysexits.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <libgen.h>
#include <getopt.h>

static int BF_SIZE = 4096;

void usage();
int valueof(const char *text);

int lazy(char* ifn, char* ofn);
int output(char* bf, int n, char* ofn, mode_t mode);
int reset(char* ofn, mode_t mode);
int load(char *fn, char* bf, int m, bool ellipsis);

int main(int ac, char **av) {
	static struct option longopts[] = {
		{ "bs", optional_argument, NULL, 'b' },
		{ NULL, 0, NULL, 0 }
	};

	const char *self = basename(av[0]);

	int ch;
	while((ch = getopt_long(ac, av, "b:", longopts, NULL)) != -1) {
		switch(ch) {
			case 'b':
				if(!optarg) { usage(); }
				BF_SIZE = valueof(optarg);
				if(errno != 0) { usage(); }
				break;
			default:
				usage();
		}
	}
	ac -= optind;
	av += optind;

	if(ac != 2) {
		usage();
	}
	if((BF_SIZE < 4) || (BF_SIZE > 0x20000)) {
		errx(EX_USAGE, "unsafe cutoff size: %d\n", BF_SIZE);
	}

	char *ifn = av[0];
	char *ofn = av[1];

	int res = lazy(ifn, ofn);
	if(!strcmp(self, "lazymove")) {
		if(unlink(ifn) < 0) {
			err(EX_OSERR, "unlink(%s)", ifn);
		}
	}

	return res;
}

void usage() {
	errx(EX_USAGE,
		"usage: [option] <ifile> <ofile>\n"
		"Does copy or move input file to output, in a lazy manner (only if they are different)\n"
		"\n"
		"  -b, --bs=SIZE  process no more than SIZE bytes from input (default 4096)\n"
	);
}

int valueof(const char *text) {
	errno = 0;
	if(!strncmp("0x", text, 2)) {
		return strtoul(text + 2, NULL, 16);
	}
	return strtoul(text, NULL, 10);
}

int lazy(char* ifn, char* ofn) {
	int res;
	struct stat st;
	off_t i_size, o_size;

	res = stat(ifn, &st);
	if(res < 0) {
		err(EX_NOINPUT, "stat(%s)", ifn);
	}
	if(!S_ISREG(st.st_mode)) {
		errx(EX_NOINPUT, "not a regular file: %s", ifn);
	}
	i_size = st.st_size;
	mode_t i_mode = st.st_mode & ACCESSPERMS;

	res = stat(ofn, &st);
	if(res < 0) {
		if(errno != ENOENT) {
			err(EX_NOINPUT, "stat(%s)", ofn);
		}
		o_size = -1;
	} else {
		if(!S_ISREG(st.st_mode)) {
			errx(EX_NOINPUT, "not a regular file: %s", ofn);
		}
		o_size = st.st_size;
	}

	if(i_size > 0) {
		char ibf[BF_SIZE+1];
		int xi = load(ifn, ibf, sizeof(ibf), true);
		if((o_size == i_size) || ((o_size == BF_SIZE) && (i_size > BF_SIZE))) {
			char obf[BF_SIZE];
			int xo = load(ofn, obf, sizeof(obf), false);
			if(xi != xo) {
				return output(ibf, xi, ofn, i_mode);
			}
			if(memcmp(ibf, obf, xi) != 0) {
				return output(ibf, xi, ofn, i_mode);
			}
		} else {
			return output(ibf, xi, ofn, i_mode);
		}
	} else {
		if(o_size != 0) {
			return reset(ofn, i_mode);
		}
	}

	return 0;
}

int output(char* bf, int n, char* ofn, mode_t mode) {
	int m = strlen(ofn) + 2;
	char *xfn = alloca(m);
	snprintf(xfn, m, "%s~", ofn);
	int os = open(xfn, O_WRONLY|O_CREAT|O_TRUNC, mode);
	if(os < 0) {
		err(EX_OSERR, "open(%s)", xfn);
	}

	n = write(os, bf, n);
	if(n < 0) {
		err(EX_OSERR, "write(%s)", xfn);
	}
	close(os);

	int res = rename(xfn, ofn);
	if(res < 0) {
		err(EX_OSERR, "rename(%s, %s)", xfn, ofn);
	}

	return 0;
}

int reset(char* ofn, mode_t mode) {
	int os = open(ofn, O_WRONLY|O_CREAT|O_TRUNC, mode);
	if(os < 0) {
		err(EX_OSERR, "open(%s)", ofn);
	}
	close(os);

	return 0;
}

int load(char *fn, char* bf, int m, bool ellipsis) {
	int is = open(fn, O_RDONLY);
	if(is < 0) {
		err(EX_OSERR, "open(%s)", fn);
	}

	int n = read(is, bf, m);
	if(n < 0) {
		err(EX_OSERR, "read(%s)", fn);
	}
	close(is);

	if(ellipsis && (n == m)) {
		snprintf(bf+(m-4), 4, "...");
		return (n-1);
	}

	return n;
}
