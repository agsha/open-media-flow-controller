/*
 *
 * Filename:  nkn_vpe_parser_api.h
 * Date:      30 APR 2009
 * Module:    iniline parser for video data
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */
#ifndef _PARSER_API_
#define _PARSER_API_

#include <stdio.h>
#include <sys/types.h>

#include "nkn_vpe_backend_io_plugin.h"

#ifdef __cplusplus
extern "C"{
#endif

#define PARSER_TYPE_FLV 0x00000001

    /** a fundamental unit of data
     */
    typedef struct tag_input_packet
    {
	uint8_t *p_data;
	uint8_t len;
    }packet_t;

    /** contains information about the input format, to be extended later
     */
    typedef struct tag_input_context
    {
	uint32_t container_id;
    }input_ctx_t;

    /** contains information about the output format, to be extended later
     */
    typedef struct tag_output_context
    {
	uint32_t container_id;
    }output_ctx_t;

    typedef struct tag_packet_writer
    {
	int64_t (*write)(void *buf, int64_t off, int64_t size);
	void *private_data;
    }packet_writer_t;

    typedef struct tag_parser
    {
	/** name of the parser, indicates the container that it can parse
	 */
	uint32_t parser_name;

	/** init function
	 * @param cb_write [in] . callback functions for write, can be NULL if
	 * meta data gleaning is the only operation (input and output context
	 * have the same container id)
	 * @param parser_state [out] . parser state, specific to each parser
	 */
	int64_t (*init)(packet_writer_t* cb_write, void **parser_state);

	/** checks if the input data can be parsed by the underlying codec 
	 * parsers
	 * @param pkt [in] . input data packet
	 * @param parser_state [in] . parser state, specific to each parser
	 * @return returns .-1. if input data not sufficient, .0. if the input 
	 * data format is unsupported and .1. if the probe was successful
	 */
	int64_t (*probe)(void *parser_state, packet_t *pkt, input_ctx_t *ctx_in, output_ctx_t *ctx_out);

	/** parse packet
	 * @param parser_state [in] . parser state, specific to each parser
	 * @param pkt [in] . input data packet
	 * @param out_buf [in/out] . output buffer allocated by the caller
	 * @return returns .-1. if input data not sufficient, any other negative 
	 * value for errors, number of bytes parser if parsing was successful
	 */
	int64_t (*parse_packet)(void *parser_state, packet_t *pkt, void 
				 *out_buf); 
    }format_parser_t;

   typedef struct _tag_parser_state{
	void *io_desc;
	io_funcs *iof;
	uint32_t container_id;
	int64_t total_size;
	int64_t bytes_processed;
   }parser_state;

    int32_t parse_flv_data(parser_state *ps, meta_data *md);
    int32_t get_video_info(parser_state *ps, meta_data *md);
    parser_state* init_parser(uint32_t container, uint8_t *p_data, int64_t blk_size, int64_t content_length);
    int32_t set_container_id(parser_state *ps, int32_t container_id);
    int32_t close_parser(parser_state *ps);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif //_PARSER_API_
