#ifndef _VIDEO_ALIGNER_H_
#define _VIDEO_ALIGNER_H_



#ifdef __cplusplus
extern "C" {
#endif


#include <stdio.h>
#include "nkn_memalloc.h"
#include "mfp_video_header.h"
#include "nkn_vpe_h264_parser.h"
#include "nkn_debug.h"

//class enums

#define DEF_ACCESS_UNIT_DELIMITER_SIZE (6)
#define MAX_CODEC_INFO_PKT 3

typedef enum frame_parse_state{
    frame_default_state = 0,
    frame_parse_partial = 1,
    frame_state_complete = 2
}frame_parse_state_e;

typedef enum aligner_retcode{
   aligner_parsepkts_fail = -6,
   aligner_ctxt_memalloc_fail = -5,
   aligner_framemem_free_fail = -4,
   aligner_pktmem_free_fail = -3,
   aligner_frame_memalloc_fail = -2,
   aligner_pkt_memalloc_fail = -1,
   aligner_success = 0,
}aligner_retcode_e;

//class data structure


struct ts_pkt_info;

typedef struct ts_pkt_info{
    uint32_t pkt_num;
    nkn_vpe_vector_t vector;/*vectorized output*/
    uint8_t *data; 	/*start address of the buffer*/
    uint32_t offset;	/*buffer offset for the current ts packet*/
    uint32_t psi_flag;  /*payload start indicator flag*/
    uint32_t dts;	/*decoding time stamp in 32bit precision at ts pkt*/
    uint32_t pts;	/*presentation time stamp in 32bit precision at ts pkt*/
    uint32_t header_len;/*length of TS and PES headers*/
    uint32_t slice_start;/*start offset of given sub frame in current packet */
    uint32_t slice_len;  /*length of sub frame in current TS packet*/
    uint32_t continuity_counter;
    struct ts_pkt_info *prev;
    struct ts_pkt_info *next;
  uint32_t is_slice_start;
  uint8_t *SEI_offset[MAX_SEI];/* SEI offset's within one frame */
  uint32_t SEI_length[MAX_SEI];/* SEI length */
  uint32_t num_SEI;/*num of seI within one frame occuring before SPS/PPS*/
}ts_pkt_info_t;

struct video_frame_info;

typedef struct video_frame_info{
    uint32_t frame_num;
    slice_type_e slice_type;	/*slice type of the given frame*/	
    uint32_t frame_len; 	/*video frame length*/
    frame_parse_state_e fr_state;/*frame parse status*/
    uint32_t dts;		/*decoding time stamp in 32bit precision*/
    uint32_t pts;		/*presentation time stamp in 32bit precision*/
    ts_pkt_info_t *first_pkt;	/*list of packets that comprises given frame*/
    ts_pkt_info_t *last_pkt;
    uint8_t *codec_info_start_offset[MAX_CODEC_INFO_PKT];
    uint32_t codec_info_len[MAX_CODEC_INFO_PKT];  
  uint32_t num_codec_info_pkt;
  uint8_t *SEI_offset[MAX_SEI];/* SEI offset's within one frame */
  uint32_t SEI_length[MAX_SEI];/* SEI length */
  uint32_t num_SEI;/*num of seI within one frame occuring before SPS/PPS*/
    uint32_t discontinuity_indicator;
    struct video_frame_info *prev;
    struct video_frame_info *next;
}video_frame_info_t;


struct aligner_ctxt;

typedef aligner_retcode_e (*create_init_frame_mem_fn)(
	video_frame_info_t **cur_frame);

typedef aligner_retcode_e (*create_frame_mem_fn)(
	video_frame_info_t *cur_frame);

typedef aligner_retcode_e (*create_init_pkt_mem_fn)(
	ts_pkt_info_t **cur_pkt);

typedef aligner_retcode_e (*create_pkt_mem_fn)(
	ts_pkt_info_t *cur_pkt);



typedef aligner_retcode_e (*delete_frame_mem_fn)(
	struct aligner_ctxt *aligner_ctxt,video_frame_info_t *cur_frame);
typedef aligner_retcode_e (*delete_pkt_mem_fn)(
	ts_pkt_info_t *cur_pkt);
typedef aligner_retcode_e (*delete_aligner_ctxt_fn)(
	struct aligner_ctxt *aligner_ctxt);

typedef aligner_retcode_e (*process_video_pkts_fn)(
	struct aligner_ctxt *aligner_ctxt,
	uint8_t *data,
	uint32_t *mstr_pkts,
	uint32_t n_mstr_pkts,
	spts_st_ctx_t *spts,
	video_frame_info_t **video_frame_tbl,	
	uint32_t *num_frames
	);

typedef aligner_retcode_e (*print_frame_info_fn)(
	struct aligner_ctxt *aligner_ctxt,
	video_frame_info_t *video_frame_tbl,
	FILE *fp,
	accum_ts_t *accum);


typedef struct aligner_ctxt{

    //create frame, pkt mem function pointers
    create_init_frame_mem_fn create_first_frame;
    create_frame_mem_fn	create_frame;
    create_init_pkt_mem_fn create_first_pkt;
    create_pkt_mem_fn create_pkt;

    //aligner processing function
    process_video_pkts_fn process_video_pkts;

    //debug log support
    print_frame_info_fn print_frameinfo;
    
    //ts packet store across chunks
    uint8_t *pkt_store;
    uint32_t tot_pkt_size;
    uint32_t num_pkts_stored;

    //video frame info struct
    video_frame_info_t *video_frame;

    //delete frame, pkt mem and ctxt function pointers
    delete_frame_mem_fn delete_frame;
    delete_pkt_mem_fn delete_pkt;
    delete_aligner_ctxt_fn del;
    
}aligner_ctxt_t;




//class function definition

aligner_retcode_e create_aligner_ctxt(
	aligner_ctxt_t **aligner_ctxt);



#ifdef __cplusplus
}
#endif


#endif

