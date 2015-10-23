/**
 * @file   de_rr.c
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Tue May  1 23:18:21 2012
 * 
 * @brief  implements round robin decision
 * 
 * 
 */
#include "de_intf.h"
#include "nkn_geodb.h"
#include "cr_common_intf.h" 

/* static  */
static int32_t de_geo_lf_init(void **state);

static int32_t de_geo_lf_decide(void *state, de_input_t *di,
	     uint8_t **result, uint32_t *result_len);

static int32_t de_geo_lf_cleanup(void *state);

static int32_t get_client_location( de_input_t *di, conjugate_graticule_t* location);

//extern 
extern obj_store_t *store_list[];

#define EARTH_CIRCUMFERENCE 25000

/* global */
de_intf_t gl_obj_de_geo_lf = {
    .init = de_geo_lf_init,
    .decide = de_geo_lf_decide,
    .cleanup = de_geo_lf_cleanup
};

typedef struct tag_de_geo_lf {
    uint8_t last_idx;
}de_geo_lf_t;
    
static int32_t de_geo_lf_init(void **state){
   
    de_geo_lf_t *ctx = NULL;
    int32_t egeo_lf = 0;
    
    if (!state) {
	egeo_lf = -EINVAL;
	goto egeo_lfor;
    }

    ctx = (de_geo_lf_t *)nkn_calloc_type(1, sizeof(de_geo_lf_t), 103);
    if (!ctx) {
	egeo_lf = -ENOMEM;
	goto egeo_lfor;
    }
    
    *state = ctx;
    return egeo_lf;

 egeo_lfor:
    if (ctx) de_geo_lf_cleanup(ctx);
    return egeo_lf;
}



static int32_t get_client_location( de_input_t *di, conjugate_graticule_t* location){

    int32_t err;
    const char *key = di->resolv_addr;
    char resp[1024];
    uint32_t resp_len = 1024;
    geo_ip_t* geo;
    obj_store_t *h_geodb;  
    h_geodb = store_list[CRST_STORE_TYPE_GEO];
    err =  h_geodb->read(h_geodb, di->resolv_addr,
			 strlen(di->resolv_addr),
			 resp,
			 &resp_len);

    if(!err){
	geo = (geo_ip_t*)(resp);
	location->latitude = geo->ginf.latitude;
	location->longitude = geo->ginf.longitude;
    }
    return err;
}


static int32_t de_geo_lf_decide(void *state, 
				de_input_t *di,
				uint8_t **result, 
				uint32_t *result_len){

    int32_t egeo_lf = -1, ind=0;
    double score, score_min=-1, distance;
    double *ce_distance, dis_max=-1, load_max=-1;    
    de_cache_attr_t *ce = NULL;
    conjugate_graticule_t client_location;
    rrecord_msg_fmt_builder_t *rrb = NULL;
    int32_t err = -1;
    uint32_t i;
    double w1 = 0.5, w2 = 0.5;


    ce_distance = (double*)nkn_calloc_type(1,sizeof(double)*di->ce_count, 0);
    get_client_location(di, &client_location);
    for(i=0;i<di->ce_count;i++){
        ce = &di->ce_attr[i];
	if(ce->stats.status == LF_UNREACHABLE ||
           ce->stats.status == CACHE_DOWN ||
           ce->stats.cpu_load > ce->load_watermark){
	    ce_distance[i] = -1;
            continue;
	}
	err= 0 ;

        ce_distance[i] = compute_geo_distance(&ce->loc_attr,&client_location);
	if(dis_max<ce_distance[i])
	    dis_max = ce_distance[i];
	if(load_max<ce->stats.cpu_load)
	    load_max = ce->stats.cpu_load;
    }

    if(err<0)
	goto error;

    /*Obtain the best index*/
    for(i=0;i<di->ce_count;i++){
	ce = &di->ce_attr[i];
	/*Check whether to process or skip this one*/
	if(ce->stats.status == LF_UNREACHABLE ||
	   ce->stats.status == CACHE_DOWN ||
	   ce->stats.cpu_load > ce->load_watermark||
	   ce_distance[i] == -1)
	    continue;

	err = 0;
	if(!load_max) load_max=1;
	score = (w1*(ce_distance[i]/dis_max)) + (w2*(ce->stats.cpu_load/load_max));
	if(score_min<0){
	    score_min = score;
	    ind = i;
	}
	if(i!=0&&score<score_min){
            score_min = score;
            ind = i;
	}
    }
    if(score_min < 0)
	goto error;
    /*Build the response*/
    if (!err) {
	de_cache_attr_t *ca =  &di->ce_attr[ind];
	uint32_t rlen = 0, j = 0, num_rr = 0;

	if (di->in_addr_type == ce_addr_type_max) {
	    uint32_t tot_len = 0;
	    num_rr = ca->num_addr;
	    for (j = 0; j < ca->num_addr; j++) {
		tot_len += rrecord_msg_fmt_builder_compute_record_size(
						       *ca->addr_len[j]);
	    }
	    rlen = tot_len;
		
	    err = rrecord_msg_fmt_builder_create(rlen, &rrb);
	    if (err) {
		goto error;
	    }

	    err = rrb->add_hdr(rrb, num_rr, di->ttl);
	    if (err) {
		goto error;
	    }
	    
	    for (j = 0; j < ca->num_addr; j++) {
		cache_entity_addr_type_t type = ca->addr_type[j];
		err = rrb->add_record(rrb, type, (uint8_t *)ca->addr[j], 
				      *ca->addr_len[j]);
		
		if (err) {
		    goto error;
		}
	    }
	    err = rrb->get_buf(rrb, result, result_len);
	    if (err) {
		goto error;
	    }
	    
	} else {
	    num_rr = 1;
	    for (j = 0; j < ca->num_addr; j++) {
		if (ca->addr_type[j] ==  di->in_addr_type) {
		    break;
		}
	    }
	    if (j == ca->num_addr) {
		err = -1;
		goto error;
	    }
	    
	    cache_entity_addr_type_t type = ca->addr_type[j];

	    rlen = rrecord_msg_fmt_builder_compute_record_size(
							       *ca->addr_len[j]);

	    err = rrecord_msg_fmt_builder_create(rlen, &rrb);
	    if (err) {
		goto error;
	    }
	    err = rrb->add_hdr(rrb, num_rr, di->ttl);
	    if (err) {
		goto error;
	    }
	    err = rrb->add_record(rrb, type, (uint8_t *)ca->addr[j], 
				  *ca->addr_len[j]);
			      
	    if (err) {
		goto error;
	    }
	
	    err = rrb->get_buf(rrb, result, result_len);
	    if (err) {
		goto error;
	    }
	}
    }

 error:
    if (rrb) rrb->delete(rrb);
    if(ce_distance)free(ce_distance);
    return err;

}






static int32_t 
de_geo_lf_cleanup(void *state)
{
    de_geo_lf_t *geo_lf = (de_geo_lf_t *)state;
    
    if (geo_lf) {
	free(geo_lf);
    }
    
    return 0;
}
