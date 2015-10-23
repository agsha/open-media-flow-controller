#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "nkn_regex.h"

#include "nkn_mem_counter_def.h"

#define _GNU_SOURCE
#include <getopt.h>

const char *option_str = "d:e:f:hmv";

static struct option long_options[] = {
    {"data", 1, 0, 'd'},
    {"expression", 1, 0, 'e'},
    {"file", 1, 0, 'f'},
    {"help", 1, 0, 'h'},
    {"matcher", 1, 0, 'm'},
    {"verbose", 0, 0, 'v'},
    {0, 0, 0, 0}
};

const char *helpstr = (
"Evaluate regex against data.\n\n"
"Options:\n"
"\t-d --data\n"
"\t-e --expression\n"
"\t-f --file\n"
"\t-h --help\n"
"\t-m --matcher\n"
"\t-v --verbose\n"
"\n"
"Examples:\n"
"  nknregex -v -e '^[a-f,0-9]{8}\\.(origin\\.|cdn\\.)?cms\\.xxx\\.com:80$'\n"
"           -d 'abcdef01.origin.cms.xxx.com:80'\n\n"
"  nknregex -v -e '^(cms[0-9]{3}).*(qcg[0-9]+|dc2|af1)\\.yyy\\.com:80$'\n"
"           -d 'cms123.x.y.qcg0.yyy.com:80'\n\n"
"  nknregex -v -e '^abc.def.com:80$' -f <data file>\n"
"  nknregex -m -e 'http://113\\.21\\.241\\.[1-9]{1,2}/videoplayback\\?.*\\&id=([^\\&]+).*' -d 'http://113.21.241.12/videoplayback?xxx&id=1xxuuu'\n"
);

char *opt_data = 0;
char *opt_expr = 0;
char *opt_file = 0;
int opt_verbose = 0;
int matcher = 0;

char *data;
char errbuf[32*1024];

int main(int argc, char **argv)
{
    int rv;
    int ev;
    int c;
    int option_index = 0;
    int fd;
    struct stat sbuf;
    ssize_t bytes;
    nkn_regex_ctx_t *regctx = 0;

    nkn_regmatch_t match[128];
    char tbuf[1024 * 1024];
    int match_cnt;
    int n;

    while (1) {
	c = getopt_long(argc, argv, option_str, long_options, &option_index);
	if (c == -1) {
	    break;
	}
	switch (c) {
	case 'd':
	    opt_data = optarg;
	    break;
	case 'e':
	    opt_expr = optarg;
	    break;
	case 'f':
	    opt_file = optarg;
	    break;
	case 'h':
	    printf("[%s]: \n%s\n", argv[0], helpstr);
	    exit(100);
	case 'm':
	    matcher = 1;
	    break;
	case 'v':
	    opt_verbose = 1;
	    break;
	}
    }

    while(1) {
    /**************************************************************************/

    if (!opt_expr || (!opt_data && !opt_file)) {
    	printf("[%s]: \n%s\n", argv[0], helpstr);
	ev = 2;
	break;
    }

    if (opt_file) {
        fd = open(opt_file, O_RDONLY);
	if (fd < 0) {
	    perror("open");
	    ev = 3;
	    break;
	}

	rv = fstat(fd, &sbuf);
	if (rv) {
	    perror("fstat");
	    ev = 4;
	    break;
	}

	data = malloc(sbuf.st_size+1);
	if (!data) {
	    printf("malloc(%ld) failed\n", sbuf.st_size+1);
	    ev = 5;
	    break;
	}

	bytes = read(fd, data, sbuf.st_size);
	if (bytes != sbuf.st_size) {
	    printf("read() failed, read=%ld size=%ld\n", bytes, sbuf.st_size);
	    ev = 6;
	    break;
	}
	data[sbuf.st_size] = '\0';
	opt_data = data;
    }

    if (!nkn_valid_regex(opt_expr, errbuf, sizeof(errbuf))) {
    	printf("Invalid regex: errbuf=[%s]\n", errbuf);
	ev = 7;
	break;
    }

    regctx = nkn_regex_ctx_alloc();
    if (!regctx) {
    	printf("nkn_regex_ctx_alloc() failed");
	ev = 8;
	break;
    }

    if (matcher) {
    	rv = nkn_regcomp_match(regctx, opt_expr, errbuf, sizeof(errbuf));
    	if (rv) {
	    printf("nkn_regcomp_match failed, rv=%d, errbuf=[%s]\n", 
		   rv, errbuf);
	    ev = 9;
	    break;
    	}
    } else {
    	rv = nkn_regcomp(regctx, opt_expr, errbuf, sizeof(errbuf));
    	if (rv) {
	    printf("nkn_regcomp failed, rv=%d, errbuf=[%s]\n", rv, errbuf);
	    ev = 10;
	    break;
    	}
    }

    if (matcher) {
    	rv = nkn_regexec_match(regctx, opt_data, 
			       sizeof(match)/sizeof(nkn_regmatch_t), match,
			       errbuf, sizeof(errbuf));
	if (!rv) {
	    for (n = 0, match_cnt = 0; 
	    	 n < (int)(sizeof(match)/sizeof(nkn_regmatch_t)); n++) {
		if (match[n].rm_so != -1) {
		    bytes = match[n].rm_eo - match[n].rm_so;
		    memcpy(tbuf, &opt_data[match[n].rm_so], bytes);
		    tbuf[bytes] = '\0';
		    printf("match[%d]: %s\n", match_cnt, tbuf);
		    match_cnt++;
		}
	    }
	    ev = 0;
	} else {
	    printf("No Match, nkn_regexec_match(), rv=%d, errbuf=[%s]\n", 
	    	   rv, errbuf);
	    ev = 1;
	}
    } else {
    	rv = nkn_regexec(regctx, opt_data, errbuf, sizeof(errbuf));
    	if (!rv) {
	    if (opt_verbose) {
            	printf("Match\n");
	    }
	    ev = 0;
    	} else {
	    if (opt_verbose) {
            	printf("No Match, nkn_regexec(), rv=%d, errbuf=[%s]\n", 
		       rv, errbuf);
	    }
	    ev = 1;
    	}
    }
    break;

    /**************************************************************************/
    } /* End of while */

    if (regctx) {
	nkn_regex_ctx_free(regctx);
    }
    exit(ev);
}

/*
 * End of nkn_regex.c
 */

/* Function is called in libnkn_memalloc, but we don't care about this. */
extern int nkn_mon_add(const char *name_tmp, char *instance, void *obj,
		       unsigned int size);
int nkn_mon_add(const char *name_tmp __attribute((unused)),
		char *instance __attribute((unused)),
		void *obj __attribute((unused)),
		unsigned int size __attribute((unused)))
{
	return 0;
}
