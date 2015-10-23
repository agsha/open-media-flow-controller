#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#include "nkn_stat.h"
#include "al_analyzer.h"
#include "nkn_hash.h"

NKNCNT_DEF(tot_transaction_entries, uint64_t, "", "")
NKNCNT_DEF(err_hostname, uint64_t, "", "hostname")
NKNCNT_DEF(err_hostname_toolong, uint64_t, "", "hostname")
NKNCNT_DEF(err_uri, uint64_t, "", "uri")
NKNCNT_DEF(err_malloc, uint64_t, "", "malloc")
NKNCNT_DEF(err_too_much_trans, uint64_t, "", "")

#define NO_PRINTF

#ifdef NO_PRINTF
#define printf(a,...) do{} while (0)
#endif


uint32_t max_samples = 10000;
uint32_t update_timer = 1;

/*
 * we only sort the top 100 domains and 100 uris for each domain 
 */
#define TOP_DOMAINS_NUM	100
domain_list_t * top_domains[TOP_DOMAINS_NUM];
#define TOP_URIS_NUM	100
uri_list_t * top_uris[TOP_URIS_NUM];

table_t sec_table[TOT_SECONDS];
table_t min_table[TOT_MINUTES];
table_t hour_table[TOT_HOURS];

#define TEMP_BUFFER 256

static pthread_mutex_t al_analyzer_lock = PTHREAD_MUTEX_INITIALIZER;

/*-----------------------------------------------------------------------------
 * PRIVATE FUNCTIONS
 *---------------------------------------------------------------------------*/
static int add_entry(table_t * ptable, analysis_data_t * data);
static void merge_uri(void * key, void * value, void * user_data);
static void merge_domain(void *key, void *n, void *p);
static void merge_tables(table_t * dst_ptable, table_t * src_ptable);
static void sort_top_uris(void * key, void * value, void * user_data);
static void prune_top_uris(void * key, void * value, void * user_data);
static void prune_domain_list(domain_list_t * p_domain);
static void sort_top_domains(void * key, void * value, void * user_data);
static void prune_top_domains(void * key, void * value, void * user_data);
static void prune_tables(table_t * ptable);
static int free_uri_list(void *key, void *value, void * user_data);
static int free_domain_list(void *key, void *value, void * user_data);
static void reset_table(table_t * ptable);
static int alan_get_respcode(data_request_t * dr);
static int alan_get_filesize(data_request_t * dr);
static int alan_get_domain(data_request_t * dr);
static int alan_get_uri(data_request_t * dr);

/*-----------------------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *---------------------------------------------------------------------------*/
int alan_update_data(analysis_data_t * data);
int alan_init_table(void);
void *alan_housekeeping(void *args);


/* Would have liked to use something like log2(size) to 
* directly return the index. But log2 might get more expensive
* than a direct jumptable (which would have been the best)
* for now, use a static array which defines the limits.
*/
static const size_dist_t size_dist_grpss[TOT_FILESIZE_GRP] = {
    {.min_size = 0,.max_size = (1 << 10) - 1, .prompt="1K"},
    {.min_size = (1 << 10),.max_size = (1 << 12) - 1, .prompt="4K"},
    {.min_size = (1 << 12),.max_size = (1 << 13) - 1, .prompt="8K"},
    {.min_size = (1 << 13),.max_size = (1 << 14) - 1, .prompt="16K"},
    {.min_size = (1 << 14),.max_size = (1 << 15) - 1, .prompt="32K"},
    {.min_size = (1 << 15),.max_size = (1 << 16) - 1, .prompt="64K"},
    {.min_size = (1 << 16),.max_size = (1 << 17) - 1, .prompt="128K"},
    {.min_size = (1 << 17),.max_size = (1 << 18) - 1, .prompt="256K"},
    {.min_size = (1 << 18),.max_size = (1 << 19) - 1, .prompt="512K"},
    {.min_size = (1 << 19),.max_size = (1 << 21) - 1, .prompt="2M"},
    {.min_size = (1 << 21),.max_size = (1 << 23) - 1, .prompt="8M"},
    {.min_size = (1 << 23),.max_size = (1 << 25) - 1, .prompt="32M"},
    {.min_size = (1 << 25),.max_size = (1 << 27) - 1, .prompt="128M"},
    {.min_size = (1 << 27),.max_size = (1 << 29) - 1, .prompt="512M"},
    {.min_size = (1 << 29),.max_size = (1 << 30) - 1, .prompt="1G"},
    {.min_size = (1 << 30),.max_size = (0) - 1, .prompt=">1G"},
};

int alan_init_table(void)
{
    int i;

    for(i=0; i<TOT_SECONDS; i++) {
	memset((char *)&sec_table[i], 0, sizeof(table_t));
    	sec_table[i].p_domain_ht = (NHashTable *)nhash_table_new(n_str_hash, n_str_equal, 0);
    }
    for(i=0; i<TOT_MINUTES; i++) {
	memset((char *)&min_table[i], 0, sizeof(table_t));
    	min_table[i].p_domain_ht = (NHashTable *)nhash_table_new(n_str_hash, n_str_equal, 0);
    }
    for(i=0; i<TOT_HOURS; i++) {
	memset((char *)&hour_table[i], 0, sizeof(table_t));
    	hour_table[i].p_domain_ht = (NHashTable *)nhash_table_new(n_str_hash, n_str_equal, 0);
    }

    return 1;
}

/*
 * API to insert a node, if it doesnt exist. If the node already exists, 
 * then update the node values.
 */
static int add_entry(table_t * ptable, analysis_data_t * data)
{
    int i;
    domain_list_t   * p_domain;
    uri_list_t   * p_uri;
    char * ps, *pd;
    char domain[MAX_DOMAIN_STRING_SIZE];
    char uri[MAX_URI_STRING_SIZE];
    int len;
    char temp[TEMP_BUFFER];
    uint64_t keyhash;

    /* update file size distribution. */
    printf("add_entry: ptable=%p\n", ptable);
    for(i=0; i<TOT_FILESIZE_GRP; i++) {
	if(data->bytes_out < size_dist_grpss[i].max_size) {
		ptable->hits_filesize[i] ++;
		break;
	}
    }

    /* update status code. */
    switch(data->errcode) {
    case 200: ptable->resp_code.hits_200++; break;
    case 206: ptable->resp_code.hits_206++; break;
    case 302: ptable->resp_code.hits_302++; break;
    case 304: ptable->resp_code.hits_304++; break;
    case 404: ptable->resp_code.hits_404++; break;
    case 500: ptable->resp_code.hits_500++; break;
    default: ptable->resp_code.hits_other++; break;
    }

    /* update domain */
    if(data->hostname == NULL) {
	glob_err_hostname ++;
	return 0;
    }
    /*
     * uri format is: "GET /xxxx HTTP/1.1"
     * What we do is: 1) get true uri, 2) truncate to up to MAX_URI_STRING_SIZE bytes
     */
    if(data->hostlen < MAX_DOMAIN_STRING_SIZE-1) {
	len = data->hostlen;
	memcpy(domain, data->hostname, len);
	domain[len] = 0;
    }
    else if (data->hostlen < TEMP_BUFFER){
	strncpy(temp, data->hostname, data->hostlen);
	temp[data->hostlen] = 0;
	ps = temp;
	while(1) {
		pd=strchr(ps, '.');
		if(!pd) {
			glob_err_hostname ++;
			return 0;
		}
		len = data->hostlen - (pd-temp);
		if(len<MAX_DOMAIN_STRING_SIZE-4) break;
		ps = pd + 1;
	}
	strcpy(domain, "..");
	memcpy(domain+2, pd, len);
	len+=2;
    	domain[len] = 0;
    }
    else {
	glob_err_hostname_toolong ++;
	return 0;
    }
    keyhash = (uint64_t)n_str_hash(domain);
    p_domain = (domain_list_t *)nhash_lookup(ptable->p_domain_ht, keyhash, domain);
    if(p_domain == NULL) {
	p_domain = (domain_list_t *)malloc(sizeof(domain_list_t));
	if(p_domain == NULL) {
		glob_err_malloc ++;
		return 0;
	}
	p_domain->domain = strdup(domain);
	p_domain->hits = 1;
	p_domain->cache_bytes = p_domain->origin_bytes = p_domain->unknown_bytes = 0.0;
	switch (data->cache_indicator){
	case bytes_cache:
		p_domain->cache_bytes = CONVERT_TO_MBYTES((float)data->bytes_out); break;
	case bytes_origin:
		p_domain->origin_bytes = CONVERT_TO_MBYTES((float)data->bytes_out); break;
	default:
		p_domain->unknown_bytes = CONVERT_TO_MBYTES((float)data->bytes_out); break;
	}
	//printf("bw= %d\n", p_domain->cache_bytes + p_domain->origin_bytes + p_domain->unknown_bytes);	

	p_domain->p_uri_ht = (NHashTable *)nhash_table_new(n_str_hash, n_str_equal, 0);
	if(p_domain->p_uri_ht == NULL) {
		free(p_domain);
		glob_err_malloc ++;
		return 0;
	}
	keyhash = (uint64_t)n_str_hash(p_domain->domain);
	nhash_insert(ptable->p_domain_ht, keyhash, p_domain->domain, (void *)p_domain);
    }
    else {
    	p_domain->hits ++;
	switch (data->cache_indicator){
	case bytes_cache:
		p_domain->cache_bytes += CONVERT_TO_MBYTES((float)data->bytes_out); break;
	case bytes_origin:
		p_domain->origin_bytes += CONVERT_TO_MBYTES((float)data->bytes_out); break;
	default:
		p_domain->unknown_bytes += CONVERT_TO_MBYTES((float)data->bytes_out); break;
	}
	//printf("bw= %d\n", p_domain->cache_bytes + p_domain->origin_bytes + p_domain->unknown_bytes);
    }
    
    /* update url */
    if(data->uri == NULL) {
	glob_err_uri ++;
	return 0;
    }
    /*
     * uri format is: "GET /xxxx HTTP/1.1"
     * What we do is: 1) get true uri, 2) truncate to up to MAX_URI_STRING_SIZE bytes
     */
    pd = uri;
    ps=data->uri;
    while(*ps!=' ' && data->urilen) {
	ps++; data->urilen--;
    }
    if(data->urilen == 0) {
	return 0;
    }
    ps++; data->urilen--;
    for(i=0; i<MAX_URI_STRING_SIZE-1; i++) {
	if(data->urilen==0) break;
	if(*ps==' ') break;
	*pd ++ = *ps ++;
	data->urilen--;
    }
    *pd = 0;
    keyhash = (uint64_t)n_str_hash(uri);
    p_uri = (uri_list_t *)nhash_lookup(p_domain->p_uri_ht, keyhash, uri);
    if(p_uri == NULL) {
	p_uri = (uri_list_t *)malloc(sizeof(uri_list_t));
	if(p_uri == NULL) {
		glob_err_malloc ++;
		return 0;
	}
	p_uri->uri = strdup(uri);
	p_uri->hits = 1;
        keyhash = (uint64_t)n_str_hash(p_uri->uri);
	nhash_insert(p_domain->p_uri_ht, keyhash, p_uri->uri, (void *)p_uri);
    }
    else {
    	p_uri->hits ++;
    }
    printf("ptable=%p p_domain=%p domain=%s (%d) uri_ht=%p uri=%s (%d)\n", 
	ptable, p_domain, domain, p_domain->hits, p_uri, uri, p_uri->hits);

    return 1;
}

int alan_update_data(analysis_data_t * data)
{
    struct timeval tv;
    struct tm now;
    table_t * p_cur_sec_table;

    glob_tot_transaction_entries++;

    if (!data )
	return 0;		// return silently - nothing to add, data is corrupt

    // Check if we crossed our time limits
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &now);


    p_cur_sec_table = &sec_table[now.tm_sec%60];
    if (p_cur_sec_table->tot_samples > max_samples) {
        printf("alan_update_data: update failed\n");
	glob_err_too_much_trans ++;
	return 0;
    }
    p_cur_sec_table->tot_samples ++;
    pthread_mutex_lock (&al_analyzer_lock);
    add_entry(p_cur_sec_table, data);
    pthread_mutex_unlock (&al_analyzer_lock);
    return 1;
}


/*
 * API to merge all 60 sec_tables into 1 min_table and 
 * merge all 60 min_tables into 1 hour_table.
 */
static void merge_uri(void * key, void * value, void * user_data)
{
   domain_list_t * dst_pdomain = (domain_list_t *)user_data;
   uri_list_t * p_uri;
   uri_list_t * p_clone_uri;
   uri_list_t * p_srcuri = (uri_list_t *)value;
   uint64_t keyhash;

   /* every uri merge is one domain hit */
   dst_pdomain->hits += p_srcuri->hits;

   keyhash = (uint64_t)n_str_hash(key);
   p_uri = nhash_lookup(dst_pdomain->p_uri_ht, keyhash, key);
   if(p_uri) {
	/* found it */
	p_uri->hits += p_srcuri->hits;
	return;
   }

   /* insert whole p_uri into p_domain */
   p_clone_uri = (uri_list_t *)malloc(sizeof(uri_list_t));
   if(p_clone_uri == NULL) {
	return;
   }
   p_clone_uri->uri = strdup(p_srcuri->uri);
   p_clone_uri->hits = p_srcuri->hits;
   keyhash = (uint64_t)n_str_hash(p_clone_uri->uri);
   nhash_insert(dst_pdomain->p_uri_ht, keyhash, p_clone_uri->uri, p_clone_uri);
}

static void merge_domain(void *key, void *n, void * arg)
{
   table_t * dst_ptable = (table_t *)arg;
   domain_list_t * p_domain;
   domain_list_t * p_clone_domain;
   domain_list_t * p_srcdomain = (domain_list_t *)n;
   uint64_t keyhash;

   keyhash = (uint64_t)n_str_hash(key);
   p_domain = nhash_lookup(dst_ptable->p_domain_ht, keyhash, key);
   if(p_domain) {
	/* dst_ptable and src_ptable has common domain, merge them */
	p_domain->cache_bytes += p_srcdomain->cache_bytes;
	p_domain->origin_bytes += p_srcdomain->origin_bytes;
	p_domain->unknown_bytes += p_srcdomain->unknown_bytes;
    	nhash_foreach(p_srcdomain->p_uri_ht, merge_uri, (void *)p_domain);
	return;
   }

   /* insert whole p_domain into dst_ptable */
    p_clone_domain = (domain_list_t *)malloc(sizeof(domain_list_t));
    if(p_clone_domain == NULL) {
	return;
    }
    p_clone_domain->domain = strdup(p_srcdomain->domain);
    p_clone_domain->hits = 0; //p_srcdomain->hits;
    p_clone_domain->cache_bytes = p_srcdomain->cache_bytes;
    p_clone_domain->origin_bytes = p_srcdomain->origin_bytes;
    p_clone_domain->unknown_bytes = p_srcdomain->unknown_bytes;

    p_clone_domain->p_uri_ht = (NHashTable *)nhash_table_new(n_str_hash, n_str_equal, 0);
    if(p_clone_domain->p_uri_ht == NULL) {
	free(p_clone_domain);
	return;
    }
    nhash_foreach(p_srcdomain->p_uri_ht, merge_uri, (void *)p_clone_domain);
    keyhash = (uint64_t)n_str_hash(p_clone_domain->domain);
    nhash_insert(dst_ptable->p_domain_ht, keyhash, p_clone_domain->domain, p_clone_domain);
}

static void merge_tables(table_t * dst_ptable, table_t * src_ptable)
{
    int i;

    //printf("merge_tables dst=%p src=%p tot_sample=%d\n", dst_ptable, src_ptable, src_ptable->tot_samples);
    dst_ptable->tot_samples += src_ptable->tot_samples;

    /* merge file size distribution */
    for(i=0; i<TOT_FILESIZE_GRP; i++) {
	dst_ptable->hits_filesize[i] += src_ptable->hits_filesize[i];
    }

    /* merge status code distribution */
    dst_ptable->resp_code.hits_200 += src_ptable->resp_code.hits_200;
    dst_ptable->resp_code.hits_206 += src_ptable->resp_code.hits_206;
    dst_ptable->resp_code.hits_302 += src_ptable->resp_code.hits_302;
    dst_ptable->resp_code.hits_304 += src_ptable->resp_code.hits_304;
    dst_ptable->resp_code.hits_404 += src_ptable->resp_code.hits_404;
    dst_ptable->resp_code.hits_500 += src_ptable->resp_code.hits_500;
    dst_ptable->resp_code.hits_other += src_ptable->resp_code.hits_other;

    /* merge the domain */
    nhash_foreach(src_ptable->p_domain_ht, merge_domain, (void *)dst_ptable);

    return;
}


static void sort_top_uris(void * key, void * value, void * user_data)
{
    uri_list_t * p_uri = (uri_list_t *)value;
    int i, j;

    printf("sort_top_uris: key=%s\n", (char *)key);

    /* bubble sort */
    for(i=0; i<TOP_URIS_NUM; i++) {
	if(top_uris[i] == NULL) {
		top_uris[i] = p_uri;
		return;
	}
	if(top_uris[i]->hits < p_uri->hits) {
		for(j=TOP_URIS_NUM-2; j>=i; j--) {
			top_uris[j+1] = top_uris[j];
		}
		top_uris[i] = p_uri;
		return;
	}
    }
}

static void prune_top_uris(void * key, void * value, void * user_data)
{
    domain_list_t * p_domain = (domain_list_t *)user_data;
    uri_list_t * p_uri = (uri_list_t *)value;
    int i;
    uint64_t keyhash;

    /*
     * if this uri is not hot, we remove it.
     */
    for(i=TOP_URIS_NUM-1; i>=0; i--) {
	if(top_uris[i] != NULL) break;
    }
    if(i==0) return;
    if(p_uri->hits < top_uris[i]->hits) {
	keyhash = (uint64_t)n_str_hash(key);
	nhash_remove(p_domain->p_uri_ht, keyhash, key);
	return;
    }
}

static void prune_domain_list(domain_list_t * p_domain)
{
    int i;

    /* Sort first, get max_disp_domains entries */ 
    for(i=0; i<TOP_URIS_NUM; i++) {
	top_uris[i] = NULL;
    }
    nhash_foreach(p_domain->p_uri_ht, sort_top_uris, NULL);
    nhash_foreach(p_domain->p_uri_ht, prune_top_uris, p_domain);
}

static void sort_top_domains(void * key, void * value, void * user_data)
{
    domain_list_t * p_domain = (domain_list_t *)value;
    int i, j;

    printf("sort_top_domains: key=%s\n", (char *)key);

    /* bubble sort */
    for(i=0; i<TOP_DOMAINS_NUM; i++) {
	if(top_domains[i] == NULL) {
		top_domains[i] = p_domain;
		return;
	}
	if(top_domains[i]->hits < p_domain->hits) {
		for(j=TOP_DOMAINS_NUM-2; j>=i; j--) {
			top_domains[j+1] = top_domains[j];
		}
		top_domains[i] = p_domain;
		return;
	}
    }
}

static void prune_top_domains(void * key, void * value, void * user_data)
{
    table_t * ptable = (table_t *)user_data;
    domain_list_t * p_domain = (domain_list_t *)value;
    int i;
    uint64_t keyhash;

    /*
     * if this domain is not hot, we remove it.
     */
    for(i=TOP_DOMAINS_NUM-1; i>=0; i--) {
	if(top_domains[i] != NULL) break;
    }
    if(i==0) return;
    if(p_domain->hits < top_domains[i]->hits) {
        keyhash = (uint64_t)n_str_hash(key);
	nhash_remove(ptable->p_domain_ht, keyhash, key);
	return;
    }

    /*
     * if this domain is hot, we prune the uri tables.
     */
    prune_domain_list(p_domain);
}

static void prune_tables(table_t * ptable)
{
    int i;

    //printf("prune_tables %p\n", ptable);
    /* Sort first, get max_disp_domains entries */ 
    for(i=0; i<TOP_DOMAINS_NUM; i++) {
	top_domains[i] = NULL;
    }
    nhash_foreach(ptable->p_domain_ht, sort_top_domains, NULL);
    nhash_foreach(ptable->p_domain_ht, prune_top_domains, ptable);
}


static int free_uri_list(void * key, void * value, void * user_data)
{
    uri_list_t * p_uri = (uri_list_t *)value;
    free(p_uri->uri);
    free(p_uri);
    return TRUE;
}


static int free_domain_list(void *key, void *value, void * user_data)
{
    domain_list_t * p_domain = (domain_list_t *)value;
    nhash_foreach_remove(p_domain->p_uri_ht, free_uri_list, NULL);
    nhash_destroy(p_domain->p_uri_ht);
    free(p_domain->domain);
    free(p_domain);
    return TRUE;
}

/* Free the node data, its key, and any allocate memory */
static void reset_table(table_t * ptable)
{
    NHashTable * tmp;

    //printf("reset_table %p\n", ptable);
    nhash_foreach_remove(ptable->p_domain_ht, free_domain_list, NULL);
    tmp = ptable->p_domain_ht;
    memset((void *)ptable, 0, sizeof(table_t));
    ptable->p_domain_ht = tmp;
    return;
}



/*
 * The thread that does housekeeping tasks on the table
 */
void *alan_housekeeping(void *args)
{
    struct timeval tv;
    struct tm now;
    int32_t last_sec = 0;
    int32_t last_min = 0;
    int32_t last_hour = 0;
    table_t * ptable;
    int i;

    // init tables
    alan_init_table();
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &now);
    last_sec = now.tm_sec;
    last_min = now.tm_min;
    last_hour = now.tm_hour;

    // main thread processing
    for (;;) {
	sleep(update_timer);	// sleep for 1 seconds before doing anything.
	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &now);

	pthread_mutex_lock (&al_analyzer_lock);

	/*
	 * merge last min_table into hour_table
	 */
	if (now.tm_hour != last_hour) {

		printf("hour now=%d last=%d\n", now.tm_hour, last_hour);
		ptable = &hour_table[now.tm_hour];
		reset_table(ptable);
		for(i=0; i<TOT_MINUTES; i++) {
			merge_tables(ptable, &min_table[i]);
		}
		prune_tables(ptable);

		last_hour = now.tm_hour;
	}

	/*
	 * merge last sec_table into min_table
	 */
	if (now.tm_min != last_min) {

		printf("min now=%d last=%d\n", now.tm_min, last_min);
		ptable = &min_table[now.tm_min];
		reset_table(ptable);
		for(i=0; i<TOT_SECONDS; i++) {
			merge_tables(ptable, &sec_table[i]);
			/*
			 * We should reset all second-level table now.
			 */
			reset_table(&sec_table[i]);
		}
		prune_tables(ptable);

		last_min = now.tm_min;
	}

	/*
	 * drop current second table
	 */
	if (now.tm_sec != last_sec) {
		//printf("====>sec now=%d last=%d\n", now.tm_sec, last_sec);
		ptable = &sec_table[now.tm_sec];
		//reset_table(ptable);
		last_sec = now.tm_sec;
	}
	pthread_mutex_unlock (&al_analyzer_lock);
    }
    return NULL;
}


//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
/*
 * API to process the request
 */
static int alan_get_respcode(data_request_t * dr)
{
	int i, j; 
	int totlen;
	struct timeval tv;
	struct tm now;
	unsigned int ret; 
	status_code_t resp_code;
	table_t * ptable;

	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &now);
	memset((void *)&resp_code, 0, sizeof(status_code_t));

	/*
	 * when dr->thour == 0: we pick up from min_table.
	 * when dr->thour != 0: we pick up from hour_table only.
	 */
	if(dr->thour == 0) {
	   for(i=0; i<TOT_SECONDS; i++) {
		ptable = &sec_table[i];
		resp_code.hits_200 += ptable->resp_code.hits_200;
		resp_code.hits_206 += ptable->resp_code.hits_206;
		resp_code.hits_302 += ptable->resp_code.hits_302;
		resp_code.hits_304 += ptable->resp_code.hits_304;
		resp_code.hits_404 += ptable->resp_code.hits_404;
		resp_code.hits_500 += ptable->resp_code.hits_500;
		resp_code.hits_other += ptable->resp_code.hits_other;
	   }

	   for(i=0; i<dr->tmin; i++) {
		ptable = &min_table[(60 + now.tm_min - i)%60];
		resp_code.hits_200 += ptable->resp_code.hits_200;
		resp_code.hits_206 += ptable->resp_code.hits_206;
		resp_code.hits_302 += ptable->resp_code.hits_302;
		resp_code.hits_304 += ptable->resp_code.hits_304;
		resp_code.hits_404 += ptable->resp_code.hits_404;
		resp_code.hits_500 += ptable->resp_code.hits_500;
		resp_code.hits_other += ptable->resp_code.hits_other;
	   }
	}
	else {
	   for(i=0; i<dr->thour; i++) {
		ptable = &hour_table[(24 + now.tm_hour - i)%24];
		resp_code.hits_200 += ptable->resp_code.hits_200;
		resp_code.hits_206 += ptable->resp_code.hits_206;
		resp_code.hits_302 += ptable->resp_code.hits_302;
		resp_code.hits_304 += ptable->resp_code.hits_304;
		resp_code.hits_404 += ptable->resp_code.hits_404;
		resp_code.hits_500 += ptable->resp_code.hits_500;
		resp_code.hits_other += ptable->resp_code.hits_other;
	   }
	}

	/*
	 * return response.
	 */
	totlen = STATUS_CODE_GRP * 50 * sizeof(char *);
        dr->argc = STATUS_CODE_GRP;
       	dr->resp = (char *)malloc(totlen);

	dr->resp_len = snprintf(dr->resp, totlen, 
				"200 %d\n"
				"206 %d\n"
				"302 %d\n"
				"304 %d\n"
				"404 %d\n"
				"500 %d\n"
				"other %d\n",
				resp_code.hits_200,
				resp_code.hits_206,
				resp_code.hits_302,
				resp_code.hits_304,
				resp_code.hits_404,
				resp_code.hits_500,
				resp_code.hits_other);
	return 0;
}

static int alan_get_filesize(data_request_t * dr)
{
	int i, j; 
	int totlen;
	char * p;
	unsigned int ret; 
        struct timeval tv;
        struct tm now;
	uint32_t hits_filesize[TOT_FILESIZE_GRP];
	table_t * ptable;

	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &now);
	for(j=0; j<TOT_FILESIZE_GRP; j++) {
		hits_filesize[j] = 0;
	}

	/*
	 * when dr->thour == 0: we pick up from min_table.
	 * when dr->thour != 0: we pick up from hour_table only.
	 */
	if(dr->thour == 0) {
	    for(i=0; i<TOT_SECONDS; i++) {
		ptable = &sec_table[i];
	    	for(j=0; j<TOT_FILESIZE_GRP; j++) {
			hits_filesize[j] += ptable->hits_filesize[j];
		}
	    }

	    for(i=0; i<dr->tmin; i++) {
		ptable = &min_table[(60 + now.tm_min - i)%60];
		for(j=0; j<TOT_FILESIZE_GRP; j++) {
			hits_filesize[j] += ptable->hits_filesize[j];
		}
	    }
	}
	else {
	    for(i=0; i<dr->thour; i++) {
		ptable = &hour_table[(24 + now.tm_hour - i)%24];
		for(j=0; j<TOT_FILESIZE_GRP; j++) {
			hits_filesize[j] += ptable->hits_filesize[j];
		}
	    }
	}

	/*
	 * return response.
	 */
	totlen = TOT_FILESIZE_GRP * 100 * sizeof(char *);
        dr->argc = TOT_FILESIZE_GRP;
       	dr->resp = (char *)malloc(totlen);

	dr->resp_len = 0;
	p = dr->resp;
	for(j=0; j<TOT_FILESIZE_GRP; j++) {
		ret = snprintf(p, totlen,  "%s %d\n", 
			size_dist_grpss[j].prompt,
			hits_filesize[j]);
		dr->resp_len += ret;
		p += ret;
		totlen -= ret;
	}
        
	return 0;
}

static int alan_get_domain(data_request_t * dr)
{
	struct timeval tv;
	struct tm now;
	int i, j;
	int totlen;
	char * p;
	int ret;
	int depth;
	table_t final_table;
	table_t * ptable;

	/* Sort first, get max_disp_domains entries */ 
	for(i=0; i<TOP_DOMAINS_NUM; i++) {
		top_domains[i] = NULL;
	}
	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &now);
	memset((char *)&final_table, 0, sizeof(table_t));
	final_table.p_domain_ht = (NHashTable *)nhash_table_new(n_str_hash, n_str_equal, 0);

	/*
	 * when dr->thour == 0: we pick up from min_table.
	 * when dr->thour != 0: we pick up from hour_table only.
	 */
	if(dr->thour == 0) {
		for(i=0; i<TOT_SECONDS; i++) {
			merge_tables(&final_table, &sec_table[i]);
		}

		for(i=0; i<dr->tmin; i++) {
			ptable = &min_table[(60 + now.tm_min - i)%60];
			merge_tables(&final_table, ptable);
		}
	}
	else {
		for(i=0; i<dr->thour; i++) {
			ptable = &hour_table[(24 + now.tm_hour - i)%24];
			merge_tables(&final_table, ptable);
		}
	}

	/*
	 * return response.
	 */
	nhash_foreach(final_table.p_domain_ht, sort_top_domains, NULL);

	for(i=0; i<TOP_DOMAINS_NUM; i++) {
		if(top_domains[i] == NULL) break; // Only so many domains there.
	}
	depth = dr->depth > i ? i : dr->depth;

	totlen = depth * 100 * sizeof(char *);
	dr->argc = depth;
	dr->resp = (char *)malloc(totlen);

	dr->resp_len = 0;
	p = dr->resp;
	for(j=0; j<depth; j++) {
		ret = snprintf(p, totlen, "%s %d %.3f\n",
			top_domains[j]->domain, top_domains[j]->hits, 
			(top_domains[j]->cache_bytes + 
			 top_domains[j]->origin_bytes + 
			 top_domains[j]->unknown_bytes) );
		dr->resp_len += ret;
		p += ret;
		totlen -= ret;
	}

	reset_table(&final_table);
	nhash_destroy(final_table.p_domain_ht);

	return 0;
}

static int alan_get_uri(data_request_t * dr)
{
	struct timeval tv;
	struct tm now;
	int i, j;
	int totlen;
	char * p;
	int ret;
	int depth;
	table_t final_table;
	table_t * ptable;
	domain_list_t   * p_domain;
	uint64_t keyhash;

	if(dr->domain == NULL) {
		return 0;
	}

	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &now);
	for(i=0; i<TOP_DOMAINS_NUM; i++) {
		top_domains[i] = NULL;
	}
	for(i=0; i<TOP_URIS_NUM; i++) {
		top_uris[i] = NULL;
	}
	memset((char *)&final_table, 0, sizeof(table_t));
	final_table.p_domain_ht = (NHashTable *)nhash_table_new(n_str_hash, n_str_equal, 0);

	/*
	 * when dr->thour == 0: we pick up from min_table.
	 * when dr->thour != 0: we pick up from hour_table only.
	 */
	if(dr->thour == 0) {
		for(i=0; i<TOT_SECONDS; i++) {
			ptable = &sec_table[i];
			keyhash = (uint64_t)n_str_hash(dr->domain);
			p_domain = (domain_list_t *)nhash_lookup(
					ptable->p_domain_ht, keyhash, dr->domain);
			if(p_domain) {
				merge_tables(&final_table, ptable);
			}
		}

		for(i=0; i<dr->tmin; i++) {
			ptable = &min_table[(60 + now.tm_min - i)%60];
			keyhash = (uint64_t)n_str_hash(dr->domain);
			p_domain = (domain_list_t *)nhash_lookup(
					ptable->p_domain_ht, keyhash, dr->domain);
			if(p_domain) {
				merge_tables(&final_table, ptable);
			}
		}
	}
	else {
		for(i=0; i<dr->thour; i++) {
			ptable = &hour_table[(24 + now.tm_hour - i)%24];
			keyhash = (uint64_t)n_str_hash(dr->domain);
			p_domain = (domain_list_t *)nhash_lookup(
					ptable->p_domain_ht, keyhash, dr->domain);
			if(p_domain) {
				merge_tables(&final_table, ptable);
			}
		}
	}

	/*
	 * return response.
	 */
	keyhash = (uint64_t)n_str_hash(dr->domain);
	p_domain = (domain_list_t *)nhash_lookup(
			final_table.p_domain_ht, keyhash, dr->domain);
	if(p_domain) {
		nhash_foreach(p_domain->p_uri_ht, sort_top_uris, NULL);
		for(i=0; i<TOP_URIS_NUM; i++) {
			if(top_uris[i] == NULL) break; // Only so many domains there.
		}
		depth = dr->depth > i ? i : dr->depth;

		totlen = depth * 100 * sizeof(char *);
		dr->argc = depth; 
		dr->resp = (char *)malloc(totlen);

		dr->resp_len = 0;
		p = dr->resp;
		for(j=0; j<depth; j++) {
			ret = snprintf(p, totlen, "%s %d\n", 
				top_uris[j]->uri, top_uris[j]->hits);
			dr->resp_len += ret;
			p += ret;
			totlen -= ret;
		}
	}
	reset_table(&final_table);
	nhash_destroy(final_table.p_domain_ht);
        
	return 0;
}


int alan_process_data_request(data_request_t * dr)
{
    pthread_mutex_lock (&al_analyzer_lock);
    switch (dr->type) {
    case dt_respcode: alan_get_respcode(dr); break;
    case dt_filesize: alan_get_filesize(dr); break;
    case dt_domain: alan_get_domain(dr); break;
    case dt_uri: alan_get_uri(dr); break;
    default: break;
    }
    pthread_mutex_unlock (&al_analyzer_lock);
    return 0;
}





/*-----------------------------------------------------------------------------
 * GDB Helpers
 * unit test cases: dbug functions 
 */
#ifdef AL_DBG_ENABLE

void gdb_insert_data(void);
void gdb_insert_data(void)
{
    analysis_data_t a_data;
    const char * host[5] = {"www.google.com",
		      "www.yahoo.com",
		      "www.baidu.com",
		      "www.cnn.com",
		      "www.123.com"};
    const char * uri[10] = {  "/aaa1.html",
			"/bbb1.html",
			"/aaa2.html",
			"/bbb2.html",
			"/aaa3.html",
			"/bbb3.html",
			"/aaa4.html",
			"/bbb4.html",
			"/aaa5.html",
			"/bbb5.html" };
    int i, j;
    for(i = 0; i< 5; i++) {
	for(j=0; j<10; j++) {
		a_data.hostname = (char *)host[i];
		a_data.hostlen = strlen(host[i]);
		a_data.uri = (char *)uri[j];
		a_data.urilen = strlen(uri[j]);
		a_data.bytes_out = i*j*1024;
		a_data.errcode = 200;
    		alan_update_data(&a_data);
	}
	printf("--\n");
   }
}


void gdb_print_domain(void);
void gdb_print_domain(void)
{
	data_request_t dr;
	int i;

	dr.type = dt_domain;
	dr.thour = 0;
	dr.tmin = 10;
	dr.depth = 5;

	alan_process_data_request(&dr);

	for(i=0; i<dr.argc; i++) {
		printf("%s", dr.argv[i]);
	}
	free(dr.resp);
}

void gdb_print_uri(void);
void gdb_print_uri(void)
{
	data_request_t dr;
	int i;

	dr.type = dt_uri;
	dr.thour = 0;
	dr.tmin = 10;
	dr.depth = 5;
	dr.domain = (char *)"www.google.com";

	alan_process_data_request(&dr);

	for(i=0; i<dr.argc; i++) {
		printf("%s", dr.argv[i]);
	}
	free(dr.resp);
}

void gdb_print_filesize(void);
void gdb_print_filesize(void)
{
	data_request_t dr;
	int i;

	dr.type = dt_filesize;
	dr.thour = 0;
	dr.tmin = 10;
	dr.depth = 5;

	alan_process_data_request(&dr);

	for(i=0; i<dr.argc; i++) {
		printf("%s", dr.argv[i]);
	}
	free(dr.resp);
}

void gdb_print_resp_code(void);
void gdb_print_resp_code(void)
{
	data_request_t dr;
	int i;

	dr.type = dt_respcode;
	dr.thour = 0;
	dr.tmin = 10;
	dr.depth = 5;

	alan_process_data_request(&dr);

	for(i=0; i<dr.argc; i++) {
		printf("%s", dr.argv[i]);
	}
	free(dr.resp);
}

#endif  // AL_DBG_ENABLE
