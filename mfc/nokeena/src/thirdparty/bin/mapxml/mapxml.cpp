//
// Copyright (c) 2014 by Juniper Networks, Inc
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

const char *option_str = "hi:o:t:v";

static struct option long_options[] = {
    {"help", 1, 0, 'h'},
    {"input", 1, 0, 'i'},
    {"output", 1, 0, 'o'},
    {"type", 1, 0, 't'},
    {"verbose", 0, 0, 'v'},
    {0, 0, 0, 0}
};

const char *helpstr = (
"Convert XML to binary format.\n"
"Options:\n"
"\t-h --help\n"
"\t-i --input (input file)\n"
"\t-o --output (output file)\n"
"\t-t --type (XML root element)\n"
"\t-v --verbose\n"
);

const char *version_str = "version 1.1";

int opt_help = 0;
char *opt_input = 0;
char *opt_output = 0;
char *opt_type = 0;
int opt_verbose = 0;

int HostOriginMap_handler(const char *input, const char *output);
extern const char* HostOriginMapDTD;

int ClusterMap_handler(const char *input, const char *output);
extern const char* ClusterMapDTD;

int OriginEscalationMap_handler(const char *input, const char *output);
extern const char* OriginEscalationMapDTD;

int OriginRoundRobinMap_handler(const char *input, const char *output);
extern const char* OriginRoundRobinMapDTD;

typedef struct XML_handler {
    const char* type;
    int (*handler)(const char* input, const char* output);
    const char* DTDdata;
} XML_handler_t;

XML_handler_t XML_type_handler[5] = {
    {"HostOriginMap", HostOriginMap_handler, HostOriginMapDTD},
    {"ClusterMap", ClusterMap_handler, ClusterMapDTD},
    {"OriginEscalationMap", OriginEscalationMap_handler, 
     OriginEscalationMapDTD},
    {"OriginRoundRobinMap", OriginRoundRobinMap_handler, 
     OriginRoundRobinMapDTD},
    {0, 0, 0} // always last
};

void print_help(const char *progname)
{
    printf("[%s]: %s\n%s\n", progname, version_str, helpstr);
}

int validateXML(const char *input_file, const char *DTDdata)
{
    int ret = 0;
    char *XMLdata;
    xmlDocPtr doc = 0;
    xmlDtdPtr dtd = 0;
    xmlValidCtxtPtr cvp = 0;
    xmlParserInputBufferPtr bp;

    doc = xmlParseFile(input_file);
    if (!doc) {
    	printf("xmlParseFile(\"%s\") failed\n", input_file);
	ret = 1;
	goto exit;
    }

    bp = xmlParserInputBufferCreateMem(DTDdata, strlen(DTDdata), 
    				       XML_CHAR_ENCODING_NONE);
    dtd = xmlIOParseDTD(NULL, bp, XML_CHAR_ENCODING_NONE);
    if (!dtd) {
    	printf("xmlIOParseDTD() failed\n");
	ret = 2;
	goto exit;
    }

    cvp = xmlNewValidCtxt();
    if (!cvp) {
    	printf("xmlNewValidCtxt() failed\n");
	ret = 3;
	goto exit;
    }
    cvp->userData = (void*)stderr;
    cvp->error = (xmlValidityErrorFunc)fprintf;
    cvp->warning = (xmlValidityWarningFunc)fprintf; 

    if (!xmlValidateDtd(cvp, doc, dtd)) {
    	printf("xmlValidateDtd() failed\n");
	ret = 4;
	goto exit;
    }

    ret = 0;

exit:

    if (doc) {
    	xmlFreeDoc(doc);
    }
    if (dtd) {
    	xmlFreeDtd(dtd);
    }
    if (cvp) {
    	xmlFreeValidCtxt(cvp);
    }
    return ret;
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

    for (int n = 0; XML_type_handler[n].type; n++) {
    	if (strcmp(opt_type, XML_type_handler[n].type)) {
	    continue;
	}
	if (XML_type_handler[n].DTDdata) {
	    // Validate XML against DTD
	    rv = validateXML(opt_input, XML_type_handler[n].DTDdata);
	    if (rv) {
	    	printf("[%s]: \"%s\" XML validation failed, ret=%d\n", 
		       argv[0], XML_type_handler[n].type, rv);
	    	exit(1);
	    }
	}

        rv = XML_type_handler[n].handler(opt_input, opt_output);
	if (rv) {
	    printf("[%s]: \"%s\" handler failed, ret=%d\n", 
	    	   argv[0], XML_type_handler[n].type, rv);
	    exit(2);
	}
	exit(0);
    }
    printf("[%s]: No handler found\n", argv[0]);
    exit(2);
}

/*
 * End of mapxml.cpp
 */
