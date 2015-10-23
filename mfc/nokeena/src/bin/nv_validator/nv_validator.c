#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

extern int yyparse(void);
int read_in_string( char *buffer, int *num_bytes, int max_bytes);

const char *option_str = "d:hv";
char *opt_data=0;
int verbose = 0;

static struct option long_options[] = {
	{"data", 1, 0, 'd'},
    {"help", 1, 0, 'h'},
    {"verbose", 0, 0, 'v'},
    {0, 0, 0, 0}
};

const char *helpstr = 
"Evaluate N-V pair(s) against input.\n\n"
"Options:\n"
"\t-d --data\n"
"\t-h --help\n"
"\t-v --verbose\n"
"\n"
"Examples:\n"
"  nknparser -d '(http.req.custom.flvseek_offset,$1),(http.req.custom.range_start,$2),(http.req.custom.range_end,$3)'\n"
"  nknparser -v -d '(http.req.custom.flvseek_offset,$1)'\n";

static int g_read_offset;
char input_string[256];

int main(int argc, char *argv[]) {
    char c;
    int option_index = 0;

    g_read_offset = 0;
	// Process command line arguments
	while(1) {
    	c = getopt_long(argc, argv, option_str, long_options, &option_index);
		if (c == -1) {
			break;
		}
    	switch (c) {
			case 'd':
				opt_data = optarg;
				break;
   			case 'h':
        		printf("[%s]: \n%s\n", argv[0], helpstr);
        		exit(100);
    		case 'v':
        		verbose = 1;
        		break;
    	}
	}

	if (!opt_data) {
		printf("[%s]: \n%s\n", argv[0], helpstr);
		exit(100);
	}

    strncpy(input_string, opt_data,256);
    (verbose) ? printf("Argv=%s\n", opt_data) : 0;

	// Call the (N,V) parser
    yyparse();
    return 0;
}

int read_in_string( char *buffer, int *num_bytes, int max_bytes) {
    int bytes_to_read = max_bytes;
    int bytes_remaining = strlen(input_string)-g_read_offset;
    int i;
    if (bytes_to_read > bytes_remaining) {
                bytes_to_read = bytes_remaining;
        }
    for (i=0;i<bytes_to_read; i++) {
        buffer[i] = input_string[g_read_offset+i];
    }
    *num_bytes = bytes_to_read;
    g_read_offset += bytes_to_read;
    return 0;
}

