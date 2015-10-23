/*
 *
 * Filename:  nkn_vpe_bitio.c
 * Date:      2009/03/28
 * Module:    bitwise I/O operations
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include "nkn_vpe_bitio.h"
#include "nkn_memalloc.h"

#include <string.h>

const uint8_t bitmask[] = {0x80, 0x40, 0x20, 0x10,
			   0x8, 0x4, 0x2, 0x1};

bitstream_state_t*
bio_init_bitstream(uint8_t *data, size_t size)
{
	bitstream_state_t *bs;
	bs = (bitstream_state_t*)nkn_calloc_type(1, 
						 sizeof(bitstream_state_t),
						 mod_vpe_nkn_bitstream_t);
	if (bs == NULL) {
	    return NULL;
	}
	bs->data = (int8_t*)data;
	bs->pos = 0;
	bs->bits = 0;
	bs->size = size;
	bs->curr = data[0];

	return bs;
}

bitstream_state_t* 
bio_init_safe_bitstream(uint8_t *data, size_t size)
{
	bitstream_state_t *bs;
	bs = (bitstream_state_t*)nkn_calloc_type(1, 
						 sizeof(bitstream_state_t),
						 mod_vpe_nkn_bitstream_t);
	bs->data = (int8_t*)data;
	bs->pos = 0;
	bs->bits = 0;
	bs->size = size;
	bs->curr = data[0];
	pthread_mutex_init(&bs->ref_cnt_mutex, NULL);
	
	return bs;
}

bitstream_state_t*
bio_init_dummy_bitstream(void)
{
	bitstream_state_t *bs;
	bs = (bitstream_state_t*)nkn_calloc_type(1, 
						 sizeof(bitstream_state_t),
						 mod_vpe_nkn_bitstream_t);
	bs->dummy_bitstream = 1;
	return bs;
}

void
bio_reinit_bitstream(bitstream_state_t *bs, uint8_t *data, size_t size)
{

	bs->data = (int8_t*)data;
	bs->pos = 0;
	bs->bits = 0;
	bs->size = size;
	bs->curr = data[0];

}

size_t
bio_get_remaining_bytes(bitstream_state_t *bs) 
{
	return bs->size - bs->pos;
}

size_t
bio_aligned_direct_read(bitstream_state_t*bs, uint8_t **buf, size_t *size)
{
    uint8_t *ptr;

    /*adjust if unaligned */
    bs->pos += (bs->bits!=0);
    ptr = (uint8_t*)bs->data;

    if ( (ptr + bs->pos + (*size)) > (ptr + bs->size) ) {
	*size = 0;
	return 0;
    }

    *buf = ptr + bs->pos;
    bs->pos += (*size) - (bs->bits!=0);

    return *size;
}

uint8_t* 
bio_get_buf(bitstream_state_t *bs)
{
    return (uint8_t*)(bs->data);
}

size_t
bio_aligned_read(bitstream_state_t *bs, uint8_t* buf, size_t size)
{
	uint8_t *ptr;

	/*adjust if unaligned */
	bs->pos += (bs->bits!=0);

	ptr = (uint8_t*)bs->data + bs->pos;
	memcpy(buf, ptr, size);
	bs->pos += size-(bs->bits!=0);

	return size;
}

uint8_t
bio_read_bit(bitstream_state_t *bs) 
{

	if(bs->bits == 8) {
		bs->curr = bs->data[++bs->pos];
		bs->bits = 0;
	}
    
	return (uint8_t)(bs->curr & bitmask[bs->bits++]) ? 1 : 0;
 
}

uint8_t
bio_read_byte(bitstream_state_t *bs)
{
	uint8_t val;
	int32_t i;

	val = 0;
	i = 0;

	if(bs->bits == 8) {
		val = bs->data[++bs->pos];
		bs->bits = 0;
		return val;
	}

	for(i=0;i < 7;i++) {
		val = val<<1;
		val |= bio_read_bit(bs);
	}
	return val;
}

uint32_t
bio_read_bits(bitstream_state_t *bs, int32_t bits_to_read)
{
	uint32_t val;

	val = 0;
	while(bits_to_read-- > 0) {
		val = val << 1;
		val |= bio_read_bit(bs);
	}

	return val;
}

uint64_t
bio_read_bits_to_uint64(bitstream_state_t *bs, int32_t bits_to_read)
{
	uint64_t val;
	while(bits_to_read-- > 0) {
		val = val << 1;
		val |= bio_read_bit(bs);
	}

	return val;
}

void
bio_flush_byte(bitstream_state_t *bs)
{
	bs->data[bs->pos] = (uint8_t)(bs->curr);
	bs->pos++;
}

size_t 
bio_write_bit(bitstream_state_t *bs, int32_t bit) 
{
	bs->curr <<= 1;
	bs->curr |= bit;
	if(++bs->bits == 8) {
		bio_flush_byte(bs);
		bs->bits = 0;
		bs->curr = 0;
	}
    
	return bit;
}

size_t
bio_write_int(bitstream_state_t *bs, int32_t val, int32_t num_bits)
{
	int32_t nb;
	int32_t value;

	nb = num_bits;
	value = val;

	value = value << (sizeof(int32_t)*8 - nb);
	while(--nb >= 0) {
		bio_write_bit(bs, value < 0);
		value = value << 1;
	}

	return num_bits;
}

size_t
bio_aligned_write_block(bitstream_state_t *bs, uint8_t* data, size_t size)
{
	if(bs->pos + size < bs->size) {
		memcpy(bs->data + bs->pos, data, size);
		bs->pos += size;
		return size;
	} else if( bs->pos + size == bs->size) {
		memcpy(bs->data + bs->pos, data, size);
		bs->pos += size;
		return 0;

	}else {
	    #if 0 
	    if (!bs->dummy_bitstream) {
		bs->data = nkn_realloc_type(bs->data, bs->size *2,
					    mod_vpe_nkn_bitstream_t);
		bs->size = bs->size * 2;
		memcpy(bs->data + bs->pos, data, size);
		bs->pos += size;
	    }
	    #endif
	    return -1;
	}

	return size;
}

size_t
bio_aligned_seek_bitstream(bitstream_state_t *bs, size_t skip, size_t whence)
{
	
	//supports only aligned skip. data needs to be aligned

	switch(whence) {
		case SEEK_SET:
			bs->curr = bs->data[skip];
			bs->pos = skip;
			bs->bits = 0;
			break;
		case SEEK_CUR:
			bs->curr = bs->curr + skip;
			bs->pos += skip;
			bs->bits = 0;
			break;
		case SEEK_END:
			bs->curr = bs->data[bs->size] + skip;
			bs->pos = bs->size + skip;
			bs->bits = 0;
			break;
	}

	return skip;
}

void
bio_close_bitstream(bitstream_state_t *bs)
{
	free(bs);
	bs = NULL;
}

size_t
bio_get_pos(bitstream_state_t *bs)
{

	return bs->pos;
}

size_t
bio_get_size(bitstream_state_t *bs) 
{
	return bs->size;
}

uint64_t bio_add_ref(bitstream_state_t *bs)
{
    uint64_t rv;
    pthread_mutex_lock(&bs->ref_cnt_mutex);
    bs->ref_cnt++;
    rv = bs->ref_cnt;
    pthread_mutex_unlock(&bs->ref_cnt_mutex);

    return rv;
}

uint64_t bio_dec_ref(bitstream_state_t *bs)
{
    uint64_t rv;
    pthread_mutex_lock(&bs->ref_cnt_mutex);
    bs->ref_cnt--;
    rv = bs->ref_cnt;
    pthread_mutex_unlock(&bs->ref_cnt_mutex);

    return rv;
}

uint64_t bio_get_ref_cnt(bitstream_state_t *bs)
{

    uint64_t rv;
    pthread_mutex_lock(&bs->ref_cnt_mutex);
    rv = bs->ref_cnt;
    pthread_mutex_unlock(&bs->ref_cnt_mutex);

    return rv;
}
