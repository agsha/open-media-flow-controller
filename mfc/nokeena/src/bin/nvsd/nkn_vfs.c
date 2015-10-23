#include "nkn_vfs_private.h"

//char *chhhh;
static void vfs_md_init(void)
{
	md_table = g_hash_table_new(g_str_hash, g_str_equal);
	pthread_mutex_init(&mdt_lock, NULL);
//	chhhh = nkn_calloc_type(1, 70 * 1024 * 1024, mod_vfs_nkn_cache_t);
}

static int vfs_create_task_data(nkn_vfs_con_t *con_info)
{
	cache_response_t *cptr;
	con_info->cptr = cptr = nkn_calloc_type(1,
				sizeof(cache_response_t),
				mod_vfs_cache_response_t);
	if (!con_info->cptr) {
//		NB: Bad coding practice to cleanup something not allocated in
//		within the function. Moved calls to free pseudo_con to failure
//		code for all calls to this function.
//		nkn_free_pseudo_con_t((nkn_pseudo_con_t *)con_info->con);
		return 1;
	}
	cptr->in_client_data.proto = NKN_PROTO_RTMP;
	memcpy(&(cptr->in_client_data.ipv4v6_address), &(con_info->con->src_ipv4v6),
		sizeof(ip_addr_t));
	cptr->in_client_data.port = con_info->con->src_port;
	cptr->in_client_data.client_ip_family = con_info->con->ip_family;
	cptr->in_proto_data = con_info->con;
	cptr->ns_token = con_info->ns_token;
	return 0;
}

static int vfs_create_http_con(nkn_vfs_con_t *con_info,
			       char *domain, char *filename, int proto_sw)
{
	char host[512], final_uri[1024], tmpuri[1024];
	int uri_length;
	int rv, hostlen;
	char *p_host = host;
	uint16_t port;
	size_t retlen;
	const namespace_config_t *nsc;

	con_info->con=(con_t *)nkn_alloc_pseudo_con_t();
	if (!con_info->con) {
		glob_vfs_err_nomem++;
		return 1;
	}
	final_uri[0] = '/';
	if (escape_str(&final_uri[1], sizeof(final_uri) - 1, filename,
		   strlen(filename), &uri_length)) {
		nkn_free_pseudo_con_t((nkn_pseudo_con_t *)con_info->con);
		return 1;
	}

	pthread_cond_init(&con_info->cond,NULL);
	con_info->con->ip_family = AF_INET; //todo, we need to detect it automatically
	SET_CON_FLAG(con_info->con, CONF_FIRST_TASK);
	init_http_header(&con_info->con->http.hdr, 0, 0);
	add_known_header(&con_info->con->http.hdr,
			 MIME_HDR_X_NKN_METHOD, "GET", 3);
	add_known_header(&con_info->con->http.hdr,
			 MIME_HDR_X_NKN_URI, final_uri, uri_length + 1);
	add_known_header(&con_info->con->http.hdr, MIME_HDR_HOST,
			 domain, strlen(domain));
	snprintf(tmpuri, sizeof(tmpuri), "/%s", filename);
	if (!proto_sw)
		con_info->ns_token =
			http_request_to_namespace(&con_info->con->http.hdr,
						  NULL);
	else {
		// using rtsp namespace query and assigning the result to the http conn token
		mime_header_t rtsp_mime_hdr;
		mime_hdr_init(&rtsp_mime_hdr, MIME_PROT_RTSP, 0, 0);
		char* port_str = strchr(domain, ':');
		add_known_header(&rtsp_mime_hdr, RTSP_HDR_X_ABS_PATH,
				 tmpuri, strlen(tmpuri));
		if (port_str++) {
			add_known_header(&rtsp_mime_hdr, RTSP_HDR_X_HOST,
				domain, strlen(domain)-strlen(port_str)-1);
			add_known_header(&rtsp_mime_hdr, RTSP_HDR_X_PORT,
				port_str, strlen(port_str));
		} else
			add_known_header(&rtsp_mime_hdr, RTSP_HDR_X_HOST,
					 domain, strlen(domain));
		con_info->ns_token = rtsp_request_to_namespace(&rtsp_mime_hdr);
	}

	if (con_info->ns_token.u.token == 0) {
		nkn_free_pseudo_con_t((nkn_pseudo_con_t *)con_info->con);
		glob_vfs_err_nomem++;
		return 1;
	}

	nsc = namespace_to_config(con_info->ns_token);
	if (!nsc) {
		DBG_LOG(MSG, MOD_RTSP,
			"namespace_to_config(token=0x%lx) failed",
			con_info->ns_token.u.token);
		nkn_free_pseudo_con_t((nkn_pseudo_con_t *)con_info->con);
		return 1;
	}
	con_info->con->http.nsconf = nsc;
	release_namespace_token_t(con_info->ns_token);

	rv = request_to_origin_server(REQ2OS_CACHE_KEY, 0,
				      &con_info->con->http.hdr,
				      nsc, 0, 0, &p_host, sizeof(host),
				      &hostlen, &port, 0, 1);
	if (rv) {
		DBG_LOG(MSG, MOD_RTSP,
			"request_to_origin_server() failed, rv=%d", rv);
		nkn_free_pseudo_con_t((nkn_pseudo_con_t *)con_info->con);
		return 1;
	}

	con_info->uri = nstoken_to_uol_uri(con_info->ns_token,
					   tmpuri, strlen(tmpuri),
					   host, hostlen,
					   &retlen);
        if (!con_info->uri) {
		DBG_LOG(MSG, MOD_RTSP,
			"nstoken_to_uol_uri(token=0x%lx) failed",
			con_info->ns_token.u.token);
		nkn_free_pseudo_con_t((nkn_pseudo_con_t *)con_info->con);
		return 1;
        }

	con_info->cod = nkn_cod_open(con_info->uri, retlen, NKN_COD_VFS_MGR);
	if (con_info->cod == NKN_COD_NULL) {
		free(con_info->uri);
		nkn_free_pseudo_con_t((nkn_pseudo_con_t *)con_info->con);
		glob_vfs_cod_open_fail++;
		return 1;
	}
	glob_vfs_cod_open++;
	return 0;
}


static off_t vfs_read_fs_cache(nkn_vfs_cache_t * pfs,
			       off_t pos, off_t reqlen,
			       off_t (*copy_function)(nkn_vfs_cache_t *pfs,
			       cache_response_t *cptr))
{
        cache_response_t *cptr;
	int task_length;
	nkn_vfs_con_t *con_info = pfs->con_info;
	off_t tot_file_size;

	if (reqlen == 0) {
		task_length = 0;
	} else {
		task_length = (pfs->tot_file_size - pos > reqlen)?
			reqlen : pfs->tot_file_size - pos;
	}
	// safety check
	assert(!RTSP_CHECK_FLAG(con_info, RTSP_FLAG_TASK_DONE));
	tot_file_size = pfs->tot_file_size;
	vfs_post_sched_a_task(con_info, tot_file_size,
			      pos, task_length);
        if (con_info->nkn_taskid[con_info->taskindex] == -1)
		return -1;

	pthread_mutex_lock(&con_info->mutex);
        if (!RTSP_CHECK_FLAG(con_info, RTSP_FLAG_TASK_DONE)) {
		pthread_cond_wait(&con_info->cond, &con_info->mutex);
	}
	RTSP_UNSET_FLAG(con_info, RTSP_FLAG_TASK_DONE);
	pthread_mutex_unlock(&con_info->mutex);

	cptr = con_info->cptr;
	if (cptr->errcode) {
		// End of file.
		vfs_cleanup_a_task(con_info->nkn_taskid[con_info->taskindex]);
		return cptr->errcode; // first time
	}
	pfs->provider = pfs->provider + (cptr->provider == OriginMgr_provider);
        if (!pfs->tot_file_size && cptr->attr) {
        // ||
	//			    RTSP_CHECK_FLAG(con_info, RTSP_FLAG_NOATTR))) {
//	    if (RTSP_CHECK_FLAG(con_info, RTSP_FLAG_NOATTR)) {
//		pfs->tot_file_size = 0;
//	    } else {
		pfs->tot_file_size = cptr->attr->content_length;
//	    }
            if (!copy_function) {
                vfs_cleanup_a_task(con_info->nkn_taskid[con_info->taskindex]);
                return 0;
            }
        }
	return copy_function(pfs, cptr);
}

// NB: pfs->data_pages is pointing to a vccmp_stat_data_t that should be 
//     filled in from the vccmp_stat_data_t->nkn_attr_t data.
static off_t get_vc_stat_data(nkn_vfs_cache_t *pfs, cache_response_t *cptr)
{
    nkn_vfs_con_t *con_info = pfs->con_info;
    nkn_attr_t* attr = cptr->attr;
    vccmp_stat_data_t* stat_data = (vccmp_stat_data_t*)pfs->meta_data;
    if (stat_data) {
	stat_data->file_length = attr->content_length;

	// TODO: Need to know how extract these out.
	// Required values for clients.
	stat_data->last_modified_time = 0;
	stat_data->expiration_time = 0;
	memset(stat_data->etag, 0, sizeof(stat_data->etag));

	// Optional, values if not much work.
	memset(stat_data->content_type, 0, sizeof(stat_data->content_type));
	stat_data->bit_rate = 0;
    }

    if (RTSP_CHECK_FLAG(pfs, RTSP_FLAG_CACHE) && pfs->data_pages) {
        copy_to_mem(pfs, cptr);
    } else {
        vfs_cleanup_a_task(con_info->nkn_taskid[con_info->taskindex]);
    }
    return 0;
}

static off_t direct_copy(nkn_vfs_cache_t *pfs, cache_response_t *cptr)
{
	nkn_vfs_con_t *con_info = pfs->con_info;
	int i;
	for (i=0; i<cptr->num_nkn_iovecs; i++) {
		memcpy(*pfs->data_pages + pfs->content_size,
			cptr->content[i].iov_base,
			cptr->content[i].iov_len);
		pfs->content_size += cptr->content[i].iov_len;
	}
	vfs_cleanup_a_task(con_info->nkn_taskid[con_info->taskindex]);
	return pfs->content_size;
}

static off_t copy_to_mem(nkn_vfs_cache_t *pfs, cache_response_t *cptr)
{
	int i =0;
	int counter = 0;
	nkn_iovec_t *content = cptr->content;
	char **data_pages = pfs->data_pages;
	int num_iter = cptr->num_nkn_iovecs / 4;

	for ( ; counter < num_iter ; ++counter,i = i+ 4) {
		*data_pages++ = content[i].iov_base;
		*data_pages++ = content[i+1].iov_base;
		*data_pages++ = content[i+2].iov_base;
		*data_pages++ = content[i+3].iov_base;
		pfs->content_size += (content[i].iov_len +
				      content[i+1].iov_len +
				      content[i+2].iov_len +
				      content[i+3].iov_len);
	}
	num_iter = cptr->num_nkn_iovecs & 3;
	switch (num_iter) {
		case 0:
			break;
		case 1:
			*data_pages++ = content[i].iov_base;
			pfs->content_size += content[i].iov_len;
			break;
		case 2:
			*data_pages++ = content[i].iov_base;
			*data_pages++ = content[i + 1].iov_base;
			pfs->content_size += (content[i].iov_len +
					      content[i+1].iov_len);
			break;
		case 3:
			*data_pages++ = content[i].iov_base;
			*data_pages++ = content[i + 1].iov_base;
			*data_pages++ = content[i + 2].iov_base;
			pfs->content_size += (content[i].iov_len +
					      content[i+1].iov_len +
					      content[i+2].iov_len);
			break;
	}
	++pfs->con_info->taskindex;
	return pfs->content_size;
}

static void vfs_post_sched_a_task(nkn_vfs_con_t *con_info,
				  off_t tot_file_size,
				  off_t offset_requested,
				  off_t length_requested)
{
        cache_response_t *cr = con_info->cptr;
	cr->uol.cod = con_info->cod;
        cr->magic = CACHE_RESP_REQ_MAGIC;
	cr->uol.offset = offset_requested;
	cr->uol.length = length_requested;
	//cr->in_rflags = CR_RFLAG_GETATTR;
	cr->attr = NULL;
	if (tot_file_size == 0) {
	    if (RTSP_CHECK_FLAG(con_info, RTSP_FLAG_NOAMHITS)) {
		cr->in_rflags = CR_RFLAG_GETATTR |
				CR_RFLAG_FIRST_REQUEST |
				CR_RFLAG_NO_AMHITS;
	    } else {
		cr->in_rflags = CR_RFLAG_GETATTR |
		    CR_RFLAG_FIRST_REQUEST;
	    }
	    //	if (length_requested == 0)
	    //  cr->in_rflags |= CR_RFLAG_NO_DATA;
	} else {
	    cr->in_rflags = 0;
	}
	if (RTSP_CHECK_FLAG(con_info, RTSP_INTERNAL_TASK)) {
		cr->in_rflags |= CR_RFLAG_INTERNAL;
	}
	if (RTSP_CHECK_FLAG(con_info, RTMP_FUSE_TASK)) {
	        cr->in_rflags |= CR_RFLAG_FUSE_HITS;
	}

        // Create a new task
        con_info->nkn_taskid[con_info->taskindex]
					= nkn_task_create_new_task(
                                        TASK_TYPE_CHUNK_MANAGER,
                                        TASK_TYPE_VFS_SVR,
                                        TASK_ACTION_INPUT,
                                        0,
                                        cr,
                                        sizeof(cache_response_t),
                                        0);
	assert(con_info->nkn_taskid[con_info->taskindex] != -1);
	nkn_task_set_private(TASK_TYPE_VFS_SVR,
			     con_info->nkn_taskid[con_info->taskindex],
			     (void *)con_info);
        nkn_task_set_state(con_info->nkn_taskid[con_info->taskindex],
			   TASK_STATE_RUNNABLE);
	glob_vfs_open_task ++;
	glob_vfs_tot_task ++;
        return;
}

static void vfs_cleanup_a_task(nkn_task_id_t task_id)
{
	if (task_id == -1)
		return;
        nkn_task_set_action_and_state(task_id, TASK_ACTION_CLEANUP,
					TASK_STATE_RUNNABLE);
}

static void vfs_mgr_cleanup(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
        return ;
}

static void vfs_mgr_input(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
        return ;
}

static void vfs_mgr_output(nkn_task_id_t id)
{
        cache_response_t *cptr;
	nkn_vfs_con_t *con_info;

        cptr = (cache_response_t *) nkn_task_get_data(id);
        if (!cptr) {
                DBG_LOG(MSG, MOD_RTSP,
			"ERROR: nkn_task_get_data returns NULL\n");
                return;
        }
        assert(cptr->magic == CACHE_RESP_RESP_MAGIC);

	con_info = (nkn_vfs_con_t *)
			nkn_task_get_private(TASK_TYPE_VFS_SVR, id);

	// sanity check
	if (!con_info ||
		con_info->nkn_taskid[con_info->taskindex] != id) {
                DBG_LOG(MSG, MOD_RTSP,
			"ERROR: !pfs || pfs->nkn_taskid != id\n");
		return;
	}
	glob_vfs_open_task--;

	pthread_mutex_lock(&con_info->mutex);
	RTSP_SET_FLAG(con_info, RTSP_FLAG_TASK_DONE);
	pthread_cond_signal(&con_info->cond);
	pthread_mutex_unlock(&con_info->mutex);
}

static void copy_iovec_to_user(nkn_vfs_cache_t *pfs,
			       char **data_ptr, off_t csize)
{
	off_t offset = pfs->cur_pos - pfs->content_pos;
	off_t direct_index = offset >> BM_PAGE_SHIFT;
	off_t index_offset = offset & (BM_PAGE_SIZE - 1);
	off_t len = BM_PAGE_SIZE - index_offset;
	if (len >= csize) {
		memcpy(*data_ptr, *(pfs->data_pages + direct_index) +
			index_offset, csize);
	} else {
		memcpy(*data_ptr, *(pfs->data_pages + direct_index) +
			index_offset, len);
		csize -= len;
		*data_ptr += len;
		++direct_index;
		while (BM_PAGE_SIZE < csize) {
			memcpy(*data_ptr, *(pfs->data_pages + direct_index),
				BM_PAGE_SIZE);
			++direct_index;
			*data_ptr += BM_PAGE_SIZE;
			csize -= BM_PAGE_SIZE;
		}
		if (csize) {
			memcpy(*data_ptr, *(pfs->data_pages + direct_index),
				csize);
		}
	}
}

static void copy_iovec_to_user_opt(nkn_vfs_cache_t *pfs,
			       char **data_ptr, off_t csize,
			       char **temp_data_ptr, int *tofree)
{
	off_t offset = pfs->cur_pos - pfs->content_pos;
	off_t direct_index = offset >> BM_PAGE_SHIFT;
	off_t index_offset = offset & (BM_PAGE_SIZE - 1);
	off_t len = BM_PAGE_SIZE - index_offset;
	if (len >= csize) {
		*temp_data_ptr = *(pfs->data_pages + direct_index) +
				 index_offset;
		*tofree = 0;
	} else {
		memcpy(*data_ptr, *(pfs->data_pages + direct_index) +
			index_offset, len);
		csize -= len;
		*data_ptr += len;
		++direct_index;
		while (BM_PAGE_SIZE < csize) {
			memcpy(*data_ptr, *(pfs->data_pages + direct_index),
				BM_PAGE_SIZE);
			++direct_index;
			*data_ptr += BM_PAGE_SIZE;
			csize -= BM_PAGE_SIZE;
		}
		if (csize) {
			memcpy(*data_ptr, *(pfs->data_pages + direct_index),
				csize);
		}
		*tofree = 1;
	}
}


static int is_metadata_avail(char *key, nkn_vfs_cache_t *pfs)
{
	pthread_mutex_lock(&mdt_lock);
	nkn_vfs_mdt_obj_t *mobj =
		(nkn_vfs_mdt_obj_t *)g_hash_table_lookup(md_table, key);
	if (mobj) {
		mobj->ref_cnt++;
		pfs->data_pages = mobj->data_pages;
		pfs->content_pos = mobj->content_pos;
		pfs->content_size = mobj->content_size;
		pfs->tot_file_size = mobj->content_pos + mobj->content_size;
		pfs->filename = mobj->filename;
		pthread_mutex_unlock(&mdt_lock);
		return 1;
	}
	pthread_mutex_unlock(&mdt_lock);
	return 0;
}

static void free_con_info(nkn_vfs_cache_t *pfs) {
	nkn_vfs_con_t *con_info = pfs->con_info;
	if (!con_info)
		return;
	// free ns_token
	release_namespace_token_t(con_info->ns_token);
	// free uri
	free(con_info->uri);
	// free con_info
	nkn_free_pseudo_con_t((nkn_pseudo_con_t *)con_info->con);
        if (con_info->cod != NKN_COD_NULL) {
		nkn_cod_close(con_info->cod, NKN_COD_VFS_MGR);
		++glob_vfs_cod_close;
	}
	if (con_info->nkn_taskid) {
		free_buffer_pointers(con_info->nkn_taskid,
				     con_info->taskindex);
		free(con_info->nkn_taskid);
	}
	// free the task data pointer
	free(con_info->cptr);
	// free con_info
	free(con_info);

	pfs->con_info = NULL;
}


static void cleanup_metadata(nkn_vfs_cache_t *pfs)
{
	nkn_vfs_con_t *con_info = pfs->con_info;
	if (con_info) {
		if (con_info->nkn_taskid)
			free(con_info->nkn_taskid);
		release_namespace_token_t(con_info->ns_token);
		nkn_free_pseudo_con_t(
			(nkn_pseudo_con_t *)con_info->con);

                free(con_info->uri);
		if (con_info->cod != NKN_COD_NULL) {
			nkn_cod_close(con_info->cod, NKN_COD_VFS_MGR);
		}

		free(con_info->cptr);
		free(con_info);

	}
        DBG_LOG(MSG, MOD_RTSP,
                "open stream=%p ret=-1 meta data failed", pfs);
        free(pfs->filename);
	free(pfs);
}

static off_t nkn_cache_read(nkn_vfs_cache_t *pfs, off_t size,
			    off_t rmemb, char **data_ptr);
static FILE *nkn_fopen(const char *path, const char *mode)
{
	char *pfilename, *pdomain;
	char domain[256];
	char in_port[256];
	struct in_addr in;
	nkn_vfs_cache_t *pfs;
	nkn_vfs_con_t	*con_info;
	nkn_task_id_t *taskid;
	int len, retval;
	char fuse_identifier;
	char *pport;

	int proto_sw = 0; //use http request_to_namespace

	if (!path || !mode) {
		return NULL;
	}

	if (*path == ':') {
                fuse_identifier = *(path+1);
		pdomain = (char *)path + 3;

		proto_sw = 1; // from rtsp : use rtsp_request_to_namespace

		if (fuse_identifier == 'R') {
		    pport = strchr(pdomain, ':');
		    if (!pport) {
			return NULL;
		    }
		    pfilename = strchr(pport + 1, ':');
		    if (!pfilename) {
			return NULL;
		    }
		    len = pport - pdomain;
		    memcpy(domain, pdomain, len);
		    domain[len] = 0;
		    pport++;
		    len = pfilename - pport;
		    memcpy(in_port, pport, len);
		    in_port[len] = '\0';

		    strcat(domain, ":");
		    strcat(domain, in_port);
		} else {
		    pfilename = strchr(pdomain + 1, ':');
		    if (!pfilename) {
			return NULL;
		    }
		    len = pfilename - pdomain;
		    memcpy(domain, pdomain, len);
		    domain[len] = 0;
		}
		// skip ':'
		pfilename ++;
	}
	else {
		in.s_addr = eth0_ip_address;
		snprintf(domain, sizeof(domain), "%s", inet_ntoa(in));

		pfilename = (char *)path;
	}
	// create vfs cache
	pfs = (nkn_vfs_cache_t *)nkn_calloc_type(1,
			sizeof(nkn_vfs_cache_t), mod_vfs_nkn_cache_t);
	if (!pfs) {
		DBG_LOG(MSG, MOD_RTSP, "open stream=%p ret=-1 ....", pfs);
		return NULL;
	}

	// create con cache [ used only on direct / indirect data cache ]
	pfs->con_info = con_info =(nkn_vfs_con_t *)nkn_calloc_type(1,
				   sizeof(nkn_vfs_con_t), mod_vfs_nkn_cache_t);
	if (!pfs->con_info) {
		DBG_LOG(MSG, MOD_RTSP,
			"open stream=%p ret=-1 conf info failed", pfs);
		free(pfs);
		return NULL;
	}

	if (strchr(mode, 'R')) {
		RTSP_SET_FLAG(pfs, RTSP_FLAG_DIRECT);
		taskid = (nkn_task_id_t *)nkn_calloc_type(1,
			  sizeof(nkn_task_id_t), mod_vfs_nkn_cache_t);
	} else if (strchr(mode, 'M')) {
		if (is_metadata_avail(pfilename, pfs)) {
			RTSP_SET_FLAG(pfs, RTSP_FLAG_META_DATA);
			return (FILE*) pfs;
		}
		RTSP_SET_FLAG(pfs->con_info, RTSP_FLAG_NOAMHITS);
		taskid = (nkn_task_id_t *)nkn_calloc_type(1,
			  sizeof(nkn_task_id_t), mod_vfs_nkn_cache_t);
	} else if (strchr(mode, 'I')) {
		RTSP_SET_FLAG(pfs, RTSP_FLAG_DIRECT);
		RTSP_SET_FLAG(pfs->con_info, RTSP_INTERNAL_TASK);
		taskid = (nkn_task_id_t *)nkn_calloc_type(1,
			sizeof(nkn_task_id_t), mod_vfs_nkn_cache_t);
	} else if (strchr(mode, 'K')) {
		RTSP_SET_FLAG(pfs, RTSP_FLAG_DIRECT);
		RTSP_SET_FLAG(pfs->con_info, RTMP_FUSE_TASK);
		taskid = (nkn_task_id_t *)nkn_calloc_type(1,
			sizeof(nkn_task_id_t), mod_vfs_nkn_cache_t);
	} else if (strchr(mode,'J')) {
		RTSP_SET_FLAG(pfs, RTSP_FLAG_CACHE);
		RTSP_SET_FLAG(pfs->con_info, RTSP_FLAG_NOAMHITS);
		pfs->data_pages = (char **)nkn_calloc_type(BM_IOV_MAX,
							   sizeof(char *),
							   mod_vfs_nkn_cache_t);
                taskid = (nkn_task_id_t *)nkn_calloc_type(MAX_TASKID,
							  sizeof(nkn_task_id_t),
							  mod_vfs_nkn_cache_t);
	} else if (strchr(mode,'V')) {
		// NB: 'V' mode is for nkn_vcopen() calls, which expect the 
		//     meta_data member to have an allocated vccm_stat_data_t.
		RTSP_SET_FLAG(pfs, RTSP_FLAG_CACHE);
		RTSP_SET_FLAG(pfs->con_info, RTMP_FUSE_TASK);
		pfs->data_pages = (char **)nkn_calloc_type(BM_IOV_MAX,
							   sizeof(char *),
							   mod_vfs_nkn_cache_t);
                taskid = (nkn_task_id_t *)nkn_calloc_type(MAX_TASKID,
							  sizeof(nkn_task_id_t),
							  mod_vfs_nkn_cache_t);
		pfs->meta_data = (nkn_vfs_cache_t *)nkn_calloc_type(1,
							   sizeof(vccmp_stat_data_t),
							   mod_vccmp_stat_data_t);
	} else if (strchr(mode,'S')) {
		// NB: 'V' mode is for nkn_vcstat() calls, which expect the 
		//     meta_data member to have an allocated vccm_stat_data_t.
		//     nkn_vcstat() closes the handle immediately, so does not
		//     allocate any data_pages and sets the FLAG_DIRECT bit.
		RTSP_SET_FLAG(pfs, RTSP_FLAG_DIRECT);
		RTSP_SET_FLAG(pfs->con_info, RTSP_INTERNAL_TASK);
                taskid = (nkn_task_id_t *)nkn_calloc_type(1,
							  sizeof(nkn_task_id_t),
							  mod_vfs_nkn_cache_t);
		pfs->meta_data = (nkn_vfs_cache_t *)nkn_calloc_type(1,
							   sizeof(vccmp_stat_data_t),
							   mod_vccmp_stat_data_t);
	} else {
		RTSP_SET_FLAG(pfs, RTSP_FLAG_CACHE);
		RTSP_SET_FLAG(pfs->con_info, RTMP_FUSE_TASK);
		pfs->data_pages = (char **)nkn_calloc_type(BM_IOV_MAX,
				sizeof(char *), mod_vfs_nkn_cache_t);
		taskid = (nkn_task_id_t *)nkn_calloc_type(MAX_TASKID,
			  sizeof(nkn_task_id_t), mod_vfs_nkn_cache_t);
	}
	// assign the filename
	pfs->filename = nkn_strdup_type(pfilename, mod_vfs_nkn_cache_t);
	// http protocol callback
	retval = vfs_create_http_con(con_info, domain, pfilename, proto_sw);
	if (retval) {
		free(pfs->con_info);
		free(pfs->filename);
		DBG_LOG(MSG,
			MOD_RTSP,
			"open stream=%p ret=-1 http con failed", pfs);
		free(pfs);
		return NULL;
	}
	// create task related tasks
	retval = vfs_create_task_data(con_info);
	if (retval) {
		nkn_free_pseudo_con_t((nkn_pseudo_con_t *)con_info->con);
		free(pfs->con_info);
		free(pfs->filename);
		DBG_LOG(MSG, MOD_RTSP,
			"open stream=%p ret=-1 task create failed", pfs);
		free(pfs);
		return NULL;
	}
	con_info->nkn_taskid = taskid;
	con_info->taskindex = 0;
retry_read:
        if (strchr(mode,'V'))
            retval = vfs_read_fs_cache(pfs, 0, ONE_RTSP_CHUNK, get_vc_stat_data);
        else if (strchr(mode,'S'))
            retval = vfs_read_fs_cache(pfs, 0, 0, get_vc_stat_data);
        else
            retval = vfs_read_fs_cache(pfs, 0, 0, NULL);
        
	if (retval != 0) {
		if (retval == NKN_BUF_VERSION_EXPIRED) {
			char *ext = strrchr(pfs->filename, '.');
			if (ext && strcasecmp(ext, ".nknc")) {
				nkn_cod_close(con_info->cod, NKN_COD_VFS_MGR);
				con_info->cod = nkn_cod_open(con_info->uri,
					strlen(con_info->uri), NKN_COD_VFS_MGR);
				if (con_info->cod != NKN_COD_NULL)
					goto retry_read;
			}
		}
		release_namespace_token_t(con_info->ns_token);
		free(con_info->cptr);
		nkn_free_pseudo_con_t((nkn_pseudo_con_t *)pfs->con_info->con);
		free(con_info->uri);
		free(con_info->nkn_taskid);
		free(pfs->data_pages);
		if (con_info->cod != NKN_COD_NULL) {
			nkn_cod_close(con_info->cod, NKN_COD_VFS_MGR);
		}
		free(pfs->con_info);
		free(pfs->filename);
		DBG_LOG(MSG, MOD_RTSP,
			"open stream=%p ret=%d error from BM",
			pfs, retval);
		free(pfs);
		glob_vfs_err_nomem++;
		return NULL;
	}
	if (strchr(mode, 'M')) {
		char *ext = strrchr(pfs->filename, '.');
		if (ext) {
			if (nkn_metadata_read_ext(pfs, ext) == -1)
			{
				cleanup_metadata(pfs);
				return NULL;
			} else
				RTSP_SET_FLAG(pfs, RTSP_FLAG_META_DATA);
		}
	} else if (!strchr(mode,'V') && !strchr(mode, 'S')) {
		if (*path == ':' && !RTSP_CHECK_FLAG(pfs, RTSP_FLAG_DIRECT)) {
			pfs->meta_data =
			(nkn_vfs_cache_t *)nkn_fopen(path, "M");
			DBG_LOG(MSG, MOD_RTSP, "open stream=%p ret=0 ....", pfs);
		} else {
			pfs->meta_data = NULL;
		}
#if 0
		nkn_cache_read(pfs, pfs->tot_file_size,1
			    , NULL);
#endif
	}
	glob_vfs_open_cache ++;
	return (FILE *) pfs;
}

static off_t nkn_direct_read(nkn_vfs_cache_t *pfs, off_t size,
			     off_t rmemb, char **data_ptr)
{
	off_t rsize, retval;
	off_t totreqlen = size * rmemb;
	nkn_vfs_con_t *con_info = pfs->con_info;
	if (pfs->eof == 1) {
		return 0;
	}
	if (totreqlen + pfs->cur_pos > pfs->tot_file_size) {
		totreqlen = pfs->tot_file_size - pfs->cur_pos;
		rmemb = totreqlen / size;
	}
	pfs->content_pos = pfs->cur_pos;
	pfs->content_size = 0;
	pfs->data_pages = data_ptr;
	con_info->taskindex = 0;
get_more_data:
	rsize = totreqlen - pfs->content_size;
	if ((pfs->content_pos + rsize) > pfs->tot_file_size) {
		rsize = pfs->tot_file_size - pfs->content_size;
	}
	retval = vfs_read_fs_cache(pfs, pfs->content_pos +
				   pfs->content_size, rsize, direct_copy);
	if (retval) {
		if (retval == NKN_BUF_VERSION_EXPIRED) {
			nkn_cod_close(con_info->cod, NKN_COD_VFS_MGR);
			con_info->cod = nkn_cod_open(con_info->uri,
					strlen(con_info->uri),
					NKN_COD_VFS_MGR);
			if (con_info->cod == NKN_COD_NULL) {
				return EIO;
			}
			if (pfs->meta_data) {
				nkn_fclose((FILE *)pfs->meta_data);
				pfs->meta_data = NULL;
			}
			goto get_more_data;
		}
		if (pfs->content_size == 0) {
			pfs->eof = 1;
			return 0;
		}
	}
	if (pfs->content_size < (off_t)totreqlen) {
		goto get_more_data;
	}
	rmemb = pfs->content_size / size;
	pfs->cur_pos += pfs->content_size;
	return rmemb;
}

static int nkn_metadata_read_ext(nkn_vfs_cache_t *pfs, char *ext)
{
	int retval = -1;
	if (strcasecmp(ext, ".mp4") == 0) {
		retval = nkn_read_mp4_metadata(pfs);
	}
	return retval;
}

static inline void copy_pages(char **dest, char **src, off_t n_pages)
{
	int i =0;
	for (; i < n_pages ; ++i) {
		dest[i] = src[i];
	}
}

#if 0
int get4Val (nkn_vfs_cache_t *pfs, off_t diff, off_t readsize,
	     char **page, off_t *j, uint64_t *u64bit)
{
	uint16_t usbit;
	off_t fin_diff = diff - (BM_PAGE_SIZE - 4);
	*j += fin_diff;
	if (*j >= readsize) {
		return -1;
	}
	switch(fin_diff) {
		case 0: // should not happen
			break;
		case 1:
			*u64bit = (*(uint8_t *)*page) * pow(2,24);
			*page = pfs->data_pages[*j / BM_PAGE_SIZE];
			usbit = (*(uint16_t *)*page);
			*u64bit += swap_uint16(usbit) * pow(2,8);
			*page += 2;
			*u64bit += (*(uint8_t *)*page);
			*page += 1;
			break;
		case 2:
			usbit = (*(uint16_t*)*page);
			*u64bit = swap_uint16(usbit) * pow(2,16);
			*page = pfs->data_pages[*j / BM_PAGE_SIZE];
			usbit =  (*(uint16_t *)*page);
			*u64bit += swap_uint16(usbit);
			*page += 2;
			break;
		case 3:
			usbit = (*(uint16_t*)*page);
			*u64bit = swap_uint16(usbit) * pow(2,16);
			*page += 2;
			*u64bit += (*(uint8_t *)*page);
			*page = pfs->data_pages[*j / BM_PAGE_SIZE];
			*u64bit += (*(uint8_t *)*page);
			*page += 1;
			break;
	}
	return 0;
}

int get8Val(nkn_vfs_cache_t *pfs, off_t diff, off_t readsize,
	    char **page, off_t *j, uint64_t *u64bit)
{
	uint16_t u16bit;
	uint32_t u32bit;
	off_t fin_diff = diff - (BM_PAGE_SIZE - 8);
	*j += fin_diff;
	if (*j >= readsize) {
		return -1;
	}
	switch(fin_diff) {
		case 0: // should not happen
			break;
		case 1:
			*u64bit = (*(uint8_t *)*page) * pow(2,56);
			*page = pfs->data_pages[*j / BM_PAGE_SIZE];
			u32bit = (*(uint32_t*)*page);
			*u64bit += swap_uint32(u32bit) * pow(2,24);
			*page += 4;
			u16bit = (*(uint16_t *)*page);
			*u64bit += swap_uint16(u16bit) * pow(2,8);
			*page += 2;
			*u64bit += (*(uint8_t *)*page);
			*page += 1;
			break;
		case 2:
			u16bit = (*(uint16_t*)*page);
			*u64bit = swap_uint16(u16bit) * pow(2,48);
			*page = pfs->data_pages[*j / BM_PAGE_SIZE];
			u32bit = (*(uint32_t*)*page);
			*u64bit += swap_uint32(u32bit) * pow(2,24);
			*page += 4;
			u16bit = (*(uint16_t *)*page);
			*u64bit += swap_uint16(u16bit) * pow(2,8);
			*page += 2;
			break;
		case 3:
			u16bit = (*(uint16_t *)*page);
			*u64bit = swap_uint16(u16bit) * pow(2,48);
			*page += 2;
			*u64bit += (*(uint8_t *)*page) * pow(2,40);
			*page = pfs->data_pages[*j / BM_PAGE_SIZE];
			u32bit = (*(uint32_t*)*page);
			*u64bit += swap_uint32(u32bit) * pow(2,8);
			*page += 4;
			*u64bit += (*(uint8_t *)*page);
			*page += 1;
			break;
		case 4:
			u32bit = (*(uint32_t*)*page);
			*u64bit = swap_uint32(u32bit) * pow(2,32);
			*page = pfs->data_pages[*j / BM_PAGE_SIZE];
			u16bit = (*(uint16_t *)*page);
			*u64bit = swap_uint16(u16bit) * pow(2,16);
			*page += 2;
			u16bit = (*(uint16_t *)*page);
			*u64bit += swap_uint16(u16bit);
			*page += 2;
			break;
		case 5:
			u32bit = (*(uint32_t*)*page);
			*u64bit = swap_uint32(u32bit) * pow(2,32);
			*page += 4;
			*u64bit += (*(uint8_t *)*page) * pow(2,24);
			*page = pfs->data_pages[*j / BM_PAGE_SIZE];
			u16bit = (*(uint16_t *)*page);
			*u64bit = swap_uint16(u16bit) * pow(2,8);
			*page += 2;
			*u64bit += (*(uint8_t *)*page);
			*page += 1;
			break;
		case 6:
			u32bit = (*(uint32_t*)*page);
			*u64bit = swap_uint32(u32bit) * pow(2,32);
			*page += 4;
			u16bit = (*(uint16_t *)*page);
			*u64bit += swap_uint16(u16bit) * pow(2,16);
			*page = pfs->data_pages[*j / BM_PAGE_SIZE];
			u16bit = (*(uint16_t *)*page);
			*u64bit = swap_uint16(u16bit);
			*page += 2;
			break;
		case 7:
			u32bit = (*(uint32_t*)*page);
			*u64bit = swap_uint32(u32bit) * pow(2,32);
			*page += 4;
			u16bit = (*(uint16_t *)*page);
			*u64bit += swap_uint16(u16bit) * pow(2,16);
			*page += 2;
			*u64bit += (*(uint8_t *)*page) * pow(2,8);
			*page = pfs->data_pages[*j / BM_PAGE_SIZE];
			*u64bit += (*(uint8_t *)*page);
			*page += 1;
			break;
	}
	return 0;
}
#endif

static int parse_for_moov_linear(nkn_vfs_cache_t *pfs,
			  off_t readsize,
			  off_t *readoffset,
			  off_t *rem_readsize,
			  int *dontfree)
{
	char *data = pfs->data_pages[0];
	char *temp_data;
	off_t j = 0, k;
	uint64_t u64bit = 0;
	uint32_t u32bit = 0;
	uint64_t type;
	while ( j < readsize) {
		temp_data = data;
		k = j;
		if ( k + 4 < readsize) {
			u32bit = *(uint32_t *)temp_data;
			u64bit = swap_uint32(u32bit);
			temp_data += 4;
			k = k + 4;
		} else {
			return -1;
		}
		if (u64bit == 1) {
			if ( k + 8 < readsize) {
				u64bit = *(uint64_t*)temp_data;
				u64bit = swap_uint64(u64bit);
				temp_data += 8;
				k = k + 8;
			} else {
				return -1;
			}
		}
		if ( k + 4 < readsize) {
			type = *(uint32_t*)temp_data;
			switch (type) {
				case MOOV_ID:        // 'm','o','o','v'
					if ((j + (off_t)u64bit) <= readsize) {
						*readoffset = 0;
						*rem_readsize = 0;
					} else {
						*readoffset = readsize + 1;
						*rem_readsize =
							u64bit + j - readsize;
					}
					*dontfree = 1;
					return 0;
				case MDAT_ID:
					*readoffset = u64bit + j;
					*rem_readsize =
					pfs->tot_file_size - *readoffset;
					*dontfree = 0;
					return 0;
			}
		} else {
			return -1;
		}
		j += u64bit;
		data += j;
	}
	return -1;
}

#if 0
static int parse_for_moov(nkn_vfs_cache_t *pfs,
                          off_t readsize ,
			  off_t *readoffset,
			  off_t *rem_readsize,
			  int *dontfree)
{
	uint64_t u64bit = 0;
	uint32_t u32bit = 0;
	uint64_t type;
	int retval;
	off_t diff;
	off_t j =0, k =0;
	char *page = pfs->data_pages[0];
	while ( j < readsize) {
		diff = k & (BM_PAGE_SIZE - 1);
		if (diff <= BM_PAGE_SIZE - 4) {
			u32bit = *(uint32_t*)page;
			u64bit = swap_uint32(u32bit);
			page += 4;
			k += 4;
		} else {
			retval = get4Val(pfs, diff, readsize,
					 &page, &k, &u64bit);
			if (retval == -1) {
				return -1;
			}
		}
		if (u64bit == 1) {
			diff = k & (BM_PAGE_SIZE - 1);
			if (diff <= BM_PAGE_SIZE - 8) {
				u64bit = *(uint64_t*)page;
				u64bit = swap_uint64(u64bit);
				page += 8;
				k += 8;
			} else {
				retval = get8Val(pfs, diff, readsize,
					 &page, &k, &u64bit);
				if (retval == -1) {
					return -1;
				}
			}
		}
		diff = k & (BM_PAGE_SIZE - 1);
		if (diff <= BM_PAGE_SIZE - 4) {
			type = *(uint32_t*)page;
			page += 4;
			k += 4;
		} else {
			retval = get4Val(pfs, diff, readsize,
					 &page, &k, &type);
			if (retval == -1) {
				return -1;
			}
			type = swap_uint32((uint32_t)type);
		}
		switch (type) {
			case MOOV_ID:        // 'm','o','o','v'
				if ((j + (off_t)u64bit) <= readsize) {
					*readoffset = 0;
					*rem_readsize = 0;
				} else {
					*readoffset = readsize + 1;
					*rem_readsize =
						u64bit + j - readsize;
				}
				*dontfree = 1;
				return 0;
			case MDAT_ID:
				*readoffset = u64bit + j;
				*rem_readsize =
					pfs->tot_file_size - *readoffset;
				*dontfree = 0;
				return 0;
		}
		j += k = u64bit;
		if (j >= readsize) {
			return -1;
		}
		page = pfs->data_pages[j / BM_PAGE_SIZE];
		page += j & (BM_PAGE_SIZE - 1);
	}
	return -1;
}
#endif

static int nkn_read_mp4_metadata(nkn_vfs_cache_t *pfs)
{
	off_t readsize, retval, readoffset, rem_readsize, rsize;
	char **temp_datapages, *final_pages, *temp_filename;
	int dontfree, total_pages, read_pages;
	nkn_vfs_con_t *con_info = pfs->con_info;
	pfs->content_size = 0;
	nkn_vfs_mdt_obj_t *mobj;


	pfs->data_pages = (char **)nkn_calloc_type(1,
				sizeof(char *), mod_vfs_nkn_cache_t);
	if (pfs->data_pages)
		pfs->data_pages[0] = (char *)nkn_calloc_type(ONE_RTSP_CHUNK,
				sizeof(char), mod_vfs_nkn_cache_t);
	if (!pfs->data_pages) {
		return -1;
	}
	if (!pfs->data_pages[0]) {
		free(pfs->data_pages);
		return -1;
	}
	con_info->taskindex = 0;
	readsize = vfs_read_fs_cache(pfs, 0, ONE_RTSP_CHUNK, direct_copy);
	if (readsize) {
		if ((readsize == NKN_BUF_VERSION_EXPIRED) ||
			(pfs->content_size == 0)) {
			free(pfs->data_pages[0]);
			free(pfs->data_pages);
			pfs->data_pages = NULL;
			return -1;
		}
	} else {
		free(pfs->data_pages[0]);
		free(pfs->data_pages);
		pfs->data_pages = NULL;
		return -1;
	}
	retval = parse_for_moov_linear(pfs,
			readsize , &readoffset, &rem_readsize, &dontfree);
	if (!retval) {
		if (dontfree) {
			total_pages =
				(readsize + rem_readsize) / BM_PAGE_SIZE + 1;
			read_pages = readsize / BM_PAGE_SIZE;
			temp_datapages = (char **)nkn_calloc_type(1,
					sizeof(char *), mod_vfs_nkn_cache_t);
			if (temp_datapages)
			final_pages = temp_datapages[0] = (char *)nkn_calloc_type(1,
						total_pages * BM_PAGE_SIZE,
						mod_vfs_nkn_cache_t);
			if (!temp_datapages || !temp_datapages[0]) {
				if (temp_datapages)
					free(temp_datapages);
				free(pfs->data_pages[0]);
				free(pfs->data_pages);
				pfs->data_pages = NULL;
				return -1;
			}
			memcpy(temp_datapages[0], pfs->data_pages[0],
				readsize);
			free(pfs->data_pages[0]);
			free(pfs->data_pages);
			pfs->data_pages = temp_datapages;
			if (rem_readsize) {
				rem_readsize += pfs->content_size;
			}
			con_info->taskindex = 0;
		} else {
			free(pfs->data_pages[0]);
			free(pfs->data_pages);
			pfs->data_pages = NULL;
			rem_readsize += (readoffset & (BM_PAGE_SIZE -1));
			total_pages = rem_readsize /BM_PAGE_SIZE + 1;
			temp_datapages =
					(char **)nkn_calloc_type(1,
					sizeof(char *), mod_vfs_nkn_cache_t);
			if (temp_datapages)
			final_pages = temp_datapages[0] =
						(char *)nkn_calloc_type(1,
						total_pages * BM_PAGE_SIZE,
						mod_vfs_nkn_cache_t);
			if (!temp_datapages || !temp_datapages[0]) {
				if (temp_datapages)
					free(temp_datapages);
				return -1;
			}
			pfs->data_pages = temp_datapages;
			pfs->content_size = 0;
			pfs->content_pos = readoffset -
					   (readoffset & (BM_PAGE_SIZE -1));
			con_info->taskindex = 0;
		}
		if (rem_readsize != 0) {
get_more_data:
			rsize = rem_readsize - pfs->content_size;
			if ((pfs->content_pos + rsize) > pfs->tot_file_size) {
				rsize = pfs->tot_file_size - pfs->content_size;
			}
			readsize = pfs->content_size;
			retval = vfs_read_fs_cache(pfs, pfs->content_pos +
					pfs->content_size, rsize, direct_copy);
			if (retval) {
				if ((retval == NKN_BUF_VERSION_EXPIRED) ||
					(pfs->content_size == readsize)) {
					// need to free the pages
					free(temp_datapages[0]);
					free(temp_datapages);
					return -1;
				}
			}
			if ((pfs->content_size != 0) &&
				(pfs->content_size < rem_readsize)) {
				goto get_more_data;
			}
		}
		//  needed data is ready
		pfs->data_pages = temp_datapages;
		pfs->data_pages[0] = final_pages;
		mobj = (nkn_vfs_mdt_obj_t*)nkn_calloc_type(1,
			  sizeof(nkn_vfs_mdt_obj_t), mod_vfs_nkn_cache_t);
		if (!mobj) {
			return -1;
		}
		mobj->data_pages = pfs->data_pages;
		mobj->ref_cnt = 1;
		mobj->content_pos = pfs->content_pos;
		mobj->content_size = pfs->content_size;
		mobj->filename = pfs->filename;
		pthread_mutex_lock(&mdt_lock);
		if (!g_hash_table_lookup(md_table, pfs->filename)) {
			g_hash_table_insert(md_table,
					    pfs->filename, mobj);
			pthread_mutex_unlock(&mdt_lock);
			free(con_info->nkn_taskid);
			con_info->nkn_taskid = NULL;
			free_con_info(pfs);
			pfs->tot_file_size =
				mobj->content_size +
				mobj->content_pos;
		} else {
			pthread_mutex_unlock(&mdt_lock);
			free(con_info->nkn_taskid);
			con_info->nkn_taskid = NULL;
			free_con_info(pfs);
			free(mobj);
			free(pfs->data_pages[0]);
			free(pfs->data_pages);
			pfs->data_pages = NULL;
			temp_filename = pfs->filename;
			pfs->filename = NULL;
			pthread_mutex_lock(&mdt_lock);
			mobj =	(nkn_vfs_mdt_obj_t *)
				g_hash_table_lookup(md_table,
				temp_filename);
			if (mobj) {
				mobj->ref_cnt++;
				pfs->data_pages = mobj->data_pages;
				pfs->content_pos = mobj->content_pos;
				pfs->content_size = mobj->content_size;
				pfs->tot_file_size =
					mobj->content_pos +
					mobj->content_size;
				pfs->filename = mobj->filename;
				retval = 0;
			} else
				retval = -1;
			pthread_mutex_unlock(&mdt_lock);
			if (temp_filename)
				free(temp_filename);
		}
	}
	return retval;
}

static off_t nkn_metadata_read(nkn_vfs_cache_t *pfs, off_t size,
			       off_t rmemb, char **data_ptr, off_t *pos)
{
	off_t len, offset;
	off_t totreqlen = size * rmemb;
	len = (pfs->content_pos + pfs->content_size - *pos);
	// safety check
	if (totreqlen <= len &&
		*pos >= pfs->content_pos &&
		*pos < (pfs->content_pos + pfs->content_size)) {
		pfs->cur_pos = *pos;
		offset = pfs->cur_pos - pfs->content_pos;
		memcpy(*data_ptr, *pfs->data_pages + offset, totreqlen);
		*pos += totreqlen;
		glob_vfs_out_bytes += totreqlen;
		return rmemb;
	}
	return 0;
}

static off_t nkn_metadata_read_opt(nkn_vfs_cache_t *pfs, off_t size,
			    off_t rmemb, off_t *pos,
			    char **temp_data_ptr, int *tofree)
{
	off_t len, offset;
	off_t totreqlen = size * rmemb;
	len = (pfs->content_pos + pfs->content_size - *pos);
	// safety check
	if (totreqlen <= len &&
		*pos >= pfs->content_pos &&
		*pos < (pfs->content_pos + pfs->content_size)) {
		pfs->cur_pos = *pos;
		offset = pfs->cur_pos - pfs->content_pos;
		*temp_data_ptr = *pfs->data_pages + offset;
		*tofree = 0;
		*pos += totreqlen;
		glob_vfs_out_bytes += totreqlen;
		return rmemb;
	}
	return 0;
}


static void free_buffer_pointers(nkn_task_id_t *nkn_taskid, int taskindex)
{
	int i = taskindex -1;
	for (; i >= 0 ; --i) {
		vfs_cleanup_a_task(nkn_taskid[i]);
	}
}

static off_t nkn_cache_read(nkn_vfs_cache_t *pfs, off_t size,
			    off_t rmemb, char **data_ptr)
{
	off_t retval, rsize, len, readsize;
	off_t totreqlen = size * rmemb;
	nkn_vfs_con_t *con_info = pfs->con_info;
	char **temp_bufpages;
	int try;
	if (pfs->eof == 1) {
		return 0;
	}
	if (totreqlen + pfs->cur_pos > pfs->tot_file_size) {
		totreqlen = pfs->tot_file_size - pfs->cur_pos;
		rmemb = totreqlen / size;
	}
	if ( (pfs->content_size==0) ||
	     (pfs->cur_pos < pfs->content_pos) ||
	     (pfs->cur_pos >= pfs->content_pos + pfs->content_size) ||
	     (totreqlen > (pfs->content_pos +
			   pfs->content_size - pfs->cur_pos)) ) {
		pfs->content_pos = pfs->cur_pos -
				   (pfs->cur_pos & (BM_PAGE_SIZE -1));
		pfs->content_size = 0;
		free_buffer_pointers(con_info->nkn_taskid,
				     con_info->taskindex);
		con_info->taskindex = 0;
		temp_bufpages = pfs->data_pages;
		try = 0;
get_more_data:
		rsize = ONE_RTSP_CHUNK - pfs->content_size;
		if ((pfs->content_pos + pfs->content_size +  rsize)
			                  > pfs->tot_file_size) {
			rsize = pfs->tot_file_size - (pfs->content_pos
				                     + pfs->content_size);
		}
		readsize = pfs->content_size;
		retval = vfs_read_fs_cache(pfs, pfs->content_pos +
				pfs->content_size, rsize, copy_to_mem);
		if (retval) {
			if (retval == NKN_BUF_VERSION_EXPIRED) {
				char *ext = strrchr(pfs->filename, '.');
				if (ext && strcasecmp(ext, ".nknc")) {
					nkn_cod_close(con_info->cod,
					      NKN_COD_VFS_MGR);
					con_info->cod =
						nkn_cod_open(con_info->uri,
						strlen(con_info->uri),
						NKN_COD_VFS_MGR);
					if (con_info->cod == NKN_COD_NULL) {
						return 0;
					}
					if (pfs->meta_data) {
						nkn_fclose((FILE *)pfs->meta_data);
						pfs->meta_data = NULL;
					}
					goto get_more_data;
				} else {
					return 0;
				}
			}
			if (pfs->content_size == readsize) {
				pfs->eof = 1;
				if (pfs->content_size == 0)
					return 0;
			}
		}
		if ((pfs->content_size != 0) &&
			(pfs->content_size <= ONE_RTSP_CHUNK / 2) &&
			((pfs->content_pos + pfs->content_size)
				< pfs->tot_file_size) &&
			try < (MAX_TASKID - 1)) {
			++try;
			pfs->data_pages +=
				((pfs->content_size - readsize ) /
				BM_PAGE_SIZE);
			goto get_more_data;
		}
		pfs->data_pages = temp_bufpages;
	}
	len = (pfs->content_pos + pfs->content_size - pfs->cur_pos);
	// safety check
	if (totreqlen <= len) {
		copy_iovec_to_user(pfs, data_ptr, totreqlen);
		pfs->cur_pos += totreqlen;
		pfs->eof = pfs->cur_pos >= pfs->tot_file_size;
		glob_vfs_out_bytes += totreqlen;
		return rmemb;
	}
	return 0;
}


static off_t nkn_cache_read_opt(nkn_vfs_cache_t *pfs, off_t size,
			    off_t rmemb, char **data_ptr,
			    char **temp_data_ptr, int *tofree)
{
	off_t retval, rsize, len, readsize;
	off_t totreqlen = size * rmemb;
	nkn_vfs_con_t *con_info = pfs->con_info;
	char **temp_bufpages;
	int try;
	if (pfs->eof == 1) {
		return 0;
	}
	if (totreqlen + pfs->cur_pos > pfs->tot_file_size) {
		totreqlen = pfs->tot_file_size - pfs->cur_pos;
		rmemb = totreqlen / size;
	}
	if ( (pfs->content_size==0) ||
	     (pfs->cur_pos < pfs->content_pos) ||
	     (pfs->cur_pos >= pfs->content_pos + pfs->content_size) ||
	     (totreqlen > (pfs->content_pos +
			   pfs->content_size - pfs->cur_pos)) ) {
		pfs->content_pos = pfs->cur_pos -
				   (pfs->cur_pos & (BM_PAGE_SIZE -1));
		pfs->content_size = 0;
		free_buffer_pointers(con_info->nkn_taskid,
				     con_info->taskindex);
		con_info->taskindex = 0;
		temp_bufpages = pfs->data_pages;
		try = 0;
get_more_data:
		rsize = ONE_RTSP_CHUNK - pfs->content_size;
		if ((pfs->content_pos + pfs->content_size +  rsize)
			                  > pfs->tot_file_size) {
			rsize = pfs->tot_file_size - (pfs->content_pos
				                     + pfs->content_size);
		}
		readsize = pfs->content_size;
		retval = vfs_read_fs_cache(pfs, pfs->content_pos +
				pfs->content_size, rsize, copy_to_mem);
		if (retval) {
			if (retval == NKN_BUF_VERSION_EXPIRED) {
				char *ext = strrchr(pfs->filename, '.');
				if (ext && strcasecmp(ext, ".nknc")) {
					nkn_cod_close(con_info->cod,
					      NKN_COD_VFS_MGR);
					con_info->cod =
						nkn_cod_open(con_info->uri,
						strlen(con_info->uri),
						NKN_COD_VFS_MGR);
					if (con_info->cod == NKN_COD_NULL) {
						return 0;
					}
					if (pfs->meta_data) {
						nkn_fclose((FILE *)pfs->meta_data);
						pfs->meta_data = NULL;
					}
					goto get_more_data;
				} else {
					return 0;
				}
			}
			if (pfs->content_size == readsize) {
				pfs->eof = 1;
				if (pfs->content_size == 0)
					return 0;
			}
		}
		if ((pfs->content_size != 0) &&
			(pfs->content_size <= ONE_RTSP_CHUNK / 2) &&
			((pfs->content_pos + pfs->content_size)
				< pfs->tot_file_size) &&
			try < (MAX_TASKID - 1)) {
			++try;
			pfs->data_pages +=
				((pfs->content_size - readsize ) /
				BM_PAGE_SIZE);
			goto get_more_data;
		}
		pfs->data_pages = temp_bufpages;
	}
	len = (pfs->content_pos + pfs->content_size - pfs->cur_pos);
	// safety check
	if (totreqlen <= len) {
		copy_iovec_to_user_opt(pfs, data_ptr, totreqlen,
				       temp_data_ptr, tofree);
		pfs->cur_pos += totreqlen;
		pfs->eof = pfs->cur_pos >= pfs->tot_file_size;
		glob_vfs_out_bytes += totreqlen;
		return rmemb;
	}
	return 0;
}


static size_t nkn_fread(void *ptr, size_t sz, size_t nmemb, FILE *stream)
{
	nkn_vfs_cache_t *pfs = (nkn_vfs_cache_t *)stream;
	off_t ret, rmemb = (off_t)nmemb, size = (off_t) sz;
	char *data_ptr = (char *)ptr;
	if (!pfs) {
		errno = EBADF;
		return 0;
	}

	pthread_mutex_lock(&pfs->mutex);
	if (pfs->meta_data) {
		ret = nkn_metadata_read(pfs->meta_data,
					size, rmemb,
					&data_ptr, &pfs->cur_pos);
		if (ret != 0) {
			pthread_mutex_unlock(&pfs->mutex);
			return ret;
		}
	}
	if (RTSP_CHECK_FLAG(pfs, RTSP_FLAG_DIRECT)) {
		ret = nkn_direct_read(pfs, size, rmemb, &data_ptr);
	} else {
		ret = nkn_cache_read(pfs, size, rmemb, &data_ptr);
	}
	pthread_mutex_unlock(&pfs->mutex);
	return (size_t)ret;
}

static size_t nkn_fread_opt(void *ptr, size_t sz, size_t nmemb, FILE *stream,
			    void **temp_ptr, int *tofree)
{
	nkn_vfs_cache_t *pfs = (nkn_vfs_cache_t *)stream;
	off_t ret, rmemb = (off_t)nmemb, size = (off_t) sz;
	char *data_ptr = (char *)ptr;
	char **temp_data_ptr = (char **)temp_ptr;
	if (!pfs) {
		errno = EBADF;
		return 0;
	}
	pthread_mutex_lock(&pfs->mutex);
	if (pfs->meta_data) {
		ret = nkn_metadata_read_opt(pfs->meta_data,
					size, rmemb,
					&pfs->cur_pos,
					temp_data_ptr, tofree);
		if (ret != 0) {
			pthread_mutex_unlock(&pfs->mutex);
			return ret;
		}
	}
	ret = nkn_cache_read_opt(pfs, size, rmemb, &data_ptr,
				 temp_data_ptr, tofree);
	pthread_mutex_unlock(&pfs->mutex);
	return (size_t)ret;
}


static int nkn_fclose(FILE * stream)
{
	nkn_vfs_cache_t *pfs = (nkn_vfs_cache_t *) stream;
	nkn_vfs_mdt_obj_t *mobj;
	if (!pfs) {
		errno = EBADF;
		return EOF;
	}
	if (pfs->meta_data) {
		nkn_fclose((FILE *)pfs->meta_data);
		DBG_LOG(MSG, MOD_RTSP, "stream=%p ret=0 ....", stream);
	}

	glob_vfs_close_cache ++;
	if (RTSP_CHECK_FLAG(pfs, RTSP_FLAG_META_DATA)) {
		pthread_mutex_lock(&mdt_lock);
		mobj = (nkn_vfs_mdt_obj_t *)
			g_hash_table_lookup(md_table, pfs->filename);
		if (mobj) {
			mobj->ref_cnt--;
			if (mobj->ref_cnt == 0) {
				g_hash_table_remove(md_table, pfs->filename);
				pthread_mutex_unlock(&mdt_lock);
				free(mobj->data_pages[0]);
				free(mobj->data_pages);
				free(pfs->filename);
				pfs->filename = NULL;
				free(mobj);
				goto skip_unlock;
			}
			pfs->filename = NULL;
		}
		pthread_mutex_unlock(&mdt_lock);
	} else if (RTSP_CHECK_FLAG(pfs, RTSP_FLAG_CACHE)) {
		free(pfs->data_pages);
	} else if (RTSP_CHECK_FLAG(pfs, RTSP_FLAG_DIRECT)) {
		if (pfs->con_info->nkn_taskid)
			free(pfs->con_info->nkn_taskid);
		pfs->con_info->nkn_taskid = NULL;
	}
skip_unlock:
	free_con_info (pfs);
	free(pfs->filename);
	free(pfs);
	return 0;
}

static void nkn_clearerr(FILE *stream)
{
	nkn_vfs_cache_t * pfs = (nkn_vfs_cache_t *)stream;
	nkn_vfs_con_t *con_info;
	if (!pfs) {
		errno = EBADF;
		return;
	}
	con_info = pfs->con_info;
	if (!con_info) {
		errno = EBADF;
		return;
	}
	DBG_LOG(MSG, MOD_RTSP, "stream=%p ....", stream);
	con_info->error = 0;
	pfs->eof = 0;
}

static int nkn_feof(FILE *stream)
{
	nkn_vfs_cache_t *pfs = (nkn_vfs_cache_t *)stream;
	if (!pfs) {
		errno = EBADF;
		return -1;
	}
	DBG_LOG(MSG, MOD_RTSP,
		"stream=%p ret=%d....", stream, pfs->eof);
	return pfs->eof;
}

static int nkn_ferror(FILE *stream)
{
	nkn_vfs_cache_t *pfs = (nkn_vfs_cache_t *)stream;
	nkn_vfs_con_t *con_info;
	if (!pfs) {
		errno = EBADF;
		return -1;
	}
	con_info = pfs->con_info;
	if (!con_info) {
		errno = EBADF;
		return -1;
	}
	DBG_LOG(MSG, MOD_RTSP,
		"stream=%p ret=%d ....", stream, con_info->error);
	return (con_info->error);
}

static int nkn_fileno(FILE *stream)
{
	nkn_vfs_cache_t *pfs = (nkn_vfs_cache_t *)stream;
	if (!pfs) {
		errno = EBADF;
		return -1;
	}
	DBG_LOG(MSG, MOD_RTSP, "stream=%p", stream);
	return 0;
}

static int nkn_fseek(FILE *stream, long offset, int whence)
{
	nkn_vfs_cache_t *pfs = (nkn_vfs_cache_t *)stream;
	if (!pfs) {
		errno = EBADF;
		return -1;
	}
	pthread_mutex_lock(&pfs->mutex);
	switch(whence) {
	case SEEK_SET:
		pfs->cur_pos = offset;
		break;
	case SEEK_CUR:
		pfs->cur_pos += offset;
		break;
	case SEEK_END:
		pfs->cur_pos = pfs->tot_file_size - offset;
		break;
	default:
		if (pfs->con_info) {
			pfs->con_info->error = EBADF;
		}
		errno = EBADF;
		pthread_mutex_unlock(&pfs->mutex);
		return -1;
	}
	pfs->eof = 0;
	if (pfs->cur_pos < 0) pfs->cur_pos = 0;
	if (pfs->cur_pos >= pfs->tot_file_size) {
		pfs->eof = 1;
		pfs->cur_pos = pfs->tot_file_size;
	}
	pthread_mutex_unlock(&pfs->mutex);
	return 0;
}

static long nkn_ftell(FILE *stream)
{
	nkn_vfs_cache_t * pfs = (nkn_vfs_cache_t *)stream;
	if (!pfs) {
		errno = EBADF;
		return -1;
	}
	return pfs->cur_pos;
}

static void nkn_rewind(FILE *stream)
{
	nkn_vfs_cache_t * pfs = (nkn_vfs_cache_t *)stream;
	if (!pfs) {
		errno = EBADF;
		return;
	}
	DBG_LOG(MSG, MOD_RTSP, "stream=%p", stream);
	pthread_mutex_lock(&pfs->mutex);
	pfs->cur_pos = 0;
	pfs->eof = 0;
	pthread_mutex_unlock(&pfs->mutex);
	return;
}

static int nkn_fgetpos(FILE *stream, fpos_t *pos)
{
	nkn_vfs_cache_t * pfs = (nkn_vfs_cache_t *)stream;
	if (!pfs) {
		errno = EBADF;
		return -1;
	}
	UNUSED_ARGUMENT(pos);
	DBG_LOG(MSG, MOD_RTSP, "stream=%p", stream);
	return 0;
}

static int nkn_fsetpos(FILE *stream, fpos_t *pos)
{
	nkn_vfs_cache_t * pfs = (nkn_vfs_cache_t *)stream;
	if (!pfs) {
		errno = EBADF;
		return -1;
	}
	UNUSED_ARGUMENT(pos);
	DBG_LOG(MSG, MOD_RTSP, "stream=%p", stream);
	return 0;
}


static size_t nkn_fwrite(const void *ptr,
			 size_t size,
			 size_t nmemb,
			 FILE *stream)
{
	nkn_vfs_cache_t * pfs = (nkn_vfs_cache_t *)stream;

	if (!pfs) {
		errno = EBADF;
		return 0;
	}

	UNUSED_ARGUMENT(ptr);
	UNUSED_ARGUMENT(size);
	UNUSED_ARGUMENT(nmemb);
	DBG_LOG(MSG, MOD_RTSP, "%s stream=%p", __FUNCTION__, stream);
	return 0;
}

static off_t nkn_ftello(FILE *stream)
{
	nkn_vfs_cache_t * pfs = (nkn_vfs_cache_t *)stream;

	if (!pfs) {
		errno = EBADF;
		return -1;
	}
	return 0;
}

static int nkn_fseeko(FILE *stream, off_t offset, int whence)
{
	nkn_vfs_cache_t * pfs = (nkn_vfs_cache_t *)stream;
	if (!pfs) {
		errno = EBADF;
		return -1;
	}


	pthread_mutex_lock(&pfs->mutex);
	switch(whence) {
	case SEEK_CUR:
		pfs->cur_pos += offset;
		break;
	case SEEK_SET:
		pfs->cur_pos = offset;
		break;
	case SEEK_END:
		pfs->cur_pos = pfs->tot_file_size - offset;
		break;
	default:
		if (pfs->con_info) {
			pfs->con_info->error = EBADF;
		}
		errno = EBADF;
		pthread_mutex_unlock(&pfs->mutex);
		return -1;
	}
	pfs->eof = 0; // reset the eof flag
	if (pfs->cur_pos < 0) pfs->cur_pos = 0;
	if (pfs->cur_pos >= pfs->tot_file_size) {
		pfs->eof = 1;
		pfs->cur_pos = pfs->tot_file_size;
	}
	pthread_mutex_unlock(&pfs->mutex);
	return 0;
}

static int nkn_fputc(int c, FILE *stream)
{
	nkn_vfs_cache_t * pfs = (nkn_vfs_cache_t *)stream;

	if (!pfs) {
		errno = EBADF;
		return EOF;
	}

	UNUSED_ARGUMENT(c);
	DBG_LOG(MSG, MOD_RTSP, "stream=%p", stream);
	return 0;
}

static int nkn_fputs(const char *s, FILE *stream)
{
	nkn_vfs_cache_t * pfs = (nkn_vfs_cache_t *)stream;

	if (!pfs) {
		errno = EBADF;
		return EOF;
	}

	UNUSED_ARGUMENT(s);
	DBG_LOG(MSG, MOD_RTSP, "stream=%p", stream);
	return 0;
}

static int nkn_fgetc(FILE *stream)
{
	nkn_vfs_cache_t * pfs = (nkn_vfs_cache_t *)stream;

	if (!pfs) {
		errno = EBADF;
		return EOF;
	}

	DBG_LOG(MSG, MOD_RTSP, "stream=%p", stream);
	return 0;
}

static char *nkn_fgets(char *s, int size, FILE *stream)
{
	nkn_vfs_cache_t * pfs = (nkn_vfs_cache_t *)stream;

	if (!pfs) {
		errno = EBADF;
		return NULL;
	}

	UNUSED_ARGUMENT(s);
	UNUSED_ARGUMENT(size);
	DBG_LOG(MSG, MOD_RTSP, "stream=%p", stream);
	return 0;
}

static int nkn_stat(const char *path, struct stat * st)
{
	FILE * fp;
	nkn_vfs_cache_t * pfs;

	fp = nkn_fopen(path, "I");
	if (fp == NULL) {
		DBG_LOG(MSG, MOD_RTSP, "nkn_lstat: path=%s", path);
		errno = EBADF;
		return -1;
	}
	/* nkn_fopen fills in field pfs->tot_file_size */
	pfs = (nkn_vfs_cache_t *)fp;
	st->st_mode = S_IFREG | 0444;
        st->st_nlink = 1;
	st->st_size = pfs->tot_file_size;
	nkn_fclose(fp);

	return 0;
}

static int nkn_fstat(int filedes, struct stat *buf)
{
	UNUSED_ARGUMENT(buf);

	DBG_LOG(MSG, MOD_RTSP, "nkn_fstat: filedes=%d", filedes);
	errno = EBADF;
	return -1;
}

static int nkn_lstat(const char *path, struct stat *buf)
{
	UNUSED_ARGUMENT(buf);

	DBG_LOG(MSG, MOD_RTSP, "nkn_lstat: path=%s", path);
	errno = EBADF;
	return -1;
}

static int nkn_fflush(FILE *stream)
{
	nkn_vfs_cache_t * pfs = (nkn_vfs_cache_t *)stream;

	if (!pfs) {
		errno = EBADF;
		return EOF;
	}
	DBG_LOG(MSG, MOD_RTSP, "stream=%p ret=0 ....",stream);
	return 0;
}

static nkn_provider_type_t
vfs_get_last_provider(FILE *stream)
{
    nkn_vfs_cache_t *pfs = (nkn_vfs_cache_t*)stream;

    if (!pfs) {
	return Unknown_provider;
    }
    if (pfs->provider == 0) {
	return BufferCache_provider;
    } else {
	return OriginMgr_provider;
    }
}


#if 0
static size_t
vfs_get_file_size(FILE *stream)
{
    nkn_vfs_cache_t *pfs = (nkn_vfs_cache_t*)stream;

    if (pfs) {
	return pfs->tot_file_size;
    } else {
	return (size_t)(0);
    }
}

static size_t
vfs_set_file_size(FILE *stream, size_t size)
{
    nkn_vfs_cache_t *pfs = (nkn_vfs_cache_t*)stream;

    if (pfs) {
	pfs->tot_file_size = size;
	return size;
    } else {
	return 0;
    }
}
#endif

// NB: virt_cache pre-read support functions.

static void vc_post_preread(
    nkn_vfs_con_t *con_info,
    off_t offset_requested,
    off_t length_requested)
{
    cache_response_t *cr, *cr_tmp = con_info->cptr;
    nkn_task_id_t id;
    int retval = vfs_create_task_data(con_info);
    cr = con_info->cptr;
    con_info->cptr = cr_tmp;
    if (retval) {
        return;
    }

    cr->uol.cod = con_info->cod;
    cr->magic = CACHE_RESP_REQ_MAGIC;
    cr->uol.offset = offset_requested;
    cr->uol.length = length_requested;
    cr->attr = NULL;
    cr->in_rflags = 0;

    if (RTSP_CHECK_FLAG(con_info, RTSP_INTERNAL_TASK)) {
        cr->in_rflags |= CR_RFLAG_INTERNAL;
    }
    if (RTSP_CHECK_FLAG(con_info, RTMP_FUSE_TASK)) {
        cr->in_rflags |= CR_RFLAG_FUSE_HITS;
    }

    // Create a new task
    id = nkn_task_create_new_task(TASK_TYPE_CHUNK_MANAGER,
                                  TASK_TYPE_VC_SVR,
                                  TASK_ACTION_INPUT,
                                  0,
                                  cr,
                                  sizeof(cache_response_t),
                                  0);
    nkn_task_set_private(TASK_TYPE_VFS_SVR, id, (void *)con_info);
    nkn_task_set_state(id, TASK_STATE_RUNNABLE);
    return;
}

static void vc_svr_cleanup(nkn_task_id_t id)
{
    UNUSED_ARGUMENT(id);
    return ;
}

static void vc_svr_input(nkn_task_id_t id)
{
    UNUSED_ARGUMENT(id);
    return ;
}

static void vc_svr_output(nkn_task_id_t id)
{
    cache_response_t *cr;
    nkn_vfs_con_t *con_info;

    cr = (cache_response_t *) nkn_task_get_data(id);
    if (!cr) {
        DBG_LOG(MSG, MOD_RTSP,
            "ERROR: nkn_task_get_data returns NULL\n");
        return;
    }
    assert(cr->magic == CACHE_RESP_RESP_MAGIC);

    con_info = (nkn_vfs_con_t *)
        nkn_task_get_private(TASK_TYPE_VFS_SVR, id);

    free(cr);
    nkn_task_set_action_and_state(id, TASK_ACTION_CLEANUP, TASK_STATE_RUNNABLE);
}


// NB: virt_cache server specific methods
static int nkn_vcstat(const char *path, vccmp_stat_data_t* stat_data)
{
    FILE * fp;
    nkn_vfs_cache_t * pfs;

    // NB: nkn_fopen() for mode 'S' allocates a vccmp_stat_data_t for
    //     the meta_data pointer. 
    fp = nkn_fopen(path, "S");
    if (!fp) {
	DBG_LOG(MSG, MOD_RTSP, "nkn_vcstat: path=%s", path);
	errno = EBADF;
	return -1;
    }

    pfs = (nkn_vfs_cache_t *)fp;
    if (!pfs->meta_data) {
	nkn_fclose(fp);
	errno = ENOMEM;
	return -1;
    }

    memcpy(stat_data, pfs->meta_data, sizeof(vccmp_stat_data_t));
    // NB: Need to free allocated vccmo_stat_data_t in meta_data now,
    //     nkn_close() will try to close it as a meta_data open!
    free(pfs->meta_data);
    pfs->meta_data = NULL;
    nkn_fclose(fp);

    return 0;
}

static FILE * nkn_vcopen(const char *path, vccmp_stat_data_t *stat_data)
{
    FILE * fp;
    nkn_vfs_cache_t * pfs;

    // NB: nkn_fopen() for mode 'V' allocates a vccmp_stat_data_t for
    //     the meta_data pointer. 
    fp = nkn_fopen(path, "V");
    if (!fp) {
	DBG_LOG(MSG, MOD_RTSP, "nkn_vcopen: path=%s", path);
	errno = EBADF;
	return NULL;
    }

    pfs = (nkn_vfs_cache_t *)fp;
    if (!pfs->data_pages || !pfs->meta_data) {
	// NB: Need to free allocated vccmo_stat_data_t in meta_data now,
	//     nkn_close() will try to close it as a meta_data open!
	if (pfs->meta_data) {
	    free(pfs->meta_data);
	    pfs->meta_data = NULL;
	}
	nkn_fclose(fp);
	errno = ENOMEM;
	return NULL;
    }
    memcpy(stat_data, pfs->meta_data, sizeof(vccmp_stat_data_t));
    // NB: Need to free allocated vccmo_stat_data_t in meta_data now,
    //     nkn_close() will try to close it as a meta_data open!
    free(pfs->meta_data);
    pfs->meta_data = NULL;

    return fp;
}

static size_t nkn_vcread(FILE *stream, void *ptr, size_t size, off_t offset, 
			 uint8_t direct, uint32_t streaming_read_size)
{
    nkn_vfs_cache_t *pfs = (nkn_vfs_cache_t *)stream;
    char *data_ptr = (char *)ptr;
    off_t ret;
    char **temp_bufpages;
    nkn_task_id_t temp_task_ids[MAX_TASKID];
    int temp_taskindex;

    if (!pfs) {
	errno = EBADF;
	return 0;
    }
    if (offset >= pfs->tot_file_size) {
	errno = EINVAL;
	return 0;
    }
    if (offset + (off_t)size > pfs->tot_file_size) {
	size = pfs->tot_file_size - offset;
    }

    pthread_mutex_lock(&pfs->mutex);
    pfs->cur_pos = offset;
    pfs->eof = 0;
    if (!direct) {
	ret = nkn_cache_read(pfs, (off_t)size, 1, &data_ptr);
	if (ret && streaming_read_size) {
	    off_t content_end = (pfs->content_pos + pfs->content_size);
	    
	    if (offset + (off_t)size >= (content_end - streaming_read_size)) {
		vc_post_preread(pfs->con_info, content_end, ONE_RTSP_CHUNK);
	    }
	}
    } else {
	temp_bufpages = pfs->data_pages;
	memcpy(temp_task_ids, pfs->con_info->nkn_taskid, sizeof(temp_task_ids));
	temp_taskindex = pfs->con_info->taskindex;
	pfs->con_info->taskindex = 0;
	ret = nkn_direct_read(pfs, (off_t)size, 1, &data_ptr);
	pfs->con_info->taskindex = temp_taskindex;
	memcpy(temp_task_ids, pfs->con_info->nkn_taskid, sizeof(temp_task_ids));
	pfs->data_pages  = temp_bufpages;
    }
    pthread_mutex_unlock(&pfs->mutex);
    return (size_t)ret;
}


/* *****************************************************
 * Initialization Functions
 ******************************************************* */
struct vfs_funcs_t vfs_funs =
{
	nkn_fopen,
	nkn_fread,
	nkn_fwrite,
	nkn_ftell,
	nkn_ftello,
	nkn_clearerr,
	nkn_ferror,
	nkn_feof,
	nkn_fileno,
	nkn_fseek,
	nkn_rewind,
	nkn_fflush,
	nkn_fgetpos,
	nkn_fsetpos,
	nkn_fseeko,
	nkn_fputc,
	nkn_fgetc,
	nkn_fputs,
	nkn_fgets,
	nkn_stat,
	nkn_fstat,
	nkn_lstat,
	nkn_fclose,
	vfs_get_last_provider,

	// NB: virt_cache server specific methods (return vccmp_stat_data).
	nkn_vcstat,
	nkn_vcopen,
        nkn_vcread,
#if 0
	vfs_get_file_size,
	vfs_set_file_size,
#endif
	nkn_rtscheduleDelayedTask,
	nkn_rtunscheduleDelayedTask,
	nkn_updateRTSPCounters,
	nkn_fread_opt,
	NULL,
	&vfs_dm2_enable
};

void nkn_init_vfs(void);
void nkn_init_vfs(void)
{
        /*
	 * Register this scheduler module.
	 */
	nkn_task_register_task_type(
				TASK_TYPE_VFS_SVR,
				&vfs_mgr_input,
				&vfs_mgr_output,
				&vfs_mgr_cleanup);
	nkn_task_register_task_type(
				TASK_TYPE_VC_SVR,
				&vc_svr_input,
				&vc_svr_output,
				&vc_svr_cleanup);
	vfs_md_init();
}
