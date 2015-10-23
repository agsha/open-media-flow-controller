#ifndef _RTP_PACKETIZER_
#define _RTP_PACKETIZER_

#include <inttypes.h>
#include <stdio.h>

#include "nkn_vpe_bufio.h"
#include "rtp_mpeg4_es.h"
#include "nkn_vpe_mp4_feature.h"
#include "rtp_avc_read.h"
#include "glib.h"

#define RTP_PKT_READY 1000
#define RTP_PKT_NOT_READY 1001

#define MTU_SIZE 1438
#define MP4_RET_ERROR -1
#define RTP_HEADER_SIZE 12

#define _MAX(A,B) (A>B)?A:B

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tag_rtp_header {
    
    uint8_t version;
    uint8_t padding;

    uint8_t extension;
    uint8_t CSRC_count;
    uint8_t marker;
    uint8_t payload_type;
    uint16_t sequence_num;
    uint32_t timestamp;
    uint32_t SSRC;
    uint32_t CSRC[16];

}rtp_header_t;

typedef struct tag_nal_info {
    uint32_t DTS;
    uint32_t CTS;
    size_t nal_size;
    size_t num_bytes_written;
    uint8_t* nal_data;
}nal_info_t;

typedef struct tag_rtp_packet_builder {

    /* Generic fields
     */
    uint32_t rtp_payload_type;
    uint32_t rtp_flags;
    uint32_t mtu_size;
    uint32_t max_pkt_time;
    int32_t payload_size; 
    int32_t num_rtp_pkts;
    uint8_t *rtp_pkts;
    int32_t *rtp_pkt_size;
    uint8_t new_pkt;
    rtp_header_t header;

    /*Codec Specific Fields*/
    uint8_t *payload_hdr;
    int32_t payload_hdr_size;
    mpeg4_es_desc_t *es_desc;
    nal_info_t *nal;

    /*Used in aggregation packets*/
    int32_t nal_bytes;
    uint8_t *nal_data;

    /*bitstream writers */
    bitstream_state_t *payload_writer;
    bitstream_state_t *payload_hdr_writer;

    /*AVC sspecific info */
    AVC_conf_Record_t *avc1;
    AVC_NAL_octet octet;
    AVC_FU_A fu_a;

    /* callback functions */
    int32_t (*packet_ready)(void *bldr, void *cb);
    void *cb;

}rtp_packet_builder_t;

typedef struct sample_pkt_info{
    int32_t cont_pkt;
    rtp_packet_builder_t* pkt_builder;
    int32_t sample_size;
    int32_t sample_time;
    uint8_t * sample_data;
    GList *pkt_list;
    double ft;
    int32_t clock;
}Sample_rtp_info_t;

#ifdef __cplusplus
}
#endif

#endif //HEADER guard

