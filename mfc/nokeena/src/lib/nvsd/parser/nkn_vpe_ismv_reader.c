#include <stdio.h>
#include "nkn_debug.h"
#include "nkn_vpe_mp4_ismv_access_api.h"
#include "nkn_vpe_ismv_reader.h"
#include "nkn_vpe_types.h"
#include "nkn_memalloc.h"

/********************************************************************
 *             API's implemented
 * 1. init_ismv_reader - intializes ismv reader context
 * 2. ismv_reader_fill_buffer -  accumulates data into the process
 *    buffer by parsing the ISMV context to find a set of MOOF
 *    [offset,length] pairs such that they are as close as possible to
 *    the  ISMV2TS_BLK_READ_SIZE. this set of MOOF's are then read
 *    from the input source into a processing buffer which can be the
 *    used in the next stage of the conversion pipline (e.g. format
 *    conversion file write etc) 
 * 3. ismv_reader_flush - flush the process buffers
 * 4. ismv_reader_reset - reset the reader state
 * 5. ismv_reader_cleanup_proc_buf - cleanup the process buffer
 * 6. ismv_reader_cleanup - cleanup the reader context
 *********************************************************************/

/** 
 * initializes the ismv reader context 
 * @param out - pointer to the context
 * @return returns 0 on success and a negative integer on error
 */

int32_t
init_ismv_reader(ismv_reader_t **out)
{
    ismv_reader_t *r;
    int32_t err;

    err = 0;
    *out = NULL;
    r = (ismv_reader_t*)
	nkn_calloc_type(1, sizeof(ismv_reader_t), \
			mod_vpe_ismv_reader_t);

    if (!r) {
	return -E_VPE_PARSER_NO_MEM;
    }
    
    ISMV_BUF_READER_SET_STATE(r, ISMV_BUF_STATE_INIT);
    err = ismv_reader_attach_new_buf(r, &r->out);
    if (err) {
	free(r);
	return -E_VPE_PARSER_NO_MEM;
    }
    
    r->cur_pos = 0;
    r->state |= ISMV_BUF_STATE_NEXT_TRAK;
    
    *out = r;
    return 0;
}

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
int32_t 
ismv_reader_fill_buffer(ismv_reader_t *reader, void *io_desc, 
			int32_t trak_num,
			ismv_parser_ctx_t *ctx, 
		        ismv_moof_proc_buf_t **proc_buf)
{

    size_t moof_offset, moof_size, prev_moof_offset, prev_moof_size,
	accum_size, start_offset; 
    uint64_t moof_time;
    int32_t start_processing, rv, n_moofs, j;
    vpe_olt_t *ol;
    int32_t err, non_contiguous_cnt;
    //void *io_desc;
    /* initialization */    
    start_processing = 0;
    accum_size = reader->accum_size; // incase a moof spanned two read(s)
    start_offset = 0;
    err = 0;
    non_contiguous_cnt = 0;

    /** 
     * BZ 7522
     * if the TRAK_DONE state is set in the buffer; there is pending
     * data and there are no more fragments to parse. return
     * NO_MORE_FRAG and force the caller to flush the pending data
     */
    if (reader->state == ISMV_BUF_STATE_TRAK_DONE) {
	rv = E_ISMV_NO_MORE_FRAG;
	return rv;
    }
    /* allocate for the [offset, length] pair array; this array is to
     * be freed at the end of processing one block of moofs. if there
     * was a moof spanning two reads, then this list would be present
     * already in which case, we need to carry over the old list
     */
    n_moofs = mp4_get_frag_count(ctx, trak_num);
    if (!reader->out->ol_list) {
	err = ismv_reader_alloc_ol_list(reader, n_moofs);
	if (err) {
	    return err;
	}
	j = 0;
    } else {
	/* carry over from the old [offset, length] list */
	j = reader->out->ol_list_num_entries;
    }
    ol = reader->out->ol_list;

    /* read the first moof to note the start offset of the current
     * block. this will ensure that we dont have to do a check this is
     * the first moof in the current read in the following loop to
     * read a block of moofs
     */    
    if (reader->state & ISMV_BUF_STATE_NEXT_TRAK) {
	rv = mp4_frag_seek(ctx, trak_num, 0, 0,\
			   &start_offset, &moof_size, &moof_time);
	ISMV_BUF_READER_CLEAR_STATE(reader, ISMV_BUF_STATE_NEXT_TRAK);
	ismv_reader_set_start_offset(reader, start_offset);
    } 
    //open the file using the io_handlers
    rv = mp4_get_next_frag_1(ctx, io_desc, ctx->iocb, &start_offset,
			     &moof_size, &moof_time);

    if (rv == E_ISMV_NO_MORE_FRAG) {
	int uu =0;
	uu++;
	
	ismv_reader_update_ol(ol, reader,	\
			      start_offset,
			      moof_size, moof_time);
	return rv;
    }

    if ((accum_size + moof_size) > ISMV2TS_BLK_READ_SIZE) {
	moof_offset = start_offset;
	/* copy the current run of moof's, update the buffer state
	 * to DATA_READY, retrive the buffer and [offset, size]
	 * list for processing and set the start processing flag
	 */
	ismv_reader_flush_continue(reader, io_desc, n_moofs,
				   start_offset, accum_size,
				   (void**)proc_buf, ctx->iocb);
				       
	start_processing = 1;
	ismv_reader_set_start_offset(reader, moof_offset);
	ismv_reader_update_ol(reader->out->ol_list, reader,
			      moof_offset, moof_size, 
			      moof_time);
	
    } else {
	accum_size += moof_size;
	//ismv_reader_set_accum_size(reader, accum_size);
	prev_moof_offset = start_offset;
	prev_moof_size = moof_size;
	ismv_reader_update_ol(ol, reader, start_offset, 
			      moof_size, moof_time);
    }

    /** 
     * this loop will read a block of moof(s) satisfying the following
     * criteria 
     * - the total size of the moofs should be as close as possible
     *   to the defined block size (OR) till the end of the trak (OR)
     *   till the specified segmenation time stamp is reached;
     *   whichever occurs first.
     * NOTE: incase there is a moof that spans two this read and the
     * next read, we create a new buffer and copy the spanning moof
     * into the new buffer. the state of the new buffer is updated
     * such that during the next call we the subsequent moofs from the
     * end of the spanned moof
     */
    while ( (!start_processing) ) {
	rv = mp4_get_next_frag_1(ctx, io_desc, ctx->iocb,
				 &moof_offset, &moof_size,
				 &moof_time);
	
	if ((prev_moof_offset + prev_moof_size) != moof_offset) {
	    non_contiguous_cnt++;
	}

	if ((accum_size + moof_size) > ISMV2TS_BLK_READ_SIZE) {
	    /* copy the current run of moof's, update the buffer state
	     * to DATA_READY, retrive the buffer and [offset, size]
	     * list for processing and set the start processing flag
	     */
	    ismv_reader_flush_continue(reader, io_desc, n_moofs,
				       start_offset, accum_size,
				       (void**)proc_buf, ctx->iocb);
				       
	    start_processing = 1;
	    ismv_reader_set_start_offset(reader, moof_offset);
	    ismv_reader_update_ol(reader->out->ol_list, reader,
				  moof_offset, moof_size, 
				  moof_time);
	} else {
	    /* copy curr position */
	    prev_moof_offset = moof_offset;
	    prev_moof_size = moof_size;
	    ismv_reader_update_ol(ol, reader,\
				  moof_offset,
				  moof_size, moof_time);
	    accum_size += moof_size;
	}
 
	if (rv == E_ISMV_NO_MORE_FRAG) {
	    /**
	     * BZ 7522
	     * if the last fragment spans the current buffer boundary
	     * then there would be pending data in the next buffer;
	     * reset the return value to force the caller to come back
	     * set the buffer state to TRAK_DONE to indicate that we
	     * dont have to do any parsing on the next call but we
	     * only need to return NO_MORE_FRAG
	     */
	    if (start_processing) {
		ISMV_BUF_READER_SET_STATE(reader,
					  ISMV_BUF_STATE_TRAK_DONE);
		rv = 0;

	    }
	    break;
	}
	
    } 
    
    return rv;
}

/*
 * flushes the proc buffer forcibly even if the accumulated data
 * did not fill the buffer
 * @param st - reader context
 * @param io_desc - the io descriptor which is the source of the data
 * @param out - process buffer
 * return returns 0 on success and negative number on error
 */
int32_t 
ismv_reader_flush(ismv_reader_t *st, void *io_desc, void **out,
		  io_handlers_t *iocb)
{
    int32_t off;

    off = 0;
    ismv_reader_read_file(st, io_desc,
			  st->start_offset,
			  st->accum_size, iocb);

    ISMV_BUF_READER_SET_STATE(st, ISMV_BUF_STATE_DATA_FLUSH);
    return ismv_reader_process_state(st, &off, out);
    
}

/* resets the readers state
 * @param st - reader context
 * return returns 0 on success and negative number on error
 */
int32_t
ismv_reader_reset(ismv_reader_t *st)
{
    int32_t err, off;

    off = 0;
    ISMV_BUF_READER_SET_STATE(st, ISMV_BUF_STATE_INIT);
    err = ismv_reader_process_state(st, &off, NULL);
    st->state |= ISMV_BUF_STATE_NEXT_TRAK;
    return err;
}

/**
 * cleans the process buffer
 */
void
ismv_reader_cleanup_proc_buf(ismv_moof_proc_buf_t *p)
{
    if (p) {
	if (p->buf) free(p->buf);
	if (p->ol_list) free(p->ol_list);
	free(p);
    }    
}

/**
 * clean up the reader context; 
 * NOTE: DOES NOT FREE THE PROCESS BUFFERS; the caller needs to
 * flush and free
 */
void
ismv_reader_cleanup(ismv_reader_t *reader)
{
    if (reader) {
	free(reader);
    }
}

static inline void
ismv_reader_set_start_offset(ismv_reader_t *reader, size_t offset)
{
    reader->start_offset = offset;
}

static inline void
ismv_reader_set_accum_size(ismv_reader_t* reader, size_t accum_size)
{
    reader->accum_size = accum_size;
}

static int32_t
ismv_reader_flush_continue(ismv_reader_t *reader,
			   void *io_desc,
			   int32_t n_moofs,
			   size_t moof_offset,
			   size_t accum_size,
			   void **proc_buf, 
			   io_handlers_t *iocb)

{
    ismv_reader_read_file(reader, io_desc,
			  reader->start_offset,
			  accum_size, iocb);
    ISMV_BUF_READER_SET_STATE(reader, ISMV_BUF_STATE_DATA_READY);
    /* this call retrieves the old buffers and also
     * initializes a new buffer for copying the next set of
     * moofs
     */
    ismv_reader_process_state(reader,
			      &moof_offset,
			      (void**)proc_buf);
    /* copy the spanning moof to the new buffer, create a new
     * ol_list for this buffer */
    ismv_reader_alloc_ol_list(reader, n_moofs);
	    
    return 0;
}

static int32_t
ismv_reader_process_state(ismv_reader_t *st, 
			  void *in1,
			  void **out1)
{
    uint8_t *buf;
    switch (st->state) {
	case ISMV_BUF_STATE_INIT:
	    ismv_reader_attach_new_buf(st, &st->out);
	    st->start_offset = *((int32_t*)(in1));
	    st->state = ISMV_BUF_STATE_NODATA;
	    break;
	case ISMV_BUF_STATE_DATA_READY:
	    *out1 = ismv_reader_get_buf(st);
	    st->state = ISMV_BUF_STATE_INIT;
	    ismv_reader_process_state(st, in1, NULL);
	    break;
	case ISMV_BUF_STATE_DATA_FLUSH:
	    *out1 = ismv_reader_get_buf(st);
	    st->state = ISMV_BUF_STATE_INIT;
	    break;
    }
    
    return 0;
}
    
static int32_t
ismv_reader_attach_new_buf(ismv_reader_t *reader,
			   ismv_moof_proc_buf_t **st_new)
{
    if (reader->state & ISMV_BUF_STATE_INIT) {
	ismv_moof_proc_buf_t *st;
	st = (ismv_moof_proc_buf_t*)\
	    nkn_malloc_type(ISMV2TS_BLK_READ_SIZE,\
			    mod_ismv_moof_proc_buf_t);
	if (!st) 
	    return -E_VPE_PARSER_NO_MEM;

	st->buf_size = ISMV2TS_BLK_READ_SIZE;
	st->buf = (uint8_t*)\
	    nkn_malloc_type(ISMV2TS_BLK_READ_SIZE,\
			mod_vpe_media_buffer);
	reader->state = \
	    (ISMV_BUF_STATE_NODATA);
	reader->cur_pos = 0;
	reader->accum_size = 0;
	st->ol_list = 0;
	st->ol_list_num_entries = 0;	
	*st_new = st;
    } else {
	return -E_VPE_PARSER_FATAL_ERR;
    }

    return 0;
}

static ismv_moof_proc_buf_t*
ismv_reader_get_buf(ismv_reader_t *st)
{
    if (st->accum_size &&
	(st->state & ISMV_BUF_STATE_DATA_READY ||
	 st->state & ISMV_BUF_STATE_DATA_FLUSH)) {
	return st->out;
    } else {
	return NULL;
    }

}

static size_t
ismv_reader_read_file(ismv_reader_t *reader, void *io_desc, 
		      size_t offset, size_t size, io_handlers_t *iocb)
{
    int32_t i=0;
    size_t rv;
    size_t temp_offset = 0;

    for (i=0; i<reader->out->ol_list_num_entries; i++) {
	iocb->ioh_seek(io_desc, reader->out->ol_list[i].offset,
		       SEEK_SET);
	if (temp_offset + reader->out->ol_list[i].length >=
	    ISMV2TS_BLK_READ_SIZE) {
	    assert(0);
	}
	rv = iocb->ioh_read(reader->out->buf + reader->cur_pos +
			     temp_offset, 1,
			     reader->out->ol_list[i].length, io_desc);
	//DBG_MFPLOG ("ISMV Reader", MSG, MOD_MFPFILE, "offset %ld, size %ld\n", temp_offset, reader->out->ol_list[i].length);
	//fseek(fin, reader->out->ol_list[i].offset, SEEK_SET);
	//rv = fread(reader->out->buf + reader->cur_pos + temp_offset,
	//   reader->out->ol_list[i].length, 1, fin);
	temp_offset+= reader->out->ol_list[i].length;
    }

    ISMV_BUF_READER_UPDATE_POS(reader, size);

    if (reader->cur_pos == ISMV2TS_BLK_READ_SIZE) {
	ISMV_BUF_READER_SET_STATE(reader, ISMV_BUF_STATE_DATA_READY);
    }
    
    return rv;
    
}

static inline int32_t
ismv_reader_alloc_ol_list(ismv_reader_t *reader, int32_t num_entries)
{
    reader->out->ol_list = (vpe_olt_t*)
	nkn_calloc_type(num_entries, sizeof(vpe_olt_t),
			mod_vpe_ol_t);
    
    if (!reader->out->ol_list) {
	return -E_VPE_PARSER_NO_MEM;
    }
    
    return 0;
}

static inline int32_t 
ismv_reader_update_ol(vpe_olt_t *ol, ismv_reader_t *reader,
		      size_t offset, size_t length,
		      uint64_t moof_time) 
{
    int32_t j;

    j = reader->out->ol_list_num_entries;

    ol[j].offset = offset;//current_moof_offset

    ol[j].length = length;
    ol[j].time = moof_time;
    reader->out->ol_list_num_entries++;
    reader->accum_size += length;

    return 0;
}

