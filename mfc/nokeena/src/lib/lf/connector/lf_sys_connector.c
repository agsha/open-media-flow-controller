#include <stdio.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/ioctl.h>

// nkn defs
#include "nkn_memalloc.h"
#include "nkn_defs.h"
#include "nkn_stat.h"

// lf defs
#include "lf_sys_connector.h"
#include "lf_common_defs.h"
#include "lf_err.h"
#include "lf_dbg.h"
#include "lf_ref_token.h"
#include "lf_proc_utils.h"

extern lf_app_metrics_intf_t *g_app_intf[LF_MAX_SECTIONS *
					 LF_MAX_APPS];
extern uint32_t g_lfd_processor_count;
extern char* g_lfd_root_disk_label;
extern lf_sys_glob_max_t g_sys_max_cfg;

static int is_device(char *name);
static void lf_sys_glob_metrics_cleanup(void *app_metrics_ctx);
static int32_t lf_sys_glob_metrics_create(void *inp, void **out);
static int32_t lf_sys_glob_metrics_hold(void *app_metrics_ctx);
static int32_t lf_sys_glob_metrics_get_out_size(void *app_metrics_ctx,
						uint32_t *out_size);
static int32_t lf_sys_glob_metrics_copy(const void *app_metrics_ctx, 
					void *const out);
static int32_t lf_sys_glob_metrics_release(void *app_metrics_ctx);
static int32_t lf_sys_glob_metrics_update(void *app_conn, 
					  void *app_metrics_ctx);
static int32_t lf_sys_glob_get_disk_stats(
			  lf_sys_glob_metrics_ctx_t *sgm, 
			  char *disk_name, 
			  lf_sys_disk_stats_ctx_t **out);
static void trie_destruct_func(void *nd);
static void * trie_copy_func(void *nd);

lf_app_metrics_intf_t lf_sys_glob_metrics_cb = {
    .create = lf_sys_glob_metrics_create,
    .cleanup = lf_sys_glob_metrics_cleanup,
    .update = lf_sys_glob_metrics_update,
    .get_out_size = lf_sys_glob_metrics_get_out_size,
    .release = lf_sys_glob_metrics_release,
    .copy = lf_sys_glob_metrics_copy,
    .hold = lf_sys_glob_metrics_hold
};

static int32_t lf_sys_update_all_app_metrics(uint64_t sample_duration,
					     const void *ctx);
static int32_t lf_sys_get_snapshot(void *const ctx,
				   void **out, void **ref_id);
static int32_t lf_sys_release_snapshot(void *ctx,
				       void *ref_id);
int32_t
lf_sys_connector_create(
	const lf_connector_input_params_t *inp,
	void **out)
{
    int32_t err = 0;
    lf_sys_connector_t *ctx = NULL;
    lf_app_metrics_intf_t *app_metrics_intf = NULL;
    uint8_t *buf = NULL;
    struct iovec *vec = NULL;
    uint32_t out_size = 0;
    uint32_t i, j;

    assert(inp);
    if (inp->sampling_interval < LF_MIN_SAMPLING_RES ||
	inp->window_len < LF_MIN_WINDOW_LEN ||
	inp->sampling_interval > LF_MAX_SAMPLING_RES ||
	inp->window_len > LF_MAX_WINDOW_LEN ||
	inp->num_tokens > LF_MAX_CLIENT_TOKENS) {
	err = -E_LF_INVAL;
	goto error;
    }

    ctx = (lf_sys_connector_t*)
	nkn_calloc_type(1, sizeof(lf_sys_connector_t),
			100);
    if (!ctx) {
	err = -E_LF_NO_MEM;
	goto error;
    }

    ctx->num_apps = 1;
    //    ctx->get_app_count = lf_sys_get_app_count;
    ctx->sampling_interval = inp->sampling_interval;
    ctx->window_len = inp->window_len;
    ctx->num_tokens = inp->num_tokens;
    ctx->update_all_app_metrics = lf_sys_update_all_app_metrics;
    ctx->get_snapshot = lf_sys_get_snapshot;
    ctx->release_snapshot = lf_sys_release_snapshot;
    err = lf_ref_token_init(&ctx->rt, inp->num_tokens);
    if (err)
        goto error;

    for (i = 0; i < ctx->num_apps; i++) {
	/* create the metrics objects */
	app_metrics_intf = lf_app_metrics_intf_query(g_app_intf,
						     LF_SYSTEM_SECTION, i);
	err = app_metrics_intf->create(ctx,
				       &ctx->app_specific_metrics[i]);
	if (err) {
	    goto error;
	}
	app_metrics_intf->get_out_size(ctx->app_specific_metrics[i],
				       &out_size);

	/* allocate and mark output buffers */
	assert(out_size);
	buf = (uint8_t*)nkn_malloc_type(ctx->num_tokens * out_size, 100);
	if (!buf) {
	    err = -E_LF_NO_MEM;
	    goto error;
	}
	vec = (struct iovec *)
	    nkn_calloc_type(ctx->num_tokens, sizeof(struct iovec), 100);
	if (!vec) {
	    err = -E_LF_NO_MEM;
	    goto error;
	}
	for (j = 0; j < ctx->num_tokens; j++) {
	    vec[j].iov_base = buf + (j * out_size);
	    vec[j].iov_len = out_size;
	}
	ctx->out[i] = vec;
    }

    *out = ctx;
    return err;
 error:
    if(ctx) lf_sys_connector_cleanup(ctx);
    return err;
}

int32_t
lf_sys_connector_cleanup(lf_sys_connector_t *sc)
{
    uint32_t i;
    lf_app_metrics_intf_t *app_metrics_intf = NULL;
    
    if (sc) {
	for (i = 0; i < sc->num_apps; i++) {
	    /* create the metrics objects */
	    app_metrics_intf = lf_app_metrics_intf_query(g_app_intf,
							 LF_SYSTEM_SECTION, i);
	    assert(app_metrics_intf);
	    if (app_metrics_intf->cleanup) 
	    app_metrics_intf->cleanup(sc->app_specific_metrics[i]);
	    
	    if(sc->out[i]) {
		if (sc->out[i][0].iov_base) {
		    free(sc->out[i][0].iov_base);
		}
	    }
	}
	free(sc);
    }

    return 0;
}

static int32_t 
lf_sys_update_all_app_metrics(uint64_t sample_duration, 
			     const void *ctx)
{
    lf_sys_connector_t *sc = NULL;
    lf_app_metrics_intf_t *app_metrics_intf = NULL;
    int32_t err = 0, free_token = LF_REF_TOKEN_UNAVAIL;
    uint32_t i;

    assert(ctx);
    sc = (lf_sys_connector_t *)ctx;

    for (i = 0; i < sc->num_apps; i++) {
	app_metrics_intf = lf_app_metrics_intf_query(g_app_intf,
						     LF_SYSTEM_SECTION, i);
	app_metrics_intf->hold(sc->app_specific_metrics[i]);
	app_metrics_intf->update(sc,
				 sc->app_specific_metrics[i]);
	lf_ref_token_hold(&sc->rt);
	free_token = lf_ref_token_get_free(&sc->rt);
	if (free_token < 0) {
	    lf_ref_token_release(&sc->rt);
	    app_metrics_intf->release(sc->app_specific_metrics[i]);
	    err = -E_LF_NO_FREE_TOKEN;
	    goto error;
	}
	lf_ref_token_release(&sc->rt);
	app_metrics_intf->copy(sc->app_specific_metrics[i],
			       sc->out[i][free_token].iov_base);
	app_metrics_intf->release(sc->app_specific_metrics[i]);
    }

 error:
    return err;
}

static int32_t
lf_sys_get_snapshot(void *const ctx,
		    void **out, void **ref_id)
{
    int32_t err = 0, free_token = LF_REF_TOKEN_UNAVAIL;
    uint32_t i;
    uint64_t *token_no = NULL;
    lf_sys_connector_t *sc = NULL;

    assert(ctx);
    assert(out);
    assert(ref_id);

    sc = (lf_sys_connector_t *)ctx;
    token_no = (uint64_t *)
	nkn_calloc_type(1, sizeof(uint64_t), 100);
    if (!token_no) {
	err = -E_LF_NO_MEM;
	goto error;
    }

    lf_ref_token_hold(&sc->rt);
    free_token = lf_ref_token_get_curr_token(&sc->rt); 
    if (free_token < 0) {
	lf_ref_token_release(&sc->rt);
	err = -E_LF_NO_FREE_TOKEN;
	goto error;
    }
    for (i = 0; i < sc->num_apps; i++) {
	lf_ref_token_add_ref(&sc->rt, free_token);
	*out = sc->out[i][free_token].iov_base;
	*token_no = free_token;
	*ref_id = token_no;
    }
    lf_ref_token_release(&sc->rt);

 error:
    return err;

}

static int32_t
lf_sys_release_snapshot(void *ctx,
		       void *ref_id)
{
    uint64_t *free_token = NULL;
    lf_sys_connector_t *sc = NULL;

    assert(ref_id);
    assert(ctx);
    
    sc = (lf_sys_connector_t *)ctx;
    lf_ref_token_hold(&sc->rt);
    free_token = (uint64_t*)ref_id;
    lf_ref_token_unref(&sc->rt, *free_token);
    free(free_token);
    lf_ref_token_release(&sc->rt);
    return 0;
}

static int32_t
lf_sys_glob_metrics_create(void *inp, void **out)
{
    int32_t err = 0;
    uint32_t num_slots = 0;
    lf_sys_glob_metrics_ctx_t *sgm = NULL;
    lf_sys_connector_t *sc = NULL;

    assert(inp);
    assert(out);

    sc = (lf_sys_connector_t *)inp;
    sgm = (lf_sys_glob_metrics_ctx_t*)
	nkn_calloc_type(1, sizeof(lf_sys_glob_metrics_ctx_t),
			100);
    if (!sgm) {
	err = -E_LF_NO_MEM;
	goto error;
    }

    num_slots = (sc->window_len / sc->sampling_interval) + 1;
    ma_init(num_slots, 1, &sgm->if_bw_ma);
    ma_init(num_slots, 1, &sgm->blk_io_rate_ma);
    pthread_mutex_init(&sgm->lock, NULL);    
    sgm->size = sizeof(lf_sys_glob_metrics_out_t);
    sgm->tcp_stat_buf = (char *) 
	nkn_malloc_type(LF_SYS_TCP_STAT_BUF_SIZE, 100);
    if (!sgm->tcp_stat_buf) {
	err = -E_LF_NO_MEM;
	goto error;
    }
    assert(g_lfd_processor_count != 0xffffffff);
    sgm->prev_cpu_times = (double *)
	nkn_calloc_type(NUM_CPU_COLS *g_lfd_processor_count,
			sizeof(double),
			100);
    if (!sgm->prev_cpu_times) {
	err = -E_LF_NO_MEM;
	goto error;
    }
    sgm->prev_if_bytes = 0xffffffffffffffff;
    sgm->prev_disk_ios = 0xffffffffffffffff;
    sgm->prev_num_tcp_conn = 0xffffffff;
    sgm->cpu_max = g_lfd_processor_count;
    sgm->tcp_conn_rate_max = g_sys_max_cfg.tcp_conn_rate_max;
    sgm->tcp_active_conn_max = g_sys_max_cfg.tcp_active_conn_max;
    nkn_mon_add("sys.active_conn_max", NULL, 
		(void *)(&sgm->tcp_active_conn_max), 
		sizeof(sgm->tcp_active_conn_max));

    sgm->http_tps_max = g_sys_max_cfg.http_tps_max;
    sgm->blk_io_rate_max = g_sys_max_cfg.blk_io_rate_max;
    sgm->if_bw_max = g_sys_max_cfg.if_bw_max;
    nkn_mon_add("sys.if_bw_max", NULL,
	(void *)(&sgm->if_bw_max), sizeof(sgm->if_bw_max));
    
    sgm->disk_list = cp_trie_create_trie(
	  (COLLECTION_MODE_NOSYNC|COLLECTION_MODE_COPY|
	   COLLECTION_MODE_DEEP),
	  trie_copy_func, trie_destruct_func);
    if (!sgm->disk_stats) {
	err = -E_LF_NO_MEM;
	goto error;
    }

    *out = sgm;
    return err;
 error:
    if (sgm->tcp_stat_buf) free(sgm->tcp_stat_buf);
    if (sgm->prev_cpu_times) free(sgm->prev_cpu_times);
    if (sgm) free(sgm);
    return err;
}

static void
lf_sys_glob_metrics_cleanup(void *app_metrics_ctx)
{
    lf_sys_glob_metrics_ctx_t *sgm = NULL;

    assert(app_metrics_ctx);
    sgm = (lf_sys_glob_metrics_ctx_t *)app_metrics_ctx;
    
    if(sgm) {
	ma_deinit(&sgm->if_bw_ma);
	ma_deinit(&sgm->blk_io_rate_ma);
	cp_trie_destroy(sgm->disk_list);
	if (sgm->tcp_stat_buf) free (sgm->tcp_stat_buf);
	if (sgm->prev_cpu_times) free (sgm->prev_cpu_times);
	free (sgm);
    }
}

static int32_t 
lf_sys_glob_metrics_hold(void *app_metrics_ctx)
{
    lf_sys_glob_metrics_ctx_t *sgm = NULL;

    assert(app_metrics_ctx);
    sgm = (lf_sys_glob_metrics_ctx_t *)app_metrics_ctx;

    pthread_mutex_lock(&sgm->lock);
    
    return 0;
}

static int32_t
lf_sys_glob_metrics_get_out_size(void *app_metrics_ctx,
				uint32_t *out_size)
{
    lf_sys_glob_metrics_ctx_t *sgm = NULL;

    assert(app_metrics_ctx);
    assert(out_size);
    sgm = (lf_sys_glob_metrics_ctx_t *)app_metrics_ctx;

    *out_size = sgm->size;
    return 0;
}

static int32_t 
lf_sys_glob_metrics_copy(const void *app_metrics_ctx, 
			void *const out)
{
    lf_sys_glob_metrics_ctx_t *sgm = NULL;

    assert(app_metrics_ctx);
    assert(out);
    sgm = (lf_sys_glob_metrics_ctx_t *)app_metrics_ctx;

    memcpy(out, sgm, sgm->size);

    return sgm->size;    
}

static int32_t 
lf_sys_glob_metrics_release(void *app_metrics_ctx)
{
    lf_sys_glob_metrics_ctx_t *sgm = NULL;

    assert(app_metrics_ctx);
    sgm = (lf_sys_glob_metrics_ctx_t *)app_metrics_ctx;

    pthread_mutex_unlock(&sgm->lock);
    
    return 0;
}

static int32_t
lf_sys_glob_metrics_update(void *app_conn, void *app_metrics_ctx)
{
    int32_t err = 0, i;
    uint32_t j;
    lf_sys_glob_metrics_ctx_t *sgm = NULL;
    FILE *f = NULL;
    char cpu_name[6] = {0}, protocol[32] = {0};
    char *buf = NULL, *retbuf = NULL;
    int32_t tm_i32[14], num_tcp_conn;
    double cpu_times[NUM_CPU_COLS] = {0.0};
    double tot_cpu = 0, idle_cpu = 0, *prev_cpu_times;
    lf_sys_connector_t *sc = NULL;

    assert(app_metrics_ctx);
    assert(app_conn);
    sgm = (lf_sys_glob_metrics_ctx_t *)app_metrics_ctx;
    sc = (lf_sys_connector_t *)app_conn;

    /* compute CPU usage */
    f = fopen("/proc/stat", "r");
    if (!f) {
	err = -E_LF_SYS_CONN;
	goto error;
    }

    fscanf(f, "%5s %lf %lf %lf %lf %lf %lf %lf %lf",
	   cpu_name, &cpu_times[0], &cpu_times[1],
	   &cpu_times[2], &cpu_times[3], &cpu_times[4],
	   &cpu_times[5], &cpu_times[6], &cpu_times[7]);

    for (j = 0; j < g_lfd_processor_count; j++) {
	fscanf(f, "%5s %lf %lf %lf %lf %lf %lf %lf %lf",
	       cpu_name, &cpu_times[0], &cpu_times[1],
	       &cpu_times[2], &cpu_times[3], &cpu_times[4],
	       &cpu_times[5], &cpu_times[6], &cpu_times[7]);
	prev_cpu_times = sgm->prev_cpu_times + (j * NUM_CPU_COLS);
	for (i = 0; i < NUM_CPU_COLS; i++) {
	    tot_cpu += (cpu_times[i] - prev_cpu_times[i]);
	}
	idle_cpu = 100 * ((cpu_times[3] - prev_cpu_times[3])/tot_cpu);
	sgm->cpu_use[j] = 100 - idle_cpu;
	tot_cpu = 0;
	for (i = 0; i < NUM_CPU_COLS; i++) {
	    prev_cpu_times[i] = cpu_times[i];
	}
    }
    fclose(f);

    /* compute CPU uptime */
    uint32_t cpu_clk = lf_get_cpu_clk_freq();
    //printf("cpu clk %d\n", cpu_clk);
    uint64_t up = 0, cpu_elapsed, usec = 0, dec = 0;
    f = fopen("/proc/uptime", "r");
    if (!f) {
	err = -E_LF_SYS_CONN;
	goto error;
    }
    fscanf(f, "%lu.%lu", &usec, &dec);
    up = (usec * cpu_clk) + (dec * cpu_clk/100);
    //printf("usec: %lu, dec: %lu\n",usec, dec);
    cpu_elapsed = up - sgm->prev_up;
    //printf("up: %lu, prev up: %lu\n", up, sgm->prev_up);
    sgm->prev_up = up;
    fclose(f);

    /* compute tcp conn rate */
    num_tcp_conn = 0;
    buf = sgm->tcp_stat_buf;
    const char *tcp_metrics_fmt =
	    "%4s %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n";
    f = fopen("/proc/net/snmp", "r");
    if (!f) {
	err = -E_LF_SYS_CONN;
	goto error;
    }
    while (!feof(f)) {
	retbuf = fgets(buf, LF_SYS_TCP_STAT_BUF_SIZE, f);
	if (!retbuf) break;
	if (strstr(buf, "Tcp:") == 0) continue;
	i = buf[5] - '0';
	if (i < 0 || i > 9) continue;
	err = sscanf(buf, tcp_metrics_fmt,
		&protocol[0], &tm_i32[0], &tm_i32[1],
		&tm_i32[2], &tm_i32[3], &tm_i32[4],
		&tm_i32[5], &tm_i32[6], &tm_i32[7],
		&tm_i32[8], &tm_i32[9], &tm_i32[10],
		&tm_i32[11], &tm_i32[12], &tm_i32[13]);
	if (err < 15) {
		continue;
	}
	num_tcp_conn = tm_i32[8];
	break;
    }
    fclose(f);

    sgm->num_tcp_conn = num_tcp_conn;
    if (sgm->prev_num_tcp_conn == 0xffffffff) {
	sgm->prev_num_tcp_conn = num_tcp_conn;
    }
    sgm->tcp_conn_rate = (num_tcp_conn - sgm->prev_num_tcp_conn) /
	((double)sc->sampling_interval/1000);
    sgm->prev_num_tcp_conn = num_tcp_conn;

    /* if stats */
    char if_name[14] = {0}, tmp[14];
    uint64_t ifm[2], tot_if_bytes = 0;
    int64_t if_bw = 0;
    const char *if_metrics_fmt = "%lu %*lu %*u %*u %*u %*u %*lu %*lu"
	" %lu %*lu %*u %*u %*u %*u %*u %*lu";
    char *p = NULL;
    sample_t s;

    f = fopen("/proc/net/dev", "r");
    if (!f) {
        err = -E_LF_SYS_CONN;
        goto error;
    }
    /* skip first two lines */
    retbuf = fgets(buf, LF_SYS_TCP_STAT_BUF_SIZE, f);
    retbuf = fgets(buf, LF_SYS_TCP_STAT_BUF_SIZE, f);
    while (!feof(f)) {
	retbuf = fgets(buf, LF_SYS_TCP_STAT_BUF_SIZE, f);
	if (!retbuf) {
	    break;
	}

	p = strchr(buf, ':');
	if ( (p - buf) > 14) {
	    fclose(f);
	    err = -E_LF_SYS_IF_STATS;
	    goto error;
	}
	memset(tmp, 0, 14);
	memcpy(tmp, buf, p - buf);
	memset(if_name, 0, 14);
	err = sscanf(tmp, "%s", if_name); /* skip leading spaces */
	/* compute for all interfaces except eth0 */
	if (strcmp(if_name, "eth0") &&
	    strcmp(if_name, "lo")) {
	    if (!memcmp(if_name, "eth", 3)) {
		err = sscanf(p+1, if_metrics_fmt,
			 &ifm[0], &ifm[1]);
		if (err < 2) {
		    continue;
		}
		tot_if_bytes += (ifm[0] + ifm[1]);
	    }
	}
    }
    fclose(f);
    if (sgm->prev_if_bytes == 0xffffffffffffffff) {
	sgm->prev_if_bytes = tot_if_bytes;
    }
    if_bw = (tot_if_bytes - sgm->prev_if_bytes)/
	((double)sc->sampling_interval/1000);
    s.i64 = if_bw;
    ma_move_window(&sgm->if_bw_ma, s);
    sgm->if_bw = (uint64_t)sgm->if_bw_ma.prev_ma;
    sgm->prev_if_bytes = tot_if_bytes;

    const char *io_metrics_fmt =
	"%*u %*u %s %lu %*lu %*llu %*lu %lu %*lu %*llu"
	"%*lu %*lu %lu %*lu";
    char disk_name[11];
    uint64_t tot_disk_ios = 0, disk_ios_rate = 0, iom[2], 
	io_cpu_util = 0;

    sgm->num_disks = 0;
    f = fopen("/proc/diskstats", "r");
    if (!f) {
        err = -E_LF_SYS_CONN;
        goto error;
    }
    while (!feof(f)) {
	retbuf = fgets(buf, LF_SYS_TCP_STAT_BUF_SIZE, f);
	if (!retbuf) {
	    break;
	}

	memset(disk_name, 0, 11);
	err = sscanf(buf, io_metrics_fmt, disk_name, &iom[0], &iom[1],
		     &io_cpu_util);
	if ((err < 3) || (err > 10)) {
	    continue;
	}
	if (is_device(disk_name)) {
	    if (!memcmp(disk_name, "sd", 2) || 
		!memcmp(disk_name, "hd", 2)) {
		lf_sys_disk_stats_ctx_t *ds = NULL;
		double val = 0.0;
		uint64_t elapsed = 0;
		lf_sys_glob_get_disk_stats(sgm, disk_name, &ds);
		if (ds) {
		    snprintf(sgm->disk_stats[sgm->num_disks].name, 
			     LF_SYS_DISK_LABEL_LEN, "%s",
			     disk_name);
		    elapsed = io_cpu_util - ds->prev_disk_util;
		    val = (((double)elapsed * cpu_clk/1000) / cpu_elapsed);
		    val = val * 100;
		    //printf("elapsed %lu, tot cpu %lu disk util %f\n", 
		    //	   elapsed, cpu_elapsed, val);
		    sgm->disk_stats[sgm->num_disks].util = val;
		    ds->prev_disk_util = io_cpu_util;
		    tot_disk_ios += iom[0] + iom[1];
		    sgm->num_disks++;
		}
	    }
	}
    }
    fclose(f);
    if (sgm->prev_disk_ios == 0xffffffffffffffff) {
	sgm->prev_disk_ios = tot_disk_ios;
    }
    disk_ios_rate = (tot_disk_ios - sgm->prev_disk_ios) /
	((double)sc->sampling_interval/1000);
    sgm->blk_io_rate = disk_ios_rate;
    sgm->prev_disk_ios = tot_disk_ios;

    LF_LOG(LOG_NOTICE, LFD_SYS_SECTION, "idle cpu %lf, cpu use %lf,"
	   " num tcp conn %d, tcp conn rate %d"
	   " if bw %lu, disk ios %lu\n",
	   idle_cpu, sgm->cpu_use[0], num_tcp_conn,
	   sgm->tcp_conn_rate, sgm->if_bw, sgm->blk_io_rate);

 error:
    return err;
}

static int32_t
lf_sys_glob_get_disk_stats(lf_sys_glob_metrics_ctx_t *sgm, 
			   char *disk_name, 
			   lf_sys_disk_stats_ctx_t **out)
{
    lf_sys_disk_stats_ctx_t *ds = NULL, *ds_new = NULL;
    int32_t err = 0;

    assert(out);

    *out = NULL;
    ds = (lf_sys_disk_stats_ctx_t*)
	cp_trie_exact_match(sgm->disk_list,
			    disk_name);

    /* new disk hot plugged? */
    if (!ds) {
	ds_new = (lf_sys_disk_stats_ctx_t*)
	    nkn_calloc_type(1,
			    sizeof(lf_sys_disk_stats_ctx_t), 100);
	if (!ds_new) {
	    err = -E_LF_NO_MEM;
	    goto error;
	}
	err = cp_trie_add(sgm->disk_list,
			  strdup(disk_name),
			  ds_new);
	if (err) {
	    err = -E_LF_NO_MEM;
	    goto error;
	}
	*out = ds_new;
    } else {
	/* existing disk */
	*out = ds;
    }

 error:
    return err;
}

    
static int is_device(char *name)
{
    char path[PATH_MAX];
    char *p;

    while ((p = strchr(name, '/'))) {
	*p = '!';
    }
    snprintf(path, PATH_MAX, "%s/%s", 
	     "/sys/block", name);
    
    return !(access(path, F_OK));
}

static void *
trie_copy_func(void *nd)
{
    UNUSED_ARGUMENT(nd);
    return nd;
}

static void
trie_destruct_func(void *nd)
{
    free(nd);
    return;
}
