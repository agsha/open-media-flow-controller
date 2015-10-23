#ifndef _LF_STATS_H_
#define _LF_STATS_H_

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <sys/types.h>

#include "lf_common_defs.h"

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* keeps emacs happy */
}
#endif

typedef union tag_sample_arr {
    int64_t *i64;
    uint32_t *u32;
    float *f32;
} sample_arr_t;

typedef union tag_sample {
    int64_t i64;
    uint32_t u32;
    float f32;
} sample_t;

struct tag_ma;
typedef void (*ma_compute_fnp)(struct tag_ma *, sample_t );
typedef struct tag_ma {
    uint8_t type;
    uint32_t num_slots;
    sample_arr_t sample;
    double prev_ma;
    uint32_t pivot;
    double sum;
    uint32_t slot_fill;

    sample_t dbg_pivot_sample;
    ma_compute_fnp compute_ma;
} ma_t;
 
int32_t ma_create(uint32_t num_slots, uint8_t type, ma_t **out);
void ma_cleanup(ma_t *ma);
int32_t ma_init(uint32_t num_slots, uint8_t type, ma_t *ma);
void ma_move_window(ma_t *ma, sample_t new_sample);
void ma_pop_prev(ma_t *ma);
void ma_deinit(ma_t *ma);

#ifdef __cplusplus
}
#endif

#endif //_LF_STATS_H_
