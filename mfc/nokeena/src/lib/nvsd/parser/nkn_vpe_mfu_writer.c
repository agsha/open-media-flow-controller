#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <getopt.h>

#include "nkn_vpe_mfu_writer.h"

#define VPE_SUCCESS 0
#ifdef _BIG_ENDIAN_
/* data -> o/p pointer where data has to be written
 * pos -> i/p position of data where value has to e written
 * val -> i/p actual value to be written
 */
static int32_t  write32_byte_swap( uint8_t *data,uint32_t pos,uint32_t
                                   val)
{
    data[pos] = val>>24;
    data[pos+1] = (val>>16)&0xFF;
    data[pos+2] = (val>>8)&0xFF;
    data[pos+3] = val&0xFF;
    return 1;
}
static int32_t write16_byte_swap( uint8_t *data,uint32_t pos,uint16_t
                                  val)
{
    data[pos] = val>>8;
    data[pos+1] = val&0xFF;
    return 1;
}
static int32_t write64_byte_swap( uint8_t *data,uint32_t pos,uint64_t
                                  val)
{
    data[pos] = val>>56;
    data[pos+1] = (val>>48)&0xFF;
    data[pos+2] = (val>>40)&0xFF;
    data[pos+3] = (val>>32)&0xFF;
    data[pos+4] = (val>>24)&0xFF;
    data[pos+5] = (val>>16)&0xFF;
    data[pos+6] = (val>>8)&0xFF;
    data[pos+7] = (val&0xFF);
    return 1;
}
static uint32_t uint32_byte_swap(uint32_t input){
    uint32_t ret=0;
    ret = (((input&0x000000FF)<<24) + ((input&0x0000FF00)<<8) +
           ((input&0x00FF0000)>>8) + ((input&0xFF000000)>>24));
    return ret;
}
#else
static int32_t  write32( uint8_t *data,uint32_t pos,uint32_t val)
{
    data[pos] = val&0xFF;
    data[pos+1] = (val>>8)&0xFF;
    data[pos+2] = (val>>16)&0xFF;
    data[pos+3] = (val>>24)&0xFF;
    return 1;
}
#endif


/*
 * mfub_box_size -> i/p size of that box
 * mfubox_header ->i/p pointer having the actual data to be written
 * into the box
 * mfubox_pos -> o/p indicates the pos where this box has tobe written
 */
int32_t
mfu_writer_mfub_box(uint32_t mfub_box_size, mfub_t* mfubox_header,
		   uint8_t* mfubox_pos)
{
    uint32_t pos =0;
    /* writting box_name */
    mfubox_pos[pos] = 'm';
    mfubox_pos[pos+1] = 'f';
    mfubox_pos[pos+2] = 'u';
    mfubox_pos[pos+3] = 'b';
    pos += BOX_NAME_SIZE;
#ifdef _BIG_ENDIAN_
    /* write box size */
    write32_byte_swap(mfubox_pos, pos, mfub_box_size);
    pos += BOX_SIZE_SIZE;
    /* writting actual mfu data */
    write16_byte_swap(mfubox_pos, pos,mfubox_header->version);
    pos+=2;
    write16_byte_swap(mfubox_pos, pos,mfubox_header->program_number);
    pos+=2;
    write16_byte_swap(mfubox_pos, pos,mfubox_header->stream_id);
    pos+=2;
    write16_byte_swap(mfubox_pos, pos,mfubox_header->flags);
    pos+=2;
    write32_byte_swap(mfubox_pos, pos,mfubox_header->sequence_num);
    pos+=4;
    write32_byte_swap(mfubox_pos, pos,mfubox_header->timescale);
    pos+=4;
    write32_byte_swap(mfubox_pos, pos,mfubox_header->duration);
    pos+=4;
    write32_byte_swap(mfubox_pos, pos,mfubox_header->audio_duration);
    pos+=4;
    write16_byte_swap(mfubox_pos, pos,mfubox_header->video_rate);
    pos+=2;
    write16_byte_swap(mfubox_pos, pos,mfubox_header->audio_rate);
    pos+=2;
    write16_byte_swap(mfubox_pos, pos,mfubox_header->mfu_rate);
    pos+=2;
    mfubox_pos[pos] = mfubox_header->video_codec_type;
    //pos+=1;
    mfubox_pos[pos] = mfubox_header->audio_codec_type;
    pos+=2;
    write32_byte_swap(mfubox_pos,
                      pos,mfubox_header->compliancy_indicator_mask);
    pos+=8;
    write64_byte_swap(mfubox_pos, pos,mfubox_header->offset_vdat);
    pos+=8;
    write64_byte_swap(mfubox_pos, pos,mfubox_header->offset_adat);
    pos+=8;
    write32_byte_swap(mfubox_pos, pos,mfubox_header->mfu_size);
    pos+=4;
#else//_BIG_ENDIAN_
    /* write box size */
    write32(mfubox_pos, pos, mfub_box_size);
    pos += BOX_SIZE_SIZE;
    /* writting actual mfu data */
    memcpy((mfubox_pos + pos), mfubox_header, mfub_box_size);
#endif//_BIG_ENDIAN_
    return VPE_SUCCESS;
}

/*
 * vdat_size -> i/p size of the box
 * vdat -> i/p pointer having the actual data tobe written
 * vdatbox_pos -> o/p indicates the pos where this box has tobe
 * written
 */
int32_t
mfu_writer_vdat_box(uint32_t vdat_size, uint8_t *vdat, uint8_t*
		   vdatbox_pos)
{
    uint32_t pos = 0;
    /* writting box_name */
    vdatbox_pos[pos] = 'v';
    vdatbox_pos[pos+1] = 'd';
    vdatbox_pos[pos+2] = 'a';
    vdatbox_pos[pos+3] = 't';
    pos += BOX_NAME_SIZE;
    /* write box size */
#ifdef _BIG_ENDIAN_
    write32_byte_swap(vdatbox_pos, pos, vdat_size);
#else
    write32(vdatbox_pos, pos, vdat_size);
#endif
    pos += BOX_SIZE_SIZE;
    /* writting actual vdat data */
    memcpy((vdatbox_pos + pos), vdat, vdat_size);
    return VPE_SUCCESS;
}
/*
 * adat_size -> i/p size of the box
 * adat -> i/p pointer having the actual data tobe written
 * adatbox_pos -> o/p indicates the pos where this box has tobe
 * written
 */
int32_t
mfu_writer_adat_box(uint32_t adat_size, uint8_t *adat, uint8_t*
		   adatbox_pos)
{
    uint32_t pos = 0;
    /* writting box_name */
    adatbox_pos[pos] = 'a';
    adatbox_pos[pos+1] = 'd';
    adatbox_pos[pos+2] = 'a';
    adatbox_pos[pos+3] = 't';
    pos += BOX_NAME_SIZE;
    /* write box size */
#ifdef _BIG_ENDIAN_
    write32_byte_swap(adatbox_pos, pos, adat_size);
#else
    write32(adatbox_pos, pos, adat_size);
#endif
    pos += BOX_SIZE_SIZE;
    /* writting actual vdat data */
    memcpy((adatbox_pos + pos), adat, adat_size);
    return VPE_SUCCESS;
}

/*
 * mfu_raw_box_size -> i/p size of the box
 * mfu_raw_box -> i/p pointer having the actual data tobe written
 * rwfgbox_pos -> o/p indicates the pos where this box has tobe
 * written
 * mfu_desc -> i/p mfu desriptor structure
 */
int32_t
mfu_writer_rwfg_box(uint32_t mfu_raw_box_size, mfu_rwfg_t
		   *mfu_raw_box, uint8_t* rwfgbox_pos,
		   mfu_desc_t* mfu_desc)
{
    uint32_t pos = 0;
#ifndef _BIG_ENDIAN_
    uint32_t temp_size = 0;
#endif
    /* writting box_name */
    rwfgbox_pos[pos] = 'r';
    rwfgbox_pos[pos+1] = 'w';
    rwfgbox_pos[pos+2] = 'f';
    rwfgbox_pos[pos+3] = 'g';
    pos += BOX_NAME_SIZE;
    /* write box size */
#ifdef _BIG_ENDIAN_
    write32_byte_swap(rwfgbox_pos, pos, mfu_raw_box_size);
#else
    write32(rwfgbox_pos, pos, mfu_raw_box_size);
#endif
    pos += BOX_SIZE_SIZE;
    /* write actual data into box */
    if (mfu_desc->is_video != 0) {
#ifdef _BIG_ENDIAN_
	write32_byte_swap(rwfgbox_pos, pos,
                          mfu_desc->mfu_raw_box->videofg_size);
        pos+=4;
        /* write uint_count,timescale,default unit duration,unit
	   size*/
        write32_byte_swap(rwfgbox_pos, pos,
			  mfu_raw_box->videofg.unit_count);
        pos+=4;
        write32_byte_swap(rwfgbox_pos, pos,
			  mfu_raw_box->videofg.timescale);
        pos+=4;
        write32_byte_swap(rwfgbox_pos, pos,
			  mfu_raw_box->videofg.default_unit_duration);
        pos+=4;
        write32_byte_swap(rwfgbox_pos, pos,
			  mfu_raw_box->videofg.default_unit_size);
        pos+=4;
#else
        write32(rwfgbox_pos, pos,
                mfu_desc->mfu_raw_box->videofg_size);
        pos+=4;
        temp_size = RAW_FG_FIXED_SIZE;
        memcpy((rwfgbox_pos + pos), &mfu_raw_box->videofg, temp_size);
        pos += temp_size;
#endif
        /* writting sample_duration and composition_duration,
           sample_sizes*/
        memcpy((rwfgbox_pos +pos),
               mfu_raw_box->videofg.sample_duration,
               3*4*mfu_raw_box->videofg.unit_count);
        pos += 3*4*mfu_raw_box->videofg.unit_count;
        /* writting codec_info_size */
#ifdef _BIG_ENDIAN_
        write32_byte_swap(rwfgbox_pos, pos,
                          mfu_raw_box->videofg.codec_info_size);
        pos += 4;
#else
        write32(rwfgbox_pos, pos,
                mfu_raw_box->videofg.codec_info_size);
        pos += 4;
#endif
        /* writting sps and pps info */
        memcpy((rwfgbox_pos +
		pos),mfu_raw_box->videofg.codec_specific_data,
               mfu_raw_box->videofg.codec_info_size);
        pos += mfu_raw_box->videofg.codec_info_size;
    }
    if (mfu_desc->is_audio != 0) {
#ifdef _BIG_ENDIAN_
        write32_byte_swap(rwfgbox_pos, pos,
                          mfu_desc->mfu_raw_box->audiofg_size);
        pos+=4;
        /* write uint_count,timescale,defaultunit duration,unit size*/
        write32_byte_swap(rwfgbox_pos, pos,
			  mfu_raw_box->audiofg.unit_count);
        pos+=4;
        write32_byte_swap(rwfgbox_pos, pos,
			  mfu_raw_box->audiofg.timescale);
        pos+=4;
        write32_byte_swap(rwfgbox_pos, pos,
			  mfu_raw_box->audiofg.default_unit_duration);
        pos+=4;
        write32_byte_swap(rwfgbox_pos, pos,
			  mfu_raw_box->audiofg.default_unit_size);
        pos+=4;
#else
        write32(rwfgbox_pos, pos,
                mfu_desc->mfu_raw_box->audiofg_size);
        pos+=4;
        temp_size = RAW_FG_FIXED_SIZE;
        memcpy((rwfgbox_pos + pos), &mfu_raw_box->audiofg, temp_size);
        pos += temp_size;
#endif
        /* writting sample_duration and composition_duration,
	   sample_sizes*/
        memcpy((rwfgbox_pos +
		pos),mfu_raw_box->audiofg.sample_duration,
               3*4*mfu_raw_box->audiofg.unit_count);
        pos += 3*4*mfu_raw_box->audiofg.unit_count;
        /* writting codec_info_size */
#ifdef _BIG_ENDIAN_
        write32_byte_swap(rwfgbox_pos, pos,
                          mfu_raw_box->audiofg.codec_info_size);
        pos += 4;
#else
        write32(rwfgbox_pos, pos,
                mfu_raw_box->audiofg.codec_info_size);
        pos += 4;
#endif
        /* writting sps and pps info */
        memcpy((rwfgbox_pos + pos),
    mfu_raw_box->audiofg.codec_specific_data,
	       mfu_raw_box->audiofg.codec_info_size);
	pos += mfu_raw_box->audiofg.codec_info_size;
    }
    return VPE_SUCCESS;
}

int32_t
mfu_writer_get_sample_duration(uint32_t *sample_duration, uint32_t *PTS,
                        uint32_t num_AU)
{
    uint32_t i=0;
#ifdef _BIG_ENDIAN_
    for(i=0;i<(num_AU-1);i++){
        sample_duration[i] = uint32_byte_swap(PTS[i+1] -PTS[i]);
    }
    //last sample_duration
    sample_duration[num_AU-1] = sample_duration[num_AU-2];
#else
    for(i=0;i<(num_AU-1);i++){
        sample_duration[i] = PTS[i+1] -PTS[i];
    }
    //last sample_duration
    sample_duration[num_AU-1] = sample_duration[num_AU-2];
#endif


    return VPE_SUCCESS;
}
