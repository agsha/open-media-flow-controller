/*
 *
 * Filename:  nkn_vpe_mp4_parser.c
 * Date:      2009/03/23
 * Module:    FLV pre - processing for Smooth Flow
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

/***************************************************************
 *                                                             *
 *             IMPLETMENTS ISO/IEC 14492-12                    *
 *       (MP4 Multimedia Container Format Parser)              *
 *                                                             * 
 **************************************************************/

#include "nkn_vpe_mp4_parser_legacy.h"
#include "nkn_vpe_types.h"
#include "nkn_vpe_bitstream_io.h"
#include "nkn_vpe_bitstream.h"
#include "nkn_vpe_backend_io_plugin.h"
#include <string.h>

int32_t 
read_next_root_level_box(void *desc, io_funcs *iof, box_t *p_box)
{
    int32_t error_no;
    int32_t size;

    error_no = 0;
    size = 0;

    //initiazlize
    memset(p_box, 0, sizeof(box_t));

    //read box size
    iof->nkn_read(&(p_box->short_size), 1, sizeof(int32_t), desc);
    size = nkn_vpe_swap_uint32(p_box->short_size);

    //if box has size greater than > 2GB, then read the long size
    if(p_box->short_size == 1){
       iof->nkn_read(&(p_box->long_size), 1, sizeof(uint64_t), desc);
       size = nkn_vpe_swap_uint64(p_box->long_size);
    }

    //read the box type
    iof->nkn_read(&(p_box->type), 1, sizeof(int32_t), desc);

    return size;

}

int32_t
read_next_child(uint8_t* p_data, box_t *p_box)
{
    int32_t error_no;
    int32_t size;
    uint8_t *p_buffer;

    error_no = 0;
    size = 0;
    p_buffer = p_data;

    //initiazlize
    memset(p_box, 0, sizeof(box_t));

    //read box size
    p_box->short_size  = *((int32_t*)(p_buffer));
    p_buffer += sizeof(int32_t);
    size = nkn_vpe_swap_uint32(p_box->short_size);

    //if box has size greater than > 2GB, then read the long size
    if(p_box->short_size == 1){
	p_box->short_size  = *((int64_t*)(p_buffer));
	p_buffer += sizeof(int64_t);
        size = nkn_vpe_swap_uint64(p_box->long_size);
    }

    //read the box type
    memcpy(p_box->type, p_buffer, 4);
    p_buffer += sizeof(int32_t);

    return size;

}

int32_t 
write_box(void *desc, io_funcs *iof, box_t *p_box, uint8_t *p_box_data)
{
    int32_t error_no;
    int32_t size;

    error_no = 0;
    size = 0;

    //read box size
    size = nkn_vpe_swap_uint32(p_box->short_size);
    iof->nkn_write(&(p_box->short_size), 1, sizeof(int32_t), desc);

    //if box has size greater than > 2GB, then read the long size
    if(p_box->short_size == 1){
       size = nkn_vpe_swap_uint64(p_box->long_size);
       iof->nkn_write(&(p_box->long_size), 1, sizeof(uint64_t), desc);
    }

    //write the box type
    iof->nkn_write(&(p_box->type), 1, sizeof(int32_t), desc);

    //write the box contents
    iof->nkn_write(p_box_data, 1, size - 8, desc);

    return size;

}

int32_t
box_type_moof(box_t *box)
{
    const char moof[]={'m','o','o','f'};

    return !memcmp(box->type, moof, 4);
}


int32_t 
check_box_type(box_t *box, const char *id)
{
    return !memcmp(box->type, id, 4);
}

void *
mdhd_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size)
{
    uint8_t *p_buf;
    mdhd_t *mdhd;

    p_buf = p_data;
    size = size;
    box_hdr_size = box_hdr_size;
    mdhd = NULL;

    mdhd = (mdhd_t*)calloc(1, sizeof(mdhd_t));

    p_buf+=8;
    mdhd->version = read_byte(p_buf);
    p_buf += 1;
    mdhd->flags = read_signed_24bit(p_buf);
    p_buf += 3;
    if(mdhd->version == 0){
	mdhd->creation_time = read_unsigned_dword(p_buf);
	p_buf += 4;
	mdhd->modification_time = read_unsigned_dword(p_buf);
	p_buf += 4;
	mdhd->timescale = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
        p_buf += 4;
	mdhd->duration = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
        p_buf += 4;
    }else{
	mdhd->creation_time = read_unsigned_qword(p_buf);
	p_buf += 8;
	mdhd->modification_time = read_unsigned_qword(p_buf);
	p_buf += 8;
	mdhd->timescale = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
        p_buf += 4;
	mdhd->duration = nkn_vpe_swap_uint64(read_unsigned_qword(p_buf));
        p_buf += 8;
    }

    return mdhd;
}

void *
tfhd_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size)
{
    tfhd_t *tfhd;
    uint8_t *p_buf;

    box_hdr_size = box_hdr_size;
    size = size;

    p_buf = p_data;
    tfhd = (tfhd_t*)malloc(sizeof(tfhd_t));
    memset(tfhd, 0, sizeof(tfhd_t));
    p_buf += 8;

    tfhd->version = read_byte(p_buf);
    p_buf += 1;
    tfhd->flags = read_signed_24bit(p_buf);
    p_buf += 3;

    tfhd->track_id = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
    p_buf += 4;
    //    tfhd->flags = nkn_vpe_swap_uint32(tfhd->flags);

    if(tfhd->flags & 0x00000001){
	tfhd->base_data_offset = read_unsigned_qword(p_buf);
	p_buf += 8;
    }else if(tfhd->flags & 0x00000002){
	tfhd->sample_desc_index = read_unsigned_dword(p_buf);
	p_buf += 4;
    }else if(tfhd->flags & 0x00000008){
	tfhd->def_sample_duration = read_unsigned_dword(p_buf);
        p_buf += 4;
    }else if(tfhd->flags & 0x00000010){
	tfhd->def_sample_size = read_unsigned_dword(p_buf);
        p_buf += 4;
    }else if(tfhd->flags & 0x00000020){
	tfhd->def_sample_flags = read_unsigned_dword(p_buf);
        p_buf += 4;
    }else if(tfhd->flags & 0x00010000){
	return NULL;
    }


    return tfhd;    
}


void *
trun_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size)
{
    trun_t *trun;
    uint8_t *p_buf;
    uint32_t i;

    box_hdr_size = box_hdr_size;
    size = size;
    i = 0;

    p_buf = p_data;
    trun = (trun_t*)malloc(sizeof(trun_t));
    memset(trun, 0, sizeof(trun_t));
    p_buf += 8;

    trun->version = read_byte(p_buf);
    p_buf += 1;
    trun->flags = read_signed_24bit(p_buf);
    p_buf += 3;

    trun->sample_count = read_unsigned_dword(p_buf);
    p_buf += 4;
    trun->sample_count = nkn_vpe_swap_uint32(trun->sample_count);

    trun->tr_stbl = (trun_stable_t*)calloc(trun->sample_count, sizeof(trun_stable_t));

    if(trun->flags & 0x00000001){
	trun->data_offset = nkn_vpe_swap_int32(read_signed_dword(p_buf));
	p_buf += 4;
    }

    if(trun->flags & 0x00000004){
	trun->first_sample_flags =nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
	p_buf += 4;
    }

    for(i =0;i < trun->sample_count;i++){
	if(trun->flags & 0x00000100){
	    trun->tr_stbl[i].sample_duration = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
	    p_buf += 4;
	}
	if(trun->flags & 0x00000200){
    	    trun->tr_stbl[i].sample_size = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
	    p_buf += 4;
	}
    }

    return trun;
    
}


void*
tkhd_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size)
{
    tkhd_t *tkhd;
    uint8_t *p_buf;
    uint32_t i;

    box_hdr_size = box_hdr_size;
    size = size;
    i = 0;

    p_buf = p_data;
    tkhd = (tkhd_t*)malloc(sizeof(tkhd_t));
    memset(tkhd, 0, sizeof(tkhd_t));
    p_buf += 8;

    tkhd->version = read_byte(p_buf);
    p_buf += 1;
    tkhd->flags = read_signed_24bit(p_buf);
    p_buf += 3;

    if(tkhd->version == 1){
	tkhd->creation_time = nkn_vpe_swap_uint64(read_unsigned_qword(p_buf));
	p_buf += 8;
	tkhd->modification_time = nkn_vpe_swap_uint64(read_unsigned_qword(p_buf));
        p_buf += 8;
	tkhd->track_id = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
        p_buf += 4;
	tkhd->reserved = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
        p_buf += 4;
	tkhd->duration = nkn_vpe_swap_uint64(read_unsigned_qword(p_buf));
        p_buf += 8;
    }else{
        tkhd->creation_time = nkn_vpe_swap_uint32(read_unsigned_qword(p_buf));
        p_buf += 4;
        tkhd->modification_time = nkn_vpe_swap_uint32(read_unsigned_qword(p_buf));
        p_buf += 4;
        tkhd->track_id = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
        p_buf += 4;
        tkhd->reserved = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
        p_buf += 4;
        tkhd->duration = nkn_vpe_swap_uint64(read_unsigned_qword(p_buf));
        p_buf += 8;
    }

    memcpy(tkhd->reserved1, p_buf, sizeof(int32_t)*2);
    p_buf += (sizeof(int32_t)*2);

    tkhd->layer = read_word(p_buf);
    p_buf += 2;

    tkhd->alternate_group =  read_word(p_buf);
    p_buf += 2;

    tkhd->volume =  read_word(p_buf);
    p_buf += 2;

    tkhd->reserved2 =  read_word(p_buf);
    p_buf += 2;

    memcpy(tkhd->matrix, p_buf, sizeof(uint32_t)*9);
    p_buf += sizeof(uint32_t)*9;

    tkhd->width = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
    p_buf += 4;

    tkhd->height = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
    p_buf += 4;

    return tkhd;
}

double
get_sequence_duration(mdhd_t *mdhd)
{
    uint64_t timescale, duration;

    duration = (mdhd->duration);
    timescale = (mdhd->timescale);

    return ((double)(duration)/timescale);
}

int32_t 
is_container_box(box_t *box)
{
    uint32_t i = 0;
    char root_boxes[][4]={{'m','o','o','v'},
			 {'t','r','a','k'},
			 {'e','d','t','s'},
			 {'m','d','i','a'},
			 {'m','o','o','f'},
			 {'t','r','a','f'}};
    

    for(i = 0; i < sizeof(root_boxes);i++){
	if(check_box_type(box, root_boxes[i])){
	    return 1;
	}
    }

    return 0;
	   
}


/* read functions */
uint8_t 
read_byte(uint8_t *p_buf)
{
    return p_buf[0];
}

uint16_t
read_word(uint8_t *p_buf)
{
    return p_buf[0] <<8 | p_buf[1];
}

int32_t
read_signed_dword(uint8_t *p_buf)
{ 
    return *((int32_t*)(p_buf));
}

int64_t
read_signed_qword(uint8_t *p_buf)
{
    return *((int64_t*)(p_buf));
}

int32_t
read_signed_24bit(uint8_t *p_buf)
{
    return p_buf[0] << 16 | p_buf[1] << 8 | p_buf[2] << 0;
}

uint32_t
read_unsigned_dword(uint8_t *p_buf)
{ 
    return *((uint32_t*)(p_buf));
}

uint64_t
read_unsigned_qword(uint8_t *p_buf)
{
    return *((uint64_t*)(p_buf));
}
