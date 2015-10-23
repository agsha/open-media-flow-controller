#ifndef __RTP_AVC__
#define __RTP_AVC__

#include <inttypes.h>
#include "rtp_avc_read.h"
#include "nkn_vpe_rtp_packetizer.h"
#include "nkn_vpe_mp4_unhinted.h"
#ifdef __cplusplus
extern "C" {
#endif

    int32_t  build_rtp_avc_pkt(rtp_packet_builder_t*,int32_t,int32_t,uint8_t*,int32_t, uint32_t);
    int32_t rtp_packetize_avc(Sample_rtp_info_t*);
    void read_avc1_box(AVC_conf_Record_t*,uint8_t*, uint8_t);
    void form_rtp_header(rtp_header_t h, uint8_t *header);
    // int32_t build_sdp_avc_info(rtp_packet_builder_t*, char** ,int32_t *);
    int32_t build_sdp_avc_info(rtp_packet_builder_t* , char** ,int32_t *,int32_t,int32_t);
    void init_avc_pkt_builder(parser_t*,Sample_rtp_info_t *);
    void cleanup_avc_packet_builder(rtp_packet_builder_t*);

#ifdef __cplusplus
}
#endif //extern C

#endif //header protection
