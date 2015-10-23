
/* nkn_fqueue.c -- File based queue */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <alloca.h>

#define _GNU_SOURCE
#include <getopt.h>

#include "fqueue.h"
#include "http_header.h"
#include "nkn_mem_counter_def.h"

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), \
        memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

static struct option long_options[] = {
    {"create", 1, 0, 'c'},
    {"enqueue", 1, 0, 'e'},
    {"dequeue", 1, 0, 'd'},
    {"elementdump", 1, 0, 'E'},
    {"fullobjectpath", 0, 0, 'f'},
    {"httpheader", 1, 0, 'H'},
    {"help", 0, 0, 'h'},
    {"init", 0, 0, 'i'},
    {"maxqsize", 1, 0, 'm'},
    {"nodelete", 0, 0, 'n'},
    {"name", 1, 0, 'N'},
    {"pathname", 1, 0, 'p'},
    {"queue", 1, 0, 'q'},
    {"queuedump", 1, 0, 'Q'},
    {"uri", 1, 0, 'u'},
    {"verbose", 0, 0, 'v'},
    {"wait", 1, 0, 'w'},
    {"remove", 1, 0, 'r'},
    {"namespace_domain", 1, 0, 's'},
    {0, 0, 0, 0}
};

const char *option_str = "c:e:E:fH:d:hi:m:nN:o:p:q:Q:u:vw:r:s:";

const char *helpstr = {
    "\t-N, --namevalue queue element <name:value> -i optional arg\n"
    "\t-Q, --queuedump <queue file name>\n"
    "\t-E, --elementdump <queue element file name>\n"
    "\t-H, --httpheader <name:value> -i optional arg\n"
    "\t-c, --create <queue filename>\n"
    "\t-e, --enqueue <input data filename>\n"
    "\t-f, --fullobjectpath\n"
    "\t-d, --dequeue <output data filename>\n"
    "\t-r, --remove <URI> remove from cache\n"
    "\t-s, --namespace_domain <domain> domain to associate with remove\n"
    "\t-h, --help\n"
    "\t-i, --init queue element <output filename>\n"
    "\t-m, --maxqsize <blocks> -c optional arg\n"
    "\t-n, --nodelete <no delete data file> -i optional arg\n"
    "\t-p, --pathname <absolute path to data> -i optional arg\n"
    "\t-q, --queue <queue filename>\n"
    "\t-u, --uri <URI used to access object> -i optional arg\n"
    "\t-v, --verbose\n"
    "\t-w, --wait <seconds> -d optional arg\n\n"
    "Create Queue:\n"
    "  nknfqueue -c <output filename> -m 1024\n\n"
    "Create Queue Element:\n"
    "  nknfqueue -i <output filename>\n"
    "    -N \"qattr1:value1\" -N\"qattr2:value2\"\n"
    "    -H \"User-Agent:ua-xyz\" -H\"host:nokeena.com\"\n"
    "    -n -p /tmp/oomgr.xxx/data.1 -u /vod/video/video1\n\n"
    "Enqueue element:\n"
    "  nknfqueue -e <input element filename> -q <queue filename>\n\n"
    "Dequeue element:\n"
    "  nknfqueue -d <output element filename> -q <queue filename> -w 5\n\n"
    "Remove element from cache using domain:\n"
    "  nknfqueue -s <namespace_domain> -r <uri> -q <queue filename>\n\n"
    "Remove element from cache using full object path:\n"
    "  nknfqueue -f -r <full object path> -q <queue filename>\n\n"
    "Dump Queue:\n"
    "  nknfqueue -Q <queue filename>\n\n"
    "Dump Queue Element:\n"
    "  nknfqueue -E <element filename>\n"
};

int action = 0;
#define ACT_CREATEQUEUE		(1 << 0)
#define ACT_ENQUEUE		(1 << 1)
#define ACT_DEQUEUE		(1 << 2)
#define ACT_INITQELEMENT	(1 << 3)
#define ACT_DUMP_QUEUE		(1 << 4)
#define ACT_DUMP_QELEMENT	(1 << 5)
#define ACT_REMOVE		(1 << 6)
#define ACT_ALL	(ACT_CREATEQUEUE|ACT_ENQUEUE|ACT_DEQUEUE|\
		ACT_INITQELEMENT|ACT_DUMP_QUEUE|ACT_DUMP_QELEMENT|ACT_REMOVE)

char *inputfilename = 0;
char *outputfilename = 0;
int help_opt = 0;
char *queuefilename = 0;
int verbose_output = 0;
int dequeue_wait_secs = 0;
int maxqueue_size = 1024;
char *pathname;
char *uri = NULL;
int no_delete = 0;
char *name;
char *value;
mime_header_t httphdr;
mime_header_t fqueue_namevalue;
fqueue_element_t fqueue_element;

void prt_serialized_http_header(const char *serialdata, int serialdatasz);
void prt_fqueue_element(const fqueue_element_t *qe, off_t off);
void prt_fqueue_header(fhandle_t fh, const fqueue_header_t *qh);
int dump_fqueue(const char *l_queuefilename);
int dump_fqueue_element(const char *elementfilename);
int initialize_queue_element(const char *l_outputfilename, int l_no_delete, 
			const char *l_pathname, const char *l_uri, 
			const mime_header_t *l_httphdr, 
			const mime_header_t *fqueue_namevalue);
/* Extern Functions */
extern void disable_http_headers_log(void);

void prt_serialized_http_header(const char *serialdata, int serialdatasz)
{
    int rv;
    mime_header_t hdr;
    int n;
    const char *data;
    int datalen;
    int nth;
    u_int32_t attr;
    int hdrcnt;
    char name_str[FQUEUE_ELEMENTSIZE];
    char value_str[FQUEUE_ELEMENTSIZE];
    const char *l_name;
    int namelen;
    const char *l_value;
    int valuelen;

    init_http_header(&hdr, 0, 0);
    rv = deserialize(serialdata, serialdatasz, &hdr, 0, 0);
    if (rv) {
    	printf("%s:%d deserialize() failed, rv=%d\n",
		__FUNCTION__, __LINE__, rv);
    }
    // printf("\t%s:\r\n", "known http_header");

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
    	rv = get_known_header(&hdr, n, &data, &datalen, &attr, &hdrcnt);
	if (rv) {
	    continue;
	}

	memcpy((void *)value_str, data, datalen);
	value_str[datalen] = '\0';
	printf("\t\t%s:%s", http_known_headers[n].name, value_str);

	for (nth = 1; nth < hdrcnt; nth++) {
    	    rv = get_nth_known_header(&hdr, n, nth, 
	    			&data, &datalen, &attr);
	    if (!rv) {
		memcpy((void *)value_str, data, datalen);
		value_str[datalen] = '\0';
		printf(",%s", value_str);
	    } else {
	    	break;
	    }
	}
	printf("\r\n");
    }

    // printf("\t%s:\r\n", "unknown http_header");
    n = 0;
    while ((rv = get_nth_unknown_header(&hdr, n, &l_name, &namelen,
    			&l_value, &valuelen, &attr)) == 0) {
	memcpy((void *)name_str, l_name, namelen);
	name_str[namelen] = '\0';
        
	memcpy((void *)value_str, l_value, valuelen);
	value_str[valuelen] = '\0';
	printf("\t\t%s:%s\r\n", name_str, value_str);
	n++;
    }
}

void prt_fqueue_element(const fqueue_element_t *qe, off_t off)
{
    int n;
    int num_attrs;
    int rv;
    const char *l_name;
    int namelen;
    const char *data;
    int datalen;
    char namebuf[FQUEUE_ELEMENTSIZE];
    char valuebuf[FQUEUE_ELEMENTSIZE];

    printf("fqueue_element_t (seq: %ld off=0x%lx):\n"
    	   "\tmagic=0x%x\n"
    	   "\tcksum=0x%x\n"
    	   "\tattr_bytes_used=%hd\n"
    	   "\tno_delete_datafile=%s\n"
    	   "\tnum_attrs=%hhd\n",
	   off/FQUEUE_ELEMENTSIZE,
	   off, qe->u.e.magic, qe->u.e.cksum, qe->u.e.attr_bytes_used,
	   qe->u.e.no_delete_datafile ? "yes" : "no",
	   qe->u.e.num_attrs);

    num_attrs = qe->u.e.num_attrs;
    n = 0;
    while (n < num_attrs) {
        rv = get_attr_fqueue_element(qe, 
		((n == 0) ? T_ATTR_PATHNAME : 
			(n == 1) ? T_ATTR_URI : T_ATTR_NAMEVALUE),
		n-2, &l_name, &namelen, &data, &datalen);
	if (!rv) {
	    if (strncasecmp(l_name, "http_header", 11) == 0) {
	    	printf("\t[http_header]:\r\n");
	    	prt_serialized_http_header(data, datalen);
	    } else {
	    	memcpy((void *)namebuf, l_name, namelen);
	    	namebuf[namelen] = '\0';
	    	memcpy((void *)valuebuf, data, datalen);
	    	valuebuf[datalen] = '\0';
	    	printf("\t[%s]:%s\r\n", namebuf, valuebuf);
	    }
	}
	n++;
    }
}

void prt_fqueue_header(fhandle_t fh, const fqueue_header_t *qh)
{
    int rv;
    struct stat statbuf;
    int total_elements = 0;
    off_t off;
    fqueue_element_t el;

    rv = fstat(fh, &statbuf);
    if (!rv) {
    	total_elements = statbuf.st_size / FQUEUE_ELEMENTSIZE;
	total_elements = total_elements ? total_elements - 1 : 0;
    }

    printf("fqueue_header_t:\n"
    	   "\tmagic=0x%x\n"
    	   "\tcksum=0x%x\n"
    	   "\tmax_entries=%d\n"
    	   "\thead=%d\n"
    	   "\ttotal_elements=%d\n",
	   qh->u.h.magic, qh->u.h.cksum, qh->u.h.max_entries, qh->u.h.head,
	   total_elements);

    off = FQUEUE_ELEMENTSIZE;
    while (total_elements--) {
    	rv = get_fqueue_element_fh(fh, off, &el);
	if (!rv) {
	    prt_fqueue_element(&el, off);
	} else {
	    return;
	}
        off += FQUEUE_ELEMENTSIZE;
    }
}

int dump_fqueue(const char *l_queuefilename)
{
    int rv;
    fqueue_header_t hdr;
    fhandle_t fh = -1;

    fh = open_queue_fqueue(l_queuefilename);
    rv = get_fqueue_header_fh(fh, &hdr);
    if (!rv) {
    	prt_fqueue_header(fh, &hdr);
    } else {
        rv = 1;
    }

    if (fh >= 0) {
    	close_queue_fqueue(fh);
    }
    return rv;
}

int dump_fqueue_element(const char *elementfilename)
{
    int rv;
    fqueue_element_t el;

    rv = get_fqueue_element(elementfilename, &el);
    if (!rv) {
    	prt_fqueue_element(&el, 0);
    }
    return 0;
}

int initialize_queue_element(const char *l_outputfilename, int l_no_delete, 
			const char *l_pathname, const char *l_uri, 
			const mime_header_t *l_httphdr, 
			const mime_header_t *l_fqueue_namevalue)
{
    int rv;
    int cnt_known_headers, cnt_unknown_headers, cnt_namevalue_headers; 
    int cnt_querystring_headers;
    int serialbufsz;
    char *serialbuf;
    const char *l_name;
    int namelen;
    const char *l_value;
    int valuelen;
    u_int32_t attrs;
    int fd;
    int n;

    rv = init_queue_element(&fqueue_element, l_no_delete, l_pathname, l_uri);
    if (rv) {
    	printf("%s:%d init_queue_element() failed, rv=%d\n",
		__FUNCTION__, __LINE__, rv);
	return 1;
    }

    // Add HTTP headers
    cnt_known_headers = 0;
    cnt_unknown_headers = 0;
    cnt_namevalue_headers = 0; 
    cnt_querystring_headers = 0;
    rv = get_header_counts(l_httphdr, &cnt_known_headers, &cnt_unknown_headers,
    			&cnt_namevalue_headers, &cnt_querystring_headers);
    if (cnt_known_headers || cnt_unknown_headers) {
    	serialbufsz = serialize_datasize(l_httphdr);
	serialbuf = alloca(serialbufsz);
	rv = serialize(l_httphdr, serialbuf, serialbufsz);
	if (rv) {
    	    printf("%s:%d serialize() failed, rv=%d\n",
		__FUNCTION__, __LINE__, rv);
	    return 2;
	}
    	rv = add_attr_queue_element_len(&fqueue_element, "http_header", 11,
				serialbuf, serialbufsz);
	if (rv) {
    	    printf("%s:%d add_attr_queue_element_len() failed, rv=%d\n",
		__FUNCTION__, __LINE__, rv);
	    return 3;
	}
    }

    // Add queue element name value pairs
    cnt_unknown_headers = 0;
    rv = get_header_counts(l_fqueue_namevalue, &cnt_known_headers, 
    			&cnt_unknown_headers, &cnt_namevalue_headers, 
			&cnt_querystring_headers);
    if (cnt_unknown_headers) {
      	n = 0;
        while ((rv = get_nth_unknown_header(l_fqueue_namevalue, n, 
			&l_name, &namelen, &l_value, &valuelen, &attrs)) == 0) {
	    rv = add_attr_queue_element_len(&fqueue_element, l_name, namelen,
	    		l_value, valuelen);
	    if (rv) {
    	        printf("%s:%d add_attr_queue_element_len() failed, rv=%d\n",
		    __FUNCTION__, __LINE__, rv);
	        return 4;
	    }
	    n++;
	}
    }
    if (l_outputfilename) {
    	fd = open(l_outputfilename, O_CREAT|O_RDWR|O_TRUNC, 0666);
    	n = write(fd, &fqueue_element, sizeof(fqueue_element));
    	close(fd);
    	if (n != sizeof(fqueue_element)) {
	    printf("%s:%d write() failed, sent=%d expected=%ld\n",
		    __FUNCTION__, __LINE__, n, sizeof(fqueue_element));
	    return 4;
    	}
    }

    return 0;
}

int main(int argc, char **argv)
{
    int c;
    int option_index = 0;
    int rv;
    const char *p_name, *p_name_end;
    const char *p_value, *p_value_end;
    int known_hdr_enum;
    char * namespace_domain = NULL;
    int is_object_fullpath = 0;

    mime_hdr_startup(); // Init mime_hdr structures (one time process init)

    init_http_header(&httphdr, 0, 0);
    init_http_header(&fqueue_namevalue, 0, 0);
    disable_http_headers_log();	/* We do not want debug logs going into syslog */

    while (1) {
	c = getopt_long(argc, argv, option_str, long_options, &option_index);
	if (c == -1) {
	    break;
	}

	switch (c) {
	case 'c':
	    action |= ACT_CREATEQUEUE;
	    queuefilename = optarg;
	    if (!queuefilename) {
	    	printf("--[%s] missing arg\n", long_options[option_index].name);
		_exit(1);
	    }
	    break;

	case 'e':
	    action |= ACT_ENQUEUE;
	    inputfilename = optarg;
	    if (!inputfilename) {
	    	printf("--[%s] missing arg\n", long_options[option_index].name);
		_exit(1);
	    }
	    break;

	case 'E':
	    action |= ACT_DUMP_QELEMENT;
	    inputfilename = optarg;
	    if (!inputfilename) {
	    	printf("--[%s] missing arg\n", long_options[option_index].name);
		_exit(1);
	    }
	    break;

	case 'f':
	    is_object_fullpath = 1;
	    break;

	case 'H':
	    p_name = optarg;
	    p_name_end = strchr(p_name, ':');
	    if (!p_name_end) {
	    	printf("--[%s] invalid \"name:value\" [%s] arg\n", 
			long_options[option_index].name, optarg);
	    }
	    p_value = p_name_end + 1;
	    p_value_end = p_value + strlen(p_value);
	    rv = string_to_known_header_enum(p_name, p_name_end-p_name, 
	    				&known_hdr_enum);
	    if (!rv) {
	    	rv = add_known_header(&httphdr, known_hdr_enum,
				p_value, p_value_end-p_value);
		if (rv) {
		    printf("-H %s, add_known_header() failed rv=%d\n",
		    	optarg, rv);
		    _exit(1);
		}
	    } else {
	    	rv = add_unknown_header(&httphdr, p_name, p_name_end-p_name,
				p_value, p_value_end-p_value);
		if (rv) {
		    printf("-H %s, add_unknown_header() failed rv=%d\n",
		    	optarg, rv);
		    _exit(1);
		}
	    }
	    break;

	case 'd':
	    action |= ACT_DEQUEUE;
	    outputfilename = optarg;
	    if (!outputfilename) {
	    	printf("--[%s] missing arg\n", long_options[option_index].name);
		_exit(1);
	    }
	    break;

	case 'r':
	    action |= ACT_REMOVE;
	    uri = optarg;
	    if (!uri) {
	    	printf("--[%s] missing arg\n", long_options[option_index].name);
		_exit(1);
	    }
	    break;

	case 's':
	    action |= ACT_REMOVE;
	    namespace_domain = optarg;
	    if (!namespace_domain) {
	    	printf("--[%s] missing arg\n", long_options[option_index].name);
		_exit(1);
	    }
	    break;


	case 'h':
	    help_opt = 1;
	    break;

	case 'i':
	    action |= ACT_INITQELEMENT;
	    outputfilename = optarg;
	    if (!outputfilename) {
	    	printf("--[%s] missing arg\n", long_options[option_index].name);
		_exit(1);
	    }
	    break;

	case 'm':
	    maxqueue_size = atoi(optarg);
	    break;

	case 'n':
	    no_delete = 1;
	    break;

	case 'N':
	    p_name = optarg;
	    p_name_end = strchr(p_name, ':');
	    if (!p_name_end) {
	    	printf("--[%s] invalid queue \"name:value\" [%s] arg\n", 
			long_options[option_index].name, optarg);
	    }
	    p_value = p_name_end + 1;
	    p_value_end = p_value + strlen(p_value);
	    rv = add_unknown_header(&fqueue_namevalue, 
	    			p_name, p_name_end-p_name,
			    	p_value, p_value_end-p_value);
	    if (rv) {
		printf("-N %s, add_unknown_header() failed rv=%d\n",
		    optarg, rv);
		_exit(1);
	    }
	    break;

	case 'p':
	    pathname = optarg;
	    break;
	    
	case 'Q':
	    action |= ACT_DUMP_QUEUE;
	    // Fall through
	case 'q':
	    queuefilename = optarg;
	    if (!queuefilename) {
	    	printf("--[%s] missing arg\n", long_options[option_index].name);
		_exit(1);
	    }
	    break;

	case 'u':
	    uri = optarg;
	    break;

	case 'v':
	    verbose_output = 1;
	    break;

	case 'w':
	    dequeue_wait_secs = atoi(optarg);
	    break;

	default:
	    break;
	}
    }

    if (help_opt || (argc <= 1) || !(action & ACT_ALL) ||
        ((action & (ACT_CREATEQUEUE|ACT_ENQUEUE|ACT_DEQUEUE|ACT_REMOVE) && 
		!queuefilename))) {
    	printf("[%s]: \n%s\n", argv[0], helpstr);
	_exit(1);
    }

    switch(action & ACT_ALL) {
    case ACT_CREATEQUEUE:
    	rv = create_fqueue(queuefilename, maxqueue_size);
	break;
    case ACT_ENQUEUE:
    	rv = enqueue_fqueue(queuefilename, inputfilename);
	break;
    case ACT_DEQUEUE:
    	rv = dequeue_fqueue(queuefilename, outputfilename, dequeue_wait_secs);
	break;
    case ACT_REMOVE:
	if(!uri || (!is_object_fullpath && !namespace_domain)) {
    		printf("[%s]: \n%s\n", argv[0], helpstr);
		_exit(1);
	}
        rv = initialize_queue_element(0 /* no outfile */, 1 /* no_delete */, 
				"/tmp/ignore", uri, 
				&httphdr, &fqueue_namevalue);
	if (rv) {
	    rv += 100;
	    break;
	}
    	rv = enqueue_remove(&fqueue_element, queuefilename, uri, 
			    namespace_domain, is_object_fullpath);
	break;
    case ACT_INITQELEMENT:
        rv = initialize_queue_element(outputfilename, no_delete, 
				pathname, uri, &httphdr, &fqueue_namevalue);
    	break;
    case ACT_DUMP_QUEUE:
    	rv = dump_fqueue(queuefilename);
    	break;
    case ACT_DUMP_QELEMENT:
    	rv = dump_fqueue_element(inputfilename);
    	break;
    default:
    	printf("[%s]: error, multiple actions requested\n", argv[0]);
	_exit(1);
    }
    if (rv) {
    	printf("[%s]: operation failed, status=%d\n", argv[0], rv);
    	_exit(1);
    }
    return 0;
}

/*
 * End of nkn_fqueue.c
 */

/* Function is called in libnkn_memalloc, but we don't care about this. */
extern int nkn_mon_add(const char *name_tmp, char *instance, void *obj,
		       uint32_t size);
int nkn_mon_add(const char *name_tmp __attribute((unused)),
		char *instance __attribute((unused)),
		void *obj __attribute((unused)),
		uint32_t size __attribute((unused)))
{
	return 0;
}
