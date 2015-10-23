
/*
 * file_manager.c -- fqueue consumer which processes cache ingest
 *	requests from the OOMGR.
 *	Requests consist of URL, pathname to data object and optional
 *	mime_header_t data.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>
#include <unistd.h>
#include <libgen.h>
#include <malloc.h>
#include <assert.h>
#include <sys/prctl.h>

#include "nkn_mgmt_agent.h"
#include "nkn_log_strings.h"

#include "nkn_defs.h"
#include "nkn_cfg_params.h"
#include "nkn_diskmgr_api.h"
#include "nkn_mediamgr_api.h"
#include "nkn_stat.h"
#include "nkn_attr.h"
#include "nkn_debug.h"

#include "file_manager.h"
#include "fqueue.h"
#include "http_header.h"
#include "nkn_am_api.h"
#include "cache_mgr.h"
#include "nkn_nfs_api.h"
#include "nkn_cod.h"

#define DBG(_fmt, ...) DBG_LOG(MSG, MOD_FM, _fmt, __VA_ARGS__)

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), \
        memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )
#define SEGMENT_INIT_LEN (-1)
#define SEGMENT_INIT_OFF (-1)

#if 0
#define STATIC static
#else
#define STATIC
#endif

/* Fqueue strings */
#define CACHE_PIN		"cache_pin"
#define DEL_URI_FILE		"del_uri_file"
#define END_PUT			"end_put"
#define EVICT_CACHE		"evict_cache"
#define EXPIRY_OFFSET		"expiry_offset"
#define HOTNESS			"hotness"
#define PROVIDER_TYPE		"provider_type"
#define SEGMENT_LENGTH		"segment_length"
#define SEGMENT_OFFSET		"segment_offset"
#define START_TIME_OFFSET	"start_time_offset"
#define TRANSFLAG		"transflag"
#define DISK_NAME		"dump_disk"
#define EVICT_BKT		"evict_bkt"

static pthread_t fmgr_req_thread_id;
fqueue_element_t qe;
extern int glob_cachemgr_init_done;
#define FILEDATABUFSIZE (2*1024*1024)
static char *filedatabuf;
#define WRITE_BLK (64*1024) // must be power of 2

#define ATTRDATABUFSIZE (NKN_DEFAULT_ATTR_SIZE)
static char *attrdatabuf;

STATIC int ingest_object(namespace_token_t token, const namespace_config_t *nsc,
			 const char *uri, int uri_len,
			 const char *datafile, int datafile_len,
			 mime_header_t *httpresp, int no_delete_datafile,
			 const int segment_offset, int segment_length,
			 int end_put, int cache_pin,
			 const int expiry_offset, const int start_time_offset,
			 const int provider_type,
			 const char *nsdomain, int nsdomain_len);
STATIC int remove_single_object(namespace_token_t token, const char *uri,
				int uri_len, int evict_flag, int evict_ptype,
				int evict_trans_flag, uint64_t evict_hotness,
				int provider_type, int del_reason,
				const void *object_fullpath,
				const mime_header_t *hdr,
				const namespace_config_t *nsc);
STATIC int remove_object(namespace_token_t token, const char *uri, int uri_len,
			 int evict_flag, int evict_ptype, int evict_trans_flag,
			 uint64_t evict_hotness, int provider_type,
			 const void *object_fullpath,
			 const mime_header_t *hdr,
			 const namespace_config_t *nsc,
			 const char* uri_list_file);
STATIC int process_request(fqueue_element_t *qe);
STATIC void *fmgr_req_func(void *arg);
STATIC void demux_fq_destination(fqueue_element_t *qe_l,
                                 const char *hdrdata, int hdrdata_len,
                                 const char *in_filename, int in_filelen);
STATIC int process_uri(const char* obj_full_path, const char* uri,
		       int *uri_len, char* out_uri);

#if 0

// This function will delete an object from
// the cache.
static int
delete_mm_cache_obj(nkn_uol_t uol,
		    nkn_provider_type_t ptype,
		    int8_t sptype)
{
    MM_delete_resp_t dp;
    int              ret = -1;
    char             uri[MAX_URI_SIZE];

    dp.in_uol.uri = uri;

    dp.in_uol.offset = 0;
    dp.in_uol.length = 0;

    dp.in_uol.uri = uol.uri;
    dp.in_ptype = ptype;
    dp.in_sub_ptype = sptype;

    ret = MM_delete(&dp);
    if (ret < 0) {
	return -1;
    }
    return 0;
}
#endif

STATIC
int remove_single_object(namespace_token_t token,
			 const char	    *uri,
			 int		    uri_len,
			 int		    evict_flag,
			 int		    evict_ptype,
			 int		    evict_trans_flag,
			 uint64_t	    evict_hotness,
			 int		    provider_type,
			 int		    del_reason,
			 const void	    *object_fullpath,
			 const mime_header_t *hdr,
			 const namespace_config_t *nsc)
{
    int rv;
    MM_delete_resp_t MM_delete_data;
    char *tmpuri = NULL;
    const char *coduri;

    int ret;
    char host[2048];
    int hostlen;
    char *p_host = host;
    uint16_t port;
    size_t length;

    if (!uri || !uri_len) {
	DBG("invalid data uri=%p uri_len=%d", uri, uri_len);
	return 1;
    }

    length = strlen(uri);

    memset(&MM_delete_data, 0, sizeof(MM_delete_data));
    if (evict_flag == 0 && object_fullpath == NULL) {
	ret = request_to_origin_server(REQ2OS_CACHE_KEY, 0, hdr, nsc, 0, 0,
				       &p_host, sizeof(host),
				       &hostlen, &port, 0, 1);
	if (ret) {
	    DBG("request_to_origin_server() failed, rv=%d", ret);
	    rv = 2;
	    return rv;
	}
	tmpuri = nstoken_to_uol_uri(token, uri, uri_len, host, hostlen, &length);
	if (!tmpuri) {
	    DBG("nstoken_to_uol_uri() failed, uri_len=%d hostlen=%d",
		uri_len, hostlen);
	    rv = 3;
	    return rv;
	}
	MM_delete_data.in_uol.cod = nkn_cod_open(tmpuri, length, NKN_COD_FILE_MGR);
	if (MM_delete_data.in_uol.cod == NKN_COD_NULL) {
	    DBG("nkn_cod_open(%s) failed", tmpuri);
	    rv = 4;
	    return rv;
	}
	coduri = tmpuri;
    } else {
	MM_delete_data.in_uol.cod = nkn_cod_open(uri, length, NKN_COD_FILE_MGR);
	if (MM_delete_data.in_uol.cod == NKN_COD_NULL) {
            DBG("nkn_cod_open(%s) failed", uri);
            rv = 6;
            return rv;
        }
	coduri = uri;
    }
    MM_delete_data.in_uol.offset = 0;
    MM_delete_data.in_uol.length = 0;
    if (evict_ptype > 0) {
	MM_delete_data.in_ptype = evict_ptype;
    } else {
	MM_delete_data.in_ptype = provider_type;
    }
    MM_delete_data.in_sub_ptype = 0;
    MM_delete_data.evict_flag = evict_flag;
    MM_delete_data.evict_trans_flag = evict_trans_flag;
    MM_delete_data.evict_hotness = evict_hotness;
    /* Dont check the version while deleting */
    MM_delete_data.dm2_no_vers_check = 1;
    MM_delete_data.dm2_del_reason = del_reason;

    rv = MM_delete(&MM_delete_data);

    if (rv == 0) {
	DBG("removed  object uri:[%s]", coduri);
    } else {
	DBG("Object remove failed uri:[%s]", coduri);
    }
    nkn_cod_close(MM_delete_data.in_uol.cod, NKN_COD_FILE_MGR);
    if (tmpuri)
	free(tmpuri);
    return rv;
}	/* remove_single_object */

STATIC int
remove_object(namespace_token_t		token,
	      const char		*uri,
	      int			uri_len,
	      int			evict_flag,
	      int			evict_ptype,
	      int			evict_trans_flag,
	      uint64_t			evict_hotness,
	      int			provider_type,
	      const void		*object_fullpath,
	      const mime_header_t	*hdr,
	      const namespace_config_t	*nsc,
	      const char		*del_uri_file)
{
#define URI_ENTRY_LEN MAX_URI_SIZE + 64 //64 bytes for hotness
    struct stat sb;
    FILE* fp = NULL;
    int rv = 0, line = 0;
    char uri_buf[URI_ENTRY_LEN];
    char *tmp, *hotness_str;

    if (!del_uri_file) {
	/* This is a delete request for a single URI */
	return remove_single_object(token, uri, uri_len, evict_flag,
				    evict_ptype, evict_trans_flag,
				    evict_hotness, provider_type,
				    NKN_DEL_REASON_CLI,
				    object_fullpath, hdr, nsc);
    }

    if ((rv = stat(del_uri_file, &sb)) != 0) {
	DBG("unable to open URI list file %s", del_uri_file);
	return 1;
    }

    if (!S_ISREG(sb.st_mode)) {
	DBG("%s is not a regular file", del_uri_file);
	return 2;
    }

    fp = fopen(del_uri_file, "r");
    if (fp == NULL) {
	DBG("Unable to fopen URI list file %s", del_uri_file);
	return 3;
    }

    /* The URI list file will have the format as below
     * <URI> <tab> <hotness>
     * An URI must start with a '/' and any other lines without this
     * will be ignored.
     */
    while (fgets(uri_buf, URI_ENTRY_LEN, fp) != NULL) {
	/* skip the header/trailer lines */
	if (uri_buf[0] != '/')
	    continue;

	/* separate the URI and the hotness from the read line */
	tmp = strrchr(uri_buf, '\t');
	if (!tmp)
	    continue;
	*tmp = '\0';
	uri_len = strlen(uri_buf);

	hotness_str = tmp+1;
	evict_hotness = strtoul(hotness_str, NULL, 10);

	/* set the EVICT_START flag */
	if (line == 0 && evict_flag)
	    evict_trans_flag = NKN_EVICT_TRANS_START;
	else
	    evict_trans_flag = 0;

	rv = remove_single_object(token, uri_buf, uri_len,
				  evict_flag, evict_ptype, evict_trans_flag,
				  evict_hotness, provider_type,
				  NKN_DEL_REASON_EXT_EVICTION,
				  object_fullpath, hdr, nsc);
	if (!rv)
	    line++;
    }

    if (evict_flag && line) {
	/* Send the TRANS_STOP flag with URI '/tmp/ignore' on reaching EOF */
	evict_trans_flag = NKN_EVICT_TRANS_STOP;
	rv = remove_single_object(token, uri, strlen(uri),
				  evict_flag, evict_ptype, evict_trans_flag,
				  evict_hotness, provider_type,
				  NKN_DEL_REASON_EXT_EVICTION,
				  object_fullpath, hdr, nsc);
    }

    fclose(fp);
    return rv;
}

STATIC int
ingest_object(namespace_token_t		token,
	      const namespace_config_t	*nsc,
	      const char		*uri,
	      int			uri_len,
	      const char		*datafile,
	      int			datafile_len,
	      mime_header_t		*httpresp,
	      int			no_delete_datafile,
	      const int			segment_offset,
	      int			segment_length,
	      int			end_put,
	      int			cache_pin,
	      const int			expiry_offset,
	      const int			start_time_offset,
	      const int			provider_type,
	      const char		*nsdomain,
	      int			nsdomain_len)
{
    int rv;
    int fd = -1;
    ssize_t off;
    ssize_t bytes_read;
    time_t object_expire_time;
    time_t object_corrected_initial_age;
    time_t object_create_time;
    int cache_object, codisopen=0;
    off_t content_length;
    int s_maxage_ignore = 0;

    const cooked_data_types_t *cd;
    int cd_len;
    mime_hdr_datatype_t dtype;

    nkn_attr_t *cache_attr = (nkn_attr_t *)attrdatabuf;

    nkn_iovec_t iovec;
    MM_put_data_t MM_put_data;
    int bytes_to_write;
    int cur_write;
    char *buf;
    struct stat statdata;
    char *uri_str = 0;
    size_t uri_strlen;
    nkn_cod_t cod = NKN_COD_NULL;
    nkn_objv_t objv;

    char host[2048];
    int hostlen, try_count = 0;
    char *p_host = host;
    uint16_t port;
    uint32_t tot_bytes_read = 0, read_size = 0;
    uint16_t invalid_data = 1;

    if (!nsc || !uri || !datafile) {
	DBG("invalid data nsc=%p uri=%p datafile=%p", nsc, uri, datafile);
	rv = 1;
	goto exit;
    }

    rv = request_to_origin_server(REQ2OS_CACHE_KEY, 0, httpresp, nsc, 0, 0,
				  &p_host, sizeof(host), &hostlen, &port, 0, 1);
     if (rv) {
	DBG("invalid data nsc=%p uri=%p datafile=%p", nsc, uri, datafile);
	rv = 2;
	goto exit;
     }

    uri_str = nstoken_to_uol_uri(token, uri, uri_len,
				 host, hostlen, &uri_strlen);
    if (!uri_str) {
	DBG("nstoken_to_uol_uri(token=0x%lu) failed", token.u.token);
	rv = 3;
	goto exit;
    }
    invalid_data = 0;
    TSTR(datafile, datafile_len, filename_str);

    /* restrict the ingest of URIs greater than 511 bytes here */
    if ((uri_strlen) >= (MAX_URI_SIZE - MAX_URI_HDR_SIZE)) {
	DBG("uri_len(%ld) >= (MAX_URI_SIZE-MAX_URI_HDR_SIZE)(%d)",
	    uri_strlen, MAX_URI_SIZE-MAX_URI_HDR_SIZE);
	rv = 4;
	goto exit;
    }

    // Compute cache expiration
    object_create_time = nkn_cur_ts;
    if (nsc->http_config->policies.ignore_s_maxage) {
	s_maxage_ignore = 1;
    }
    cache_object = compute_object_expiration(httpresp,
					     s_maxage_ignore,
					     object_create_time,
					     object_create_time,
					     nkn_cur_ts,
					     &object_corrected_initial_age,
					     &object_expire_time);
    if (!cache_object) {
        if (nsc->http_config->policies.cache_no_cache) {
	    DBG("Cache no-cache object uri:[%s] pathname:[%s]",
		uri_str, filename_str);
	} else {
	    DBG("Ignoring, no-cache object uri:[%s] pathname:[%s]",
		uri_str, filename_str);
            rv = 0; // No action required
	    goto exit;
	}
    }
    if (object_expire_time == NKN_EXPIRY_FOREVER) {
	object_expire_time = nkn_cur_ts +
		nsc->http_config->policies.cache_age_default;
    } else {
	if (object_expire_time < nkn_cur_ts) {
	    DBG("Ignoring, already expired, expire_time=%ld cur_time=%ld "
	        "uri:[%s] pathname:[%s]",
		object_expire_time, nkn_cur_ts, uri_str, filename_str);
	    rv = 5;
	    goto exit;
	}
    }

    // Compute content length
    rv = get_cooked_known_header_data(httpresp, MIME_HDR_CONTENT_LENGTH,
				    &cd, &cd_len, &dtype);
    if (!(rv || (dtype != DT_SIZE))) {
	content_length = cd->u.dt_size.ll;
    } else {
	DBG("get_cooked_known_header_data(MIME_HDR_CONTENT_LENGTH) "
	    "failed, rv=%d type=%d", rv, dtype);
	rv = 6;
	goto exit;
    }

    // Establish COD for URI
    cod = nkn_cod_open(uri_str, uri_strlen, NKN_COD_FILE_MGR);
    if (cod == NKN_COD_NULL) {
	DBG("nkn_cod_open(%s) failed", uri_str);
	rv = 7;
	goto exit;
    }

    int nonmatch_type;
    // Compute object version
    if (nsc->http_config->policies.object_correlation_etag_ignore) {
	    rv = compute_object_version_use_lm(httpresp, uri_str, uri_strlen, 0, &objv, &nonmatch_type);
    } else {
	    rv = compute_object_version(httpresp, uri_str, uri_strlen, 0, &objv, &nonmatch_type);
    }
    if (rv) {
	DBG("compute_object_version() failed, rv=%d cod=0x%lx uri=%s",
	    rv, cod, uri_str);
	rv = 8;
	goto exit;
    }

    // Set object version
    rv = nkn_cod_test_and_set_version(cod, objv, 0);
    if (rv) {
	DBG("nkn_cod_test_and_set_version() failed, rv=%d "
	    "cod=0x%lx uri=%s", rv, cod, uri_str);
	rv = 9;
	goto exit;
    }

    // Build cache attribute data
    mime_hdr_remove_hop2hophdrs(httpresp);
    if (nsdomain) {
	add_known_header(httpresp, MIME_HDR_X_NKN_CLIENT_HOST,
			 nsdomain, nsdomain_len);
    }
    nkn_attr_init(cache_attr, ATTRDATABUFSIZE);
    rv = http_header2nkn_attr(httpresp, 0, cache_attr);
    if (rv) {
	DBG("http_header2nkn_attr() failed, rv=%d", rv);
	rv = 10;
	goto exit;
    }

    cache_attr->origin_cache_create =
	object_create_time - object_corrected_initial_age;
    if (cache_attr->origin_cache_create < 0) {
	cache_attr->origin_cache_create = object_create_time;
    }
    cache_attr->cache_expiry = object_expire_time;
    /*
     *  t < 0 is invalid
     *  t = 0 means value is not set
     *  t other values indicates a real value
     */
    if (start_time_offset <= 0)
	cache_attr->cache_time0 = 0;
    else
	cache_attr->cache_time0 = start_time_offset + nkn_cur_ts;
    /*
     * t < -1 is invalid
     * t = -1 means no pin expiry
     * t = 0 means value is not set
     * t other values indicates a real value
     */
    if (expiry_offset < -1) {
	/* -1 is NKN_EXPIRY_FOREVER : this is an error */
	cache_attr->cache_expiry = 0;
    } else if (expiry_offset == NKN_EXPIRY_FOREVER) {
	cache_attr->cache_expiry = NKN_EXPIRY_FOREVER;
	if (cache_pin)
	    cache_attr->na_flags |= NKN_OBJ_CACHE_PIN;
    } else if (expiry_offset > 0) {
	cache_attr->cache_expiry = nkn_cur_ts + expiry_offset;
	if (cache_pin)
	    cache_attr->na_flags |= NKN_OBJ_CACHE_PIN;
    }

    cache_attr->content_length = content_length;
    cache_attr->obj_version = objv;

    // open file and push the data into the cache
    fd = open(filename_str, O_RDONLY);
    if (fd < 0) {
	DBG("open(%s) failed errno=%d", filename_str, errno);
	rv = 11;
	goto exit;
    }

    // Verify Content-Length == Data file size
    rv = fstat(fd, &statdata);
    if (rv) {
	DBG("fstat(%s) failed fd=%d errno=%d", filename_str, fd, errno);
	rv = 12;
	goto exit;
    }

    if (content_length != statdata.st_size) {
	DBG("Content-Length(%ld) != File size(%ld) uri=%s file=%s, "
	    "object ignored", content_length,  statdata.st_size,
	    uri_str, filename_str);
	rv = 13;
	goto exit;
    }

    MM_put_data.uol.cod = nkn_cod_open(uri_str, uri_strlen, NKN_COD_FILE_MGR);
    if (MM_put_data.uol.cod == NKN_COD_NULL) {
        DBG("nkn_cod_open(%s) failed", uri_str);
        rv = 14;
        goto exit;
    }
    codisopen = 1;
    MM_put_data.uol.offset = 0;
    MM_put_data.uol.length = 0;
    MM_put_data.errcode = 0;
    MM_put_data.iov_data_len = 1;
    MM_put_data.domain = 0;
    if (segment_offset == SEGMENT_INIT_OFF ||
	segment_offset == 0 ) {				// -1 => not set by user
	MM_put_data.attr = cache_attr;
    } else {
	MM_put_data.attr = NULL;
    }
    iovec.iov_base = 0;
    iovec.iov_len = 0;
    MM_put_data.iov_data = &iovec;
    MM_put_data.total_length = 0;
    MM_put_data.ptype = provider_type;

    // if end_put is set, just the cod is the required input for PUT
    if (end_put == 0)
	MM_put_data.flags = 0;
    else {
	MM_put_data.flags |= MM_FLAG_DM2_END_PUT;
	rv = MM_put(&MM_put_data);

	if ((rv < 0) && (rv != (-EEXIST))) {
	    DBG("MM_put() failed for ending PUT: return value: %d ", rv);
	    rv = 15;
	}
	goto exit;
    }

    if (segment_offset != SEGMENT_INIT_OFF) {		// -1 => not set by user
	rv = lseek(fd, segment_offset, SEEK_SET);
	if (rv != segment_offset) {
	    DBG("lseek(%s) failed fd=%d errno=%d", filename_str, fd, errno);
	    rv = 16;
	    goto exit;
	}
	off = segment_offset;
    } else {
	off = 0;
    }

    buf = filedatabuf;
    read_size = FILEDATABUFSIZE;
    while ((bytes_read = read(fd, buf, read_size)) > 0) {
	tot_bytes_read += bytes_read;
	/* Handle partial reads by reading the remaining bytes again */
	if (tot_bytes_read < content_length && bytes_read < read_size) {
	    if (try_count > 5) {
		DBG("Read only %ld/%d bytes, Aborting ingest for file %s",
		    bytes_read, FILEDATABUFSIZE, filename_str);
		rv = 17;
		goto exit;
	    }
	    try_count++;
	    buf += bytes_read;
	    read_size -= bytes_read;
	    continue;
	}
	if (segment_length != SEGMENT_INIT_LEN) {	// -1 => not set by user
	    if (segment_length == 0) {
		break;
	    } else if (bytes_read > segment_length) {
		bytes_read = segment_length;
		segment_length = 0;
	    } else {
		segment_length -= bytes_read;
	    }
	}
	bytes_to_write = bytes_read;
	/* reset the buf to the start, in case it was changed */
	buf = filedatabuf;
	while (bytes_to_write) {
#if 1
	    cur_write = bytes_to_write;
#else
	    // Write in multiples of WRITE_BLK
	    if (bytes_to_write > WRITE_BLK) {
	        cur_write = bytes_to_write & ~(WRITE_BLK-1);
	    } else {
		cur_write = bytes_to_write;
	    }
#endif

	    // write data
	    MM_put_data.uol.offset = off;
	    MM_put_data.uol.length = cur_write;
	    iovec.iov_base = buf;
	    iovec.iov_len = cur_write;

	    rv = MM_put(&MM_put_data);

            if ((rv < 0) && (rv != (-EEXIST))) {
	        DBG("MM_put() failed: return value: %d ", rv);
		rv = 18;
		goto exit;
            } else {
		rv = 0;
	    }

	    bytes_to_write -= cur_write;
	    buf += cur_write;
	    off += cur_write;

	    if (MM_put_data.attr) {
		MM_put_data.attr = 0;
	    }
	}
	buf = filedatabuf;
	read_size = FILEDATABUFSIZE;
    }

    nkn_uol_setprovider(&MM_put_data.uol, MM_put_data.ptype);
    AM_change_obj_provider(MM_put_data.uol.cod, MM_put_data.ptype);

    if (bytes_read < 0) {
        DBG("read() error errno=%d", errno);
	rv = 19;
	goto exit;
    }

exit:
    if (fd >= 0) {
	close(fd);
	if (!no_delete_datafile) {
	    unlink(filename_str);
	}
    }
    if (codisopen)
	nkn_cod_close(MM_put_data.uol.cod, NKN_COD_FILE_MGR);

    if (!invalid_data){
        if (rv == 0) {
	    DBG("Ingested object uri:[%s] pathname:[%s]",
	        uri_str ? uri_str : "", filename_str);
        } else {
	    DBG("Object ingestion failed uri:[%s] pathname:[%s]",
	        uri_str ? uri_str : "", filename_str);
        }
    }
    if (uri_str) {
	free(uri_str);
    }
    if (cod != NKN_COD_NULL) {
	nkn_cod_close(cod, NKN_COD_FILE_MGR);
    }
    return rv;
}

/*
 * This function demuxes the attr fq_destination from the
 * fqueue entry and calls the appropriate functions. For eg:
 * for nfs, the hdrdata needs to be a special string.
 */
STATIC void
demux_fq_destination(fqueue_element_t	*qe_l,
		     const char		*hdrdata,
		     int		hdrdata_len,
                     const char		*in_filename,
		     int		in_file_len)
{
    char  *filename;
    char  *token;
    int   token_len = -1;
    int   rv = -1;

    if (!qe_l) {
        return;
    }

    if (strncmp(hdrdata, YAHOO_NFS_FILE_STR, hdrdata_len) == 0) {
        /* The in_filename is not NULL terminated leading to some
           really crazy behavior. I am making sure here. */

	rv = get_nvattr_fqueue_element_by_name(qe_l, "fq_cookie", 9,
					       (const char **)&token,
					       &token_len);
	if (rv) {
	    return;
	}

        filename = (char *) nkn_malloc_type(in_file_len + 1, mod_fm_filename_t);
        strncpy(filename, in_filename, in_file_len);
        filename[in_file_len] = '\0';
        NFS_oomgr_response(YAHOO_NFS_FILE_STR, filename, token);
    }
    return;
}

STATIC int
process_uri(const char *object_fullpath,
	    const char *uri,
	    int	       *uri_len,
	    char       *out_uri)
{
    int rv = 0;
    char *p;
    int mods, err, tlen = *uri_len;
    int bytesused;
    int tbuf_len = *uri_len + 1;
    char *tbuf;

    if (object_fullpath) {
	if (tlen >= MAX_URI_SIZE) {
	    DBG("uri_len(%d) >= MAX_URI_SIZE(%d)", tlen, MAX_URI_SIZE);
	    rv = 1;
	}
    } else {
	if (tlen >= (MAX_URI_SIZE - MAX_URI_HDR_SIZE)) {
	    DBG("uri_len(%d) >= (MAX_URI_SIZE-MAX_URI_HDR_SIZE)(%d)",
		tlen, MAX_URI_SIZE-MAX_URI_HDR_SIZE);
	    rv = 1;
	}
    }

    if (rv)
	goto exit;

    tbuf = alloca(tbuf_len);
    rv = canonicalize_url(uri, *uri_len, tbuf, tbuf_len, &bytesused);
    if (rv) {
	TSTR(uri, *uri_len, uristr);
	DBG("canonicalize_url() failed, uri=%s rv=%d", uristr, rv);
	rv = 2;
	goto exit;
    }
    uri = tbuf;
    *uri_len = bytesused;

    tbuf = out_uri;
    tbuf_len = *uri_len + 1;
    p = normalize_uri_path(uri, *uri_len, tbuf, &tbuf_len, &mods, &err);
    if (!p || err) {
	TSTR(uri, *uri_len, uristr);
	DBG("normalize_uri_path() failed, uri=%s p=%p err=%d",
	    uristr, p, err);
	rv = 3;
	goto exit;
    }

    if (mods) {
	out_uri = p;
	*uri_len = tbuf_len;
    }

exit:
   return rv;
}

STATIC int
process_request(fqueue_element_t *qe_l)
{
    int rv;

    const char *name;
    int name_len;

    const char *datafile;
    int datafile_len;

    const char *uri;
    int uri_len;
    char *turi;

    const char *hdrdata;
    int hdrdata_len;

    const char *remove_obj = NULL;
    int remove_obj_len;

    const char *nstokendata;
    int nstokendata_len;
    namespace_token_t nstoken;
    const namespace_config_t *nsc = NULL;
    const char *nsdomain = NULL;
    int nsdomain_len;

    const char *object_fullpath = NULL;
    int object_fullpath_len;

    mime_header_t httpresp;

    const char *evict_cache;		// remove
    int evict_cache_len;		// remove

    const char *evict_hotness_str;	// remove
    int evict_hotness_str_len;		// remove

    const char *evict_trans_flag_str;	// remove
    int evict_trans_flag_str_len;	// remove

    int evict_flag = 0;			// remove
    int evict_ptype = 0;		// remove
    uint64_t evict_hotness = 0;		// remove
    int evict_trans_flag = 0;		// remove

    const char *remove_type_evict;	// remove
    int remove_type_evict_len = 0;	// remove

    const char *del_uri_filename = NULL;// remove
    int del_uri_filename_len = 0;	// remove

    int segment_offset;			// ingest
    int segment_length;			// ingest
    const char *segment_offset_str;	// ingest
    const char *segment_length_str;	// ingest
    int segment_offset_str_len;		// ingest
    int segment_length_str_len;		// ingest

    int provider_type;			// ingest
    const char *provider_type_str;	// ingest
    int provider_type_str_len;		// ingest

    int end_put = 0;			// ingest
    const char *end_put_str;		// ingest
    int end_put_str_len;		// ingest

    int cache_pin = 0;			// ingest
    const char *cache_pin_str;		// ingest
    int cache_pin_str_len;		// ingest

    int expiry_offset = 0;		// ingest
    const char *expiry_offset_str;	// ingest
    int expiry_offset_str_len;		// ingest

    int start_time_offset = 0;		// ingest
    const char *start_time_offset_str;	// ingest
    int start_time_offset_str_len;	// ingest

    mime_header_t ns_httphdr;
    int httpresp_init = 0;
    int ns_httphdr_init = 0;

    const char *disk_name_str;
    int	  disk_name_str_len = 0;

    const char *bucket_str;
    int	  bucket_str_len;
    int	  bucket = -1;

    rv = init_http_header(&httpresp, 0, 0);
    if (rv) {
	DBG("init_http_header() failed, rv=%d", rv);
	rv = 1;
	goto exit;
    }
    httpresp_init = 1;

    rv = init_http_header(&ns_httphdr, 0, 0);
    if (rv) {
	DBG("init_http_header() failed, rv=%d", rv);
	rv = 2;
	goto exit;
    }
    ns_httphdr_init = 1;

    rv = get_attr_fqueue_element(qe_l, T_ATTR_URI, 0, &name, &name_len,
				 &uri, &uri_len);
    if (rv) {
	DBG("get_attr_fqueue_element(T_ATTR_URI) failed, rv=%d", rv);
	rv = 3;
	goto exit;
    }

    rv = get_nvattr_fqueue_element_by_name(qe_l, DISK_NAME,
					   strlen(DISK_NAME),
					   &disk_name_str, &disk_name_str_len);
    if (rv == 0) {
	rv = get_nvattr_fqueue_element_by_name(qe_l, EVICT_BKT,
					   strlen(EVICT_BKT),
					   &bucket_str, &bucket_str_len);
	if (rv == 0) {
	    bucket = atoi(bucket_str);
	}
	dm2_evict_dump_list(disk_name_str, bucket);
	goto exit;
    }

    rv = get_nvattr_fqueue_element_by_name(qe_l, "object_full_path", 16,
                                      &object_fullpath, &object_fullpath_len);

    turi = alloca(uri_len + 16);
    if (process_uri(object_fullpath, uri, &uri_len, turi) != 0) {
	rv = 4;
	goto exit;
    }
    uri = turi;

    rv = get_nvattr_fqueue_element_by_name(qe_l, "remove_object", 13,
					&remove_obj, &remove_obj_len);
    if (rv) {
	/* Not remove operation */
	remove_obj_len = 0;

	rv = get_attr_fqueue_element(qe_l, T_ATTR_PATHNAME, 0, &name,
				     &name_len, &datafile, &datafile_len);
	if (rv) {
	    DBG("get_attr_fqueue_element(T_ATTR_PATHNAME) failed, "
		"rv=%d", rv);
	    rv = 5;
	    goto exit;
	}

	rv = get_nvattr_fqueue_element_by_name(qe_l, "http_header", 11,
					       &hdrdata, &hdrdata_len);
	if (!rv) {
	    rv = deserialize(hdrdata, hdrdata_len, &httpresp, 0, 0);
	    if (rv) {
		DBG("deserialize() failed, rv=%d", rv);
		rv = 6;
		goto exit;
	    }
	} else {
	    DBG("get_nvattr_fqueue_element_by_name() "
		"for \"http_header\" failed, rv=%d", rv);
	}
	rv = get_nvattr_fqueue_element_by_name(qe_l, "fq_destination", 14,
					       &hdrdata, &hdrdata_len);
	if (!rv) {
	    demux_fq_destination(qe_l, hdrdata, hdrdata_len, datafile,
				 datafile_len);
	}
    }

    get_nvattr_fqueue_element_by_name(qe_l, "evict_cache", 11,
				      &remove_type_evict, &remove_type_evict_len);

    /*
     * "evict_cache" should only be used for a delete/evict request
     * "namespace_token" is only used for OOM ingest request
     * "namespace_domain" is used for delete (non-evict) and ingest
     */
    nstokendata = NULL;
    get_nvattr_fqueue_element_by_name(qe_l, "namespace_token", 15,
				      &nstokendata, &nstokendata_len);
    if (nstokendata) {
        nstoken.u.token = (uint64_t)atol_len(nstokendata, nstokendata_len);
	nsc = namespace_to_config(nstoken);
	if (!nsc) {
	    rv = 7;
	    goto exit;
	}
    } else if (remove_type_evict_len || (remove_obj && object_fullpath)) {
	// No action since uri is really the uol.uri ("/{namespace:UID}/uri")
	nstoken.u.token = 0;
	nsc = 0;
    } else {
	add_known_header(&ns_httphdr, MIME_HDR_X_NKN_URI, uri, uri_len);

	rv = get_nvattr_fqueue_element_by_name(qe_l, "namespace_domain", 16,
					       &nsdomain, &nsdomain_len);
	if (!rv) {
	    add_known_header(&ns_httphdr, MIME_HDR_HOST, nsdomain,nsdomain_len);
	}
	nstoken = http_request_to_namespace(&ns_httphdr, NULL);
	nsc = namespace_to_config(nstoken);
	if (!nsc) {
	    rv = 8;
	    goto exit;
	}
    }

    provider_type = Unknown_provider;
    if (remove_obj_len) {
	/* remove object */
	rv = get_nvattr_fqueue_element_by_name(qe_l, EVICT_CACHE,
					       strlen(EVICT_CACHE),
					       &evict_cache, &evict_cache_len);
	if (!rv) {
	    /* Eviction delete */
	    evict_flag = 1;
	    evict_ptype = atoi(evict_cache);
	} else {
	    /* Normal delete */
	    rv = get_nvattr_fqueue_element_by_name(qe_l, PROVIDER_TYPE,
						   strlen(PROVIDER_TYPE),
						   &provider_type_str,
						   &provider_type_str_len);
	    if (!rv) {
		provider_type = atoi(provider_type_str);
	    }
	}

	rv = get_nvattr_fqueue_element_by_name(qe_l, DEL_URI_FILE,
					       strlen(DEL_URI_FILE),
					       &del_uri_filename,
					       &del_uri_filename_len);

	rv = get_nvattr_fqueue_element_by_name(qe_l, TRANSFLAG,
					       strlen(TRANSFLAG),
					       &evict_trans_flag_str,
					       &evict_trans_flag_str_len);
	if (!rv) {
	    evict_trans_flag = atoi(evict_trans_flag_str);
	}

	rv = get_nvattr_fqueue_element_by_name(qe_l, HOTNESS, strlen(HOTNESS),
					       &evict_hotness_str,
					       &evict_hotness_str_len);
	if (!rv) {
	    evict_hotness = atoi(evict_hotness_str);
	}

	rv = remove_object(nstoken, uri, uri_len, evict_flag, evict_ptype,
			   evict_trans_flag, evict_hotness, provider_type,
			   object_fullpath, &ns_httphdr, nsc, del_uri_filename);
    } else {
	if (!nsc && !object_fullpath) {
	    TSTR(uri, uri_len, uristr);
	    DBG("No namespace for object uri=%s, ingest request ignored",
		uristr);
	    rv = 9;
	    goto exit;
	}

	/* Ingest Object into the disk cache */
	segment_offset = SEGMENT_INIT_OFF;
	segment_length = SEGMENT_INIT_LEN;

	/* offset in the input file for this ingestion */
	rv = get_nvattr_fqueue_element_by_name(qe_l, SEGMENT_OFFSET,
					       strlen(SEGMENT_OFFSET),
					       &segment_offset_str,
					       &segment_offset_str_len);
	if (!rv) {
	    segment_offset = atoi(segment_offset_str);
	}
	/* length of data from 'offset' for this ingestion */
	rv = get_nvattr_fqueue_element_by_name(qe_l, SEGMENT_LENGTH,
					       strlen(SEGMENT_LENGTH),
					       &segment_length_str,
					       &segment_length_str_len);
	if (!rv) {
	    segment_length = atoi(segment_length_str);
	}

	/* provider type for this ingestion */
	rv = get_nvattr_fqueue_element_by_name(qe_l, PROVIDER_TYPE,
					       strlen(PROVIDER_TYPE),
					       &provider_type_str,
					       &provider_type_str_len);
	if (!rv) {
	    provider_type = atoi(provider_type_str);
	}

	/*
	 * If an URI needs to be made partial in the disk, end_put would
	 * be set indicating no more data would come in after this PUT
	 */
	rv = get_nvattr_fqueue_element_by_name(qe_l, END_PUT, strlen(END_PUT),
					       &end_put_str,
					       &end_put_str_len);
	if (!rv)
	    end_put = atoi(end_put_str);

	/*
	 * If an URI needs to be pinned in the lowest tier, cache_pin should
	 * be set.
	 */
	rv = get_nvattr_fqueue_element_by_name(qe_l, CACHE_PIN,
					       strlen(CACHE_PIN),
					       &cache_pin_str,
					       &cache_pin_str_len);
	if (!rv)
	    cache_pin = atoi(cache_pin_str);

	/*
	 * This URI will be pinned for this many seconds past NOW.  If -1
	 * is passed in, then the object will never get unpinned.
	 */
	rv = get_nvattr_fqueue_element_by_name(qe_l, EXPIRY_OFFSET,
					       strlen(EXPIRY_OFFSET),
					       &expiry_offset_str,
					       &expiry_offset_str_len);
	if (!rv)
	    expiry_offset = atoi(expiry_offset_str);

	/*
	 * When a URI gets ingested, it may not be served until a specific
	 * time.  This is the start time or time0.
	 */
	rv = get_nvattr_fqueue_element_by_name(qe_l, START_TIME_OFFSET,
					       strlen(START_TIME_OFFSET),
					       &start_time_offset_str,
					       &start_time_offset_str_len);
	if (!rv)
	    start_time_offset = atoi(start_time_offset_str);

	rv = ingest_object(nstoken, nsc, uri, uri_len, datafile, datafile_len,
			   &httpresp, qe_l->u.e.no_delete_datafile,
			   segment_offset, segment_length, end_put, cache_pin,
			   expiry_offset, start_time_offset,
			   provider_type, nsdomain, nsdomain_len);
    }

    //release_namespace_token_t(nstoken);
    if (rv) {
	DBG("%s failed, rv=%d",
	    remove_obj_len ? "remove_object()" : "ingest_object()", rv);
	rv = 10;
	goto exit;
    }

exit:
    if (nsc)
	release_namespace_token_t(nstoken);
    if (httpresp_init)
	shutdown_http_header(&httpresp);
    if (ns_httphdr_init)
	shutdown_http_header(&ns_httphdr);
    return rv;
}

STATIC
void *fmgr_req_func(void *arg)
{
    int rv;
    fhandle_t fh;
    struct pollfd pfd;

    UNUSED_ARGUMENT(arg);
    prctl(PR_SET_NAME, "nvsd-fmgr", 0, 0, 0);

    // wait until cache init is done
    while (!glob_cachemgr_init_done)
	sleep(5);

    // Create input fqueue
    do {
	rv = create_recover_fqueue(fmgr_queuefile, fmgr_queue_maxsize,
				   1 /* delete old entries */);
	if (rv) {
	    DBG_LOG(SEVERE, MOD_FM,
		    "Unable to create input queuefile [%s], rv=%d",
		    fmgr_queuefile, rv);
	    DBG_ERR(SEVERE,
		    "Unable to create input queuefile [%s], rv=%d",
		    fmgr_queuefile, rv);
	    sleep(10);
	}
    } while (rv);

    // Open input fqueue
    do {
	fh = open_queue_fqueue(fmgr_queuefile);
	if (fh < 0) {
	    DBG("Unable to open input queuefile [%s]", fmgr_queuefile);
	    sleep(10);
	}
    } while (fh < 0);

    while (1) {
	rv = dequeue_fqueue_fh_el(fh, &qe, 1);
	if (rv == -1) {
	    continue;
	} else if (rv) {
	    DBG("dequeue_fqueue_fh_el(%s) failed, rv=%d", fmgr_queuefile, rv);
	    poll(&pfd, 0, 100);
	    continue;
	}
	rv = process_request(&qe);
	if (rv) {
	    DBG("process_request() failed, rv=%d", rv);
	}
    }
    return 0;
}

int file_manager_init()
{
    int rv;
    pthread_attr_t attr;
    int stacksize = 128 * KiBYTES;

    rv = nkn_posix_memalign_type((void **)&filedatabuf,
			         (4*1024), FILEDATABUFSIZE, mod_fm_posix_memalign);
    if (rv) {
	// Allocation failed
	filedatabuf = 0;
	return 0;
    }

    rv = nkn_posix_memalign_type((void **)&attrdatabuf,
			         (512), ATTRDATABUFSIZE, mod_fm_posix_memalign);
    if (rv) {
	// Allocation failed
	attrdatabuf = 0;
	return 0;
    }

    rv = pthread_attr_init(&attr);
    if (rv) {
	DBG("pthread_attr_init() failed, rv=%d", rv);
	return 0;
    }

    rv = pthread_attr_setstacksize(&attr, stacksize);
    if (rv) {
	DBG("pthread_attr_setstacksize() failed, rv=%d", rv);
	return 0;
    }

    if ((rv = pthread_create(&fmgr_req_thread_id, &attr,
			     fmgr_req_func, NULL))) {
	DBG("pthread_create() failed, rv=%d", rv);
	return 0;
    }
    return 1;
}

/*
 * End of file_manager.c
 */
