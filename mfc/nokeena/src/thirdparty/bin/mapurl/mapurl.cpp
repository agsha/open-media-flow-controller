//
// (C) Copyright 2014, Juniper Networks, Inc.
// All rights reserved.
//

// mapurl.cpp -- map URL data to binary format
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

const char *option_str = "hi:k:o:t:v";

static struct option long_options[] = {
    {"help", 1, 0, 'h'},
    {"input", 1, 0, 'i'},
    {"key", 1, 0, 'k'},
    {"output", 1, 0, 'o'},
    {"type", 1, 0, 't'},
    {"verbose", 0, 0, 'v'},
    {0, 0, 0, 0}
};

const char *helpstr = (
"Convert URL data to binary format.\n"
"Options:\n"
"\t-h --help\n"
"\t-i --input (input file)\n"
"\t-k --key (decryption key)\n"
"\t-o --output (output file)\n"
"\t-t --type [url|iwf|calea]\n"
"\t-v --verbose\n"
);

const char *version_str = "version 1.0";

int opt_help = 0;
char *opt_input = 0;
char *opt_key = 0;
char *opt_output = 0;
char *opt_type = 0;
int opt_verbose = 0;

typedef struct URL_handler {
    const char* type;
    int (*handler)(const char* input, const char* output);
} URL_handler_t;

int URLfmt_handler(const char *input, const char *output);
int IWFfmt_handler(const char *input, const char *output);
int CALEAfmt_handler(const char *input, const char *output);

URL_handler_t URL_type_handler[4] = {
    {"url", URLfmt_handler},
    {"iwf", IWFfmt_handler},
    {"calea", CALEAfmt_handler},
    {0, 0}
};

void print_help(const char *progname)
{
    printf("[%s]: %s\n%s\n", progname, version_str, helpstr);
}

int main(int argc, char **argv)
{
    int c;
    int option_index = 0;
    int rv;

    while (1) {
	c = getopt_long(argc, argv, option_str, long_options, &option_index);
	if (c == -1) {
	    break;
	}
	switch (c) {
	case 'h':
	    opt_help = 1;
	    break;
	case 'i':
	    opt_input = optarg;
	    break;
	case 'k':
	    opt_key = optarg;
	    break;
	case 'o':
	    opt_output = optarg;
	    break;
	case 't':
	    opt_type = optarg;
	    break;
	case 'v':
	    opt_verbose = 1;
	    break;
	default:
	    print_help(argv[0]);
	    exit(10);
	    break;
	}
    }

    if (!opt_input || !opt_output || !opt_type) {
	print_help(argv[0]);
	exit(11);
    }

    for (int n = 0; URL_type_handler[n].type; n++) {
    	if (!strcmp(opt_type, URL_type_handler[n].type)) {
	    rv = URL_type_handler[n].handler(opt_input, opt_output);
	    if (rv) {
		printf("[%s]: Invalid data, \"%s\" handler failed, ret=%d\n", 
		       argv[0], URL_type_handler[n].type, rv);
		exit(12);
	    }
	    exit(0);
	}
    }
    printf("[%s]: Invalid type \"%s\", no handler found\n", argv[0], opt_type);
    exit(2);
}

//
// End of mapxml.cpp
//
