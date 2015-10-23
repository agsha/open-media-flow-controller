#include <sys/shm.h>
#include "agentd_mgmt.h"
#include "agentd_op_cmds_base.h"
#include "agentd_utils.h"
#include "agentd_cli.h"
#include "nkn_stat.h"
#include "nkncnt_client.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;
nkncnt_client_ctx_t g_nvsd_mem_ctx;
int agentd_shm_init_done ;

/* ---- Function prototype for initializing / deinitializing module ---- */

int
agentd_get_stats_init(
        const lc_dso_info *info,
        void *context);
int
agentd_get_stats_deinit(
        const lc_dso_info *info,
        void *context);

struct conx_counters {
    /* requests counters */
    uint64_t active_conx;
    uint64_t total_conx;
    uint64_t cache_hits;
};

struct served_bytes {
    uint64_t total;
    uint64_t cache;
    uint64_t origin;
    uint64_t tunnel;
    uint64_t disk;
    /* only valid for global */
    uint64_t nfs;
};
/* ---- Operational command handler prototype and structure definition ---- */
struct ns_counters {
    struct ns_counters *next;

    char ns_name[32];
    struct served_bytes bytes;
    /* requests counters */
    struct conx_counters ns_reqs;
    uint64_t pinned_objects;
    uint64_t pinned_bytes;
};

struct cache_tier {
    /* tells if tier is available or not */
    int tier_available;
    char tier_name[16];
    uint64_t read_bytes;
    uint64_t write_bytes;
    uint64_t evict_bytes;
};

typedef struct agentd_get_stats {
    lt_time_sec timestamp;
    /* nsvd start timestamp */
    uint64_t nvsd_start_time;

    /* system stats */
    /* agregate cpu load */
    int cpu1min;
    int cpu5min;
    int cpu15min;
    /* in KB */
    uint64_t total_memory;
    uint64_t used_memory;

    /* global cache counters */
    struct served_bytes bytes;

    /* requests counters */
    struct conx_counters global_reqs;

    /* ram-cache details */
    uint64_t ram_cache_size;
    uint64_t ram_cache_used;

    struct cache_tier ram_tier;
    struct cache_tier disk_tier[3];

    /* diskspace in MB */
    uint64_t total_disk_size;
    uint64_t total_disk_used;

    /* namespace coutners */
    int ns_num;
    struct ns_counters *ns_head;
} agentd_get_stats_t ;

int32_t
nvsd_counter_strip_char(const char *str,
			 uint32_t base_name_len,
			 char *out,char strip_char);
int32_t
nvsd_counter_strip_name(const char *str,
			 uint32_t base_name_len,
			 char *out);
int agentd_get_tier_details(agentd_get_stats_t *stats);
int agentd_get_used_ram_cache(agentd_get_stats_t *stats);
int agentd_get_system_values(bn_binding_array *bindings, agentd_get_stats_t *stats);
int nvsd_lib_get_names(nkncnt_client_ctx_t *ctx, uint32_t len,
	cp_vector *matches, tstr_array **names);
int nvsd_get_all_ns_names(nkncnt_client_ctx_t *ctx, tstr_array **ns_names);
int nvsd_get_all_disk_names(nkncnt_client_ctx_t *ctx, tstr_array **disk_names);
int agentd_get_namesapce_stats(agentd_get_stats_t *stats, tstr_array *ns_names);
int agentd_get_global_from_ns(agentd_get_stats_t *stats);
int agentd_get_disk_stats(agentd_get_stats_t *stats, tstr_array *ns_names);
int agentd_free_stats(agentd_context_t *context, agentd_get_stats_t *stats);

int agentd_send_all_namespaces(agentd_context_t *context, agentd_get_stats_t *stats);
int agentd_send_system_values(agentd_context_t *context, agentd_get_stats_t *stats);
int agentd_send_cache_tier(agentd_context_t *context, struct cache_tier *tier);
int agentd_send_bytes(agentd_context_t *context, struct served_bytes *bytes);
int agentd_send_request(agentd_context_t *context, struct conx_counters *reqs);
int agentd_get_stats(agentd_context_t *context, void *data);
int agentd_get_stats_once(agentd_context_t *context, agentd_get_stats_t *stats);
int agentd_send_stats_once(agentd_context_t *context, agentd_get_stats_t *stats);
int agentd_nvsd_shm_init(void);

static void *
trie_copy_func(void *nd)
{
    UNUSED_ARGUMENT(nd);
    return nd;
}

static void
trie_destruct_func(void *nd)
{
    UNUSED_ARGUMENT(nd);
    return;
}

int32_t
nvsd_counter_strip_char(const char *str,
			 uint32_t base_name_len,
			 char *out,char strip_char)
{
    char *p1  = (char*) (str);
    char *p2 = p1, *p3;
    int32_t size = 0, err = 0;

    p3 = (p2 += base_name_len);
    while ( *p2++ != strip_char ) {
	    };
    size = (p2 - p3);
    if (size <= 0) {
	out[0] = '\0';
	goto error;
    }
    memcpy(out, p3, size - 1);
    out[size] = '\0';

 error:
    return err;
}

int32_t
nvsd_counter_strip_name(const char *str,
			 uint32_t base_name_len,
			 char *out)
{
    return nvsd_counter_strip_char(str, base_name_len, out, '.');
}


int
nvsd_lib_get_names(nkncnt_client_ctx_t *ctx, uint32_t len,
	cp_vector *matches, tstr_array **names)
{
    int32_t err = 0, num_elements = 0, i = 0, num_rp = 0;
    glob_item_t *item = NULL, *item1 = NULL;
    char *varname = (char *)(ctx->shm +  sizeof(nkn_shmdef_t) +
			   sizeof(glob_item_t) * ctx->max_cnts);
    temp_buf_t tmp_name = {0} ;
    cp_trie *tmp_trie = NULL;
    tstr_array *tmp_names = NULL;

    err = tstr_array_new(&tmp_names, 0);
    bail_error(err);

    tmp_trie = cp_trie_create_trie(
		(COLLECTION_MODE_NOSYNC|COLLECTION_MODE_COPY|
		 COLLECTION_MODE_DEEP),
		trie_copy_func, trie_destruct_func);
    bail_null(tmp_trie);


    num_elements = cp_vector_size(matches);

    for (i = 0; i < num_elements; i++) {
	item = (glob_item_t *)cp_vector_element_at(matches, i);

	bzero(tmp_name, sizeof(tmp_name));
	nvsd_counter_strip_name(varname + item->name,
		len, tmp_name);
	
	item1 = (glob_item_t*)cp_trie_exact_match(tmp_trie,
					  tmp_name);
	if (!item1) {
	    /* new entry in cache */
	    err = cp_trie_add(tmp_trie,
			      tmp_name, item);
	    bail_error(err);
	    /* add into the names array */
	    err = tstr_array_append_str(tmp_names, tmp_name);
	    bail_error(err);
	}
    }

    *names = tmp_names;
    tmp_names = NULL;
bail:
    if (tmp_trie != NULL)
	cp_trie_destroy(tmp_trie);
    tstr_array_free(&tmp_names);
    return err;
}
int
nvsd_get_all_disk_names(nkncnt_client_ctx_t *ctx, tstr_array **disk_names)
{
    int err = 0;
    cp_vector *matches = NULL;

    /* check if there is a change or not */
    err = nkncnt_client_get_submatch(ctx, "dc_", &matches);
    if (err == E_NKNCNT_CLIENT_SEARCH_FAIL) {
	/* this is not a error */
	err = 0;
    }
    bail_error(err);

    if (matches == NULL) {
	/* no valid disks */
	goto bail;
    }
    /* get unique namespaces names */
    err = nvsd_lib_get_names(ctx, 0, matches, disk_names);
    bail_error(err);

    //tstr_array_dump(*disk_names, "DISKS");
bail:
    if (matches) cp_vector_destroy(matches);
    return err;
}

int
nvsd_get_all_ns_names(nkncnt_client_ctx_t *ctx, tstr_array **ns_names)
{
    int err = 0, len = 0, i = 0;
    cp_vector *matches = NULL;
    tstr_array *tmp_names = NULL, *tmp_array = NULL;
    tstring *ts = NULL;
    int32_t offset = 0;

    /* check if there is a change or not */
    err = nkncnt_client_get_submatch(ctx, "ns.", &matches);
    if (err == E_NKNCNT_CLIENT_SEARCH_FAIL) {
	/* this is not a error */
	err = 0;
    }
    bail_error(err);

    if (matches == NULL) {
	goto bail;
    }

    /* get unique namespaces names */
    err = nvsd_lib_get_names(ctx, strlen("ns."), matches, &tmp_names);
    bail_error(err);

    /* now we have all names with "<ns-name:UUID>" and "<ns-name>" */
    //tstr_array_dump(tmp_names, "NS-NAMES");

    err = tstr_array_new(&tmp_array, NULL);
    bail_error(err);

    len = tstr_array_length_quick(tmp_names);
    for (i = 0; i < len; i++) {
	ts = tstr_array_get_quick(tmp_names, i);
	if(ts == NULL) continue;

	err = ts_find_char(ts, 0,':', &offset);
	bail_error(err);

	/* ignore "<ns-name:UUID>" */
	if (offset == -1) {
	    /* ignore mfc_probe namespace */
	    if (!ts_equal_str(ts, "mfc_probe",false)) {
		err = tstr_array_append_str(tmp_array, ts_str_maybe(ts));
		bail_error(err);
	    }
	}
    }

    *ns_names = tmp_array;
    tmp_array = NULL;

    //tstr_array_dump(*ns_names, "NS-NAMES");

bail:
    tstr_array_free(&tmp_names);
    tstr_array_free(&tmp_array);
    if (matches) cp_vector_destroy(matches);
    return err;
}
int
agentd_nvsd_shm_init(void)
{
    int err = 0;
    key_t shmkey;
    int shmid;
    uint64_t shm_size = 0;
    char *shm = NULL;

    if (agentd_shm_init_done) {
	/* shared memory init done */
	goto bail;
    }
    shm_size = (sizeof(nkn_shmdef_t)+ MAX_CNTS_NVSD *(sizeof(glob_item_t)+30));
    shmid = shmget(NKN_SHMKEY, shm_size, 0666);
    if (shmid < 0) {
	err = 14000;
	bail_error(err);
    }

    shm = shmat(shmid, NULL, 0);
    if (shm == (char *)-1) {
	err = 1;
	bail_error(err);
    }

    err = nkncnt_client_init(shm, MAX_CNTS_NVSD, &g_nvsd_mem_ctx);
    bail_error(err);

    agentd_shm_init_done = 1;

bail:
    return err;
}

int
agentd_get_stats_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;

    oper_table_entry_t op_tbl[] = {
        {"/stats/mfc-cluster/*", DUMMY_WC_ENTRY, agentd_get_stats , NULL},
        OP_ENTRY_NULL
    };

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_op_cmds_array(op_tbl);
    bail_error (err);

    err = agentd_nvsd_shm_init();
    bail_error(err);

bail:
    return err;
}

int
agentd_get_stats_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_get_stats_deinit");

bail:
    return err;
}


int
agentd_get_system_values(bn_binding_array *bindings, agentd_get_stats_t *stats)
{
    int err = 0;
    uint32_t cpuload = 0, memory = 0;

    if ((bindings == NULL) || (stats == NULL)) {
	return 0;
    }
    err = agentd_binding_array_get_value_uint32_fmt(bindings, &cpuload,
	    "/system/load/1");
    stats->cpu1min = cpuload;

    cpuload = 0;
    err = agentd_binding_array_get_value_uint32_fmt(bindings, &cpuload,
	    "/system/load/5");
    stats->cpu5min = cpuload;

    cpuload = 0;
    err = agentd_binding_array_get_value_uint32_fmt(bindings, &cpuload,
	    "/system/load/15");
    stats->cpu15min = cpuload;

    err = agentd_binding_array_get_value_uint32_fmt(bindings, &memory,
	    "/system/memory/physical/total");
    stats->total_memory = memory;

    memory = 0;
    err = agentd_binding_array_get_value_uint32_fmt(bindings, &memory,
	    "/system/memory/physical/used");
    stats->used_memory = memory;

bail:
    return err;
}

int
agentd_get_stats_once(agentd_context_t *context, agentd_get_stats_t *stats)
{
    int err = 0, i = 0, num_nodes = 0;
    glob_item_t *item = NULL;
    bn_binding_array *bindings = NULL;
    uint32_t ret_code = 0;
    tstring *ret_msg = NULL;
    bn_request *req = NULL;
    tstr_array *ns_names = NULL, *disk_names = NULL;

    const char *stat_nodes[] = {
	"/system/load",
	"/system/memory/physical",
    };

    bail_null(stats);

    if (agentd_shm_init_done == 0) {
	agentd_nvsd_shm_init();
    }
    err = bn_request_msg_create(&req,bbmt_query_request);
    bail_error(err);

    /* Now add the node's characteristics to the message */
    num_nodes = sizeof(stat_nodes)/sizeof (const char *);
    for(i = 0;i < num_nodes; i++) {
	err = bn_query_request_msg_append_str(req,
		bqso_iterate, 0,
		stat_nodes[i], NULL);
	bail_error(err);
    }
    err = mdc_send_mgmt_msg(agentd_mcc, req, true, &bindings, &ret_code, &ret_msg);
    bail_error(err);

    if (ret_code) {
	/* query failed */
	lc_log_basic(LOG_WARNING, "Unable to fetch values from mgmtd (%d)", ret_code);
    }
    stats->timestamp = lt_curr_time();

    /* following array should be fetched from mgmtd */
    agentd_get_system_values(bindings, stats);

    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"glob_nvsd_uptime_since", &item);
    if (item) {
	stats->nvsd_start_time = (uint64_t)item->value;
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"glob_tot_size_from_cache", &item);
    if (item) {
	stats->bytes.cache = (uint64_t)item->value;
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"glob_tot_size_from_disk", &item);
    if (item) {
	stats->bytes.disk = (uint64_t)item->value;
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"glob_tot_size_from_origin", &item);
    if (item) {
	stats->bytes.origin = (uint64_t)item->value;
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"glob_tot_size_from_nfs", &item);
    if (item) {
	stats->bytes.nfs = (uint64_t)item->value;
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"glob_tot_size_from_tunnel", &item);
    if (item) {
	stats->bytes.tunnel = (uint64_t)item->value;
    }

    stats->bytes.total = stats->bytes.tunnel + stats->bytes.nfs
	+ stats->bytes.origin + stats->bytes.disk + stats->bytes.cache;

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"glob_tot_video_delivered", &item);
    if (item) {
	stats->global_reqs.total_conx = (uint64_t)item->value;
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"glob_http_cache_hit_cnt", &item);
    if (item) {
	stats->global_reqs.cache_hits = (uint64_t)item->value;
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"glob_cur_open_http_socket", &item);
    if (item) {
	stats->global_reqs.active_conx = (uint64_t)item->value;
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"glob_cur_open_http_socket_ipv6", &item);
    if (item) {
	stats->global_reqs.active_conx += (uint64_t)item->value;
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"glob_cur_open_rtsp_socket", &item);
    if (item) {
	stats->global_reqs.active_conx += (uint64_t)item->value;
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"glob_cp_tot_cur_open_sockets", &item);
    if (item) {
	stats->global_reqs.active_conx += (uint64_t)item->value;
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"glob_cp_tot_cur_open_sockets_ipv6", &item);
    if (item) {
	stats->global_reqs.active_conx += (uint64_t)item->value;
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"glob_ram_cachesize", &item);
    if (item) {
	stats->ram_cache_size += (uint64_t)item->value;
    }

    agentd_get_used_ram_cache(stats);

    agentd_get_tier_details(stats);

    err = nvsd_get_all_disk_names(&g_nvsd_mem_ctx, &disk_names);
    bail_error(err);

    /* Get disk Coutners and, add them up */
    err = agentd_get_disk_stats(stats, disk_names);
    bail_error(err);


    err = nvsd_get_all_ns_names(&g_nvsd_mem_ctx, &ns_names);
    bail_error(err);

    err = agentd_get_namesapce_stats(stats, ns_names);
    bail_error(err);

    err = agentd_get_global_from_ns(stats);
    bail_error(err);

bail:
    bn_binding_array_free(&bindings);
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    tstr_array_free(&ns_names);
    tstr_array_free(&disk_names);
    return err;
}

/*
 * following will update some global variable from namespace values
 */
int
agentd_get_global_from_ns(agentd_get_stats_t *stats)
{
    int err = 0, i = 0;
    struct ns_counters *ns = NULL;

    stats->global_reqs.active_conx = 0;
    for (i = 0; i < stats->ns_num; i++) {
	ns = stats->ns_head + i ;
	stats->global_reqs.active_conx+=ns->ns_reqs.active_conx;
    }

    return 0;
}

int
agentd_get_disk_stats(agentd_get_stats_t *stats, tstr_array *disk_names)
{
    int err = 0, i = 0, len = 0;
    const char *disk_name = NULL;
    glob_item_t *item = NULL;
    counter_name_t cname = {0};
    uint64_t block_size = 0, total_blocks = 0, free_blocks = 0,
	     total_free_size = 0;

    len = tstr_array_length_quick(disk_names);

    for (i = 0; i < len; i++) {
	disk_name = tstr_array_get_str_quick(disk_names, i);
	if (disk_name == NULL) continue;
	if (strlen(disk_name) == 0) continue;

	/* dm2_total_blocks, dm2_free_blocks, dm2_block_size */
	snprintf(cname, sizeof(counter_name_t),
		"%s.dm2_block_size", disk_name);
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx, cname, &item);
	if (err) {
	    lc_log_debug(LOG_DEBUG, "%s not found", cname);
	    continue;
	}
	if (item) {
	    block_size = (uint64_t)item->value;
	}

	/* dm2_total_blocks, dm2_free_blocks, dm2_block_size */
	snprintf(cname, sizeof(counter_name_t),
		"%s.dm2_free_blocks", disk_name);
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx, cname, &item);
	if (err) {
	    lc_log_debug(LOG_DEBUG, "%s not found", cname);
	    continue;
	}
	if (item) {
	    free_blocks = (uint64_t)item->value;
	}

	/* dm2_total_blocks, dm2_free_blocks, dm2_block_size */
	snprintf(cname, sizeof(counter_name_t),
		"%s.dm2_total_blocks", disk_name);
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx, cname, &item);
	if (err) {
	    lc_log_debug(LOG_DEBUG, "%s not found", cname);
	    continue;
	}
	if (item) {
	    total_blocks = (uint64_t)item->value;
	}
	stats->total_disk_size += ((total_blocks * block_size)/(1024*1024));
	total_free_size += ((block_size * free_blocks)/(1024*1024));
    }

    stats->total_disk_used = stats->total_disk_size - total_free_size;


bail:
    /* err is not returned as this could cause caller to fail */
    return 0;
}

int
agentd_get_namesapce_stats(agentd_get_stats_t *stats, tstr_array *ns_names)
{
    int err = 0, i = 0, len = 0;;
    counter_name_t cname = {0};
    struct ns_counters *ns_stats =  NULL;
    const char *ns_name = NULL;
    glob_item_t *item = NULL;
    tstring *t_current_uid = NULL;
    node_name_t node_name = {0};

    len = tstr_array_length_quick(ns_names);
    stats->ns_head = calloc(len, sizeof(struct ns_counters));
    bail_null(stats->ns_head);

    stats->ns_num = len;
    for (i = 0; i < len; i++) {
	ns_name = tstr_array_get_str_quick(ns_names, i);
	ns_stats = (stats->ns_head + i);
	if (ns_name == NULL) continue;
	if (strlen(ns_name) == 0) continue;

	snprintf(ns_stats->ns_name, sizeof(ns_stats->ns_name), "%s", ns_name);
	snprintf(node_name, sizeof(node_name), "/nkn/nvsd/namespace/%s/uid", ns_name);
	ts_free(&t_current_uid);
	err = mdc_get_binding_tstr(agentd_mcc, NULL, NULL, NULL, &t_current_uid, node_name, NULL);
	bail_error(err);

	ts_trim_head(t_current_uid, '/');
	snprintf(cname, sizeof(counter_name_t),
		"ns.%s.http.client.out_bytes", ns_name);
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx, cname, &item);
	bail_error(err);
	if (item) {
	    ns_stats->bytes.total= (uint64_t)item->value;
	}

	snprintf(cname, sizeof(counter_name_t),
		"ns.%s.http.client.out_bytes_origin", ns_name);
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx, cname, &item);
	bail_error(err);
	if (item) {
	    ns_stats->bytes.origin = (uint64_t)item->value;
	}

	snprintf(cname, sizeof(counter_name_t),
		"ns.%s.http.client.out_bytes_ram", ns_name);
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx, cname, &item);
	bail_error(err);
	if (item) {
	    ns_stats->bytes.cache= (uint64_t)item->value;
	}

	snprintf(cname, sizeof(counter_name_t),
		"ns.%s.http.client.out_bytes_disk", ns_name);
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx, cname, &item);
	bail_error(err);
	if (item) {
	    ns_stats->bytes.disk = (uint64_t)item->value;
	}

	snprintf(cname, sizeof(counter_name_t),
		"ns.%s.http.client.out_bytes_tunnel", ns_name);
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx, cname, &item);
	bail_error(err);
	if (item) {
	    ns_stats->bytes.tunnel = (uint64_t)item->value;
	}

	snprintf(cname, sizeof(counter_name_t),
		"ns.%s.http.client.requests", ns_name);
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx, cname, &item);
	bail_error(err);
	if (item) {
	    ns_stats->ns_reqs.total_conx = (uint64_t)item->value;
	}

	snprintf(cname, sizeof(counter_name_t),
		"ns.%s.http.client.active_conns", ns_name);
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx, cname, &item);
	bail_error(err);
	if (item) {
	    ns_stats->ns_reqs.active_conx = (uint64_t)item->value;
	}

	snprintf(cname, sizeof(counter_name_t),
		"ns.%s.http.client.cache_hits", ns_name);
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx, cname, &item);
	bail_error(err);
	if (item) {
	    ns_stats->ns_reqs.cache_hits = (uint64_t)item->value;
	}

	snprintf(cname, sizeof(counter_name_t),
		"ns.%s.cache.tier.lowest.cache_pin.obj_count", ts_str(t_current_uid));
	lc_log_basic(LOG_NOTICE, "getting %s", cname);
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx, cname, &item);
	bail_error(err);
	if (item) {
	    ns_stats->pinned_objects = (uint64_t)item->value;
	}
	snprintf(cname, sizeof(counter_name_t),
		"ns.%s.cache.tier.lowest.cache_pin.bytes_used", ts_str(t_current_uid));
	lc_log_basic(LOG_NOTICE, "getting %s", cname);
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx, cname, &item);
	bail_error(err);
	if (item) {
	    ns_stats->pinned_bytes = (uint64_t)item->value;
	}

    }

bail:
    /*
     * returning zero, so avoid RPC to fail becasue of this,
     * failure is already logged
     */
    ts_free(&t_current_uid);
    return 0;
}


int
agentd_get_used_ram_cache(agentd_get_stats_t *stats)
{
    int err = 0;
    uint64_t alloc_pages = 0;
    glob_item_t *item = NULL;

    stats->ram_cache_used = 0 ;

    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"pp32k.alloc", &item);
    /* ignoring the the error */
    if (item) {
	alloc_pages = (uint64_t)item->value;
	alloc_pages = alloc_pages * 32768;
    }

    stats->ram_cache_used += alloc_pages;

    alloc_pages = 0;
    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"pp4k.alloc", &item);
    if (item) {
	alloc_pages = (uint64_t)item->value;
	alloc_pages = alloc_pages * 4096;
    }

    stats->ram_cache_used += alloc_pages;

    alloc_pages = 0;
    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"pp3k.alloc", &item);
    if (item) {
	alloc_pages = (uint64_t)item->value;
	alloc_pages = alloc_pages * 3072;
    }
    stats->ram_cache_used += alloc_pages;

    alloc_pages = 0;
    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"pp2.5k.alloc", &item);
    if (item) {
	alloc_pages = (uint64_t)item->value;
	alloc_pages = alloc_pages * 2560;
    }
    stats->ram_cache_used += alloc_pages;

    alloc_pages = 0;
    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"pp2k.alloc", &item);
    if (item) {
	alloc_pages = (uint64_t)item->value;
	alloc_pages = alloc_pages * 2048;
    }
    stats->ram_cache_used += alloc_pages;

    alloc_pages = 0;
    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"pp1.5k.alloc", &item);
    if (item) {
	alloc_pages = (uint64_t)item->value;
	alloc_pages = alloc_pages * 1536;
    }
    stats->ram_cache_used += alloc_pages;

    alloc_pages = 0;
    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"pp1k.alloc", &item);
    if (item) {
	alloc_pages = (uint64_t)item->value;
	alloc_pages = alloc_pages * 1024;
    }
    stats->ram_cache_used += alloc_pages;

    alloc_pages = 0;
    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"pp512.alloc", &item);
    if (item) {
	alloc_pages = (uint64_t)item->value;
	alloc_pages = alloc_pages * 512;
    }
    stats->ram_cache_used += alloc_pages;

    stats->ram_cache_used = stats->ram_cache_used/(1024 * 1024);

    return err;
}


int
agentd_get_tier_details(agentd_get_stats_t *stats)
{
    int err = 0, i = 0, num_tiers = 0;
    glob_item_t *item = NULL;
    char counter_name[128] = {0};
    const char *tier_name = NULL;

    const char *tiers[] = {
	"SATA", "SSD", "SAS"
    };
    if (stats == NULL) {
	goto bail;
    }

    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"bm.global.bm.bufcache.hitbytes", &item);
    if (item) {
	stats->ram_tier.read_bytes = (uint64_t)item->value;
	stats->ram_tier.tier_available = 1;
	snprintf(stats->ram_tier.tier_name, sizeof(stats->ram_tier.tier_name),
		"ram");
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"bm.global.bm.bufcache.fillbytes", &item);
    if (item) {
	stats->ram_tier.write_bytes = (uint64_t)item->value;
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		"bm.global.bm.bufcache.evictbytes", &item);
    if (item) {
	stats->ram_tier.evict_bytes = (uint64_t)item->value;
    }

    num_tiers = sizeof(tiers)/sizeof(const char *) ;
    for (i = 0; i < num_tiers; i++) {
	tier_name = tiers[i];
	item = NULL;
	snprintf(counter_name, sizeof(counter_name),
		"%s.dm2_raw_read_bytes", tier_name);
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		counter_name, &item);
	if (item) {
	    stats->disk_tier[i].read_bytes = (uint64_t)item->value;
	    stats->disk_tier[i].tier_available = 1;
	    snprintf(stats->disk_tier[i].tier_name,
		    sizeof(stats->disk_tier[i].tier_name),tier_name);
	} else {
	    /* tier not available */
	    //lc_log_basic(LOG_NOTICE, "Tier : %s is not available", tier_name);
	    continue;
	}

	item = NULL;
	snprintf(counter_name, sizeof(counter_name),
		"%s.dm2_raw_write_bytes", tier_name);
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		counter_name, &item);
	if (item) {
	    stats->disk_tier[i].write_bytes = (uint64_t)item->value;
	}

	item = NULL;
	snprintf(counter_name, sizeof(counter_name),
		"%s.dm2_delete_bytes", tier_name);
	err = nkncnt_client_get_exact_match(&g_nvsd_mem_ctx,
		counter_name, &item);
	if (item) {
	    stats->disk_tier[i].evict_bytes = (uint64_t)item->value;
	}
    }
bail:
    return err;
}

int
agentd_send_stats_once(agentd_context_t *context, agentd_get_stats_t *stats)
{
    int err = 0;
    str_value_t str_value;

    bail_null(stats);

    OP_EMIT_TAG_START(context,ODCI_MFC_ALL_STATS);
	OP_EMIT_TAG_START(context,ODCI_MFC_STATS);

	    bzero(str_value, sizeof(str_value));
	    snprintf(str_value, sizeof(str_value), "%d", (uint32_t)stats->timestamp);
	    OP_EMIT_TAG_VALUE(context, ODCI_TIMESTAMP, str_value);

	    bzero(str_value, sizeof(str_value));
	    snprintf(str_value, sizeof(str_value), "%ld", (uint64_t)stats->nvsd_start_time);
	    OP_EMIT_TAG_VALUE(context, ODCI_START_TIME, str_value);

	    agentd_send_system_values(context, stats);

	    OP_EMIT_TAG_START(context,ODCI_GLOBAL);
		agentd_send_bytes(context, &stats->bytes);
		agentd_send_request(context, &stats->global_reqs);

		OP_EMIT_TAG_START(context,ODCI_RAM_CACHE);
		    bzero(str_value, sizeof(str_value));
		    snprintf(str_value, sizeof(str_value), "%ld", stats->ram_cache_size);
		    OP_EMIT_TAG_VALUE(context, ODCI_TOTAL, str_value);

		    bzero(str_value, sizeof(str_value));
		    snprintf(str_value, sizeof(str_value), "%ld", stats->ram_cache_used);
		    OP_EMIT_TAG_VALUE(context, ODCI_USED, str_value);
		OP_EMIT_TAG_END(context, ODCI_RAM_CACHE);

		agentd_send_cache_tier(context, &stats->ram_tier);
		agentd_send_cache_tier(context, &stats->disk_tier[0]);
		agentd_send_cache_tier(context, &stats->disk_tier[1]);
		agentd_send_cache_tier(context, &stats->disk_tier[2]);

		OP_EMIT_TAG_START(context,ODCI_DISKSPACE);
		    bzero(str_value, sizeof(str_value));
		    snprintf(str_value, sizeof(str_value), "%ld", stats->total_disk_size);
		    OP_EMIT_TAG_VALUE(context, ODCI_TOTAL, str_value);

		    bzero(str_value, sizeof(str_value));
		    snprintf(str_value, sizeof(str_value), "%ld", stats->total_disk_used);
		    OP_EMIT_TAG_VALUE(context, ODCI_USED, str_value);
		OP_EMIT_TAG_END(context, ODCI_DISKSPACE);

	    OP_EMIT_TAG_END(context, ODCI_GLOBAL);
	    OP_EMIT_TAG_START(context,ODCI_SERVICES);
		OP_EMIT_TAG_START(context,ODCI_HTTP);
		    agentd_send_all_namespaces(context, stats);
		OP_EMIT_TAG_END(context, ODCI_HTTP);
	    OP_EMIT_TAG_END(context, ODCI_SERVICES);
	OP_EMIT_TAG_END(context, ODCI_MFC_STATS);
    OP_EMIT_TAG_END(context, ODCI_MFC_ALL_STATS);

bail:
    return err;
}

int
agentd_send_all_namespaces(agentd_context_t *context, agentd_get_stats_t *stats)
{
    int err = 0, i = 0;
    struct ns_counters *ns = NULL;
    str_value_t str_value;

    if (stats->ns_num == 0) {
	/* no namespaces, nothing to send */
	goto bail;
    }

    ns = stats->ns_head;
    for(i = 0; i < stats->ns_num; i++) {
	ns = stats->ns_head + i ;

	OP_EMIT_TAG_START(context,ODCI_NAMESPACE);
	    OP_EMIT_TAG_VALUE(context, ODCI_SNAME, ns->ns_name);
	    agentd_send_bytes(context, &ns->bytes);
	    agentd_send_request(context, &ns->ns_reqs);
	    OP_EMIT_TAG_START(context,ODCI_OBJECTS);

		bzero(str_value, sizeof(str_value));
		snprintf(str_value, sizeof(str_value), "%ld", ns->pinned_objects);
		OP_EMIT_TAG_VALUE(context, ODCI_PINNED_OBJECTS, str_value);

		bzero(str_value, sizeof(str_value));
		snprintf(str_value, sizeof(str_value), "%ld", ns->pinned_bytes);
		OP_EMIT_TAG_VALUE(context, ODCI_PINNED_BYTES, str_value);

	    OP_EMIT_TAG_END(context, ODCI_OBJECTS);
	OP_EMIT_TAG_END(context, ODCI_NAMESPACE);
    }

bail:
    return err;
}

int
agentd_send_system_values(agentd_context_t *context, agentd_get_stats_t *stats)
{
    int err = 0;
    str_value_t str_value;

    OP_EMIT_TAG_START(context,ODCI_SYSTEM);
	OP_EMIT_TAG_START(context,ODCI_SYSTEM_CPU);
	    bzero(str_value, sizeof(str_value));
	    snprintf(str_value, sizeof(str_value), "%d", stats->cpu1min);
	    OP_EMIT_TAG_VALUE(context, ODCI_LOAD1, str_value);

	    bzero(str_value, sizeof(str_value));
	    snprintf(str_value, sizeof(str_value), "%d", stats->cpu5min);
	    OP_EMIT_TAG_VALUE(context, ODCI_LOAD5, str_value);

	    bzero(str_value, sizeof(str_value));
	    snprintf(str_value, sizeof(str_value), "%d", stats->cpu15min);
	    OP_EMIT_TAG_VALUE(context, ODCI_LOAD15, str_value);
	OP_EMIT_TAG_END(context,ODCI_CPU);

	OP_EMIT_TAG_START(context,ODCI_SYSTEM_MEMORY);
	    bzero(str_value, sizeof(str_value));
	    snprintf(str_value, sizeof(str_value), "%ld", stats->total_memory);
	    OP_EMIT_TAG_VALUE(context, ODCI_TOTAL, str_value);

	    bzero(str_value, sizeof(str_value));
	    snprintf(str_value, sizeof(str_value), "%ld", stats->used_memory);
	    OP_EMIT_TAG_VALUE(context, ODCI_USED, str_value);
	OP_EMIT_TAG_END(context,ODCI_MEMORY);
    OP_EMIT_TAG_END(context,ODCI_SYSTEM);

bail:
    return err;
}

int
agentd_send_cache_tier(agentd_context_t *context, struct cache_tier *tier)
{
    int err = 0;
    str_value_t str_value;

    /* check if tier is available */
    if ((tier == NULL) || (tier->tier_available == 0)) {
	goto bail;
    }

    OP_EMIT_TAG_START(context,ODCI_TIER);
	OP_EMIT_TAG_VALUE(context, ODCI_PROVIDER, tier->tier_name);

	bzero(str_value, sizeof(str_value));
	snprintf(str_value, sizeof(str_value), "%ld",tier->read_bytes);
	OP_EMIT_TAG_VALUE(context, ODCI_READ, str_value);

	bzero(str_value, sizeof(str_value));
	snprintf(str_value, sizeof(str_value), "%ld",tier->write_bytes);
	OP_EMIT_TAG_VALUE(context, ODCI_WRITE, str_value);

	bzero(str_value, sizeof(str_value));
	snprintf(str_value, sizeof(str_value), "%ld",tier->evict_bytes);
	OP_EMIT_TAG_VALUE(context, ODCI_EVICT, str_value);

    OP_EMIT_TAG_END(context,ODCI_TIER);

bail:
    return err;
}

int
agentd_send_request(agentd_context_t *context, struct conx_counters *reqs)
{
    int err = 0;
    str_value_t str_value;

    OP_EMIT_TAG_START(context,ODCI_REQUESTS);

	bzero(str_value, sizeof(str_value));
	snprintf(str_value, sizeof(str_value), "%ld", reqs->active_conx);
	OP_EMIT_TAG_VALUE(context, ODCI_ACTIVE, str_value );

	bzero(str_value, sizeof(str_value));
	snprintf(str_value, sizeof(str_value), "%ld", reqs->total_conx);
	OP_EMIT_TAG_VALUE(context, ODCI_TOTAL, str_value);

	bzero(str_value, sizeof(str_value));
	snprintf(str_value, sizeof(str_value), "%ld", reqs->cache_hits);
	OP_EMIT_TAG_VALUE(context, ODCI_CACHE_HIT, str_value);

    OP_EMIT_TAG_END(context, ODCI_REQUESTS);

bail:
    return err;
}

int
agentd_send_bytes(agentd_context_t *context, struct served_bytes *bytes)
{
    int err = 0;
    str_value_t str_value;

    OP_EMIT_TAG_START(context,ODCI_BYTES);

	bzero(str_value, sizeof(str_value));
	snprintf(str_value, sizeof(str_value), "%ld", bytes->cache);
	OP_EMIT_TAG_VALUE(context, ODCI_RAM, str_value );

	bzero(str_value, sizeof(str_value));
	snprintf(str_value, sizeof(str_value), "%ld", bytes->disk);
	OP_EMIT_TAG_VALUE(context, ODCI_DISK, str_value);

	bzero(str_value, sizeof(str_value));
	snprintf(str_value, sizeof(str_value), "%ld", bytes->nfs);
	OP_EMIT_TAG_VALUE(context, ODCI_NFS, str_value);

	bzero(str_value, sizeof(str_value));
	snprintf(str_value, sizeof(str_value), "%ld", bytes->origin);
	OP_EMIT_TAG_VALUE(context, ODCI_ORIGIN, str_value);

    OP_EMIT_TAG_END(context, ODCI_BYTES);

bail:
    return err;
}

int
agentd_free_stats(agentd_context_t *context, agentd_get_stats_t *stats)
{
    int err =0 ;
    safe_free(stats->ns_head);
    return err;
}

int
agentd_get_stats(agentd_context_t *context, void *data)
{
    int err = 0;
    agentd_get_stats_t all_stats;

    bzero(&all_stats, sizeof(all_stats));

    err = agentd_get_stats_once(context, &all_stats);
    bail_error(err);

    err = agentd_send_stats_once(context, &all_stats);
    bail_error(err);

    err = agentd_free_stats(context, &all_stats);
    bail_error(err);
bail:
    return err;
}
