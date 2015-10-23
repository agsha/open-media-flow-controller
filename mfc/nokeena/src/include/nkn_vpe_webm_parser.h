/*
 * Filename: nkn_vpe_webm_parser.h
 * Data:
 * Function: parser for webm container
 *
 *
 *
 */

#ifndef _WEBM_PARSER_
#define _WEBM_PARSER_

#include <stdlib.h>
#include "nkn_vpe_webm.h"
#include "nkn_vpe_backend_io_plugin.h"
#include "nkn_vpe_types.h"
#ifdef __cplusplus
extern "C"{
#endif

#define EBML_INVALID_LENGTH -2
#define MAX_WEBM_TRAK 20 /* To simplify the implementation */

typedef struct ebml_elem_s {
    uint64_t ebml_elem_ID;
    uint64_t ebml_headsize;
    uint64_t ebml_datasize;
    nkn_vpe_ebml_type_t ebml_type;
    // need more ?
} nkn_vpe_ebml_elem_t;

typedef struct webm_ebml_header_s {
    uint64_t ebml_version;
    uint64_t ebml_read_verson;
    uint64_t ebml_max_ID_length;
    uint64_t ebml_max_size_length;
    char*    p_ebml_doc_type;
    uint64_t ebml_doc_type_version;
    uint64_t ebml_doc_type_read_vesion;
} nkn_vpe_webm_ebml_header_t;

#if 0
/* webm Meta Seek Info*/

/*
 * The webm seek is only for meta_seek.
 * Key frame seek is called cueing in webm.
 */

typedef struct webm_seek_s {
    uint64_t webm_seek_ID;
    uint64_t webm_seek_positon;
} nkn_vpe_webm_seek_t;

typedef struct webm_seekhead_s {
    nkn_vpe_webm_seek_t* p_webm_seeks;
    int                  webm_num_seeks;

} nkn_vpe_webm_seekhead_t;

typedef struct webm_meta_seek_info_s {
    nkn_vpe_webm_seekhead_t* p_webm_seekhead;
}

#endif

typedef union tag_ebml_float{
    uint32_t ui32;
    float f;
} ebml_float;

typedef union tag_ebml_double{
    uint64_t ui64;
    double d;
} ebml_double;
/*
 * Only the info used for nkn_attr_priv is listed below
 * Others will be dropped during parse.
 */
typedef struct webm_segment_info_s {
    double webm_duration; /* duration of one segment */
    uint64_t webm_timescale; /* timecode scale */
    /* bitstream info */
    uint64_t pos;
    uint8_t* data;
    uint64_t headsize;
    uint64_t datasize;
} nkn_vpe_webm_segment_info_t;

typedef struct webm_trak_entry_s {
    uint64_t webm_trak_type;/* video or audio*/
    char*    p_webm_codec_ID; /* codec info for v/a traks*/
    /* bitstream info */
    uint64_t pos;
    uint8_t* data;
    uint64_t headsize;
    uint64_t datasize;
} nkn_vpe_webm_trak_entry_t;

typedef struct webm_traks_s {
    nkn_vpe_webm_trak_entry_t* p_webm_trak_entry[MAX_WEBM_TRAK];
    /* possible multiple traks */
    int                        webm_num_traks;
    /* bitstream info */
    uint64_t pos;
    uint8_t* data;
    uint64_t headsize;
    uint64_t datasize;
} nkn_vpe_webm_traks_t;


typedef struct webm_segment_s {
    nkn_vpe_webm_segment_info_t* p_webm_segment_info;
    nkn_vpe_webm_traks_t* p_webm_traks;
    /* bitstream info */
    uint64_t pos;
    uint8_t* data;
    uint64_t headsize;
    uint64_t datasize;
} nkn_vpe_webm_segment_t;

/* webm initial parser */
nkn_vpe_webm_segment_t*
nkn_vpe_webm_init_segment(uint8_t* p_data, uint64_t off,
                          uint64_t headsize, uint64_t datasize);

nkn_vpe_webm_segment_info_t*
nkn_vpe_webm_init_segment_info(uint8_t* p_data, uint64_t off,
                               uint64_t headsize, uint64_t datasize);

nkn_vpe_webm_traks_t*
nkn_vpe_webm_init_traks(uint8_t* p_data, uint64_t off,
                        uint64_t headsize, uint64_t datasize);

nkn_vpe_webm_trak_entry_t*
nkn_vpe_webm_init_trak_entry(uint8_t* p_data, uint64_t off,
                             uint64_t headsize, uint64_t datasize);

int
nkn_vpe_webm_clean_segment(nkn_vpe_webm_segment_t* p_webm_segment);

int
nkn_vpe_webm_clean_trak_entry(nkn_vpe_webm_trak_entry_t* p_webm_trak_entry);

int
nkn_vpe_webm_clean_traks(nkn_vpe_webm_traks_t* p_webm_traks);

/*
 * webm header parser
 * It will check the ebml-ID for EBML  and "webm" string
 */
int
nkn_vpe_webm_head_parser(uint8_t *p_data,
			 uint64_t *p_webm_head_size);

int
nkn_vpe_webm_segment_pos(uint8_t* p_data, uint64_t datasize,
			 vpe_ol_t *ol);

/* webm parse Segment */
int
nkn_vpe_webm_segment_parser(nkn_vpe_webm_segment_t* p_webm_segment);


/* webm parse Segment information*/
int
nkn_vpe_webm_segment_info_parser(nkn_vpe_webm_segment_info_t*
				 p_webm_segment_info);

/* webm parse Track */
int
nkn_vpe_webm_traks_parser(nkn_vpe_webm_traks_t* p_webm_traks);

/* webm parse Track entry*/
int
nkn_vpe_webm_trak_entry_parser(nkn_vpe_webm_trak_entry_t* p_webm_trak_entry);

int
nkn_vpe_ebml_parser(uint8_t* p_data, uint64_t* p_elem_ID_val,
                    uint64_t* p_elem_datasize_val,
                    uint64_t* p_ebml_head_size);

#ifdef __cplusplus
}
#endif

#endif // _WEBM_PARSER_
