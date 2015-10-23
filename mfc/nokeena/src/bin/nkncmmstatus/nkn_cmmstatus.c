/*
 * nkn_cmmstatus.c - Dump CMM node status information
 */
#include <stdio.h>
#include <glib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/param.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <poll.h>
#include <alloca.h>
#include <errno.h>

#include "nkn_mem_counter_def.h"
#include "nkn_cmm_shm.h"
#include "nkn_cmm_shm_api.h"

#define _GNU_SOURCE
#include <getopt.h>

#define DBG(_fmt, ...) { \
    if (opt_verbose) { \
       fprintf(stdout, "[%s:%d]: ", __FUNCTION__, __LINE__); \
       fprintf(stdout, (_fmt), __VA_ARGS__); \
       fprintf(stdout, "\n"); \
    } \
}

const char *option_str = "d:hi:n:rv";

static struct option long_options[] = {
    {"delay", 1, 0, 'd'},
    {"help", 0, 0, 'h'},
    {"iter", 1, 0, 'i'},
    {"namespace", 1, 0, 'n'},
    {"raw", 0, 0, 'r'},
    {"verbose", 0, 0, 'v'},
    {0, 0, 0, 0}
};

const char *helpstr = (
"CMM node status.\n\n"
"Options:\n"
"\t-d --delay (update delay in seconds)\n"
"\t-h --help\n"
"\t-i --iter (iterations to dump and exit)\n"
"\t-n --namespace [namespace name]\n"
"\t-r --raw\n"
"\t-v --verbose\n"
);

const char *invalid_data_str = "????????????????????????????????????????????????????????????????????????????????";

int opt_delay = 1;
long opt_iterations = -1;
int opt_raw = 0;
int opt_verbose = 0;
char *opt_namespace = 0;
int opt_namespace_strlen = 0;

cmm_shm_hdr_t *cmm_hdr = (cmm_shm_hdr_t *)-1;
GHashTable *ht_nodehandle_to_chan;
GHashTable *ht_nodehandle_to_rawdata;
int update_collisions;
int update_collision_fails;

static void exit_proc(void)
{
    if (cmm_hdr != (cmm_shm_hdr_t *) -1) {
    	shmdt(cmm_hdr);
    }
    return;
}

static long get_node_loadmetric(const char *node, uint64_t *version)
{
    int rv;
    void *orig_key;
    cmm_shm_chan_t *chan;
    const char *key;
    long ret_val = 0;
    long *data;
    int datalen;

    if (!node) {
	*version = -1;
	return -1;
    }

    if (!g_hash_table_lookup_extended(ht_nodehandle_to_chan, 
    				      (gconstpointer)node,
				      (gpointer)&orig_key, (gpointer)&chan)) {
	chan = (cmm_shm_chan_t *)calloc(1, sizeof(cmm_shm_chan_t));
	if (!chan) {
	    printf("%s:%d calloc() failed\n", __FUNCTION__, __LINE__);
	    exit(200);
	}
	key = strdup(node);
	g_hash_table_insert(ht_nodehandle_to_chan, (gpointer)key, 
			    (gpointer)chan);
	rv = cmm_shm_open_chan(node, CMMSHM_CH_LOADMETRIC, chan);
    }
    rv = cmm_shm_chan_getdata_ptr(chan, (void **)&data, &datalen, version);
    if (!rv && (datalen == sizeof(long))) {
    	ret_val = *data;
    } else {
    	rv = cmm_shm_open_chan(node, CMMSHM_CH_LOADMETRIC, chan);
	ret_val = -1;
	*version = -1;
    }
    return ret_val;
}

static char *get_value(const char *buf, const char *name, int *val_len)
{
    char *p;
    char *p_end;
    char *rp;
    int size;
    int namelen;

    if (!name) {
    	return 0;
    }
    namelen = strlen(name);

    p = strstr(buf, name);
    if (p && *(p + namelen) == '=') {
    	p += (namelen+1);
	p_end = strchr(p, '|');
	if (p_end) {
	    *val_len = (p_end - p);
	    return p;
	}
    } 
    return 0;
}

static int rawdata_to_node(const char *rawbuf, char *nodebuf, int nodebufsize)
{
    char *node;
    int vallen;
    int len;

    node = get_value(rawbuf, CMM_NM_NODE, &vallen);
    if (node) {
    	if ((vallen + 1) <= nodebufsize) {
	    len = vallen;
	    memcpy(nodebuf, node, len);
	    nodebuf[len] = '\0';
	    return 0; // Success
	} else {
	    return 1;
	}
    }
    return 2;
}

static int save_rawdata(const char *rawdata, int rawdatasize) 
{
    int rv;
    char *node;
    const char *key;
    void *orig_key;
    char *last_rawdata;

    node = alloca(rawdatasize);
    rv = rawdata_to_node(rawdata, node, rawdatasize);
    if (rv) {
    	return 1;
    }

    rv = g_hash_table_lookup_extended(ht_nodehandle_to_rawdata, 
    				      (gconstpointer)node,
				      (gpointer)&orig_key, 
				      (gpointer)&last_rawdata);
    if (!rv) {
    	last_rawdata = malloc(rawdatasize);
	if (!last_rawdata) {
	    return 2;
	}
	key = strdup(node);
	g_hash_table_insert(ht_nodehandle_to_rawdata, (gpointer)key, 
			    (gpointer)last_rawdata);
    }
    memcpy(last_rawdata, rawdata, rawdatasize);
    return 0;
}

static const char *get_last_rawdata(const char *rawdata, int rawdatasize)
{
    int rv;
    char *node;
    void *orig_key;
    char *last_rawdata;

    node = alloca(rawdatasize);
    rv = rawdata_to_node(rawdata, node, rawdatasize);
    if (rv) {
    	return 0;
    }

    rv = g_hash_table_lookup_extended(ht_nodehandle_to_rawdata, 
    				      (gconstpointer)node,
				      (gpointer)&orig_key, 
				      (gpointer)&last_rawdata);
    if (rv) {
    	return (const char *)last_rawdata;
    } else {
    	return 0;
    }
}

static int
print_short_format(char *buf)
{
#define MKSTR(_str) (_str) ? (_str) : ""

    long lm;
    uint64_t version;
    int vallen;
    char *namespace = get_value(buf, CMM_NM_NAMESPACE, &vallen);
    char *node = get_value(buf, CMM_NM_NODE, &vallen);
    char *state = get_value(buf, CMM_NM_STATE, &vallen);
    char *op_ok = get_value(buf, CMM_NM_OP_SUCCESS, &vallen);
    char *op_to = get_value(buf, CMM_NM_OP_TIMEOUT, &vallen);
    char *op_other = get_value(buf, CMM_NM_OP_OTHER, &vallen);
    char *op_unexp = get_value(buf, CMM_NM_OP_UNEXPECTED, &vallen);
    char *op_on = get_value(buf, CMM_NM_OP_ONLINE, &vallen);
    char *op_off = get_value(buf, CMM_NM_OP_OFFLINE, &vallen);
    char *lookup = get_value(buf, CMM_NM_LOOKUP_TIME, &vallen);
    char *conn = get_value(buf, CMM_NM_CONNECT_TIME, &vallen);
    char *prexfer = get_value(buf, CMM_NM_PREXFER_TIME, &vallen);
    char *startxfer = get_value(buf, CMM_NM_STARTXFER_TIME, &vallen);
    char *tot = get_value(buf, CMM_NM_TOTAL_TIME, &vallen);
    char *p = buf;

    while ((p = strchr(p, '|'))) {
    	*p = '\0';
	p++;
    }

    lm = get_node_loadmetric(node, &version);
    printf("%s:%s:[%s] ok:%s to:%s ot:%s un:%s on:%s off:%s "
	   "lookup:%s conn:%s pre:%s start:%s tot:%s load=%ld vers=%ld\n",
	   MKSTR(namespace),
	   MKSTR(node), MKSTR(state), MKSTR(op_ok), MKSTR(op_to),
	   MKSTR(op_other), MKSTR(op_unexp), MKSTR(op_on), MKSTR(op_off),
	   MKSTR(lookup), MKSTR(conn), MKSTR(prexfer), MKSTR(startxfer),
	   MKSTR(tot), lm, version);
    return 0;

#undef MKSTR
}

int main(int argc, char **argv)
{
    int rv;
    int ev = 0;
    int c;
    int option_index = 0;
    int shmid;
    cmm_segment_t *cmm_seg;
    int n;
    char *buf;
    long iterations = 0;

    time_t time_val;
    struct tm time_tm;
    char timebuf[1024];
    long lm;
    uint64_t version;
    int vallen;
    int retries;
    struct pollfd null_pfd;
    const char *last_rawdata;

    while (1) {
	c = getopt_long(argc, argv, option_str, long_options, &option_index);
	if (c == -1) {
	    break;
	}
	switch (c) {
	case 'd':
	    opt_delay = atoi(optarg);
	    break;
	case 'h':
	    printf("[%s]: \n%s\n", argv[0], helpstr);
	    exit(100);
	case 'i': 
	    opt_iterations = atoi(optarg);
	    break;
	case 'n': 
	    opt_namespace = optarg;
	    opt_namespace_strlen = strlen(opt_namespace);
	    break;
	case 'r':
	    opt_raw = 1;
	    break;
	case 'v':
	    opt_verbose = 1;
	    break;
	case '?':
	    printf("[%s]: \n%s\n", argv[0], helpstr);
	    exit(200);
	}
    }

    /* Setup process exit proc */
    atexit(exit_proc);

    /* Initialize hash tables */
    ht_nodehandle_to_chan = g_hash_table_new(g_str_hash, g_str_equal);
    ht_nodehandle_to_rawdata = g_hash_table_new(g_str_hash, g_str_equal);


    while(1) {
    /**************************************************************************/

    shmid = shmget(CMM_SHM_KEY, CMM_SHM_SIZE, 0);
    if (shmid == -1) {
    	DBG("shmget() failed, shmid=%d errno=%d", shmid, errno);
    	sleep(1); // delay and retry
	continue;
    }
    cmm_hdr = (cmm_shm_hdr_t *)shmat(shmid, (void *)0, SHM_RDONLY);
    if (cmm_hdr == (cmm_shm_hdr_t *) -1) {
    	DBG("shmat() failed, shmid=%d addr=%p errno=%d", shmid, cmm_hdr, errno);
	ev = 1;
	break;
    }
    
    if ((cmm_hdr->cmm_magic != CMM_SHM_MAGICNO) ||
    	(cmm_hdr->cmm_version != CMM_SHM_VERSION)) {
	printf("Bad shared memory header, magicno=0x%x 0x%x version=%d %d\n",
	       cmm_hdr->cmm_magic, CMM_SHM_MAGICNO,
	       cmm_hdr->cmm_version, CMM_SHM_VERSION);
	ev = 2;
	break;
    }

    buf = alloca(cmm_hdr->cmm_segment_size+1);

    while (1) {
	time(&time_val);
	localtime_r(&time_val, &time_tm);
	asctime_r(&time_tm, timebuf);
	printf("\n%s", timebuf);

    	for (n = 0; n < cmm_hdr->cmm_segments; n++) {
	    if (isset(cmm_hdr->cmm_bitmap, n)) {
		cmm_seg = CMM_SEG_PTR(cmm_hdr, n);

		for (retries = 100; retries; retries--) {
		    memcpy(buf, cmm_seg, cmm_hdr->cmm_segment_size);
		    buf[cmm_hdr->cmm_segment_size] = '\0';
		    if (!get_value(buf, CMM_NM_STATE, &vallen)) {
		    	// Update pending, retry
			poll(&null_pfd, 0, 10); // delay 10msec
			update_collisions++;
			continue;
		    } else {
		    	break;
		    }
		}

		if (retries) { // Valid entry
		    save_rawdata(buf, cmm_hdr->cmm_segment_size);
		} else {
		    last_rawdata = get_last_rawdata(buf, 
						    cmm_hdr->cmm_segment_size);
		    if (last_rawdata) {
		    	memcpy(buf, last_rawdata, cmm_hdr->cmm_segment_size);
		    } else {
		        strncpy(buf, invalid_data_str, 
				cmm_hdr->cmm_segment_size);
		    }
		    update_collision_fails++;
		}

		if (opt_namespace && opt_namespace_strlen) {
		    char *ns;
		    ns = get_value(buf, CMM_NM_NAMESPACE, &vallen);
		    if (!ns || (vallen != opt_namespace_strlen) || 
		    	bcmp(opt_namespace, ns, opt_namespace_strlen)) {
			continue;
		    }
		}

		if (opt_raw) {
		    char *node;
		    long load_metric;

		    printf("%s\n", buf);
		    node = get_value(buf, CMM_NM_NODE, &vallen);
		    if (node && vallen) {
		    	node[vallen] = '\0';
		        lm = get_node_loadmetric(node, &version);
			printf("[%s] load_metric=%ld version=%ld\n", 
			       node, lm, version);
		    }
		} else {
		    print_short_format(buf);
		}
	    }
	}
	sleep(opt_delay);
	iterations++;
	if ((opt_iterations > 0) && (iterations == opt_iterations)) {
	    break;
	}
    }

    break;

    /**************************************************************************/
    } /* End of while */

    if (cmm_hdr != (cmm_shm_hdr_t *) -1) {
    	shmdt(cmm_hdr);
    }
    exit(ev);
}

/*
 * End of nkn_cmmstatus.c
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
