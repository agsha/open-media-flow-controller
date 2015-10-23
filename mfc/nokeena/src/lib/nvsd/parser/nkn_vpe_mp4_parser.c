/*
 *
 * Filename:  nkn_vpe_mp4_parser.c
 * Date:      2009/03/23
 * Module:    MP4 parsing API implementation
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

#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_types.h"
#include "nkn_memalloc.h"
#include <string.h>

int32_t 
read_next_root_level_box(void *desc, size_t input_size, box_t *p_box, size_t *bytes_read)
{
    int32_t error_no;
    int32_t size;
    uint8_t *p_src;

    error_no = 0;
    size = 0;
    p_src = (uint8_t*)(desc);

    //initiazlize
    memset(p_box, 0, sizeof(box_t));

    //read box size
    //iof->nkn_read(&(p_box->short_size), 1, sizeof(int32_t), desc);
    p_box->short_size = *((int32_t*)(p_src));
    p_src += sizeof(int32_t);

    p_box->short_size = nkn_vpe_swap_uint32(p_box->short_size);
    size = p_box->short_size;
    //read the box type
    //   iof->nkn_read(&(p_box->type), 1, sizeof(int32_t), desc);
    memcpy(&p_box->type, p_src, 4);
    p_src += sizeof(int32_t);

    //if box has size greater than > 2GB, then read the long size
    if(p_box->short_size == 1){
	//       iof->nkn_read(&(p_box->long_size), 1, sizeof(uint64_t), desc);
	p_box->long_size = *((int64_t*)(p_src));
	p_src += sizeof(int64_t);
        p_box->long_size = nkn_vpe_swap_uint64(p_box->long_size);
	// This will have issue for box larger than 2GB
	size = (int32_t) p_box->long_size;
    }

    *bytes_read = (size_t)(p_src - (uint8_t*)(desc));
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

/*
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
*/

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

    mdhd = (mdhd_t*)nkn_calloc_type(1, 
				    sizeof(mdhd_t),
				    mod_vpe_mp4_mdhd_t);
    if (!mdhd) {
	return NULL;
    }

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
mvhd_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size)
{
    uint8_t *p_buf;
    mvhd_t *mdhd;
    int32_t tmp;

    p_buf = p_data;
    size = size;
    box_hdr_size = box_hdr_size;
    mdhd = NULL;

    mdhd = (mvhd_t*)nkn_calloc_type(1, 
				    sizeof(mvhd_t),
				    mod_vpe_mp4_mvhd_t);
    if (!mdhd) {
	return NULL;
    }

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
    p_buf += 76;

    tmp = read_signed_dword(p_buf);
    mdhd->next_track_id = nkn_vpe_swap_int32(tmp);
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
    tfhd = (tfhd_t*)nkn_malloc_type(sizeof(tfhd_t),
				    mod_vpe_mp4_tfhd_t);
    if (!tfhd) {
	return NULL;
    }
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
	tfhd->base_data_offset = \
	    nkn_vpe_swap_uint64(tfhd->base_data_offset);
	p_buf += 8;
    }else if(tfhd->flags & 0x00000002){
	tfhd->sample_desc_index = read_unsigned_dword(p_buf);
	tfhd->sample_desc_index = \
	    nkn_vpe_swap_uint32(tfhd->sample_desc_index);
	p_buf += 4;
    }else if(tfhd->flags & 0x00000008){
	tfhd->def_sample_duration = read_unsigned_dword(p_buf);
	tfhd->def_sample_duration = \
	    nkn_vpe_swap_uint32(tfhd->def_sample_duration);
        p_buf += 4;
    }else if(tfhd->flags & 0x00000010){
	tfhd->def_sample_size = read_unsigned_dword(p_buf);
	tfhd->def_sample_size = \
	    nkn_vpe_swap_uint32(tfhd->def_sample_size);
        p_buf += 4;
    }else if(tfhd->flags & 0x00000020){
	tfhd->def_sample_flags = read_unsigned_dword(p_buf);
	tfhd->def_sample_flags = \
	    nkn_vpe_swap_uint32(tfhd->def_sample_flags);
        p_buf += 4;
    }else if(tfhd->flags & 0x00010000){
	return NULL;
    }


    return tfhd;    
}

void*
hdlr_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size) 
{
    
    hdlr_t *hdlr;
    uint8_t *p_buf;
    int32_t i;

    p_buf = p_data;
    hdlr = (hdlr_t*)nkn_malloc_type(sizeof(hdlr_t),
				    mod_vpe_mp4_hdlr_t);
    if (!hdlr) {
	return NULL;
    }
    memset(hdlr, 0, sizeof(hdlr_t));
    p_buf += 12;//Full Box'

    hdlr->pre_defined = read_unsigned_dword(p_buf);
    p_buf += 4;

    hdlr->handler_type = read_unsigned_dword(p_buf);
    p_buf += 4;

    memset(hdlr->reserved, 0, sizeof(hdlr->reserved));
    p_buf += 12;

    i = 0;
    hdlr->name = (char*)nkn_malloc_type(box_hdr_size,
					mod_vpe_mp4_box_data);
    if (hdlr->name == NULL) {
	free(hdlr);
	return NULL;
    }
    while((*p_buf) != '\0') {
	hdlr->name[i]=*p_buf;
	p_buf++;
	i++;
    }

    return hdlr;

}

void*
stsd_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size)
{
    uint8_t *p_buf;
    stsd_t *stsd;
    uint32_t i;

    i = 0;
    p_buf = p_data;
    stsd = (stsd_t*)nkn_malloc_type(sizeof(stsd_t),
				    mod_vpe_mp4_stsd_t);
    if (!stsd) {
	return NULL;
    }
    memset(stsd, 0, sizeof(stsd_t));
    p_buf += 12;//Full box

    stsd->entry_count = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
    p_buf += 4;

    for(i = 0; i < stsd->entry_count; i++) {
    }
    
    return stsd;
}

void*
stsz_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size)
{
    uint8_t *p_buf;
    uint32_t i;
    stsz_t *stsz;

    p_buf = p_data;
    i = 0;
    stsz = NULL;

    stsz = (stsz_t*)nkn_malloc_type(sizeof(stsz_t),
				    mod_vpe_mp4_stsz_t);
    if (!stsz) {
	return NULL;
    }
    memset(stsz, 0, sizeof(stsz_t));
    p_buf += 12;//Full Box

    stsz->sample_size = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
    p_buf += 4;
    stsz->sample_count = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
    p_buf+=4;

    if(stsz->sample_size == 0) {
	    stsz->entry_size =
	    (uint32_t*)nkn_malloc_type(sizeof(uint32_t) *
				       stsz->sample_count,
				       mod_vpe_mp4_stsz_entries);
	    if (stsz->entry_size == NULL) {
		free(stsz);
		return NULL;
	    }
	for(i = 0;i < stsz->sample_count;i++) {
	    stsz->entry_size[i] = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
	    p_buf += 4;
	}
    }

    return stsz;
}

void*
hdlr_cleanup(void *box) {
    hdlr_t *hdlr = (hdlr_t*)(box);
    if(hdlr->name) {
	free(hdlr->name);
    }
    if(hdlr) {
	free(hdlr);
    }
    hdlr = NULL;

    return hdlr;
}

void*
stsz_cleanup(void *box)
{
    stsz_t *stsz = (stsz_t*)box;
    
    if(stsz && stsz->entry_size) {
	free(stsz->entry_size);
    }

    if(stsz) {
	free(stsz);
	stsz = NULL;
    }
    return stsz;
}

void *
stsc_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size)
{
    stsc_t *stsc;
    uint8_t *p_buf;
    uint32_t i;

    box_hdr_size = box_hdr_size;
    size = size;
    i = 0;

    p_buf = p_data;
    stsc = (stsc_t*)nkn_malloc_type(sizeof(stsc_t), 
				    mod_vpe_mp4_stsc_t);
    if (!stsc) {
	return NULL;
    }
    memset(stsc, 0, sizeof(stsc_t));
    p_buf += 12;//Full Box

    stsc->entry_count = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
    p_buf += 4;

    stsc->info =
	    (chunk_info_t*)nkn_malloc_type(sizeof(chunk_info_t)*stsc->entry_count,
					   mod_vpe_mp4_stsc_entries);
    if (stsc->info == NULL) {
	free(stsc);
	return NULL;
    }
    for(i = 0;i < stsc->entry_count;i++){
	stsc->info[i].first_chunk = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
	p_buf += 4;

	stsc->info[i].samples_per_chunk = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
	p_buf += 4;

	stsc->info[i].sample_description_index = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
	p_buf += 4;
    }

    return stsc;
}

void *
stsc_cleanup(void *box)
{
    stsc_t *stsc = (stsc_t*)box;
    if(stsc && stsc->info) {
	free(stsc->info);
    }

    if(stsc) {
	free(stsc);
    }

    stsc = NULL;

    return stsc;
}

void *
stco_reader(uint8_t *p_data, uint64_t size, uint32_t box_hdr_size)
{
    stco_t *stco;
    uint8_t *p_buf;
    uint32_t i;

    box_hdr_size = box_hdr_size;
    size = size;
    i = 0;

    p_buf = p_data;
    stco = (stco_t*)nkn_malloc_type(sizeof(stco_t), 
				    mod_vpe_mp4_stco_t);
    if (!stco) {
	return NULL;
    }
    memset(stco, 0, sizeof(stco_t));
    p_buf += 12;//Full Box

    stco->entry_count = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
    p_buf += 4;

    stco->chunk_offset =
	    (uint32_t*)nkn_malloc_type(sizeof(uint32_t)*stco->entry_count,
				       mod_vpe_mp4_stco_entries); 
    if (stco->chunk_offset == NULL) {
	free(stco);
	return NULL;
    }
    for(i = 0; i < stco->entry_count;i++) {
	stco->chunk_offset[i] = read_unsigned_dword(p_buf);
	p_buf += 4;
    }

    return stco;
}

void *
stco_cleanup(void *box)
{
    stco_t *stco = (stco_t*)box;

    if(stco && stco->chunk_offset) { 
	free(stco->chunk_offset);
    }

    if(stco) { 
	free(stco);
	stco = NULL;
    }

    return stco;
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
    trun = (trun_t*)nkn_malloc_type(sizeof(trun_t),
				    mod_vpe_mp4_trun_t);
    if (!trun) {
	return NULL;
    }
    memset(trun, 0, sizeof(trun_t));
    p_buf += 8;

    trun->version = read_byte(p_buf);
    p_buf += 1;
    trun->flags = read_signed_24bit(p_buf);
    p_buf += 3;

    trun->sample_count = read_unsigned_dword(p_buf);
    p_buf += 4;
    trun->sample_count = nkn_vpe_swap_uint32(trun->sample_count);

    trun->tr_stbl = (trun_stable_t*)nkn_calloc_type(trun->sample_count,
						    sizeof(trun_stable_t),
						    mod_vpe_mp4_trun_entries);
    if (trun->tr_stbl == NULL) {
	free(trun);
	return NULL;
    }

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

	if(trun->flags & 0x00000400){
            trun->tr_stbl[i].sample_flags = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
            p_buf += 4;
        }
        if(trun->flags & 0x00000800){
            trun->tr_stbl[i].scto = nkn_vpe_swap_uint32(read_unsigned_dword(p_buf));
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
    tkhd = (tkhd_t*)nkn_malloc_type(sizeof(tkhd_t),
				    mod_vpe_mp4_tkhd_t);
    if (!tkhd) {
	return NULL;
    }
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

void *
box_cleanup(void *box)
{
    if(box) {
	free(box);
	box = NULL;
    }
    return box;
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
			  {'t','r','a','f'},
			  {'m','i','n','f'},
			  {'s','t','b','l'},
			  {'m','v','e','x'},
			  {'m','d','a','t'},
			  {'m','f','r','a'},
			  {'f','r','e','e'},
			  {'s','k','i','p'},
			  {'m','e','t','a'},
			  {'d','i','n','f'},
			  {'u','d','t','a'},
    };
    for(i = 0; i < sizeof(root_boxes)/4;i++){
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
