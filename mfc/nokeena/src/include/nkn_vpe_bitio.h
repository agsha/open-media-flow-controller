/*
 *
 * Filename:  nkn_vpe_bitio.h
 * Date:      2009/03/28
 * Module:    bitwise I/O operations
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _BITIO_
#define _BITIO_

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>

typedef struct tag_bitstream_state_t {
	int8_t *data;
	size_t pos;
	uint8_t curr;
	uint8_t bits;
	size_t size;
        uint64_t ref_cnt;
        uint8_t done;
        uint8_t dummy_bitstream;
        pthread_mutex_t ref_cnt_mutex;
}bitstream_state_t;

#ifdef __cplusplus
extern "C"{
#endif
	bitstream_state_t* bio_init_bitstream(uint8_t *data, 
					      size_t  size);
	bitstream_state_t* bio_init_safe_bitstream(uint8_t *data, 
						   size_t size); 
        uint64_t bio_add_ref(bitstream_state_t *bs);
        uint64_t bio_dec_ref(bitstream_state_t *bs);
        uint64_t bio_get_ref_cnt(bitstream_state_t *bs);
      	void bio_reinit_bitstream(bitstream_state_t *bs, 
				  uint8_t *data, size_t size); 
	uint8_t bio_read_bit(bitstream_state_t *bs);
	uint8_t bio_read_byte(bitstream_state_t *bs);
	uint32_t bio_read_bits(bitstream_state_t *bs, 
			       int32_t bits_to_read); 
	uint64_t bio_read_bits_to_uint64(bitstream_state_t *bs, 
					 int32_t bits_to_read);
	void bio_close_bitstream(bitstream_state_t *bs);
	size_t bio_aligned_read(bitstream_state_t *bs, 
				uint8_t* buf, size_t size);
	size_t bio_get_remaining_bytes(bitstream_state_t *bs);
	void bio_flush_byte(bitstream_state_t *bs);
	size_t bio_write_bit(bitstream_state_t *bs, int32_t bit); 
	size_t bio_write_int(bitstream_state_t *bs, 
			     int32_t val, int32_t num_bits);
	size_t bio_aligned_seek_bitstream(bitstream_state_t *bs, 
					  size_t skip, size_t whence);
	size_t bio_aligned_write_block(bitstream_state_t *bs, 
				       uint8_t* data, size_t size);
	size_t bio_get_pos(bitstream_state_t *bs);
    	size_t bio_get_size(bitstream_state_t *bs);
        size_t bio_aligned_direct_read(bitstream_state_t*bs, 
				       uint8_t **buf, size_t *size);
        bitstream_state_t* bio_init_dummy_bitstream(void);
        uint8_t* bio_get_buf(bitstream_state_t *bs);

#ifdef __cplusplus
}
#endif
#endif
