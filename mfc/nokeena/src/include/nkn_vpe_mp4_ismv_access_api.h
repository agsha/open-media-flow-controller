/*
 *
 * Filename:  nkn_vpe_mp4_ismv_access_api.h
 * Date:      2010/01/01
 * Module:    mp4 ismv random access api
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _MP4_ISMV_ACCESS_API_
#define _MP4_ISMV_ACCESS_API_

#include <sys/types.h>
#include <stdio.h>

#include "nkn_vpe_bitio.h"
#include "nkn_vpe_types.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_mp4_seek_parser.h"

#ifdef __cplusplus 
extern "C" {
#endif

    /* Error Codes */
    /** \def SEARCH WINDOW FOR MOOV BOX */
    #define MOOV_SEARCH_SIZE (32*1024)

    /* Data Types */
    /**
     * \struct context for parsing ismv content
     */
    typedef struct tag_ismv_parser_ctx {
	bitstream_state_t *bs; 
	mfra_t *mfra;
	moov_t *moov;
	uint32_t curr_trak;
	uint32_t curr_frag;
	size_t curr_moof_offset;
	io_handlers_t *iocb;
    }ismv_parser_ctx_t;

    /******************************************************************
     *            ISMV FILE PARSER API's
     *****************************************************************/

    /**
     * reads the offset of the MFRA box; the offset of the MFRA box is
     * present in the MFRO box. the MFRO box is usually the last box
     * (and is of a fixed size) in the MP4 file.  
     * @param src - pointer to the last 32KB of data; in which the
     *              mfro box is present
     * @param size - size of the buffer
     * @return - returns the offset from the EOF to the MFRA box
     */
    uint32_t mp4_get_mfra_offset(uint8_t *src, size_t size);

    /**
     * finds the size of the MFRA box
     * @param src - should point to the top start of the mfra box. the
     * offset for this box is found using the 'mp4_get_mfra_offset'
     * API
     * @param size - size of the buffer
     * @return - returns the size of the mfra box
     */
    extern io_handlers_t ioh;
    static inline size_t
    mp4_get_mfra_size(uint8_t *src, size_t size)
    {
	bitstream_state_t *bs;
	box_t box;
	size_t box_pos;

	bs = ioh.ioh_open((char*)src, "rb", size);
	mp4_read_next_box(bs, &box_pos, &box);
	ioh.ioh_close((void*)bs);
	return (size_t)(box.short_size);
    }


    /**
     * initializes the mfra/tfra parser context and sets it up for
     * seek and parsing
     * @param p_mfra - buffer pointing to the start of the mfra box
     * @param size - size of the buffer
     * @param moov - the fully populated moov box (see
     * nkn_vpe_mp4_seek_api.c for more details on how to populate the
     * moov box) for the MP4 file. Note this can be NULL; if NULL trak
     * timescale needs to be explicitly passed to the subsequent
     * 'mp4_frag_seek' API
     * @return - returns the context. NULL if any error occurs
     */
    ismv_parser_ctx_t* mp4_init_ismv_parser_ctx(uint8_t *p_mfra, 
						size_t size, 
						moov_t *moov);

    /**
     * seeks to a particular timestamp within a give track
     * id(profile). converts a timestamp in a profile into its
     * corresponding moof fragment's offset and length
     * @param ctx - the context, correctly instantiated by calling
     * 'mp4_init_ismv_parser_ctx'
     * @param track_id - the track id(profile) in which the fragment
     * corresponding to the timestamp needs to be found
     * @param seek_to - the time stamp to seek to
     * @param trak_timescale - the time offset in trak time units                         
     * for the trak in which the moof offset needs to be found.
     * this needs to be explicitly passed if the moov box was not 
     * initialized as a part of the ismv context. This can be '0' 
     * otherwise
     * @param moof_offset [out] - the offset of the moof box
     * corresponding to the seek timestamp
     * @param moof_length [out] - the length of the moof box
     * corresponding to the seek timestamp
     * @param moof_time [out] = the timestamp of the seek'ed moof box
     * @return - returns a negative value if error occurs; 0 otherwise
     */
    int32_t mp4_frag_seek(ismv_parser_ctx_t *ctx, uint32_t track_id, 
			  float seek_to, uint64_t trak_timescale, 
			  size_t *moof_offset, size_t *moof_length, 
			  size_t *moof_time);

    /**
     * this API can be used to continue to get the offset and lengths
     * the moof fragments that follow the seeked moof fragment
     * (continuous mode)
     * @param ctx - the parser context, correctly instantiated by
     * calling 'mp4_init_ismv_parser_ctx'
     * @param moof_offset [out] - the offset of the moof box
     * corresponding to the seek timestamp
     * @param moof_length [out] - the length of the moof box
     * corresponding to the seek timestamp
     * @return - returns a negative value if error occurs; 0 otherwise
     */
    int32_t mp4_get_next_frag(ismv_parser_ctx_t *ctx, 
			      size_t *moof_offset, size_t *moof_len, 
			      size_t *moof_time);

    /**
     * this API can be used to continue to get the offset and lengths
     * the moof fragments that follow the seeked moof fragment
     * (continuous mode)
     * @param ctx - the parser context, correctly instantiated by
     * calling 'mp4_init_ismv_parser_ctx'
     * @io_desc - the input which is already been opened
     * @iocb - indicates the which io_handler
     * @param moof_offset [out] - the offset of the moof box
     * corresponding to the seek timestamp
     * @param moof_length [out] - the length of the moof box
     * corresponding to the seek timestamp
     * @return - returns a negative value if error occurs; 0 otherwise
     */
    int32_t mp4_get_next_frag_1(ismv_parser_ctx_t *ctx, void *io_desc,
				io_handlers_t *iocb, size_t
				*moof_offset, size_t *moof_len,
				size_t *moof_time);

    /**
     * find the total number of moof's in the traf
     * @param ctx - a valid ismv parser context
     * @param trak_num - the trak number for which we need to find the
     * number of moof's
     * @return returns the number of moofs in a traf
     */
    int32_t mp4_get_frag_count(ismv_parser_ctx_t *ctx, int32_t trak_num);

    /**
     * find the total number of moof's in the traf
     * @param ctx - a valid ismv parser context
     * @param trak_num - the trak number corresponding to trak_id
     * @trak_id - the trak id to which trak_num has to be found
     * @return returns a negative value if error occurs; 0 otherwise
     */
    int32_t mp4_get_trak_num(ismv_parser_ctx_t *ismv_ctx, int32_t *trak_num,
		 int32_t trak_id);

    /** 
     * API to create an ismv parser context from a file
     * @param ismv_path - path to the ismv file
     * @param out - a fully populated valid ismv context
     * @return returns 0 on success, non-zero negative interger on
     * error
    */
    int32_t mp4_create_ismv_ctx_from_file(char *ismv_path,
					  ismv_parser_ctx_t **out,
					  io_handlers_t *iocb);
    

    /**
     * cleans up the buffer allocated and attached to this context;
     * typically a buffer containing the mfra data. 
     * NOTE: this API needs to be called before the call to
     * mp4_cleanup ismv_ctx
     * @param ctx - the ismv parser context
     */
    void mp4_cleanup_ismv_ctx_attach_buf(ismv_parser_ctx_t *ctx);

    /**
     *  clean up the parser state
     */
    void mp4_cleanup_ismv_ctx(ismv_parser_ctx_t *ctx);

    /********************************************************************
     *              HELPER FUNCTIONS 
     *******************************************************************/
    tfra_t* mp4_tfra_init(uint8_t *data, size_t pos, size_t size);
    int32_t mp4_read_tfra_header(bitstream_state_t *bs, tfra_t *tfra);
    int32_t mp4_read_tfra_table_entry_v1(bitstream_state_t *bs, tfra_t *tfra,
					 size_t *moof_offset,size_t *moof_time);
    int32_t mp4_read_tfra_table_entry_v0(bitstream_state_t *bs, tfra_t *tfra,
					 size_t *moof_offset, size_t *moof_time);
    void mp4_cleanup_mfra(mfra_t *mfra);
    void mp4_cleanup_tfra(tfra_t *tfra);
    int32_t mp4_set_reader(ismv_parser_ctx_t *ctx, io_handlers_t *ioh);

#ifdef __cplusplus
}
#endif

#endif
