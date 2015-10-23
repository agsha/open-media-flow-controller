#include <stdint.h>
#include "nkn_vpe_mfu_defs.h"
#include "nkn_vpe_mfu_parse.h"
#include "nkn_vpe_mfp_ts2mfu.h"

#define MFP_SEG_MAX_MFUS 20
#define MFU_AGG_SUCCESS 1
#define MFU_AGG_WAIT 0

typedef struct mediadatapkts{
    uint8_t* pkts;
    uint32_t num_pkts;
}media_pkt_data_t;

typedef struct mfudemux{
    mfub_t mfub;
    mfu_contnr_t* mfuc ;
}mfu_demux_data_t;


typedef struct fmtrmfulist{
    uint32_t duration_acced; //Total duration of acced mfus
    uint32_t target_duration; //Target duration of segment to be produced
    uint32_t max_num_mfus;
    uint32_t num_mfus; //Number of mfus acced
    mfu_data_t *mfus; //Pointer to the MFUs acced.
    mfu_data_t *out_mfu; //Output MFU
    mfu_demux_data_t *mfud;
    ts2mfu_desc_t **ts2mfu_desc;
    char sess_name[500];
    uint32_t sess_id;
}fmtr_mfu_list_t;


//int32_t mfu_demux_mux_data(fmtr_mfu_list_t *fmt);
//void mfu_merge_ut(mfu_data_t *mfu);
void clear_fmtr_mfu_list(fmtr_mfu_list_t *fmt);


void* init_merge_mfu(uint32_t target_duration,
                       uint32_t max_mfus);


int32_t mfp_live_accum_mfus(mfu_data_t *,
			    fmtr_mfu_list_t *,
			    void* ,
			    uint32_t ,
			    mfu_data_t **);


int32_t clear_fmtr_vars( fmtr_mfu_list_t *);
void delete_merge_mfu(void*);

void init_ts2mfu_desc_ref(ts2mfu_desc_t **ptr,
                          uint32_t max_mfus);




void set_sessid_name(fmtr_mfu_list_t* fmt,
                     char* sess_name,
                     uint32_t sess_id);
