/**
 * @file   rrecord_msg_utils.h
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Wed May  2 19:45:38 2012
 * 
 * @brief  
 * 
 * 
 */
#ifndef _RRECORD_MSG_UTILS_
#define _RRECORD_MSG_UTILS_

#include <stdio.h>
#include <sys/types.h>
#include <inttypes.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "stored/crst_common_defs.h"
#include "nkn_memalloc.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0 //keeps emacs happy
}
#endif    

struct tag_rrecord_msg_fmt_builder;

typedef int32_t (*add_header_fnp)
(struct tag_rrecord_msg_fmt_builder *,
 uint32_t num_record, uint32_t ttl);
typedef int32_t (*add_record_fnp)
(struct tag_rrecord_msg_fmt_builder *,
 cache_entity_addr_type_t type, 
 const uint8_t *record, uint32_t record_len);
typedef int32_t (*delete_fnp) 
(struct tag_rrecord_msg_fmt_builder *);
typedef int32_t (*get_buf_fnp)
(struct tag_rrecord_msg_fmt_builder *rrb, 
 uint8_t **buf, uint32_t *buf_len);

typedef struct tag_rrecord_msg_fmt_builder {
    add_header_fnp add_hdr;
    add_record_fnp add_record;
    get_buf_fnp get_buf;
    delete_fnp delete;

    int32_t state;
    uint8_t *buf;
    uint32_t wpos;
    uint32_t buf_size;
    uint32_t num_records_written;
    uint32_t num_records;
} rrecord_msg_fmt_builder_t;

typedef struct tag_rrecord_msg_hdr {
    uint32_t num_records;
    uint32_t ttl;
} rrecord_msg_hdr_t;

typedef struct tag_rrecord_rdata {
    uint32_t type;
    uint32_t len;
} rrecord_msg_data_preamble_t;

/* BUILDER API's */
int32_t rrecord_msg_fmt_builder_create(
	       uint32_t tot_record_len,
	       rrecord_msg_fmt_builder_t **out);

static inline uint32_t 
rrecord_msg_fmt_builder_compute_record_size(uint32_t raw_rdata_size)
{
    return raw_rdata_size + 1 + sizeof(rrecord_msg_data_preamble_t);
}

/* READER API's */
int32_t rrecord_msg_fmt_reader_fill_hdr(
		const uint8_t *buf, uint32_t buf_size, 
		const rrecord_msg_hdr_t **hdr);

int32_t rrecord_msg_fmt_reader_get_record_at(const uint8_t *buf, 
		     uint32_t buf_size, uint32_t idx, 
		     const rrecord_msg_data_preamble_t **rr_preamble, 
		     const uint8_t **rdata);

#ifdef __cplusplus
}
#endif

#endif //_RRECORD_MSG_UTILS_
