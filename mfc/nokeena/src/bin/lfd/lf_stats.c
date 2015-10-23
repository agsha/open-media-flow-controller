#include <stdio.h>
#include <math.h>

// nkn defs
#include "nkn_memalloc.h"

// lf defs
#include "lf_stats.h"
#include "lf_err.h"

static void ma_compute_f32(ma_t *ma, sample_t new_sample);
static void ma_compute_i64(ma_t *ma, sample_t new_sample);

int32_t
ma_create(uint32_t num_slots, uint8_t type, ma_t **out)
{
    int32_t err = 0;
    ma_t *ma;

    if (!num_slots) {
	err = -E_LF_INVAL;
	goto error;
    }
    ma = (ma_t *)nkn_calloc_type(1, sizeof(ma_t),
				 100);
    if (!ma) {
	err = -E_LF_NO_MEM;
	goto error;
    }
    
    err = ma_init(num_slots, type, ma);
    if (err)
	goto error;

    *out = ma;
    return err;

 error:
    ma_cleanup(ma);
    return err;    
  
}

void 
ma_cleanup(ma_t *ma)
{

    if (ma) {
	ma_deinit(ma);
	free(ma);
    }
}

int32_t
ma_init(uint32_t num_slots, uint8_t type, ma_t *ma)
{
    int32_t err = 0;

    ma->pivot = 0;
    ma->sum = 0;
    ma->type = type;
    ma->num_slots = num_slots;
    ma->slot_fill = 0;

    switch(ma->type) {
	case 0:
	    ma->sample.f32 = 
		nkn_calloc_type(num_slots, sizeof(float),
				100);
	    if (!ma->sample.f32) {
		err = -E_LF_NO_MEM;
		goto error;
	    }
	    ma->compute_ma = ma_compute_f32;
	    break;
	case 1:
	    ma->sample.i64 = 
		nkn_calloc_type(num_slots, sizeof(uint64_t),
				100);
	    if (!ma->sample.i64) {
		err = -E_LF_NO_MEM;
		goto error;
	    }
	    ma->compute_ma = ma_compute_i64;
	    break;
	case 2:
	    ma->sample.u32 = 
		nkn_calloc_type(num_slots, sizeof(uint32_t),
				100);
	    if (!ma->sample.u32) {
		err = -E_LF_NO_MEM;
		goto error;
	    }
	    break;
    }

 error:
    return err;
}

void 
ma_deinit(ma_t *ma)
{
    if (ma) {
	switch (ma->type) {
	    case 0:
		free(ma->sample.f32);
		break;
	    case 1:
		free(ma->sample.i64);
		break;
	    case 2:
		free(ma->sample.u32);
		break;
	}
    }
}

static void
ma_compute_f32(ma_t *ma, sample_t new_sample)
{
    if (ma->slot_fill < ma->num_slots) {
	ma->sample.f32[ma->pivot] = new_sample.f32;
	ma->sum += new_sample.f32;
	ma->prev_ma = ma->sum/(ma->pivot + 1);
	ma->pivot = (++ma->pivot)%ma->num_slots;
	ma->slot_fill++;
	return;
    }

    float pivot_sample = ma->sample.f32[ma->pivot];
    double prev_ma = ma->prev_ma;
    float ns = new_sample.f32;
    uint32_t div1 = ma->num_slots;
    float new_ma = ( ((ma->num_slots * prev_ma) - pivot_sample) + ns )
	/ div1;
    ma->prev_ma = new_ma;
    ma->sample.f32[ma->pivot] = new_sample.f32;
    ma->pivot = (++ma->pivot)%ma->num_slots;
   
    return;
}

static void
ma_compute_i64(ma_t *ma, sample_t new_sample)
{
    if (ma->slot_fill < ma->num_slots) {
	ma->sample.i64[ma->pivot] = new_sample.i64;
	ma->sum += new_sample.i64;
	ma->prev_ma = ma->sum/(ma->pivot + 1);
	ma->pivot = (ma->pivot + 1)%ma->num_slots;
	ma->slot_fill++;
	return;
    }

    int64_t pivot_sample = ma->sample.i64[ma->pivot];
    double prev_ma = ma->prev_ma;
    int64_t ns = new_sample.i64;
    uint32_t div1 = ma->num_slots;
    double new_ma = ( ((ma->num_slots * prev_ma) - pivot_sample) + ns )
	/ div1;
    ma->prev_ma = new_ma;

    /*
    printf("prev ma %ld, new sample %lu, idx %d, addr %p\n", 
    (uint64_t)(ma->prev_ma), ns, ma->pivot, ma);*/
	   
    if (ns < 0) {
	int uu = 0;
	uu++;
    }
    if ( (int64_t)(ma->prev_ma) == 0) {
	int uu = 0;
	uu++;
    }
    ma->dbg_pivot_sample.i64 = pivot_sample;
    ma->sample.i64[ma->pivot] = new_sample.i64;
    ma->pivot = (ma->pivot + 1)%ma->num_slots;

    return;
}

void
ma_move_window(ma_t *ma, sample_t new_sample)
{
    return ma->compute_ma(ma, new_sample);
}

#if 0
void
ma_pop_prev(ma_t *ma)
{
    if (ma->pivot == 0) {
	return (ma->num_slots - 1);
    }

    switch(ma->type) {
	case 0:
	    return ma->sample.f32[ma->pivot - 1];
	case 1:
	    return ma->sample.i64[ma->pivot - 1];
	case 2:
	    return ma->sample.u32[ma->pivot - 1];
    }
}
#endif
