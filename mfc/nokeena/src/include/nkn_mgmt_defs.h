#ifndef NKN_MGMT_DEFS_H
#define NKN_MGMT_DEFS_H

/*
 * This file contains list of all the scripts, files used by any of mgmt
 * module
 */

#include <sys/stat.h>


typedef char node_name_t[512];	    /* for usage of "/nkn/nvsd/cluster/config/%s/cluster-type" */
typedef char file_path_t[256];	    /* for any system directory/file path "/proc/sys/vm/drop_caches" */
typedef char str_value_t[128];	    /* for values which needs to be converted to string. */
typedef char wc_name_t[128];	    /* for wildcard names/values */
typedef char error_msg_t[256];	    /* for any message (success/failure). 
				       In nvsd/mgmtd/gmgmtd/cli or any other daemon */
typedef char temp_buf_t[512];	    /* for any other usage, which doesn't fits in other typdefs.  */
typedef char counter_name_t[128];   /* for usage of accessing counter names
				       e.g ns.mfc_probe.rtsp.server.resp_5xx */

#define JNPR_PROC_FILE_MODE  (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) 

#define DICTIONARY_PRELOAD_FILE		"/config/nkn/cache.no_preread"

#define VALID_LICENSE_MAXCONN   "132000"
#define INVALID_LICENSE_MAXCONN "10"
#define VALID_LICENSE_MAXBW     "0"
#define INVALID_LICENSE_MAXBW   "200"

#define GLOBAL_RSRC_POOL        "global_pool"
#define    STR_TMP_MAX           256
#define    PROBE_URI            "http://127.0.0.1/mfc_probe/mfc_probe_object.dat"
#define    PROBE_OBJ            "/mfc_probe/mfc_probe_object.dat"
#define    PROBE_OBJ_STRLEN     31

#define	MAX_CLUSTER_MAPS	256

#define MAX_SERVER_MAP_NAMELEN	16
#define MAX_FILTER_MAP_NAMELEN	16
#define MAX_DEVICE_MAP_NAMELEN	32

#define   SSLD_CERT_PATH         "/config/nkn/cert/"
#define	  SSLD_KEY_PATH		 "/config/nkn/key/"
#define	  SSLD_CSR_PATH		 "/config/nkn/csr/"
#define	  SSLD_CA_PATH 	 	"/config/nkn/ca/"
#define   SSLD_CERT_VERIFY       "/opt/nkn/bin/ssld_verify_cert.sh"
#define   NKN_NAME_ILLEGAL_CHARS	" ^#+=-,;$&@!~%/\\*:)(|'`\"?"

#endif /* NKN_MGMT_DEFS_H */
