#include "nkn_vpe_mfu2moof.h"

static int8_t prepareMoof(mfu_contnr_t const* mfuc, uint32_t seq_num,
		uint32_t track_id, sl_live_uuid1_t* uuid1, sl_live_uuid2_t* uuid2,
		mfu2moof_ret_t* ret);

static uint32_t* insertSampleLen(uint8_t const*mdat_fmt,
		mfu_rw_desc_t const* rw_desc, uint8_t**mdat_fmt_out, 
		uint32_t* mdat_fmt_out_size);

static uint32_t* removeAdtsFromStart(uint8_t const* mdat_fmt,
		mfu_rw_desc_t const* rw_desc, uint8_t** mdat_fmt_out,
		uint32_t* mdat_fmt_out_size, uint8_t **adts, uint32_t *adts_len); 

static uint32_t replaceNalDelimWithLen(uint8_t const* mdat_fmt,
		uint32_t mdat_fmt_size, uint8_t** mdat_fmt_out); 

static void deleteMfu2Moof(mfu2moof_ret_t* ret);

static void* slMfu2MoofMalloc(uint32_t size);

static void* slMfu2MoofCalloc(uint32_t num_blocks, uint32_t block_size);

static uint32_t convertTimescale(uint32_t val,
		uint32_t in_ts, uint32_t out_ts);

extern uint32_t glob_latm_audio_encapsulation_enabled;

mfu2moof_ret_t** util_mfu2moof(uint32_t seq_num,
	struct sl_live_uuid1 **uuid1,
	struct sl_live_uuid2 **uuid2,
	mfu_contnr_t const* mfuc) {

	int32_t rc = 1;
	mfu2moof_ret_t** ret = NULL;
	uint32_t rw_desc_count = mfuc->rawbox->desc_count;

	// Allocate struct for results
	int32_t i = 0;
	ret = slMfu2MoofCalloc(rw_desc_count + 1, sizeof(mfu2moof_ret_t*));
	ret[rw_desc_count] = NULL;//NULL terminated ret ptr array

	for (i = 0; i < (int32_t)rw_desc_count; i++)
		ret[i] = (mfu2moof_ret_t*)slMfu2MoofCalloc(1, sizeof(mfu2moof_ret_t));

	for (i = 0; i < (int32_t)rw_desc_count; i++) {

		if ((strncmp(&(mfuc->datbox[i]->cmnbox->name[0]), "vdat", 4) != 0) &&
				(strncmp(&(mfuc->datbox[i]->cmnbox->name[0]),"adat", 4) != 0)) {
			printf("vdat or adat not found.\n");
			rc = -3;
			goto clean_return;
		}
		uint8_t track_type = 0;
		if (strncmp(&(mfuc->datbox[i]->cmnbox->name[0]),"adat", 4) == 0) {
			track_type = 1;
		}
		struct sl_live_uuid1 *uuid1_tmp = NULL;
		struct sl_live_uuid2 *uuid2_tmp = NULL;
		if ((uuid1 != NULL) && (uuid2 != NULL)) {
			uuid1_tmp = uuid1[i];
			uuid2_tmp = uuid2[i];
		}
		if (prepareMoof(mfuc, seq_num, i, uuid1_tmp, uuid2_tmp, ret[i]) < 0) {
			rc = -3;
			break;
		}
	}

clean_return:
	if (rc < 0) {
		if (ret != NULL) {
			if (ret[0] != NULL)
				free(ret[0]);
			free(ret);
		}
		ret = NULL;
	}
	return ret;
}


void destroyMfu2MoofRet(mfu2moof_ret_t** ret) {

	uint32_t i = 0;
	while (ret[i] != NULL) {
		ret[i]->delete_ctx(ret[i]);
		i++;
	}
	free(ret);
}


static int8_t prepareMoof(mfu_contnr_t const* mfuc, uint32_t seq_num,
		uint32_t track_id, sl_live_uuid1_t* uuid1, sl_live_uuid2_t* uuid2,
		mfu2moof_ret_t* ret) {

	mfub_t const* mfub = mfuc->mfubbox->mfub;
	mfu_rw_desc_t const* rw_desc = mfuc->rawbox->descs[track_id];
	uint8_t track_type = 0;
	if (strncmp(&(mfuc->datbox[track_id]->cmnbox->name[0]),"adat", 4) == 0)
		track_type = 1;

	// make an estimate on the size of moof based on the mdat size
	uint32_t msize = mfuc->datbox[track_id]->cmnbox->len; 
	uint32_t max_moof_size = (uint32_t)((((double)msize) * 2.5) + 500);

	/*DBG_MFPLOG("SL_FMTR", MSG, MOD_MFPLIVE, "Max Moof Size %d\n",
	  max_moof_size);*/
	uint8_t* moof = slMfu2MoofMalloc(max_moof_size);
	if (moof == NULL) {
		perror("malloc failed: ");
		return -1;
	}
	uint32_t start_offset = 0;
	int32_t i = 0;

	//build moof box
	uint32_t* moof_size_pos = (uint32_t*)moof; //store moof size write pos
	memcpy(moof + start_offset + 4, "moof", 4);
	start_offset = 8;

	//build mfhd box
	uint32_t mfhd_size_nb = htonl(16);
	memcpy(moof + start_offset, &mfhd_size_nb, 4);
	memcpy(moof + start_offset + 4, "mfhd", 4);
	memset(moof + start_offset + 8, 0x00000000, 4);
	uint32_t seq_num_nb = htonl(seq_num);
	memcpy(moof + start_offset + 12, &seq_num_nb, 4);
	start_offset += 16;

	//build traf box
	uint32_t* traf_size_pos = (uint32_t*)(moof + start_offset);
	memcpy(moof + start_offset + 4, "traf", 4);
	start_offset += 8;

	uint64_t *uuid1_duration_pos;
	uint64_t *uuid2_ts_pos;
	//build uuid1 box
	uint32_t uuid1_size = 0; 
	if (uuid1 != NULL) {
		uuid1_size = 8 + 16 + 4 + 8 + 8; 
		uint32_t uuid1_size_nb = htonl(uuid1_size);
		uint32_t vers_flags1 = uuid1->uuid_cmn.version;
		vers_flags1 |= uuid1->uuid_cmn.flags;
		memcpy(moof + start_offset, &uuid1_size_nb, 4);
		memcpy(moof + start_offset + 4, "uuid", 4);
		memcpy(moof + start_offset + 8, &(uuid1->uuid_cmn.uuid_type[0]) , 16);
		memcpy(moof + start_offset + 24, &(vers_flags1), 4);
		uint64_t uuid1_ts_nb = nkn_vpe_swap_uint64(uuid1->timestamp);
		uint64_t uuid1_dn_nb = nkn_vpe_swap_uint64(uuid1->duration);
		memcpy(moof + start_offset + 28, &uuid1_ts_nb, 8);
		uuid1_duration_pos = (uint64_t*) (moof + start_offset + 36);
		memcpy(moof + start_offset + 36, &uuid1_dn_nb, 8);
		start_offset += uuid1_size;
	}

	//build uuid2 box
	uint32_t uuid2_size = 0;
	if (uuid2 != NULL) {
		uuid2_size = 4 + 4 + 16 + 4 + 1 + (16 * uuid2->count);
		uint32_t uuid2_size_nb = htonl(uuid2_size);
		uint32_t vers_flags2 = uuid2->uuid_cmn.version;
		vers_flags2 |= uuid2->uuid_cmn.flags;
		memcpy(moof + start_offset, &uuid2_size_nb, 4);
		memcpy(moof + start_offset + 4, "uuid", 4);
		memcpy(moof + start_offset + 8, &(uuid2->uuid_cmn.uuid_type[0]) , 16);
		memcpy(moof + start_offset + 24, &(vers_flags2), 4);
		memcpy(moof + start_offset + 28, &(uuid2->count), 1);
		start_offset += 29;
		for (i = 0; i < uuid2->count; i++) {
			uint64_t uuid2_ts_nb = nkn_vpe_swap_uint64(uuid2->timestamp[i]);
			uint64_t uuid2_dn_nb = nkn_vpe_swap_uint64(uuid2->duration[i]);
			if (i == 0)
				uuid2_ts_pos = (uint64_t*)(moof + start_offset);
			memcpy(moof + start_offset, &uuid2_ts_nb, 8);
			memcpy(moof + start_offset + 8, &uuid2_dn_nb, 8);
			start_offset += 16;
		}
	}

	//build tfhd box
	uint32_t tfhd_size =16;
	char tfhd_vers_flags[4] = {0x00, 0x00, 0x00, 0x00};
	if (rw_desc->default_unit_duration != 0) {
		tfhd_vers_flags[3] |= 0x08;
		tfhd_size += 4;
	}
	if (rw_desc->default_unit_size != 0) {
		tfhd_vers_flags[3] |= 0x10;
		tfhd_size += 4;
	}
	uint32_t tfhd_size_nb = htonl(tfhd_size);
	memcpy(moof + start_offset, &tfhd_size_nb, 4);
	memcpy(moof + start_offset + 4, "tfhd", 4);
	memcpy(moof + start_offset + 8, &(tfhd_vers_flags[0]), 4);
	uint32_t track_id_nb = htonl(track_id + 1);
	memcpy(moof + start_offset + 12, &track_id_nb, 4);
	start_offset += 16;
	if (rw_desc->default_unit_duration != 0) {
		uint32_t def_unit_dur_nb = htonl(rw_desc->default_unit_duration);
		memcpy(moof + start_offset, &(def_unit_dur_nb), 4);
		start_offset += 4;
	}
	uint32_t *def_sample_size = NULL;
	if (rw_desc->default_unit_size != 0) {
		def_sample_size = (uint32_t*)moof + start_offset +20;
		// def_sample_size would be updated after processing mdat
		start_offset += 4;
	}

	//build trun
	//compute trun size
	uint32_t trun_size = 8; //type and size fields
	char trun_vers_flags[4] = {0x00, 0x00, 0x00, 0x00};
	trun_size += 4 + 4; //flags and sample count
	if (rw_desc->default_unit_duration == 0) {
		trun_size +=  2 * (rw_desc->unit_count * 4);
		trun_vers_flags[2] |= 0x01;
		trun_vers_flags[2] |= 0x08;
	}
	if (rw_desc->default_unit_size == 0) {
		trun_size += (rw_desc->unit_count * 4);
		trun_vers_flags[2] |= 0x02;
	}
	/*note: currently mfu_rw_desc_t does not have fields indicating
	  sample_flags and offset, so they are not part of the trun */
	uint32_t trun_size_nb = htonl(trun_size);
	memcpy(moof + start_offset, &trun_size_nb, 4);
	memcpy(moof + start_offset + 4, "trun", 4);
	//flags indicating the presence of sample size and duration
	memcpy(moof + start_offset + 8, &(trun_vers_flags[0]), 4);
	uint32_t unit_count_nb = htonl(rw_desc->unit_count);
	memcpy(moof + start_offset + 12, &unit_count_nb, 4);
	start_offset += 16;
	uint32_t* sample_sizes[rw_desc->unit_count];
	uint32_t unit_count = rw_desc->unit_count;
	uint64_t exact_duration = 0;
	for (i = 0; i < (int32_t)unit_count; i++) {
		if (trun_vers_flags[2] & 0x01) {

			uint32_t conv_scale = 
				convertTimescale(rw_desc->sample_duration[i],
						rw_desc->timescale, SL_MOOF_TIMESCALE);
			exact_duration += rw_desc->sample_duration[i]; 
			conv_scale = htonl(conv_scale);
			memcpy(moof + start_offset, &(conv_scale), 4);
			start_offset += 4;
		}
		if (trun_vers_flags[2] & 0x02) {
			// sample sizes would be updated after processing mdat
			sample_sizes[i] = (uint32_t*)(moof + start_offset);
			start_offset += 4;
		}
		if (trun_vers_flags[2] & 0x08) {

			uint32_t conv_scale = convertTimescale(
					rw_desc->composition_duration[i],
					rw_desc->timescale, SL_MOOF_TIMESCALE);
			conv_scale = htonl(conv_scale);
			memcpy(moof + start_offset, &(conv_scale), 4);
			start_offset += 4;
		}
	}
	if (exact_duration > 0)
		exact_duration = convertTimescale(exact_duration,
				rw_desc->timescale, SL_MOOF_TIMESCALE);

	// build mdat
	uint8_t* moof_mdat_pos = moof + start_offset + 8;
	uint32_t* updated_sample_sizes;
	uint32_t mdat_size = 0, adts_len = 0; 
	uint8_t adts[9];

	if (track_type == 0)
		updated_sample_sizes = insertSampleLen(mfuc->datbox[track_id]->dat,
				rw_desc, &moof_mdat_pos, &mdat_size);
	else {
		uint8_t* adts_ptr = &(adts[0]);
		updated_sample_sizes = removeAdtsFromStart(mfuc->datbox[track_id]->dat,
				rw_desc, &moof_mdat_pos, &mdat_size, &adts_ptr, &adts_len);
	}
	if (mdat_size == 0) {
		if (track_type == 0)
			printf("replaceNalDelimWithLen returns 0\n");
		else
			printf("removeAdts returns 0\n");
		free(moof);
		return -1;
	}
	mdat_size += 8;
	uint32_t mdat_size_nb = htonl(mdat_size);
	memcpy(moof + start_offset, &mdat_size_nb, 4);
	memcpy(moof + start_offset + 4, "mdat", 4);
	start_offset += mdat_size;

	// Fill in the size at required positions
	*moof_size_pos = ntohl(8 + 16 + 8 + tfhd_size + trun_size +
			uuid1_size + uuid2_size);
	*traf_size_pos = ntohl(8 + tfhd_size + trun_size + uuid1_size + uuid2_size);
	if (def_sample_size != NULL)
		*def_sample_size = htonl(updated_sample_sizes[0]);
	else
		for (i = 0; i < (int32_t)unit_count; i++)
			*(sample_sizes[i]) = htonl(updated_sample_sizes[i]);
	if (uuid1 != NULL) {
		uint64_t exact_duration_nb = nkn_vpe_swap_uint64(exact_duration);
		*uuid1_duration_pos = exact_duration_nb;
		uint64_t next_ts = uuid1->timestamp + exact_duration;
		next_ts = nkn_vpe_swap_uint64(next_ts);
		*uuid2_ts_pos = next_ts;
	}

	free(updated_sample_sizes);

	// Identify media and codec type
	ret->media_type = track_type;
	if (track_type == 0) {// video
		strncpy(&(ret->mdat_type[0]), "H264", 5);
		ret->bit_rate = mfub->video_rate * 1000;
	} else {// audio
		strncpy(&(ret->mdat_type[0]), "AACL", 5);
		ret->bit_rate = mfub->video_rate * 100; 
		// As audio_rate is currently not set in mfu, we set it as 10 % of video
	}
	ret->track_id = track_id + 1;
	ret->chunk_interval = mfub->duration * SL_MOOF_TIMESCALE; 
	ret->exact_duration = (uint32_t)exact_duration;
	ret->moof_ptr = moof;
	ret->moof_len = start_offset;
	assert(ret->moof_len <= max_moof_size);//to detect buffer overrun
	ret->codec_info_size = rw_desc->codec_info_size;
	if (ret->codec_info_size > 0) {
		ret->codec_specific_data = slMfu2MoofCalloc(ret->codec_info_size , 1);
		if (ret->codec_specific_data == NULL) {
			perror("calloc: ");
			free(moof);
			return -1;
		}
		memcpy(ret->codec_specific_data, rw_desc->codec_specific_data,
				ret->codec_info_size);
	} else if (track_type == 1) {
		// if adts was found, pack in codec spcecific data
		ret->codec_info_size = adts_len; 
		if (adts_len > 0) {
			ret->codec_specific_data =
				slMfu2MoofCalloc(ret->codec_info_size , 1);
			if (ret->codec_specific_data == NULL) {
				perror("calloc: ");
				free(moof);
				return -1;
			}
			memcpy(ret->codec_specific_data, adts, ret->codec_info_size);
		}
	}
	ret->delete_ctx = deleteMfu2Moof;
	return 1;
}


static uint32_t* insertSampleLen(uint8_t const*mdat_fmt,
		mfu_rw_desc_t const* rw_desc, uint8_t**mdat_fmt_out, 
		uint32_t* mdat_fmt_out_size) {

	uint32_t i = 0, unit_count = rw_desc->unit_count;
	uint32_t in_offset = 0, out_offset = 0;
	uint32_t *sample_sizes = slMfu2MoofCalloc(unit_count, sizeof(uint32_t));
	if (sample_sizes == NULL)
		return NULL;
	for (; i < unit_count; i++) {

		uint32_t sample_size = rw_desc->default_unit_size;
		if (sample_size == 0)
			sample_size = rw_desc->sample_sizes[i];

		if (sample_size == 0) {
			printf("Sample size ZERO\n");
			//DBG_MFPLOG("SL_FMTR", MSG, MOD_MFPLIVE, "Sample size is ZERO\n");
		}
		uint8_t* mdat_out = *mdat_fmt_out;
		mdat_out += out_offset;
		sample_sizes[i] = replaceNalDelimWithLen(mdat_fmt + in_offset,
				sample_size, &mdat_out);
				
		in_offset += sample_size; 
		out_offset += sample_sizes[i];
	}
	*mdat_fmt_out_size = out_offset;
	return sample_sizes;
}


static uint32_t replaceNalDelimWithLen(uint8_t const* mdat_fmt,
		uint32_t mdat_fmt_size, uint8_t** mdat_fmt_out) {

	uint8_t const* start_ptr = NULL;
	uint32_t in_offset = 0, out_offset = 0;
	uint8_t first_entry = 0;

	while (in_offset < mdat_fmt_size) {

		int32_t rc = -1;
		if (mdat_fmt_size - in_offset > 2) {
			if ((mdat_fmt[in_offset] == 0x00) &&
					(mdat_fmt[in_offset + 1] == 0x00) &&
					(mdat_fmt[in_offset + 2] == 0x01))
				rc = 3;
		}
		if ((rc == -1) && (mdat_fmt_size - in_offset > 3)) {
			if ((mdat_fmt[in_offset] == 0x00) &&
					(mdat_fmt[in_offset + 1] == 0x00) &&
					(mdat_fmt[in_offset + 2] == 0x00) &&
					(mdat_fmt[in_offset + 3] == 0x01))
				rc = 4;
		}
		if (rc == -1) {
			in_offset++;
			continue;
		}

		if (first_entry == 1) {
			uint32_t len = (mdat_fmt + in_offset) - start_ptr;
			uint32_t len_nb = htonl(len);
			memcpy(*mdat_fmt_out + out_offset, &len_nb, 4);
			memcpy(*mdat_fmt_out + out_offset + 4, start_ptr, len);
			out_offset += len + 4;
		} else
			first_entry = 1;

		in_offset += rc;
		start_ptr = mdat_fmt + in_offset;
	}
	if (first_entry == 1) {//last code will not have end pair
		uint32_t len = (mdat_fmt + in_offset) - start_ptr;
		uint32_t len_nb = htonl(len);
		memcpy(*mdat_fmt_out + out_offset, &len_nb, 4);
		memcpy(*mdat_fmt_out + out_offset + 4, start_ptr, len);
		out_offset += len + 4;
	}
	return out_offset;
}

static int32_t create_adts_header(
	uint8_t *adts_header, 
	mfu_rw_desc_t const* aud){
	
    int32_t ret = 0;

#define ADTS_HEADER_SIZE (7)    

    //frame duration in 90000 clock ticks for various sample frequency
    //96k,	  88k,	 64k,	    48k,
    //44.1k, 32k,	24k,	   22.05k,
    //16k,	  12k,	 11.025k, 8k, 
    //undef, undef, undef, undef

    uint32_t std_sample_duration[16] = {960, 1045, 1440, 1920, 
	2090, 2880, 3840, 4180, 
	5760, 7680, 8360, 11520, 
	0, 0, 0, 0};

    uint16_t sync_word = 0x0fff;
    uint8_t id = 0; //mpeg4
    uint8_t layer  = 0;
    uint32_t protection_absent = 1;
    uint8_t profile_objecttype = 1; //aac LC
    uint32_t sampling_frequency_index;
    uint32_t i;

    uint32_t lhs, rhs;
    lhs = aud->sample_duration[0];
    for(i = 0; i < 16; i++){
    	rhs = std_sample_duration[i];
    	if((lhs == rhs)||(lhs == rhs-1)){
		sampling_frequency_index = i;
		break;
    	}
    }
    uint8_t private_bit = 0;
    uint8_t channel_configuration = 2;
    uint8_t original_copy = 0;
    uint8_t home = 0;

    uint8_t copyright_identification_bit = 0;
    uint8_t copyright_identification_start = 0;

    uint16_t aac_frame_length;
    uint16_t adts_buffer_fullness = 7;
    uint8_t num_of_datablocks_in_frame = 0;

    *(adts_header + 0) = (uint8_t)( (sync_word & 0xff0) >> 4);
    *(adts_header + 1) = (uint8_t)( (sync_word & 0x00f) << 4 | 
    				((id&0x1) << 3) | 
    				((layer&0x3) << 1) | 
    				(protection_absent&0x1));
    *(adts_header + 2) = (uint8_t)(	((profile_objecttype&0x3) << 6) | 
    		                ((sampling_frequency_index&0xf) << 2) |  
    		                ((private_bit&0x1) << 1) | 
    		                ((channel_configuration&0x4) >> 2));
    *(adts_header + 3) = (uint8_t)( ((channel_configuration&0x3) << 6)|
    				((original_copy&0x1)<< 5) |
    				((home&0x1)<< 4)|
    				((copyright_identification_bit&0x1)<< 3)|
    				((copyright_identification_start&0x1)<< 2));
    //frame length is 2 bits in adts_header3, 8 bits in adts_header4, 3bits in adts_header5
    *(adts_header + 6) = (uint8_t)( ((adts_buffer_fullness&0x3f) << 2)|
			       (num_of_datablocks_in_frame&0x3));

    uint32_t aac_frame_len;
    aac_frame_len = aud->sample_sizes[0] + ADTS_HEADER_SIZE;
    *(adts_header + 3) = (uint8_t)( (adts_header[3]) |
	    ((aac_frame_len&0x1800) >> 11));
    *(adts_header + 4) = (uint8_t)( ((aac_frame_len&0x7f8) >> 3));
    *(adts_header + 5) = (uint8_t)( ((aac_frame_len&0x7) << 5) |
    	((adts_buffer_fullness&0x7c0) >> 6));
   
   return (ret);

}



static uint32_t* removeAdtsFromStart(uint8_t const* mdat_fmt,
		mfu_rw_desc_t const* rw_desc, uint8_t** mdat_fmt_out,
		uint32_t* mdat_fmt_out_size, uint8_t **adts, uint32_t *adts_len) {

	uint32_t unit_count = rw_desc->unit_count;
	uint32_t i = 0, in_offset = 0, out_offset = 0;
	uint32_t *sample_sizes = slMfu2MoofCalloc(unit_count, sizeof(uint32_t));
	if (sample_sizes == NULL)
		return NULL;
	for (; i < unit_count; i++) {

		uint32_t sample_size = rw_desc->default_unit_size;
		if (sample_size == 0)
			sample_size = rw_desc->sample_sizes[i];
		uint32_t adts_size = 0;
		if ((sample_size > 0) &&
				((uint8_t)(*(mdat_fmt + in_offset)) == 0xFF) &&
				((((uint8_t)*(mdat_fmt + in_offset + 1))
				  & 0xF0) == 0xF0)) {
			adts_size = (mdat_fmt[in_offset + 1] & 0x01) ? 7 : 9;
			if (adts_size > sample_size) {
				//bug 7111 : MFU feed has wrong data : Preventive check
				/*DBG_MFPLOG("SL_FMTR", MSG, MOD_MFPLIVE,
						"ADTS size greater than sample size\n");
				printf("ADTS ERROR found adts_size: %d sample_size: %d\n",
						adts_size, sample_size);*/
				adts_size = 0;
			}
			if (*adts_len == 0) {
				memcpy(*adts, (mdat_fmt+in_offset), adts_size);
				*adts_len = adts_size;
			}
		} else {
			/*DBG_MFPLOG("SL_FMTR", MSG, MOD_MFPLIVE,
					"ADTS Not found at Start\n");
			printf("ADTS Error Not found at Start\n");*/
			if(glob_latm_audio_encapsulation_enabled){
				create_adts_header(*adts, rw_desc);
				*adts_len = 7;			
			}
		}
		memcpy(*mdat_fmt_out + out_offset, 
				mdat_fmt + in_offset + adts_size,
				sample_size - adts_size);
		sample_sizes[i] = (sample_size - adts_size);
		out_offset += sample_sizes[i];
		in_offset += sample_size;
	}
	*mdat_fmt_out_size = out_offset;
	return sample_sizes;
}


static void deleteMfu2Moof(mfu2moof_ret_t* ret) {

	if (ret->moof_ptr != NULL)
		free(ret->moof_ptr);
	if (ret->codec_specific_data != NULL)
		free(ret->codec_specific_data);
	free(ret);
}


int32_t updateUUID(mfu2moof_ret_t* ret, uint32_t duration) {

	uint32_t moof_len = ret->moof_len;
	uint8_t* moof_data = ret->moof_ptr;
	uint64_t* uuid1_duration = NULL;
	uint64_t* uuid1_timestamp = NULL;
	uint64_t* uuid2_timestamp = NULL;

	uint32_t box_len = 0;
	char box_name[5] = {'\0', '\0', '\0', '\0', '\0'};
	uint32_t offset = 0;
	while (offset < moof_len) {

		uint32_t tmp_box_len = *(uint32_t*)(moof_data + offset);
		box_len = ntohl(tmp_box_len);
		memcpy(&box_name[0], (moof_data + offset + 4), 4);
		if (strncmp(&box_name[0], "uuid", 4) == 0) {

			if (uuid1_duration == NULL) {
				uuid1_timestamp = (uint64_t*)
					(moof_data + offset + 8 + 20);
				uuid1_duration = (uint64_t*)(moof_data + offset + 8 + 20 + 8);
			} else {
				uuid2_timestamp = (uint64_t*)(moof_data + offset + 8 + 21);
			}
			offset += box_len;
		} else if ((strncmp(&box_name[0], "moof", 4) == 0) ||
					(strncmp(&box_name[0], "traf", 4) == 0)) {
			offset += 8;
		} else {
			offset += box_len;
		}
		if ((uuid1_duration != NULL) && (uuid2_timestamp != NULL))
			break;
	}
	if ((uuid1_duration != NULL) && (uuid2_timestamp != NULL)) {

		ret->exact_duration = duration;
		uint64_t duration_nb = nkn_vpe_swap_uint64(((uint64_t)duration));
		*uuid1_duration = duration_nb;
		uint64_t uuid1_timestamp_val = *uuid1_timestamp;
		uuid1_timestamp_val = nkn_vpe_swap_uint64(uuid1_timestamp_val);
		uint64_t next_ts = uuid1_timestamp_val + duration;
		next_ts = nkn_vpe_swap_uint64(next_ts);
		*uuid2_timestamp = next_ts;
	} else
		return -1;
	return 0;
}

static void* slMfu2MoofMalloc(uint32_t size) {
#ifdef SL_MFU2MOOF_NORMAL_ALLOC
	return malloc(size);
#else
	return nkn_malloc_type(size, mod_mfp_sl_fmtr);
#endif
}


static void* slMfu2MoofCalloc(uint32_t num_blocks, uint32_t block_size) {

#ifdef SL_MFU2MOOF_NORMAL_ALLOC
	return calloc(num_blocks, block_size);
#else
	return nkn_calloc_type(num_blocks, block_size, mod_mfp_sl_fmtr);
#endif
}


static uint32_t convertTimescale(uint32_t val,
		uint32_t in_ts, uint32_t out_ts) {

	double conv_scale = (double)val;
	conv_scale /= in_ts;
	conv_scale *= out_ts;
	return (uint32_t)conv_scale;
}


// Silverlight LIVE specific UUID create and destroy functions
static void destroySlLiveUuid1(sl_live_uuid1_t*);

sl_live_uuid1_t* createSlLiveUuid1(void) {
	
	sl_live_uuid1_t* uuid = slMfu2MoofCalloc(1, sizeof(sl_live_uuid1_t));
	if (uuid == NULL)
		return uuid;
	uuid->uuid_cmn.version = 1;
	uuid->uuid_cmn.flags = 0;
	uuid->timestamp = 0;
	uuid->duration = 0;

	char uuid_type1[16] = { 0x6D, 0x1D, 0x9B, 0x05, 0x42, 0xD5, 0x44, 0xE6,
			0x80, 0xE2,	0x14, 0x1D, 0xAF, 0xF7, 0x57, 0xB2};
	memcpy(&(uuid->uuid_cmn.uuid_type[0]), &(uuid_type1[0]), 16);
	uuid->delete_sl_live_uuid1 = destroySlLiveUuid1;
	return uuid;
}


static void destroySlLiveUuid1(sl_live_uuid1_t* uuid) {

	free(uuid);
}


static void destroySlLiveUuid2(sl_live_uuid2_t*);

sl_live_uuid2_t* createSlLiveUuid2(uint8_t count) {
	
	sl_live_uuid2_t* uuid = slMfu2MoofCalloc(1, sizeof(sl_live_uuid2_t));
	if (uuid == NULL)
		return uuid;
	uuid->uuid_cmn.version = 1;
	uuid->uuid_cmn.flags = 0;
	uuid->count = count;
	uuid->timestamp = slMfu2MoofCalloc(count, sizeof(uint64_t));
	if (uuid->timestamp == NULL) {
		free(uuid);
		return NULL;
	}
	uuid->duration = slMfu2MoofCalloc(count, sizeof(uint64_t));
	if (uuid->duration == NULL) {
		free(uuid->timestamp);
		free(uuid);
		return NULL;
	}

	char uuid_type2[16] = { 0xD4, 0x80, 0x7E, 0xF2, 0xCA, 0x39, 0x46, 0x95,
			0x8E, 0x54, 0x26, 0xCB, 0x9E, 0x46, 0xA7, 0x9F};
	memcpy(&(uuid->uuid_cmn.uuid_type[0]), &(uuid_type2[0]), 16);
	uuid->delete_sl_live_uuid2 = destroySlLiveUuid2;
	return uuid;
}


static void destroySlLiveUuid2(sl_live_uuid2_t* uuid) {

	if (uuid->timestamp != NULL)
		free(uuid->timestamp);
	if (uuid->duration != NULL)
		free(uuid->duration);
	free(uuid);
}


#ifdef UNIT_TEST_MFU2MOOF

int main(int argc, char** argv) {

	if (argc < 2) {
		printf("Usage: ./mfu2moof <Mfu file name>\n");
		return 0;
	}
	FILE* fp = fopen(argv[1], "rb");
	if (fp == NULL) {
		perror("File open failed: ");
		exit(-1);
	}

	fseek(fp, 0, SEEK_END);
	uint32_t file_size = ftell(fp);
	uint8_t* file_data = malloc(file_size);
	rewind(fp);
	fread(file_data, file_size, 1, fp);

	printf("File size: %d\n", file_size);
	mfu_contnr_t* mfuc = parseMFU(file_data);
	if (mfuc == NULL) {
		printf("MFU parse failed\n");
		free(file_data);
		exit(-1);
	}
	mfu2moof_ret_t** ret = util_mfu2moof(1, NULL, NULL, mfuc);
	mfuc->del(mfuc);
	free(file_data);
	fclose(fp);

	// count of moof returned
	uint32_t i = 0;
	if (ret != NULL)
		while (ret[i++] != NULL);
	printf("Count of moof returned: %d\n", i-1);

	FILE *fp1, *fp2;
	fp1 = fopen("out1.moof", "wb");
	if (fp1 == NULL) {
		perror("File open for write failed: ");
		exit(-1);
	}
	fp2 = fopen("out2.moof", "wb");
	if (fp2 == NULL) {
		perror("File open for write failed: ");
		exit(-1);
	}
	if ((ret != NULL) && (i - 1 == 2)) {
		fwrite(ret[0]->moof_ptr, ret[0]->moof_len, 1, fp1);
		fwrite(ret[1]->moof_ptr, ret[1]->moof_len, 1, fp2);
		destroyMfu2MoofRet(ret);
	}
	fclose(fp1);
	fclose(fp2);

	return 0;
}

#endif

