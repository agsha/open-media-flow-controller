#ifndef _MP4_UNHINTED_
#define _MP4_UNHINTED_

#include "nkn_vpe_rtp_packetizer.h"
#include "nkn_vpe_mp4_feature.h"

#pragma pack(push, 1)

#ifdef __cplusplus
extern "C"{
#endif

    typedef struct mp4parser{
	all_info_t* st_all_info;
	int32_t trak_type;
	//    size_t (*nkn_fread)( void *, size_t, size_t, FILE*);
	FILE* fid;
	FILE* prev_fid;
	int32_t *look_ahead_time;
	output_details_t *output;
    }parser_t;
#pragma pack(pop)

    typedef int32_t (*fnp_packet_ready_handler)(void *, void *);

    /*initialization and cleanup functions for the mp4 parser
     */
    parser_t *init_parser_state(all_info_t *inf, FILE *fin, int32_t track_type);
    Sample_rtp_info_t *init_rtp_sample_info(void);
    int32_t cleanup_rtp_sample_info(Sample_rtp_info_t *sample);
    //int32_t cleanup_mp4_hinted_parser(parser_t *parser, Sample_rtp_info_t *sample);
    //int32_t cleanup_parser_state(parser_t *parser);

    /* API's
     */
    int32_t send_next_pkt_unhinted(parser_t* parser, Sample_rtp_info_t *rtp_sample);
    int32_t packet_ready_handler(rtp_packet_builder_t *bldr, void *cb);
    int32_t get_hinted_sdp_info(parser_t *parser, Sample_rtp_info_t *sample, SDP_info_t *sdp_info);
    int32_t init_rtp_packet_builder(parser_t *parser, Sample_rtp_info_t *sample);
    void cleanup_rtp_packet_builder(parser_t *parser, Sample_rtp_info_t *sample);
    int32_t append_list_deterministic(Sample_rtp_info_t *);

#ifdef __cplusplus
}
#endif //extern C

#endif
