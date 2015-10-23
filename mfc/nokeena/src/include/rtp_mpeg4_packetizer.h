/*
 *
 * Filename:  rtp_mpeg4_packetizer.c
 * Date:      2009/03/28
 * Module:    MPEG4 RTP packetization based on RFC 3640 for AAC
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _RTP_MPEG4_PACKETIZER_
#define _RTP_MPEG4_PACKETIZER_

#include <string.h>
#include <sys/time.h>

#include "nkn_vpe_types.h"
#include "nkn_vpe_rtp_packetizer.h"
#include "nkn_vpe_mp4_unhinted.h"
#include "rtp_mpeg4_es.h"
#include "nkn_vpe_bufio.h"

#ifdef __cplusplus
extern "C"{
#endif

#define MAX_SDP_SIZE 2048

    /* HELPER FUNCTIONS */
    static inline void add_name_value_pair_char(char *buf, const char *name, char *value);
    static inline void add_name_value_pair_int(char *buf, const char *name, int32_t value);
    static inline void add_first_name_value_pair_char(char *buf, const char *name, char *value);
    static inline void add_first_name_value_pair_int(char *buf, const char *name, int32_t value);
    static inline int32_t byte_align_size(int32_t s);
    static inline int32_t add_to_payload(rtp_packet_builder_t *bldr, uint8_t* payload, int32_t offset, int32_t size);
    int32_t add_au_header(rtp_packet_builder_t *bldr, int32_t nal_size);
    int32_t write_rtp_header(rtp_packet_builder_t *bldr);
    static inline int32_t finalize_rtp_packet(rtp_packet_builder_t *bldr);
    int32_t get_mpeg4_pl_name(mpeg4_es_desc_t *esd, char *pl_name, char *media_type);
    int32_t compute_profile_level_id(uint32_t base_object_type, uint32_t channel_config, uint32_t sampling_rate); /* refer section 1.5.2.4 of 14496-3 sp-1, table 1.12 & 1.11 */
    static inline void constuct_m4a_config_str(uint8_t* data, size_t size, char *out);
    int32_t create_new_rtp_packet(rtp_packet_builder_t *bldr);
    int32_t map_mpeg4_systems_to_rfc_3640(rtp_packet_builder_t *bldr, size_t max_au_size);

    /* API implementation */
    int32_t build_mpeg4_rtp_packet(rtp_packet_builder_t* bldr,int32_t mode,int32_t nal_size,uint8_t *nalu,int32_t AUEnd, uint32_t AUStart);
    int32_t cleanup_mpeg4_packet_builder(rtp_packet_builder_t *bldr);
    int32_t init_mpeg4_packet_builder(parser_t *parser, rtp_packet_builder_t *bldr, int32_t payload_type, int32_t(*packet_ready_handler)(void *, void*), void *cb);
    int32_t build_mpeg4_sdp_lines(rtp_packet_builder_t *bldr, char **sdp, int32_t *sdp_size,int32_t,int32_t);
#ifdef __cplusplus
}
#endif

#endif
