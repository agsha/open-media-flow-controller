/*
 * Filename: nkn_vpe_asf_parser.h
 * Data:
 * Function: parser for asf container
 *
 *
 *
 */

#ifndef _ASF_PARSER_
#define _ASF_PARSER_

#include <stdlib.h>
//#include "nkn_vpe_asf.h"
#include "nkn_vpe_backend_io_plugin.h"
#include "nkn_vpe_types.h"
#ifdef __cplusplus
extern "C"{
#endif


typedef struct asf_file_prop_obj_s {
    uint64_t obj_size;
    uint8_t  file_id[16];
    uint64_t file_size;
    uint64_t create_data;
    uint64_t data_pak_cnt;
    uint64_t play_duration;
    uint64_t send_duration;
    uint64_t preroll;
    uint32_t flags;
    uint32_t min_data_pakt_size;
    uint32_t max_data_pakt_size;
    uint32_t max_bitrate;

    /* bitstream info */
    uint64_t pos;
    uint64_t datasize;
    uint8_t* data;
} nkn_vpe_asf_file_prop_obj_t;


typedef struct asf_header_obj_s {
    uint64_t obj_size;
    uint32_t num_obj;
    /* bitstream info */
    uint64_t pos;
    uint64_t datasize;
    uint8_t* data;

    nkn_vpe_asf_file_prop_obj_t* p_asf_file_prop_obj;
} nkn_vpe_asf_header_obj_t;


nkn_vpe_asf_header_obj_t*
nkn_vpe_asf_initial_header_obj(uint8_t* p_data, uint64_t off,
                               uint64_t datasize);

nkn_vpe_asf_file_prop_obj_t*
nkn_vpe_asf_initial_file_prop_obj(uint8_t* p_data, uint64_t off,
                                  uint64_t datasize);

int
nkn_vpe_asf_clean_header_obj(nkn_vpe_asf_header_obj_t* p_asf_header_obj);

int
nkn_vpe_asf_clean_file_prop_obj(nkn_vpe_asf_file_prop_obj_t* p_asf_file_prop_obj);

int
nkn_vpe_asf_header_guid_check(uint8_t* p_data);

int
nkn_vpe_asf_header_obj_pos(uint8_t* p_data, uint64_t datasize,
			   vpe_ol_t* ol);

int
nkn_vpe_asf_header_obj_parser(nkn_vpe_asf_header_obj_t* p_asf_header_obj);

int
nkn_vpe_asf_file_prop_obj_parser(nkn_vpe_asf_file_prop_obj_t* p_asf_file_prop_obj);

#ifdef __cplusplus
}
#endif

#endif // _ASF_PARSER_
