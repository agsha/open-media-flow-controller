#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <getopt.h>
#include <math.h>

#include "nkn_vpe_types.h"
#include "nkn_vpe_mfp_ts2mfu.h"
//#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_mfp_ts.h"
//#include "mfu2iphone.h"
#include "nkn_vpe_utils.h"
#include "nkn_memalloc.h"
#include "nkn_vpe_mfu_defs.h"
#include "nkn_vpe_mfu_parser.h"


//#define DEBUG_MFU_PARSER_LOG


#if defined(DEBUG_MFU_PARSER_LOG)

#define A_(x) x
#define B_(x) x
#define C_(x) x


#else

#define A_(x) 
#define B_(x)
#define C_(x)

#endif



#define FIXED_BOX_SIZE 8


#ifdef _BIG_ENDIAN_
static uint32_t _read32_byte_swap(uint8_t *buf, int32_t pos)
{
    return
	(buf[pos]<<24)|(buf[pos+1]<<16)|(buf[pos+2]<<8)|(buf[pos+3]);
}
static uint16_t _read16_byte_swap(uint8_t *buf, int32_t pos)
{
    return (buf[pos]<<8)|(buf[pos+1]);
}

static uint64_t _read64_byte_swap(uint8_t *buf, int32_t pos)
{

    return nkn_vpe_swap_uint64(*((uint64_t*)(&buf[pos])));
#if 0
    uint32_t temp, temp_1;
 
    temp =
	(buf[pos]<<24)|(buf[pos+1]<<16)|(buf[pos+2]<<8)|(buf[pos+3]);
    temp_1 =
	(buf[pos+4]<<24)|(buf[pos+5]<<16)|(buf[pos+6]<<8)|(buf[pos+7]);
    return (uint64_t)(temp | temp_1);
#endif
}
#else
static uint32_t _read32(uint8_t *buf, int32_t pos)
{
    return
	(buf[pos+3]<<24)|(buf[pos+2]<<16)|(buf[pos+1]<<8)|(buf[pos]);
}
#endif



/* 
   This function is used to parse the mfu and update the elements of mfub
   box, rwfg box, vdat box, adat box to local structure like vmd,
   amd,mfu_header.
   mfu_data_orig -> i/p pointer pointing to mfu box
   mfu_data_size -> i/p indicates the size of mfu
   vmd ->o/p structure ponintg to  video related details
   amd ->o/p pointing to audio related dtails
   sps_pps -> o/p pointer for sps and pps data
   sps_pps_size -> o/p will have the sps and pps sixe
   is_audio -> o/p will indicate whether audio is present(adat)
   is_video -> o/p will indicate whether video is present(vdat)
 */
int32_t mfu_parse(unit_desc_t* vmd, unit_desc_t* amd,
	uint8_t *mfu_data_orig,uint32_t mfu_data_size, 
	uint8_t *sps_pps, uint32_t  *sps_pps_size, 
	uint32_t *is_audio,uint32_t *is_video, 
	mfub_t *mfu_header)
{
    uint32_t mfub_box_size = 0, rwfg_box_size = 0, mfub_pos = 0;
    uint32_t rwfg_pos = 0,vdat_pos =0,adat_pos =0;
    uint32_t vdat_box_size = 0, adat_box_size = 0;
    //uint8_t is_big_endian = 0;
    uint32_t act_vdat_box_size = 0;
    uint32_t ret;

    /* get the mfub box fully */
    ret = mfu_get_mfub_box(mfu_data_orig, mfub_pos,
	    mfu_header);
    if (ret == VPE_ERROR)
	return VPE_ERROR;
    mfub_box_size = ret;
    
    if (mfu_header->offset_vdat == 0xffffffffffffffff &&
	mfu_header->offset_adat == 0xffffffffffffffff) {
	*is_video = 0;
	*is_audio = 0;
	return VPE_SUCCESS;
    }
    *is_video = mfu_header->offset_vdat;
    *is_audio = mfu_header->offset_adat;

    /* get the rwfg box fully */
    rwfg_pos = mfub_box_size + FIXED_BOX_SIZE;
    ret = mfu_get_rwfg_box(mfu_data_orig, rwfg_pos,
	    is_audio, is_video, vmd, amd,
	    sps_pps, sps_pps_size);
    if (ret == VPE_ERROR)
	return VPE_ERROR;
    rwfg_box_size = ret;
    /* get the vdat box fully */
    if (*is_video != 0) {
	vdat_pos = mfub_box_size + FIXED_BOX_SIZE + rwfg_box_size +
	    FIXED_BOX_SIZE;
	ret = mfu_get_vdat_box(mfu_data_orig, vdat_pos,
		vmd);
	if (ret == VPE_ERROR)
	    return VPE_ERROR;
	vdat_box_size = ret;
	act_vdat_box_size = vdat_box_size + FIXED_BOX_SIZE;
    }
    /* get the adat box fully */
    if (*is_audio != 0) {
	adat_pos = mfub_box_size + FIXED_BOX_SIZE + rwfg_box_size +
	    FIXED_BOX_SIZE + act_vdat_box_size;
	ret = mfu_get_adat_box(mfu_data_orig, adat_pos,
		amd);
	if (ret == VPE_ERROR)
	    return VPE_ERROR;
	adat_box_size = ret;
    }

    return VPE_SUCCESS;
}

/*
   this function is used to extract raw audio from the adat box.
   mfu_data_orig -> i/p pointing to mfu
   adat_pos -> i/p pointing to adat box in mfu
   amd -> o/p here amd->mdat_pos is updated which is raw audio data
 */
uint32_t
mfu_get_adat_box(uint8_t *mfu_data_orig, uint32_t adat_pos, unit_desc_t
	*amd)
{
    char adat_id[] = {'a','d','a','t'};
    char box_name[4];
    uint32_t adat_size = 0;
    /* extract box name and check whether it is the required box */
    box_name[0] = mfu_data_orig[adat_pos];
    box_name[1] = mfu_data_orig[adat_pos+1];
    box_name[2] = mfu_data_orig[adat_pos+2];
    box_name[3] = mfu_data_orig[adat_pos+3];
    if (memcmp(box_name, adat_id, 4)) {
#ifdef PRINT_MSG
	printf("adat box is not found \n");
#endif
	return VPE_ERROR;
    }
    C_(printf("-------------------------------\n"));
    C_(printf("parse adat box\n"));
    adat_pos += BOX_NAME_SIZE;
    /* extracting the adat box size */
#ifdef _BIG_ENDIAN_
    adat_size = _read32_byte_swap(mfu_data_orig, adat_pos);
#else
    adat_size = _read32(mfu_data_orig, adat_pos);
#endif
    C_(printf("adat_size %u\n", adat_size));

    adat_pos += BOX_SIZE_SIZE;
    /*extract the raw audio data */
    amd->mdat_pos = (uint8_t*) nkn_malloc_type(adat_size,
					       mod_vpe_aud_raw_data);
    memcpy(amd->mdat_pos, mfu_data_orig+adat_pos, adat_size);

    return adat_size;
}

/*
   this function is used to extract raw video from the vdat box.
   mfu_data_orig -> i/p pointing to mfu
   vdat_pos -> i/p pointing to vdat box in mfu
   vmd -> o/p here vmd->mdat_pos is updated which is raw video data
 */
uint32_t
mfu_get_vdat_box(uint8_t *mfu_data_orig, uint32_t vdat_pos, unit_desc_t
	*vmd)
{
    char vdat_id[] = {'v','d','a','t'};
    char box_name[4];
    uint32_t vdat_size = 0;
    /* extract box name and check whether it is the required box */
    box_name[0] = mfu_data_orig[vdat_pos];
    box_name[1] = mfu_data_orig[vdat_pos+1];
    box_name[2] = mfu_data_orig[vdat_pos+2];
    box_name[3] = mfu_data_orig[vdat_pos+3];
    if (memcmp(box_name, vdat_id, 4)) {
#ifdef PRINT_MSG
        printf("vdat box is not found \n");
#endif
        return VPE_ERROR;
    }
    vdat_pos += BOX_NAME_SIZE;

    C_(printf("-------------------------------\n"));
    C_(printf("parse vdat box\n"));

    /* extracting the vdat box size */
#ifdef _BIG_ENDIAN_
    vdat_size = _read32_byte_swap(mfu_data_orig, vdat_pos);
#else
    vdat_size = _read32(mfu_data_orig, vdat_pos);
#endif
    C_(printf("vdat_size %u\n", vdat_size));

    vdat_pos += BOX_SIZE_SIZE;
    /*extract the raw video data */
    vmd->mdat_pos = (uint8_t*) nkn_malloc_type(vdat_size,
						   mod_vpe_vid_raw_data);
    memcpy(vmd->mdat_pos, mfu_data_orig+vdat_pos, vdat_size);

    return vdat_size;
}
/* 
   This function is used to extract the details from rwfg box .
   mfu_data_orig -> i/p pointing to mfu
   rwfg_pos -> i/p indicate the pos of rwfg box in mfu
   is_audio -> i/p indicates whether adat box is present
   is_video -> i/p indicates whether vdat box is presnt
   vmd->o/p pointer indicates the details of video
   amd->o/p pointer indicates the details of audio
   sps_pps ->o/p indicates the spsp_pps data
   sps_pps_size -> o/p indicates the size of sps and pps
 */
uint32_t 
mfu_get_rwfg_box(uint8_t *mfu_data_orig, uint32_t rwfg_pos, uint32_t
	*is_audio,uint32_t *is_video, unit_desc_t*
	vmd, unit_desc_t* amd, uint8_t *sps_pps, uint32_t
	*sps_pps_size)
{
    char rwfg_id[] = {'r','w','f','g'};
    char box_name[4];
    uint8_t * videofg, *audiofg;
    uint32_t videofg_size,audiofg_size;
    uint32_t rwfg_size = 0;
    /* extract box name and check whether it is the required box */
    box_name[0] = mfu_data_orig[rwfg_pos];
    box_name[1] = mfu_data_orig[rwfg_pos+1];
    box_name[2] = mfu_data_orig[rwfg_pos+2];
    box_name[3] = mfu_data_orig[rwfg_pos+3];
    if (memcmp(box_name, rwfg_id, 4)) {
#ifdef PRINT_MSG
        printf("rwfg box is not found \n");
#endif
        return VPE_ERROR;
    }
    C_(printf("-------------------------------\n"));
    C_(printf("parse rwfg box\n"));

    rwfg_pos += BOX_NAME_SIZE;
    /* extracting the rwfg box */
#ifdef _BIG_ENDIAN_
    //extract rwfg box size
    rwfg_size = _read32_byte_swap(mfu_data_orig, rwfg_pos);
    C_(printf("rwfg_size %u\n", rwfg_size));

    rwfg_pos += BOX_SIZE_SIZE;
    if (*is_video != 0) {
	//extract videofg_size
	videofg_size = _read32_byte_swap(mfu_data_orig, rwfg_pos);
	C_(printf("videofg_size %u\n", videofg_size));

	rwfg_pos += 4;
	    videofg = (uint8_t*) nkn_malloc_type(videofg_size,
						mod_vpe_videofg_buf);
	memcpy(videofg, mfu_data_orig+rwfg_pos, videofg_size);
	rwfg_pos += videofg_size;
	/* update the vmd structure in this call*/
	mfu_get_video_rw_desc(videofg, vmd, sps_pps, sps_pps_size);
	free(videofg);

    }//end of is_video if
    if (*is_audio != 0) {
	//extract audiofg_size
	audiofg_size = _read32_byte_swap(mfu_data_orig, rwfg_pos);
	C_(printf("audiofg_size %u\n", audiofg_size));

	rwfg_pos += 4;
	    audiofg = (uint8_t*) nkn_malloc_type(audiofg_size,
						 mod_vpe_audiofg_buf);
	memcpy(audiofg, mfu_data_orig+rwfg_pos, audiofg_size);
	rwfg_pos += audiofg_size;
	/*update amd structure in this call*/
	mfu_get_audio_rw_desc(audiofg, amd);
	free(audiofg);
    }//end of is_audio if
#else
    //extract rwfg box size
    rwfg_size = _read32(mfu_data_orig, rwfg_pos);
    C_(printf("rwfg_size %u\n", rwfg_size));

    rwfg_pos += BOX_SIZE_SIZE;
    if (*is_video != 0) {
	//extract videofg_size
	videofg_size = _read32(mfu_data_orig, rwfg_pos);
	C_(printf("videofg_size %u\n", videofg_size));

	rwfg_pos += 4;
	    videofg = (uint8_t*) nkn_malloc_type(videofg_size,
						 mod_vpe_videofg_buf);
	memcpy(videofg, mfu_data_orig+rwfg_pos, videofg_size);
	rwfg_pos += videofg_size;
	/* update the vmd structure in this call*/
	mfu_get_video_rw_desc(videofg, vmd, sps_pps,
		sps_pps_size);
	free(videofg);
    }//is_video
    if (*is_audio != 0) {
	//extract audiofg_size
	audiofg_size = _read32(mfu_data_orig, rwfg_pos);
	C_(printf("audiofg_size %u\n", audiofg_size));

	rwfg_pos += 4;
	    audiofg = (uint8_t*) nkn_malloc_type(audiofg_size,
                                                 mod_vpe_audiofg_buf);
	memcpy(audiofg, mfu_data_orig+rwfg_pos, audiofg_size);
	rwfg_pos += audiofg_size;
	//amd = (unit_desc_t*)malloc(sizeof(unit_desc_t));
	/*update amd structure in this call*/
	mfu_get_audio_rw_desc(audiofg, amd);
	free(audiofg);
    }//is_audio
#endif
    return rwfg_size;
}

/*
   This function is used to update details of video alone from rwfg box.
   videofg -> i/p pointer indicating the videofg part in rwfg box
   vmd->o/p pointer indicates the details of video
   sps_pps ->o/p indicates the spsp_pps data
   sps_pps_size -> o/p indicates the size of sps and pps
 */
int32_t 
mfu_get_video_rw_desc(uint8_t *videofg, unit_desc_t* vmd, uint8_t
	*sps_pps, uint32_t *sps_pps_size)
{
    uint32_t pos = 0;
#ifdef _BIG_ENDIAN_
    uint32_t i=0;
#endif
    C_(printf("-------------------------------\n"));
    C_(printf("parse video rwfg\n"));

    /*extract sample_count,timescale,default duration and defaule size */
#ifdef _BIG_ENDIAN_
    vmd->sample_count = _read32_byte_swap(videofg, pos);
    C_(printf("vmd->sample_count %u\n",vmd->sample_count));

    pos+=4;
    vmd->timescale = _read32_byte_swap(videofg, pos);
    vmd->timescale = vmd->timescale &(0xffffffff);
    C_(printf("vmd->timescale %u\n",vmd->timescale));

    pos+=4;
    vmd->default_sample_duration = _read32_byte_swap(videofg, pos);
    C_(printf("vmd->default_sample_duration %u\n",vmd->default_sample_duration));

    pos+=4;
    vmd->default_sample_size = _read32_byte_swap(videofg, pos);
    C_(printf("vmd->default_sample_size %u\n",vmd->default_sample_size));

    pos+=4;
#else
    //memcpy(vmd, videofg+pos,RAW_FG_FIXED_SIZE);
    //pos += RAW_FG_FIXED_SIZE;
    vmd->sample_count = _read32(videofg, pos);
    C_(printf("vmd->sample_count %u\n",vmd->sample_count));

    pos+=4;
    vmd->timescale = _read32(videofg, pos);
    vmd->timescale = vmd->timescale &(0xffffffff);
    C_(printf("vmd->timescale %u\n", (uint32_t)vmd->timescale));

    pos+=4;
    vmd->default_sample_duration = _read32(videofg,
	    pos);
    C_(printf("vmd->default_sample_duration %u\n",vmd->default_sample_duration));

    pos+=4;
    vmd->default_sample_size = _read32(videofg, pos);
    C_(printf("vmd->default_sample_size %u\n",vmd->default_sample_size));

    pos+=4;
#endif
    /*extract sample_duration */
    vmd->sample_duration = (uint32_t*)
	nkn_malloc_type(vmd->sample_count*4,
			mod_vpe_vid_sample_dur_buf);
#ifdef _BIG_ENDIAN_
    for (i=0;i<vmd->sample_count;i++) {
	vmd->sample_duration[i] = _read32_byte_swap(videofg,
		pos+(i*4));
	C_(printf("sample %d: duration %u\n",i, vmd->sample_duration[i]));
    }
#else
    memcpy(vmd->sample_duration, videofg+pos, 4*vmd->sample_count);
#endif
    pos += (vmd->sample_count *4);

    /*Adding Composition Times*/
    vmd->composition_duration = (uint32_t*)
        nkn_malloc_type(vmd->sample_count*4,
                        mod_vpe_vid_sam_size_buf);

#ifdef _BIG_ENDIAN_
    for (i=0;i<vmd->sample_count;i++) {
        vmd->composition_duration[i] = _read32_byte_swap(videofg,
						    pos+(i*4));
        C_(printf("sample %d: duration %u\n",i, vmd->composition_duration[i]));
    }
#else
    memcpy(vmd->composition_duration, videofg+pos, 4*vmd->sample_count);
#endif


    pos += (vmd->sample_count *4);//accounting for DTS
    /*extract sample_sizes */
    vmd->sample_sizes = (uint32_t*)
	nkn_malloc_type(vmd->sample_count*4,
			mod_vpe_vid_sam_size_buf);
#ifdef _BIG_ENDIAN_
    for (i=0;i<vmd->sample_count;i++) {
	vmd->sample_sizes[i] = _read32_byte_swap(videofg, pos+(i*4));
	C_(printf("sample %d: size %u\n",i, vmd->sample_sizes[i]));

    }
#else
    memcpy(vmd->sample_sizes, videofg+pos, 4*vmd->sample_count);
#endif
    pos += (vmd->sample_count *4);
    /* extract sps and pps data(codec info) */
#ifdef _BIG_ENDIAN_
    *sps_pps_size  = _read32_byte_swap(videofg, pos);
#else
    memcpy(sps_pps_size, videofg+pos, sizeof(uint32_t));
#endif
    pos += 4;
    //sps_pps = (uint8_t *)malloc(*sps_pps_size); 
    memcpy(sps_pps, videofg+pos, *sps_pps_size);
    pos += *sps_pps_size;

    return VPE_SUCCESS;
}
/*
   This function is used to update details of audio alone from rwfg box.
   audiofg -> i/p pointer indicating the audiofg part in rwfg box
   amd->o/p pointer indicates the details of audio
 */
    int32_t
mfu_get_audio_rw_desc(uint8_t *audiofg, unit_desc_t* amd)
{
    uint32_t pos = 0;
#ifdef _BIG_ENDIAN_
    uint32_t i =0;
#endif
    C_(printf("-------------------------------\n"));
    C_(printf("parse audio rwfg\n"));
    /*extract sample_count,timescale,default duration and defaule size */
#ifdef _BIG_ENDIAN_
    amd->sample_count = _read32_byte_swap(audiofg, pos);
    C_(printf("amd->sample_count %u\n",amd->sample_count));

    pos+=4;
    amd->timescale = _read32_byte_swap(audiofg, pos);
    amd->timescale = amd->timescale &(0xffffffff);
    C_(printf("amd->timescale %u\n",amd->timescale));

    pos+=4;
    amd->default_sample_duration = _read32_byte_swap(audiofg, pos);
    C_(printf("amd->default_sample_duration %u\n",amd->default_sample_duration));

    pos+=4;
    amd->default_sample_size = _read32_byte_swap(audiofg, pos);
    C_(printf("amd->default_sample_size %u\n",amd->default_sample_size));

    pos+=4;
#else
    //memcpy(amd, audiofg+pos,RAW_FG_FIXED_SIZE);
    //pos += RAW_FG_FIXED_SIZE;
    amd->sample_count = _read32(audiofg, pos);
    C_(printf("amd->sample_count %u\n",amd->sample_count));

    pos+=4;
    amd->timescale = _read32(audiofg, pos);
    amd->timescale = amd->timescale &(0xffffffff);
    C_(printf("amd->timescale %u\n", (uint32_t)amd->timescale));

    pos+=4;
    amd->default_sample_duration = _read32(audiofg, pos);
    C_(printf("amd->default_sample_duration %u\n",amd->default_sample_duration));

    pos+=4;
    amd->default_sample_size = _read32(audiofg, pos);
    C_(printf("amd->default_sample_size %u\n",amd->default_sample_size));
    pos+=4;
#endif

    /*extract sample_duration */
    amd->sample_duration = (uint32_t*)
	nkn_malloc_type(amd->sample_count*4,
			mod_vpe_aud_sample_dur_buf);
#ifdef _BIG_ENDIAN_
    for (i=0;i<amd->sample_count;i++) {
	amd->sample_duration[i] = _read32_byte_swap(audiofg, pos+(i*4));
	C_(printf("sample %d: duration %u\n",i, amd->sample_duration[i]));
    }
#else
    memcpy(amd->sample_duration, audiofg+pos, 4*amd->sample_count);
#endif
    pos += (amd->sample_count *4);
    pos += (amd->sample_count *4);//accounting for DTS
    amd->sample_sizes = (uint32_t*)
	nkn_malloc_type(amd->sample_count*4,
			mod_vpe_aud_sam_size_buf);
    /*extract sample_sizes */
#ifdef _BIG_ENDIAN_
    for (i=0;i<amd->sample_count;i++) {
	amd->sample_sizes[i] = _read32_byte_swap(audiofg, pos+(i*4));
	C_(printf("sample %d: size %u\n",i, amd->sample_sizes[i]));
    }
#else
    memcpy(amd->sample_sizes, audiofg+pos, 4*amd->sample_count);
#endif
    pos += (amd->sample_count *4);
    return VPE_SUCCESS;
}
/*
   this function is used to extract mfu header from mfub box.
   mfu_data_orig -> i/p pointing to mfu
   mfub_pos -> i/p pointing to mfub box in mfu
   mfu_header -> o/p pointer
 */
uint32_t
mfu_get_mfub_box(uint8_t* mfu_data_orig, uint32_t mfub_pos, mfub_t
	*mfu_header)
{
    uint32_t mfub_size =0;
    char mfub_id[]={'m','f','u','b'};
    //uint16_t temp = 0;
    char box_name[4];
    /* extract box name and check whether it is the required box */
    box_name[0] = mfu_data_orig[mfub_pos];
    box_name[1] = mfu_data_orig[mfub_pos+1];
    box_name[2] = mfu_data_orig[mfub_pos+2];
    box_name[3] = mfu_data_orig[mfub_pos+3];
    if (memcmp(box_name, mfub_id, 4)) {
#ifdef PRINT_MSG
	printf("mfub box is not found \n");
#endif
	return VPE_ERROR;
    }
    C_(printf("-------------------------------\n"));
    B_(printf("parsing MFUB\n"));    
    mfub_pos += BOX_NAME_SIZE;
#if 0
    //check for endianess
    temp = _read16(mfu_data_orig, mfub_pos+6+BOX_SIZE_SIZE);
    *is_big_endian = (temp && 0x4);
#endif
    //extract the mfu header
#ifdef _BIG_ENDIAN_
    mfub_size = _read32_byte_swap(mfu_data_orig,mfub_pos);
    B_(printf("mfub_size %u \n\n", mfub_size));

    mfub_pos += BOX_SIZE_SIZE;
    mfu_header->version = _read16_byte_swap(mfu_data_orig, mfub_pos);
    B_(printf("mfu_header->version %u \n", mfu_header->version));

    mfub_pos += 2;
    mfu_header->program_number = _read16_byte_swap(mfu_data_orig, mfub_pos);
    B_(printf("mfu_header->program_number %u \n", mfu_header->program_number));

    mfub_pos += 2;
    mfu_header->stream_id = _read16_byte_swap(mfu_data_orig, mfub_pos);
    B_(printf("mfu_header->stream_id %u \n", mfu_header->stream_id));

    mfub_pos += 2;
    mfu_header->flags = _read16_byte_swap(mfu_data_orig, mfub_pos);
    B_(printf("mfu_header->flags %u \n", mfu_header->flags));

    mfub_pos += 2;
    mfu_header->sequence_num = _read32_byte_swap(mfu_data_orig, mfub_pos);
    B_(printf("mfu_header->sequence_num %u \n", mfu_header->sequence_num));

    mfub_pos += 4;
    mfu_header->timescale = _read32_byte_swap(mfu_data_orig, mfub_pos);
    B_(printf("mfu_header->timescale %u \n", mfu_header->timescale));

    mfub_pos += 4;
    mfu_header->duration = _read32_byte_swap(mfu_data_orig, mfub_pos);
    B_(printf("mfu_header->duration %u \n", mfu_header->duration));

    mfub_pos += 4;
    mfu_header->audio_duration = _read32_byte_swap(mfu_data_orig, mfub_pos);
    B_(printf("mfu_header->audio_duration %u \n", mfu_header->audio_duration));

    mfub_pos += 4;
    mfu_header->video_rate = _read16_byte_swap(mfu_data_orig, mfub_pos);
    B_(printf("mfu_header->video_rate %u \n", mfu_header->video_rate));

    mfub_pos += 2;
    mfu_header->audio_rate = _read16_byte_swap(mfu_data_orig, mfub_pos);
    B_(printf("mfu_header->audio_rate %u \n", mfu_header->audio_rate));

    mfub_pos += 2;
    mfu_header->mfu_rate = _read16_byte_swap(mfu_data_orig, mfub_pos);
    B_(printf("mfu_header->mfu_rate %u \n", mfu_header->mfu_rate));

    mfub_pos += 2;
    mfu_header->video_codec_type = mfu_data_orig[mfub_pos];//_read16_byte_swap(mfu_data_orig,
    //    mfub_pos);
    B_(printf("mfu_header->video_codec_type %u \n", mfu_header->video_codec_type));

    mfu_header->audio_codec_type = mfu_data_orig[mfub_pos];//_read16_byte_swap(mfu_data_orig,
    //mfub_pos);
    B_(printf("mfu_header->audio_codec_type %u \n", mfu_header->audio_codec_type));

    mfub_pos += 2;
    mfu_header->compliancy_indicator_mask = _read32_byte_swap(mfu_data_orig,
	    mfub_pos);
    B_(printf("mfu_header->compliancy_indicator_mask %u \n", mfu_header->compliancy_indicator_mask));

    mfub_pos += 4;
    //hole in mfub header (4 bytes)
    mfub_pos += 4;
    mfu_header->offset_vdat = _read64_byte_swap(mfu_data_orig, mfub_pos);
    B_(printf("mfu_header->offset_vdat %u \n", mfu_header->offset_vdat));

    mfub_pos += 8;
    mfu_header->offset_adat = _read64_byte_swap(mfu_data_orig, mfub_pos);
    B_(printf("mfu_header->offset_adat %u \n", mfu_header->offset_adat));

    mfub_pos += 8;
    mfu_header->mfu_size = _read32_byte_swap(mfu_data_orig, mfub_pos);
    B_(printf("mfu_header->mfu_size %u \n", mfu_header->mfu_size));

    mfub_pos += 4;
#else
    mfub_size = _read32(mfu_data_orig,mfub_pos);
    mfub_pos += BOX_SIZE_SIZE;
    memcpy(mfu_header, mfu_data_orig+mfub_pos, mfub_size);
#endif
    return mfub_size;
}
/* This function is used to free audio and video related elemnts which
 * has bee alloced */

int32_t mfu2ts_clean_vmd_amd(unit_desc_t* vmd, unit_desc_t* amd,
	uint8_t * sps_pps)
{
    if (vmd != NULL) {
	if (vmd->sample_duration != NULL)
	    free(vmd->sample_duration);
	if (vmd->composition_duration != NULL)
	    free(vmd->composition_duration);
	if (vmd->sample_sizes != NULL)
	    free(vmd->sample_sizes);
	if (vmd->mdat_pos != NULL)
	    free(vmd->mdat_pos);
	free(vmd);
    }
    if (amd != NULL) {
	if (amd->sample_duration != NULL)
	    free(amd->sample_duration);
	if (amd->sample_sizes != NULL)
	    free(amd->sample_sizes);
	if (amd->mdat_pos != NULL)
	    free(amd->mdat_pos);
	free(amd);
    }
    if (sps_pps != NULL) {
	free(sps_pps);
    }
    return VPE_SUCCESS;
}



