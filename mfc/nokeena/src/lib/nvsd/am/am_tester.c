#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <assert.h>
#include <string.h>
#include "nkn_am_api.h"
#include "nkn_am.h"

int p_id_disk = 5;
int p_id_ssd = 3;
int p_id_mm = 9;
char p_name_disk[] = "disk\0";
char p_name_ssd[] = "ssd\0";
char p_name_mm[] = "mm\0";


static AM_pk_t *
create_pk(char *name, uint32_t provider_id)
{
    AM_pk_t *pk;

    pk = (AM_pk_t *) nkn_calloc_type (1, sizeof(AM_pk_t), mod_am_test);
    memcpy(pk->name, name, strlen(name));
    pk->provider_id = provider_id;
    return pk;
}

static void
test_AM_inc_num_hits(AM_pk_t *pk, char *name, uint32_t provider_id,
		     int num_hits)
{
    if(pk == NULL)
	pk = create_pk(name, provider_id);

    assert(pk);

    AM_inc_num_hits(pk, num_hits, 0, NULL, NULL);
}

static void
test_AM_update_queue_depth(AM_pk_t *pk,
			   uint32_t value)
{
    if(pk == NULL)
	pk = create_pk(pk->name, pk->provider_id);
    assert(pk);

    AM_update_queue_depth(pk, value);
}

static void
test_AM_update_media_read_latency(AM_pk_t *pk,
				  uint32_t value)
{
    if(pk == NULL)
	pk = create_pk(pk->name, pk->provider_id);
    assert(pk);

    AM_update_media_read_latency(pk, value);
}

static void
test_AM_decay(void) {
    nkn_hot_t hotness;
    hotness = 0;

    am_encode_update_time(&hotness);
    sleep(10);
    AM_decay_hotness(&hotness);

}

static void
test_AM_update_policy(AM_pk_t *pk, AM_trig_pol_t *pol)
{
    if(pk == NULL)
	pk = create_pk(pk->name, pk->provider_id);
    assert(pk);

    AM_update_policy(pk, pol);
}

int
am_tester(void)
{
    AM_pk_t pk1;
    AM_pk_t pk2;
    AM_pk_t pk3;
    AM_trig_pol_t pol;
    AM_trig_pol_t pol1;
    AM_obj_t *pobj=NULL, tmpobj;
    char n1[] = "spider-1.flv\0";
    char n2[] = "spider-2.flv\0";
    char n3[] = "spider-3.flv\0";
    int ret;

    test_AM_decay();
    return 0;


    //g_thread_init(NULL);
    //AM_tbl_init();

    pk1.provider_id = 5;
    pk1.sub_provider_id = -2;
    pk1.type = AM_OBJ_URI;
    pk1.name = (char *) nkn_malloc_type (strlen(n1) + 1, mod_am_test);
    memcpy(pk1.name, n1, strlen(n1));
    pk2.provider_id = 5;
    pk2.sub_provider_id = -2;
    pk2.type = AM_OBJ_URI;
    pk2.name = (char *) nkn_malloc_type (strlen(n2) + 1, mod_am_test);
    memcpy(pk2.name, n2, strlen(n2));
    pk3.provider_id = 5;
    pk3.sub_provider_id = 1;
    pk3.type = AM_OBJ_URI;
    pk3.name = (char *) nkn_malloc_type (strlen(n3) + 1, mod_am_test);
    memcpy(pk3.name, n3, strlen(n3));

    pol.alert_bitmap = 0;
    pol.change_bitmap = 0;
    pol1.alert_bitmap = 0;
    pol1.change_bitmap = 0;

    pol.alert_func = NULL;
    pol1.alert_func = NULL;
    NKN_AM_BIT_SET(pol.alert_bitmap, AM_CHANGE_ALERT_FUNC);
    NKN_AM_BIT_SET(pol.change_bitmap, AM_CHANGE_ALERT_FUNC);
    NKN_AM_BIT_SET(pol1.alert_bitmap, AM_CHANGE_ALERT_FUNC);
    NKN_AM_BIT_SET(pol1.change_bitmap, AM_CHANGE_ALERT_FUNC);

    pol.cache_latency_lo = 201;
    NKN_AM_BIT_SET(pol.change_bitmap, AM_CHANGE_CACHE_LATENCY_LO);
    NKN_AM_BIT_SET(pol.alert_bitmap, AM_CHANGE_CACHE_LATENCY_LO);
    NKN_AM_BIT_SET(pol.change_bitmap, AM_CHANGE_ALERT_BITMAP);

    pol1.cache_q_depth_hi = 201;
    NKN_AM_BIT_SET(pol1.change_bitmap, AM_CHANGE_CACHE_Q_DEPTH_HI);
    NKN_AM_BIT_SET(pol1.alert_bitmap, AM_CHANGE_CACHE_Q_DEPTH_HI);
    NKN_AM_BIT_SET(pol1.change_bitmap, AM_CHANGE_ALERT_BITMAP);

    test_AM_update_policy(&pk1, &pol);
    test_AM_update_policy(&pk2, &pol1);
    test_AM_update_media_read_latency(&pk1, 200);
    test_AM_update_queue_depth(&pk2, 202);

    test_AM_inc_num_hits(&pk1, pk1.name, 1, 6);
    test_AM_inc_num_hits(&pk1, pk1.name, 1, 5);
    test_AM_inc_num_hits(&pk1, pk1.name, 1, 5);
    test_AM_inc_num_hits(&pk2, pk2.name, 1, 100);
    test_AM_inc_num_hits(&pk2, pk2.name, 1, 200);
    test_AM_inc_num_hits(&pk3, pk3.name, 1, 1);
    test_AM_inc_num_hits(&pk3, pk3.name, 1, 1);
    test_AM_inc_num_hits(&pk3, pk3.name, 1, 1);
    test_AM_inc_num_hits(&pk2, pk2.name, 1, 1);
    test_AM_inc_num_hits(&pk2, pk2.name, 1, 1);
    test_AM_inc_num_hits(&pk1, pk1.name, 1, 600);
    test_AM_inc_num_hits(&pk1, pk1.name, 1, 500);

    AM_get_next_evict_candidate_by_provider(5, 1, pobj);
    AM_get_next_evict_candidate_by_provider(5, 0, pobj);
    AM_get_next_evict_candidate_by_provider(5, -1, pobj);
    AM_get_next_evict_candidate_by_provider(5, -2, pobj);
    //AM_get_hottest_candidate_by_provider(5, 1);

	//AM_tbl_print();
    AM_get_next_evict_candidate_by_provider(5, 1, pobj);
	AM_get_next_evict_candidate_by_provider(5, 1, pobj);
	//AM_tbl_print();

    ret = AM_get_obj(&pk2, &tmpobj);
    if(ret == 0) {
	printf("\n %s: Object found success", "dummy");
    } else {
	printf("\n %s: Object found failure", "dummy");
    }

    return 0;
}
