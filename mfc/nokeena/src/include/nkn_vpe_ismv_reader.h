/*
 *
 * Filename:  nkn_vpe_ismv_reader.h
 * Date:      2010/08/02
 * Module:    implements a block based ISMV reader capable of
 *            accumulating a serial list of MOOFs for format
 *            conversion and other processing 
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _ISMV_READER_
#define _ISMV_READER_

#include <stdio.h>
#include <inttypes.h>

#include "nkn_vpe_types.h"

#ifdef __cplusplus
extern "C" {
#endif
    
#define	ISMV_BUF_STATE_INIT 0x1 //requires memory to be allocated
#define	ISMV_BUF_STATE_NEXT_TRAK 0x2 //buffer for current trak exhausted
#define	ISMV_BUF_STATE_NODATA 0x4 //memory allocated but no data present
#define	ISMV_BUF_STATE_DATA_READY 0x8 /* memory allocated, data present and
				       * ready for processing */ 
#define ISMV_BUF_STATE_DATA_FLUSH 0x16 /* flush data in the current
					* out buf dont create another
					* buf for processing
					*/
#define ISMV_BUF_STATE_TRAK_DONE 0x32 /* parsing done but the last
				       * segment spans multiple
				       * buffers. the first buffer was
				       * returned as a part of the
				       * previous data ready state. In
				       * the current call we do not do
				       * any parsing but return the
				       * next part of the span 
				       */

#define ISMV2TS_BLK_READ_SIZE (3*1024*1024)
#define ISMV_BUF_READER_SET_STATE(_r, _state) (((_r)->state) = (_state))
#define ISMV_BUF_READER_CLEAR_STATE(_r, _state) (((_r)->state) &= (~_state))
#define ISMV_BUF_READER_UPDATE_POS(_r,_size) (((_r)->cur_pos) += (_size))
    
    /**
     * \struct contains a buffer that can be processed
     */
    typedef struct tag_ismv_moof_proc_buf {
	uint8_t *buf;
	size_t buf_size;
	vpe_olt_t *ol_list;
	int32_t ol_list_num_entries;
    } ismv_moof_proc_buf_t;
    

    /** 
     * \struct context for the ISMV reader
     */
    typedef struct tag_ismv_reader {
	int32_t state;
	size_t cur_pos;
	size_t start_offset;
	size_t accum_size;
	ismv_moof_proc_buf_t *out;
    } ismv_reader_t;
    
    /** 
     * initializes the ismv reader context 
     * @param out - pointer to the context
     * @return returns 0 on success and a negative integer on error
     */
    int32_t init_ismv_reader(ismv_reader_t **out);
    
    /**
     * accumulates data into the process buffer by parsing the ISMV
     * context to find a set of MOOF [offset,length] pairs such that
     * they are as close as possible to the
     * ISMV2TS_BLK_READ_SIZE. this set of MOOF's are then read from the
     * input source into a processing buffer which can be the used in
     * the next stage of the conversion pipline (e.g. format conversion,
     * file write etc) 
     * @param reader - the reader context
     * @param fin - the I/O descriptor
     * @param trak_num - the id of the trak from which the moof's list
     * is to be computed
     * @param ctx - the ismv parser context
     * @param proc_buf - the process buffer that is output after
     * populating a list of moof's
     * @return returns 0 on success and a negative number on error and
     * E_ISMV_NO_MORE_FRAG when there are no more fragments to process
     * in the current trak num and the caller needs to seek to the
     * next trak
     */
    int32_t ismv_reader_fill_buffer(ismv_reader_t *reader, 
				    void *iodesc, 
				    int32_t trak_num,
				    ismv_parser_ctx_t *ctx, 
				    ismv_moof_proc_buf_t  **proc_buf);

    /*
     * flushes the proc buffer forcibly even if the accumulated data
     * did not fill the buffer
     * @param st - reader context
     * @param io_desc - the io descriptor which is the source of the
     * data 
     * @param out - process buffer
     * return returns 0 on success and negative number on error
     */
    int32_t ismv_reader_flush(ismv_reader_t *st, void *io_desc, void
			      **out, io_handlers_t *iocb);

    /* resets the readers state
     * @param st - reader context
     * return returns 0 on success and negative number on error
     */
    int32_t ismv_reader_reset(ismv_reader_t *st);

    /**
     * clean up the reader context; 
     * NOTE: DOES NOT FREE THE PROCESS BUFFERS; the caller needs to
     * flush and free
     */
    void ismv_reader_cleanup(ismv_reader_t *reader);

    /**
     * cleans the process buffer
     */
    void ismv_reader_cleanup_proc_buf(ismv_moof_proc_buf_t *p);
    

    static int32_t ismv_reader_process_state(ismv_reader_t *st,
					     void *in1,
					     void **out);
    static ismv_moof_proc_buf_t* ismv_reader_get_buf(\
					      ismv_reader_t *st);  
    static int32_t ismv_reader_attach_new_buf(ismv_reader_t *reader,\
				      ismv_moof_proc_buf_t **st_new); 
    static size_t ismv_reader_read_file(ismv_reader_t *reader, 
					void *io_desc ,  
					size_t offset, size_t size,
					io_handlers_t *iocb);
    static inline int32_t ismv_reader_alloc_ol_list(\
					    ismv_reader_t *reader, 
					    int32_t num_entries); 
    static inline int32_t ismv_reader_update_ol(vpe_olt_t *ol,
					     ismv_reader_t *reader,  
						   size_t offset,
						   size_t length,
						   uint64_t time);
    static inline void ismv_reader_set_start_offset(\
					   ismv_reader_t *reader, 
					   size_t offset);
    static inline void ismv_reader_set_accum_size(\
                                            ismv_reader_t* reader, 
					    size_t accum_size);

    static int32_t ismv_reader_flush_continue(ismv_reader_t *reader,
					      void *io_desc,
					      int32_t n_ol_entries,
					      size_t offset,
					      size_t accum_size,
					      void **proc_buf, 
					      io_handlers_t *iocb);
#ifdef __cplusplus
}
#endif

#endif //_ISMV_READER_
