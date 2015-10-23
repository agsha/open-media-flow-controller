#ifndef HASH_H
#define HASH_H

#define MAX_BUCKET_NUM  13
#define MAX_THRDS 1

extern GHashTable *g_pop_domain_hash_table[MAX_THRDS];
extern GHashTable *g_domain_hash_table[MAX_THRDS];
extern GHashTable *g_uri_hash_table[MAX_THRDS];
extern GHashTable *g_master_domain_hash_table;
extern GHashTable *g_master_uri_hash_table;
extern GHashTable *g_pop_domains;

/*
 * This structure stores the domain-wise hits and bytes served for 
 * various tunnel reason code.
 */
typedef struct dw_tun_stats {
    int32_t cntr;
    long long bw;
}dw_tun_t;

/*
 * This structure the hits, bytes for size-wise buckets for each domain.
 */
typedef struct CE_CHR {
    long long tot_bytes_served;
    long long bytes_served_sas;
    long long tot_content_length;
    long long bytes_served_buffer;
    long long bytes_served_ssd;
    long long bytes_served_sata;
    long long bytes_served_origin;
    long long bytes_served_tunnel;
    /*long long bytes_served_unknown;
    long long bytes_served_cacheable;
    long long bytes_served_noncacheable;
    */
    long long bw_saved;
    long long cache_size_req;
    float CE;
    int32_t tot_hit_cnt;
    int32_t hits_sas;
    int32_t hits_buffer;
    int32_t hits_ssd;
    int32_t hits_sata;
    int32_t hits_origin;
    int32_t hits_tunnel;
    /*int32_t hits_unknown;
    int32_t hits_cacheable;
    int32_t hits_noncacheable;
    */
}CE_CHR_t;

/*
 * This structure holds the domain-wise hits and bytes stats for verious 
 * providers.
 */
typedef struct domain_hash_val {
    float CHR;
    struct srcip *ip_list;
    CE_CHR_t ce_chr[MAX_BUCKET_NUM+1];

    struct dw_tun_stats dw_tun[58];
    long long tot_bytes_served;
    long long tot_content_length;
    long long bytes_served_sas;
    long long bytes_served_buffer;
    long long bytes_served_ssd;
    long long bytes_served_sata;
    long long bytes_served_origin;
    long long bytes_served_tunnel;
    /*long long bytes_served_unknown;
    long long bytes_served_cacheable;
    long long bytes_served_noncacheable;
    */

    /* 
     * To capture duplicate bandwidth stats. 
     * Dup bw is contributed by Origin-origin or cache-hit-origin 
     * access patterns
     */
    long long duplicate_bw;
    int32_t tot_hit_cnt;
    int32_t hits_sas;
    int32_t hits_buffer;
    int32_t hits_ssd;
    int32_t hits_sata;
    int32_t hits_origin;
    int32_t hits_tunnel;
    /*int32_t hits_unknown;
    int32_t hits_cacheable;
    int32_t hits_noncacheable;
    */
    int32_t cur_dup_uri;
    int32_t *dup_bw_uri_index_list;
} domain_hash_val_t;

/*
 * This structure holds the bytes served for various domains.
 * popular domains are determined based on this counter.
 */
typedef struct pop_domain_hash_val{
    long long bytes_served;
} pop_domain_hash_val_t;

typedef struct byte_range {
    uint64_t low_range;
    uint64_t high_range;
    struct byte_range *next;
} byte_range_t;
/*
 * This structure holds the bytes, hits stats for various
 * providers (buffer,sas etc)on a per uri basis.
 */
typedef struct uri_hash_val {
    int32_t tot_hit_cnt;
    int32_t hits_sas;
    int32_t hits_buffer;
    int32_t hits_ssd;
    int32_t hits_sata;
    int32_t hits_origin;
    int32_t hits_tunnel;
    int32_t hits_unknown;
    /*
    int32_t hits_cacheable;
    int32_t hits_noncacheable;
    */
    long long tot_bytes_served;
    long long tot_content_length;
    long long bytes_served_sas;
    long long bytes_served_buffer;
    long long bytes_served_ssd;
    long long bytes_served_sata;
    long long bytes_served_origin;
    long long bytes_served_tunnel;
    long long bytes_served_unknown;
    /*
    long long bytes_served_cacheable;
    long long bytes_served_noncacheable;
    */
    //long long duplicate_bw;
    int32_t prev_origin;
    int32_t uri_list_index; /* to know where the uri is prsent in g_uri_list*/
    char *domain_name; /*Point32_t to which domain this uri belong to*/

    byte_range_t *byte_range_head;
} uri_hash_val_t;

struct srcip {
    char *ip;
    struct srcip *next;
};
#endif

