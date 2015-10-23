/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains implementation of Cache Interface Module for media management
 *
 * Author: Jeya ganesh babu
 *
 */
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "nkn_debug.h"
#include "nkn_stat.h"
#include "nkn_mediamgr_api.h"
#include "nkn_am_api.h"
#include "nkn_cod.h"
#include "nkn_cim.h"
#include "http_header.h"
#include "nkn_http.h"
#include "nkn_pseudo_con.h"
#include "nkn_iccp_srvr.h"

static pthread_mutex_t cim_stat_hash_table_mutex;
static GHashTable *nkn_cim_stat_hash_table = NULL;

/* Ingest failure counters */
uint64_t glob_cim_buf_alloc_failed;
uint64_t glob_cim_cod_set_version_failed;
uint64_t glob_cim_attr_buf_alloc_failed;
uint64_t glob_cim_data_buf_alloc_failed;
uint64_t glob_cim_header2attr_failed;
uint64_t glob_cim_inget_interval_exceeded;
uint64_t glob_cim_entry_already_present;

/* Debug counters */
uint64_t glob_cim_callback_count;

TAILQ_HEAD(nkn_cim_xfer_list_t, ccp_client_data) nkn_cim_xfer_list;

AO_t glob_crawl_num_objects_added;
AO_t glob_crawl_num_objects_deleted;
AO_t glob_crawl_num_objects_add_failed;
AO_t glob_crawl_num_objects_delete_failed;
AO_t crawl_name_uniq_id;

/* CIM module init
 * Called from Media Manager 
 */
int
cim_init()
{
    int ret = 0;

    iccp_srvr_init(cim_callback);

    ret = pthread_mutex_init(&cim_stat_hash_table_mutex, NULL);
    if(ret < 0) {
        DBG_LOG(SEVERE, MOD_CIM, "CIM stat hash table mutex not created. ");
        return ret;
    }
    nkn_cim_stat_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
    if(!nkn_cim_stat_hash_table)
	ret = -1;

    return ret;
}

/* 
 * Set content length in the attribute with mime header
 */
static void
nkn_ccp_set_attr_content_length(nkn_attr_t *in_ap, mime_header_t *mh,
				uint64_t in_len)
{
    char	ascii_length[64];

    in_ap->content_length = in_len;

    snprintf(ascii_length, sizeof(ascii_length)-1, "%ld", in_len);
    ascii_length[sizeof(ascii_length)-1] = '\0';

    mime_hdr_add_known(mh, MIME_HDR_CONTENT_LENGTH, ascii_length,
		       strlen(ascii_length));
}

/*
 * Register the data given by the CUE in BM for ingest.
 * The buffers are stored in iccp_info and freed up
 * after ingest is complete
 */
static void
cim_register_data(iccp_info_t *iccp_info)
{
    nkn_buffer_t	 *abuf = NULL;
    nkn_attr_t		 *attr;
    size_t               data_len = iccp_info->data_len;
    void		 *content_ptr, *ptr_vobj;
    off_t                offset = 0;
    nkn_uol_t            uol;
    mime_header_t        mh;
    int                  ret;
    const char           *cache_uri;
    nkn_objv_t           ver;

    /*
     * Don't register the data if an entry for the same cod is already 
     * present in the iccp_entry_list.
     */
    if(iccp_srvr_find_dup_entry_cod(iccp_info->cod, iccp_info))
	return;

    iccp_info->buffer_ptr = (void *)nkn_calloc_type(
					    ((data_len/CM_MEM_PAGE_SIZE)+2),
					    sizeof(void*),
					    mod_cim_bufptr);
    if(!iccp_info->buffer_ptr) {
	DBG_LOG(ERROR, MOD_CIM, "Buffer allocation "
		    "failed (URI=%s): %d",
		    iccp_info->uri, ENOMEM);
	glob_cim_buf_alloc_failed++;
	return;
    }

    iccp_info->buffer_count = 0;
    abuf = nkn_buffer_alloc(NKN_BUFFER_ATTR, 0, 0);
    if(abuf == NULL) {
	DBG_LOG(ERROR, MOD_CIM, "Attribute buffer allocation "
		    "failed (URI=%s): %d",
		    iccp_info->uri, ENOMEM);
	glob_cim_attr_buf_alloc_failed++;
	return;
    }

    attr = nkn_buffer_getcontent(abuf);
    attr->obj_version.objv_l[0] = 0;
    attr->obj_version.objv_l[1] = nkn_cur_ts;
    iccp_info->buffer_ptr[iccp_info->buffer_count] = abuf;
    iccp_info->buffer_count++;

    ver.objv_l[0] = 0;
    ver.objv_l[1] = 0;

    ret = nkn_cod_get_version(iccp_info->cod, &ver);
    if(!ret && ver.objv_l[1]) {
	/* We have already a version present in the cache,
	 * Need to expire the existing cod, remove from the cim_hash_table
	 * and open a new cod and add it to the cim_hash_table
	 */
	cache_uri = nkn_cod_get_cnp(iccp_info->cod);
	nkn_cod_set_expired(iccp_info->cod);
	ver.objv_l[0] = attr->obj_version.objv_l[0];
	ver.objv_l[1] = attr->obj_version.objv_l[1];
	nkn_cod_set_new_version(iccp_info->cod, ver);
	nkn_cod_close(iccp_info->cod, NKN_COD_CIM_MGR);
	iccp_info->cod = nkn_cod_open(cache_uri,
					strlen(cache_uri),
					NKN_COD_CIM_MGR);

	if(iccp_info->cod == NKN_COD_NULL)
	    return;

    }

    if(nkn_cod_test_and_set_version(iccp_info->cod, attr->obj_version,
			NKN_COD_SET_ORIGIN)) {
	DBG_LOG(ERROR, MOD_CIM, "Cod test and set version "
		    "failed (URI=%s): %d",
		    iccp_info->uri, ENOMEM);
	glob_cim_cod_set_version_failed++;
	return;
    }

    uol.cod = iccp_info->cod;
    uol.length = 0;
    uol.offset = 0;
    attr->cache_expiry = iccp_info->expiry;
    // cache corrected age is zero in this case.
    attr->origin_cache_create = nkn_cur_ts;
    attr->content_length = data_len;

    mime_hdr_init(&mh, MIME_PROT_HTTP, 0, 0);

    /* Set the content length attribute in the mime header. */
    nkn_ccp_set_attr_content_length(attr, &mh, data_len);

    ret = http_header2nkn_attr(&mh, 0, attr);
    if(ret) {
	DBG_LOG(ERROR, MOD_CIM,
		"http_header2nkn_attr() failed ret=%d", ret);
	glob_cim_header2attr_failed++;
	mime_hdr_shutdown(&mh);
	return;
    }

    mime_hdr_shutdown(&mh);

    nkn_buffer_setid(abuf, &uol, BufferCache_provider, 0);

    while(data_len) {
	uol.offset = offset;
	ptr_vobj = nkn_buffer_alloc(NKN_BUFFER_DATA, CM_MEM_PAGE_SIZE, 0);
	if(!ptr_vobj) {
	    DBG_LOG(ERROR, MOD_CIM, "Data buffer allocation "
			"failed (URI=%s): %d",
			iccp_info->uri, ENOMEM);
	    glob_cim_data_buf_alloc_failed++;
	    return;
	}
	content_ptr = nkn_buffer_getcontent(ptr_vobj);
	if(data_len > CM_MEM_PAGE_SIZE) {
	    uol.length = CM_MEM_PAGE_SIZE;
	    memcpy(content_ptr, ((char *)iccp_info->data + offset),
		    CM_MEM_PAGE_SIZE);
	    data_len -= CM_MEM_PAGE_SIZE;
	} else {
	    uol.length = data_len;
	    memcpy(content_ptr, ((char *)iccp_info->data + offset), data_len);
	    data_len = 0;
	}
	nkn_buffer_setid(ptr_vobj, &uol, BufferCache_provider, 0);
	iccp_info->buffer_ptr[iccp_info->buffer_count] = ptr_vobj;
	iccp_info->buffer_count++;
	offset += CM_MEM_PAGE_SIZE;
    }
}

/* 
 * Cleanup iccp info, called during ingest/delete complete
 */
static void
cim_cleanup_iccp_info(iccp_info_t *iccp_info, int errcode)
{
    int count = 0;

    /* OM fetch error might be a temporary error.
     * We retry thrice before we declare as a failure case
     */
    if(errcode == NKN_OM_SERVER_FETCH_ERROR) {
	iccp_info->retry_count++;
	if(iccp_info->retry_count <= MAX_ICCP_RETRY) {
	    iccp_info->flag |= ICCP_RETRY;
	    errcode = NKN_MM_CIM_RETRY;
	}
    }
    /* If buf version expired, then it means new object is present
	* in the origin. Need to fetch again. Set the status to 0,
	* so it will be automatically done by the CIM thread */
    if(errcode == NKN_BUF_VERSION_EXPIRED || errcode == NKN_MM_CIM_RETRY) {
	iccp_info->status = 0;
	iccp_info->flag |= VERSION_EXPIRED;
    } else {
	iccp_info->errcode = errcode;
	if(errcode)
	    iccp_info->status = ICCP_NOT_OK;
	else
	    iccp_info->status = ICCP_OK;
    }

    if(iccp_info->cod != NKN_COD_NULL)
	nkn_cod_close(iccp_info->cod, NKN_COD_CIM_MGR);

    iccp_info->cod = 0;

    if(iccp_info->buffer_ptr) {
	for(count=0;count<iccp_info->buffer_count;count++) {
	    nkn_buffer_release(iccp_info->buffer_ptr[count]);
	}
	free(iccp_info->buffer_ptr);
	iccp_info->buffer_ptr = NULL;
    }
    if(!iccp_info->status)
	cim_callback((void *)iccp_info);
}

/* 
 * Returns a valid cod created based on the uri
 */
static nkn_cod_t
cim_register_and_get_cod_locked(iccp_info_t *iccp_info, int *rv,
				char *cache_uri, size_t cache_uri_len,
				char *nkn_uri, size_t nkn_uri_len,
				namespace_token_t *ns_token)
{
    nkn_cod_t cod = 0;
    mime_header_t hdr;
    const char *method;
    char *uri = iccp_info->uri;
    char *client_domain = iccp_info->client_domain;
    const char *http_hdr="http://";
    int orig_uri_len = strlen(uri);
    int counter = 0;
    size_t cachelen;
    int http_hdr_len = strlen(http_hdr);
    const namespace_config_t *nsconf = NULL;
    int hostlen;
    char host[2049];
    char *p_host = host;
    uint16_t port;
    int hex_encoded = 0;
    char *test_uri, *cod_uri;
    int bytes_used;
    size_t uri_len = 0;


    ns_token->u.token = 0;

    init_http_header(&hdr, 0, 0);

    if(orig_uri_len < http_hdr_len) {
	*rv = NKN_MM_CIM_INVALID_URI;
	goto exit;
    } else {
	if(memcmp(uri, http_hdr, http_hdr_len)) {
	    *rv = NKN_MM_CIM_INVALID_URI;
	    goto exit;
	}
	uri += http_hdr_len;
	while(*uri != '/') {
	    if(*uri == '%')
		hex_encoded = 1;
	    counter++;
	    uri++;
	    if(counter >= (orig_uri_len - http_hdr_len)) {
		*rv = NKN_MM_CIM_INVALID_URI;
		goto exit;
	    }
	}
	/* find whether the rest of the uri has '%' 
	 */
	test_uri = uri;
	while(*test_uri) {
	    if(hex_encoded)
		break;
	    if(*test_uri == '%')
		hex_encoded = 1;
	    test_uri++;
	}
    }

    uri_len = strlen(uri);

    // MUST: uri size < Max URI Size (256 for now)
    if(uri_len >= (MAX_URI_SIZE - MAX_URI_HDR_SIZE)) {
	    DBG_LOG(ERROR, MOD_CIM, "uri %s is large than max uri len %d",
		    uri, MAX_URI_SIZE - MAX_URI_HDR_SIZE);
	    *rv = NKN_MM_CIM_INVALID_URI;
	    goto exit;
    }

    method = "GET";
    *rv = add_known_header(&hdr, MIME_HDR_X_NKN_METHOD,
			    method, strlen(method));
    if(*rv) {
	DBG_LOG(ERROR, MOD_CIM, "add_known_header() failed, rv = %d", *rv);
	*rv = NKN_MM_CIM_ADD_HDR_ERR;
	goto exit;
    }

    *rv = add_known_header(&hdr, MIME_HDR_X_NKN_URI,
			    uri, uri_len);
    if(*rv) {
	DBG_LOG(ERROR, MOD_CIM, "add_known_header() failed, rv = %d", *rv);
	*rv = NKN_MM_CIM_ADD_HDR_ERR;
	goto exit;
    }

    if(client_domain) {
	*rv = add_known_header(&hdr, MIME_HDR_HOST,
				client_domain, strlen(client_domain));
	if(*rv) {
	    DBG_LOG(ERROR, MOD_CIM, "add_known_header() failed, rv = %d", *rv);
	    *rv = NKN_MM_CIM_ADD_HDR_ERR;
	    goto exit;
	}
    }

    *ns_token = http_request_to_namespace(&hdr, &nsconf);
    *p_host = '\0';
    *rv = request_to_origin_server(REQ2OS_CACHE_KEY, 0, &hdr,
				    nsconf, 0, 0, &p_host, sizeof(host),
				    &hostlen, &port, 0, 1);
    if(*rv) {
	DBG_LOG(ERROR, MOD_CIM, "request_to_origin_server() failed, rv=%d",
		*rv);
	*rv = NKN_MM_CIM_NAMESPACE_MATCH;
	goto exit;
    }

    cache_uri[0] = '\0';
    /* create cacheuri */
    *rv = nstoken_to_uol_uri_stack(*ns_token, uri,
				      uri_len, host, hostlen,
				      cache_uri, cache_uri_len,
				      nsconf, &cachelen);

    if(!(*rv)) {
	DBG_LOG(ERROR, MOD_CIM, "nstoken_to_uol_uri_stack() failed, rv=%d",
		*rv);
	*rv = NKN_MM_CIM_NAMESPACE_MATCH;
	goto exit;
    }

    cod_uri = cache_uri;
    /* If the uri has hex encoded chars, get thh canonicalized for of it.
     * This will be used to open the cod and should be passed to AM.
     */
    if(hex_encoded) {
	iccp_info->flag |= HEX_ENCODED_URI;
	cod_uri = alloca(cachelen + 1);
	if(!canonicalize_url(cache_uri, cachelen, cod_uri,
					cachelen + 1, &bytes_used)) {
	    cachelen = bytes_used - 1;
	} else {
	    DBG_LOG(ERROR, MOD_CIM, "canonicalize_url() failed, rv = %d", *rv);
	    *rv = NKN_MM_CIM_INVALID_URI;
	    goto exit;
	}
	memcpy(cache_uri, cod_uri, cachelen);
	cache_uri[cachelen] = '\0';
	/* Copy the original uri to nkn_uri */
	if(uri_len > nkn_uri_len) {
	    DBG_LOG(ERROR, MOD_CIM, "uri %s is large than %ld", uri, nkn_uri_len);
	    *rv = NKN_MM_CIM_INVALID_URI;
	    goto exit;
	}
	memcpy(nkn_uri, uri, uri_len);
    }

    /* open cod */
    cod = nkn_cod_open(cod_uri, cachelen, NKN_COD_CIM_MGR);

    if(cod != NKN_COD_NULL) {
	iccp_info->cod = cod;
	*rv = 0;
    } else {
	*rv = NKN_MM_CIM_COD_OPEN_FAILURE;
    }
exit:;

    if(ns_token->u.token)
	release_namespace_token_t(*ns_token);

    shutdown_http_header(&hdr);
    return (cod);
}

/* Get expiry and cache-pin configuration during ingest
 */
int
cim_get_params(nkn_cod_t cod, time_t *tm, int *cache_pin, int *flags)
{
    iccp_info_t *iccp_info = NULL;
    int ret = -1;

    iccp_info = iccp_srvr_find_entry_cod(cod);
    if(iccp_info) {
	*tm = iccp_info->expiry;
	*cache_pin = iccp_info->cache_pin;
	*flags = iccp_info->flag;
	pthread_mutex_unlock(&iccp_info->entry_mutex);
	ret = 0;
    }
    return ret;
}

/*
 * Log the crawl ingest/delete status.
 */
static void
cim_log_crawl_status(iccp_info_t *iccp_info, char *ns_uuid,
			    iccp_action_type_t action, size_t file_size)
{
    char timestr[MAX_URI_SIZE];
    time_t uri_expiry = 0;
    char *cp = NULL;
    char ns_uuid_copy[MAX_URI_SIZE];
    char crawl_name[MAX_URI_SIZE];
    cim_stat_str_t *cim_stat_ptr = NULL;

    /* Get the crawl name from stat_ptr, otherwise log "-"
     */
    if(iccp_info->stat_ptr) {
	cim_stat_ptr = (cim_stat_str_t *)iccp_info->stat_ptr;
	if(cim_stat_ptr->crawl_name) {
	    memset(crawl_name, 0, MAX_URI_SIZE);
	    snprintf(crawl_name, MAX_URI_SIZE, "%s", cim_stat_ptr->crawl_name);
	    crawl_name[MAX_URI_SIZE-1] = '\0';
	} else {
	    crawl_name[0] = '-';
	    crawl_name[1] = '\0';
	}
    }
    if(ns_uuid) {
	memset(ns_uuid_copy, 0, MAX_URI_SIZE);
	snprintf(ns_uuid_copy, MAX_URI_SIZE, "%s", ns_uuid);
	ns_uuid_copy[MAX_URI_SIZE-1] = '\0';
    } else {
	ns_uuid_copy[0] = '-';
	ns_uuid_copy[1] = '\0';
    }

    if(action == ICCP_ACTION_ADD) {
	memset(timestr, 0, MAX_URI_SIZE);
	uri_expiry = iccp_info->expiry;
	ctime_r(&uri_expiry, timestr);
	if(timestr[0] == 0) {
	    strcpy(timestr, "unknown_time");
	}
	if((cp = strchr(timestr, '\n')) != 0)
	    *cp = 0;
	DBG_CRAWL(" ADD \"%s\" %s %s %s %d %lu [%s]", iccp_info->uri,
		crawl_name, ns_uuid_copy,
		(iccp_info->errcode ? "FAILED":"SUCCESS"), iccp_info->errcode,
		file_size, timestr);
	if(iccp_info->errcode) {
	    AO_fetch_and_add1(&glob_crawl_num_objects_add_failed);
	    if(iccp_info->stat_ptr) {
		cim_stat_ptr = (cim_stat_str_t *)iccp_info->stat_ptr;
		AO_fetch_and_add1(&cim_stat_ptr->num_objects_add_failed);
	    }
	} else {
	    AO_fetch_and_add1(&glob_crawl_num_objects_added);
	    if(iccp_info->stat_ptr) {
		cim_stat_ptr = (cim_stat_str_t *)iccp_info->stat_ptr;
		AO_fetch_and_add1(&cim_stat_ptr->num_objects_added);
	    }
	}
    } else {
	DBG_CRAWL(" DELETE \"%s\" %s %s %s %d", iccp_info->uri,
		crawl_name, ns_uuid_copy,
		(iccp_info->errcode ? "FAILED":"SUCCESS"), iccp_info->errcode);
	if(iccp_info->errcode) {
	    if(iccp_info->stat_ptr) {
		cim_stat_ptr = (cim_stat_str_t *)iccp_info->stat_ptr;
		AO_fetch_and_add1(&cim_stat_ptr->num_objects_delete_failed);
	    }
	    AO_fetch_and_add1(&glob_crawl_num_objects_delete_failed);
	} else {
	    if(iccp_info->stat_ptr) {
		cim_stat_ptr = (cim_stat_str_t *)iccp_info->stat_ptr;
		AO_fetch_and_add1(&cim_stat_ptr->num_objects_deleted);
	    }
	    AO_fetch_and_add1(&glob_crawl_num_objects_deleted);
	}
    }
}

static void
cim_cleanup_and_log(iccp_info_t *iccp_info, char *ns_uuid, int errcode,
	            iccp_action_type_t action, size_t file_size)
{
    cim_cleanup_iccp_info(iccp_info, errcode);
    if(iccp_info->status) {
	/* Possible abort miss case */
	if(iccp_info->expiry && (iccp_info->expiry <= nkn_cur_ts) &&
		(errcode == EINVAL || errcode == NKN_DM2_URI_EXPIRED ||
		 errcode == NKN_OM_NON_CACHEABLE)) {
	    glob_cim_inget_interval_exceeded++;
	} else {
	    cim_log_crawl_status(iccp_info, ns_uuid, action, file_size);
	}
    }
}


/*
 * Ingest/Delete task complete function called from MM
 */
void
cim_task_complete(nkn_cod_t cod, char *ns_uuid, int errcode,
		    iccp_action_type_t action, size_t file_size,
		    time_t expiry)
{
    iccp_info_t *iccp_info = NULL, *tmp = NULL;
    int do_callback = 0;

    iccp_info = iccp_srvr_find_all_entry_cod(cod);

    while(iccp_info) {

	do_callback = 1;

	/* PR: 770438 expiry in iccp_info should be  overwritten only
	 * when expiry is not_zero.
	 * if errcode == NKN_MM_CIM_RETRY, it is a non-crawler request.
	 * neednot update in that case.
	 */
	DBG_LOG(MSG, MOD_CIM, "changing expiry for uri %s from %ld to %ld, errcode = %d",
		    iccp_info->uri, iccp_info->expiry, expiry, errcode);

	if(expiry && (errcode != NKN_MM_CIM_RETRY))
	    iccp_info->expiry = expiry;

	cim_cleanup_and_log(iccp_info, ns_uuid, errcode,
			    action, file_size);
	tmp = iccp_info->next;
	pthread_mutex_unlock(&iccp_info->entry_mutex);
	iccp_info = tmp;
    }
    if(do_callback) {
	/* Call iccp srvr callback to return the status back to cue
 	 */
	iccp_srvr_callback();
    }
    return;
}

/*
 * Get the stat pointer based on the crawl name.
 * Check for it if its already created and stored in the hash table
 */
static void *
cim_get_stat_ptr(iccp_info_t *objp)
{
    int name_len;
    char def_stat_name[] = "unknown_crawl";
    char *crawl_name = objp->crawl_name;
    cim_stat_str_t *cim_stat_ptr = NULL;

    if(!crawl_name)
	crawl_name = def_stat_name;
	name_len = strlen(crawl_name);
    pthread_mutex_lock(&cim_stat_hash_table_mutex);
    cim_stat_ptr = g_hash_table_lookup(nkn_cim_stat_hash_table,
					crawl_name);
    if(!cim_stat_ptr) {
	cim_stat_ptr = (cim_stat_str_t *)nkn_calloc_type(1,
						    sizeof(cim_stat_str_t),
						    mod_cim_statptr);
	if(cim_stat_ptr) {
	    cim_stat_ptr->crawl_name = nkn_calloc_type(1, name_len+1,
							mod_cim_statptr);
	}
	if(cim_stat_ptr && cim_stat_ptr->crawl_name) {
	    memcpy(cim_stat_ptr->crawl_name, crawl_name, name_len);
	    cim_stat_ptr->crawl_name[name_len] = '\0';
	    g_hash_table_insert(nkn_cim_stat_hash_table,
				cim_stat_ptr->crawl_name,
				cim_stat_ptr);
	    (void)nkn_mon_add("cim_total_num_requests", crawl_name,
		    (void *)&cim_stat_ptr->num_post_tasks,
		    sizeof(cim_stat_ptr->num_post_tasks));
	    (void)nkn_mon_add("cim_objects_added", crawl_name,
		    (void *)&cim_stat_ptr->num_objects_added,
		    sizeof(cim_stat_ptr->num_objects_added));
	    (void)nkn_mon_add("cim_objects_add_failed", crawl_name,
		    (void *)&cim_stat_ptr->num_objects_add_failed,
		    sizeof(cim_stat_ptr->num_objects_add_failed));
	    (void)nkn_mon_add("cim_objects_deleted", crawl_name,
		    (void *)&cim_stat_ptr->num_objects_deleted,
		    sizeof(cim_stat_ptr->num_objects_deleted));
	    (void)nkn_mon_add("cim_objects_delete_failed", crawl_name,
		    (void *)&cim_stat_ptr->num_objects_delete_failed,
		    sizeof(cim_stat_ptr->num_objects_delete_failed));
	}
    }
    pthread_mutex_unlock(&cim_stat_hash_table_mutex);
    if(cim_stat_ptr) {
	AO_fetch_and_add1(&cim_stat_ptr->num_post_tasks);
	objp->stat_ptr = (void *)cim_stat_ptr;
    }
    return cim_stat_ptr;
}

/*
 * CIM callback function 
 * Called whenever there is data from CUE
 * Called with iccp_info entry mutex locked.
 */
void
cim_callback(void *ptr)
{
    iccp_info_t *iccp_info = (iccp_info_t *)ptr;
    nkn_cod_t cod;
    int ret = 0;
    char cache_uri[MAX_URI_HDR_HOST_SIZE + MAX_URI_SIZE + 1024];
    char nkn_uri[MAX_URI_HDR_HOST_SIZE + MAX_URI_SIZE + 1024];
    AM_pk_t pk;
    am_object_data_t object_data;
    nkn_pseudo_con_t *pcon = NULL;
    char ns_uuid[NKN_MAX_UID_LENGTH];
    namespace_token_t ns_token;

    if(iccp_info->status == ICCP_CLEANUP) {
	cim_cleanup_iccp_info(iccp_info, 0);
	return;
    }

    glob_cim_callback_count++;
    cim_get_stat_ptr(iccp_info);
    memset(cache_uri, 0, sizeof(cache_uri));
    memset(nkn_uri, 0, sizeof(nkn_uri));

    cod = cim_register_and_get_cod_locked(iccp_info, &ret, cache_uri,
					    sizeof(cache_uri), nkn_uri,
					    sizeof(nkn_uri),
					    &ns_token);

    if(cod) {
	if(iccp_info->command == ICCP_ACTION_DELETE) {
	    ret = nkn_mm_create_delete_task(cod);
	    if(ret)
		ret = NKN_MM_CIM_DEL_TASK_CR_FAILED;
	} else {
	    pk.name = cache_uri;
	    pk.provider_id = OriginMgr_provider;
	    pk.key_len = strlen(cache_uri);
	    memset(&object_data, 0, sizeof(object_data));
	    object_data.flags = AM_CIM_INGEST;
	    if(iccp_info->flag & VERSION_EXPIRED)
		object_data.flags |= AM_NEW_INGEST;
	    object_data.client_ip_family = AF_INET;
	    object_data.host_hdr = iccp_info->client_domain;
	    object_data.ns_token = ns_token;
	    if(iccp_info->data) {
		cim_register_data(iccp_info);
	    }
	    /* Cod can be changed in cim_register_data
	     */
	    object_data.in_cod   = iccp_info->cod;
	    object_data.client_ipv4v6.family = AF_INET;
	    object_data.client_ipv4v6.addr.v4.s_addr = inet_addr("127.0.0.1");
	    object_data.client_port = 0;
	    object_data.client_ip_family = AF_INET;

	    /* In case of hex encoded uri, the decoded uri has to be
	     * added to the header as MIME_HDR_X_NKN_DECODED_URI and the 
	     * original client requested uri has to be added to
	     * MIME_HDR_X_NKN_URI
	     */
	    if(iccp_info->flag & HEX_ENCODED_URI) {
		pcon = nkn_create_pcon();
		if(pcon) {
		    add_known_header(&pcon->con.http.hdr,
				    MIME_HDR_X_NKN_DECODED_URI,
				    cache_uri, strlen(cache_uri));
		    add_known_header(&pcon->con.http.hdr,
				    MIME_HDR_X_NKN_URI,
				    nkn_uri, strlen(nkn_uri));
		    pcon->con.module_type = NORMALIZER;
		    object_data.proto_data = (void *)pcon;
		}
	    }

	    /* Check if we atleast 1 tier to ingest */
	    ret = MM_get_fastest_put_ptype();
	    if(!ret) {
		ret = NKN_MM_CIM_NO_DISK_AVAIL;
	    } else {
		ret = 0;
		AM_inc_num_hits(&pk, 1, 0, NULL, &object_data, NULL);
	    }

	    if(pcon) {
		nkn_free_pseudo_con_t(pcon);
	    }
	}
    }

    if(ret) {
	if(cod != NKN_COD_NULL) {
	    ns_uuid[0]='\0';
	    mm_uridir_to_nsuuid(cache_uri, ns_uuid);
	    cim_cleanup_and_log(iccp_info, ns_uuid, ret,
				iccp_info->command, 0);
	} else {
	    cim_cleanup_and_log(iccp_info, NULL, ret,
				iccp_info->command, 0);
	}
	iccp_srvr_callback();
    }
}

