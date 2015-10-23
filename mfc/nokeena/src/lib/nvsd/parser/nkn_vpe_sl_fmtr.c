#include "nkn_vpe_sl_fmtr.h"
#include "nkn_vpe_types.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_assert.h"

#include <string.h>
#include <aio.h>
#include <signal.h>

int32_t glob_use_async_io_sl_fmtr = 1;
double min_open_time = 0, max_open_time = 0;
extern uint32_t glob_enable_stream_ha;

AO_t glob_sl_fmtr_tot_write_pending = 0;
AO_t glob_sl_fmtr_tot_write_begin = 0;
AO_t glob_sl_fmtr_tot_write_complete = 0;

// Functions accessed using the function pointers
static int16_t slFmtrHandleMfu(ref_count_mem_t *ref_cont);

static mfu2moof_track_state_t const* getTrackState(sl_fmtr_t const*
		fmtr_state, uint32_t channel_id, uint32_t track_idx);

static void slFmtrDelete(sl_fmtr_t* fmtr_state);

static int32_t acquireFmtr(sl_fmtr_t* fmtr_ctx); 

static int32_t releaseFmtr(sl_fmtr_t* fmtr_ctx); 

static uint32_t getSessChunkCtr(sl_fmtr_t const* fmtr_ctx); 

static void resetFormatterState(sl_fmtr_t* fmtr_state);

// Functions used locally for init and update of the track state variables
static int32_t initTrackState(mfu2moof_ret_t const*,
		mfu2moof_track_state_t* t_state, sl_fmtr_t *fmtr_state, 
		uint32_t stream_id);

static int32_t updateTrackState(mfu2moof_ret_t const* ret_track,
		mfu2moof_track_state_t* fmtr_track, 
		mfu_discont_ind_t const* discon_ind, sl_fmtr_t const *fmtr_state);

// Function to reset Accumulated state information (For LIVE Publ)
static int32_t clearFmtrManifestState(sl_fmtr_t *fmtr_state);


// Funcion to process and publish the meida
static int16_t processMfu(sl_fmtr_t *fmtr_state, mfu_contnr_t const* mfuc,
		mfu_discont_ind_t const** discont_ind);

static int32_t handlePublishRequest(sl_fmtr_t *fmtr_state);

	/* publish : generate ismc from internal state*/
static int32_t publishManifest(sl_fmtr_t* fmtr_state);


// Functions used locally for init. and write of media data into store location
static int32_t createStoreDir(char const* store_url);

static int32_t initTrackWriteLocation(char const* store_url, uint32_t bitrate);

static int32_t writeTrackMoof(sl_fmtr_t *fmtr_state, 
		mfu2moof_track_state_t const* t_state,
		ref_count_mem_t* ref_ret, uint32_t idx); 

static int32_t handleEOS(sl_fmtr_t *fmtr_state, mfu_contnr_t const *mfu);

// Functions used locally as utils for retrieving specific values from structs
static uint32_t util_getMfuStreamId(char const *mfu);

static uint32_t util_getMfuDuration(char const *mfu);

static uint32_t getQualityLevelCount(sl_fmtr_t const* fmtr_state,
		uint32_t m_type); 

static uint32_t getChunkCount(sl_fmtr_t const* fmtr_state,
		uint32_t m_type); 

static uint64_t getTotalDuration(sl_fmtr_t const* fmtr_state);

static int32_t slTestEOS(mfu_contnr_t const *mfuc);

static void* slFmtrMalloc(uint32_t size);

static void* slFmtrCalloc(uint32_t num_blocks, uint32_t block_size);

static int32_t SL_FwriteHandler(void* ctx, uint8_t const* file_path,
		uint8_t const* buf, uint32_t len); 

static int32_t SL_WriteHandler(uint8_t const* file_path,
		uint8_t const* buf, uint32_t len); 

static void removeHandler(void* ctx, char const* dir_path,
		char const* file_name);

static int32_t correctSmallDurationDiffs(sl_fmtr_t* fmtr_state, 
		mfu_contnr_t const* mfuc, mfu2moof_ret_t** ret);

static int32_t checkChunkIntvlConsistency(sl_fmtr_t const* fmtr_state); 

static int32_t diffTimevalToMs(struct timeval const* from, 
		struct timeval const * val);

int32_t SL_WriteNewFile(char const* sess_name, uint8_t const* path,
		uint8_t* buf, uint32_t size, io_handler_t const* io_cb,
		io_complete_hdlr_fptr io_complete_hdlr, io_complete_ctx_t* io_ctx); 

static void destroyMfu2Moof(void *arg);

static void deleteCharPtr(void* arg);

static void setErrorState(sl_fmtr_t *fmtr_state, uint32_t group_id);

// Handler registered for IO write complete signal
static void SL_AIO_CompleteHdlr(sigval_t sig_val); 
static void SL_IO_CompleteHdlr(sigval_t sig_val); 

static void SL_RemoveHandler(void* ctx);

uint32_t adts_sampling_freq_table[16] = { 96000, 88200, 64000, 48000,
					  44100, 32000,24000, 22050, 16000, 12000, 11025, 8000,
					  7350, 1, 1, 2};

//#define SL_LOG_PERFORMANCE

#define SL_PUB_INTERVAL 10
#define SL_CHUNK_KEEP_INTERVAL 900UL //15 minutes

#define SL_MAX_FILE_MEDIA_DURATION 10800 //180 minutes
#define SL_MIN_CHUNK_DURATION 2

#define SL_AUDIO_TRACK_ID 0

#define SLFMTR_ERRORCALLBACK(x){\
	if(fmtr_ctx->error_callback_fn){\
		mfu_data_t *mfu_data_l = (mfu_data_t*)ref_cont->mem;\
		mfu_data_l->fmtr_type = FMTR_SILVERLIGHT;\
		if(x == 2)\
		mfu_data_l->fmtr_err = ERROR_INPUT_DATA;\
		else\
		mfu_data_l->fmtr_err = ERROR_OUTPUT_IO;\
		fmtr_ctx->error_callback_fn(ref_cont);\
	}\
}

//#define SL_MFU_DUMP

sl_fmtr_t* createSLFormatter(
	char const* sess_name, 
	char const* store_url, 
	sl_fmtr_publ_type_t type,
	uint32_t chunk_duration_approx, 
	uint32_t dvr_chunk_count,
	sched_timed_event_fptr sched_timed_event,
	register_formatter_error_t error_callback_fn) {

	if ((sess_name == NULL) || (store_url == NULL) ||
	    ((type != SL_LIVE) && (type != SL_FILE) && (type != SL_DVR))) {
		return NULL;
	}
	if ((type != SL_FILE) && ((dvr_chunk_count == 0) ||
				(chunk_duration_approx == 0)))
		return NULL;
	if ((type != SL_FILE) && (sched_timed_event == NULL))
		return NULL;
	if (type == SL_FILE)
		dvr_chunk_count = SL_MAX_FILE_MEDIA_DURATION/SL_MIN_CHUNK_DURATION;

	sl_fmtr_t* fmtr_state = slFmtrCalloc(1, sizeof(sl_fmtr_t));
	if (fmtr_state != NULL) {

		int32_t i = 0;
		for (; i < MFU2MOOF_MAX_CHANNELS; i++) {
			fmtr_state->tracks[i] = NULL;
			fmtr_state->track_count[i] = 0;
			fmtr_state->seq_num[i] = 1;
		}
		fmtr_state->sess_name = slFmtrCalloc(strlen(sess_name)+1, sizeof(char));
		if (fmtr_state->sess_name == NULL) {
			free(fmtr_state);
			DBG_MFPLOG (fmtr_state->sess_name, ERROR, MOD_MFPFILE,
				    "Error in allocation of mem for fmtr_state->sess_name");
			return NULL;
		}
		strncpy(fmtr_state->sess_name, sess_name, strlen(sess_name)+1);
		fmtr_state->store_url = slFmtrCalloc(strlen(store_url)+1, sizeof(char));
		if (fmtr_state->store_url == NULL) {
		  DBG_MFPLOG (fmtr_state->sess_name, ERROR, MOD_MFPFILE,
			      "Error in allocation of mem for fmtr_state->store_url");
		        free(fmtr_state->sess_name);
			free(fmtr_state);
			return NULL;
		}
		strncpy(fmtr_state->store_url, store_url, strlen(store_url)+1);

		fmtr_state->publ_type = type;
		fmtr_state->chunk_duration_approx = chunk_duration_approx;

		if (type != SL_FILE) {
			uint32_t pub_intv = 1;
			if (chunk_duration_approx < (SL_PUB_INTERVAL * 1000))
				pub_intv = (SL_PUB_INTERVAL*1000)/chunk_duration_approx;
			fmtr_state->chunk_wait_count = pub_intv;
		}
		fmtr_state->dvr_chunk_count = dvr_chunk_count;

		fmtr_state->eos_flag = 0;
		fmtr_state->err_flag = 0;
		fmtr_state->reset_ctr = 0;
		fmtr_state->handle_mfu = slFmtrHandleMfu;
		fmtr_state->reset_state = resetFormatterState;
		fmtr_state->acquire_fmtr = acquireFmtr;
		fmtr_state->release_fmtr = releaseFmtr;
		fmtr_state->get_sess_chunk_ctr = getSessChunkCtr;
		fmtr_state->get_track_state = getTrackState;
		fmtr_state->delete_ctx = slFmtrDelete;
		fmtr_state->set_error_state = setErrorState;

		if (recursive_mkdir(store_url, 0755) != 0) {
		  DBG_MFPLOG (fmtr_state->sess_name, ERROR, MOD_MFPFILE,
			      "Output path not found");
			free(fmtr_state->sess_name);
			free(fmtr_state->store_url);
			free(fmtr_state);
			return NULL;
		}
		fmtr_state->fmtr_time_consumed = 0;
		fmtr_state->fmtr_call_count = 0;
		fmtr_state->fmtr_min_time = 0;
		fmtr_state->fmtr_max_time = 0;
		fmtr_state->fmtr_moof_write_time_consumed = 0;
		fmtr_state->fmtr_moof_write_count = 0;
		fmtr_state->fmtr_min_moof_write_time = 0;
		fmtr_state->fmtr_max_moof_write_time = 0;

		pthread_mutex_init(&fmtr_state->lock, NULL);
		fmtr_state->sess_chunk_ctr = 0;
		if(glob_use_async_io_sl_fmtr)
			fmtr_state->io_cb = createAIO_Hdlr();
		else
			fmtr_state->io_cb = createIO_Hdlr();
			
		if (fmtr_state->io_cb == NULL) {
		        free(fmtr_state->sess_name);
			free(fmtr_state->store_url);
			free(fmtr_state);
			DBG_MFPLOG (fmtr_state->sess_name, ERROR, MOD_MFPFILE,
                                    "Error in creating AIO handler");
			return NULL;
		}
		if(glob_use_async_io_sl_fmtr)
			fmtr_state->io_end_hdlr = SL_AIO_CompleteHdlr;
		else
			fmtr_state->io_end_hdlr = SL_IO_CompleteHdlr;

		fmtr_state->sched_timed_event = sched_timed_event;
		if(error_callback_fn){
			fmtr_state->error_callback_fn = error_callback_fn;
		}
	}
#ifdef SL_MFU_DUMP
	FILE* fp = fopen("mfu.dump", "w");
	if (fp == NULL) {
		free(fmtr_state);
		return NULL;
	}
	fclose(fp);
#endif
	return fmtr_state;
}


void slTaskInterface(void* ctx) {

	slFmtrHandleMfu((ref_count_mem_t*)ctx);
}


static int32_t clearFmtrManifestState(sl_fmtr_t *fmtr_state) {

	if (fmtr_state->publ_type == SL_FILE)
		return 1;

	uint32_t i = 0;
	for (; i < MFU2MOOF_MAX_CHANNELS; i++) {
		uint32_t j = 0;
		for (; j < fmtr_state->track_count[i]; j++) {
			fmtr_state->tracks[i][j].mnf_intv_chunk_count = 0;
		}
	}
	return 1;
}


adts_data_t* parseADTS(uint8_t const *stream) {

	adts_data_t *adts = NULL;
	if ((stream[0] == 0xFF) && ((stream[1]) & 0xF0) == 0xF0) {
		// sync word identified
		adts = slFmtrCalloc(sizeof(adts_data_t), 1);
		if (adts == NULL)
			return NULL;
		adts->id = (stream[1] & 0x08) >> 3;
		adts->mpeg_layer = (stream[1] & 0x06) >> 1;
		adts->prot_ind = stream[1] & 0x01;
		adts->profile_code = ((stream[2] & 0xC0) >> 6) + 1;
		adts->sample_freq = (stream[2] & 0x3C) >> 2;
		adts->sample_freq = adts_sampling_freq_table[adts->sample_freq];
		adts->channel_count = ((stream[2] & 0x01) << 2) +
			((stream[3] & 0xC0) >> 6);
		adts->frame_length = (((uint16_t)(stream[3]) & 0x03) << 11) + 
			(((uint16_t)stream[4]) << 3) +
			((stream[5] & 0xC0) >> 5);
		if (adts->prot_ind == 0)
			adts->frame_length += 2;
		adts->frame_count = stream[6] & 0x011;
	}
	return adts;
}

static int32_t acquireFmtr(sl_fmtr_t* fmtr_ctx) {

	return pthread_mutex_lock(&fmtr_ctx->lock);
}


static int32_t releaseFmtr(sl_fmtr_t* fmtr_ctx) {

	return pthread_mutex_unlock(&fmtr_ctx->lock);
}


static uint32_t getSessChunkCtr(sl_fmtr_t const* fmtr_ctx) {

	return fmtr_ctx->sess_chunk_ctr;
}


static int16_t slFmtrHandleMfu(ref_count_mem_t *ref_cont) {

	mfu_data_t *mfu_data = ref_cont->mem;
	sl_fmtr_t *fmtr_ctx = (sl_fmtr_t*)(mfu_data->sl_fmtr_ctx);
	struct timeval now1, now2;
	gettimeofday(&now1, NULL);
	int16_t rc = -1;
	mfu_contnr_t* mfuc = parseMFUCont(mfu_data->data);

	pthread_mutex_lock(&fmtr_ctx->lock);
	if (mfuc == NULL)
		fmtr_ctx->err_flag = 1;
	if (fmtr_ctx->err_flag == 1)
		goto clean_return;
	mfu_discont_ind_t** discont_ind = mfu_data->mfu_discont;

#ifdef SL_MFU_DUMP
	FILE* fp = fopen("mfu.dump", "a+");
	fwrite(mfu_data->data, mfuc->mfubbox->mfub->mfu_size, 1, fp);
	fclose(fp);
#endif

	rc = processMfu(fmtr_ctx, mfuc, (mfu_discont_ind_t const**)discont_ind);
	gettimeofday(&now2, NULL);
	double tmp_fmtr_time = diffTimevalToMs(&now2, &now1);
	fmtr_ctx->fmtr_time_consumed += tmp_fmtr_time; 
	fmtr_ctx->fmtr_call_count++;

	if (fmtr_ctx->fmtr_min_time == 0)
		fmtr_ctx->fmtr_min_time = tmp_fmtr_time;
	if (fmtr_ctx->fmtr_max_time == 0)
		fmtr_ctx->fmtr_max_time = tmp_fmtr_time;

	if (tmp_fmtr_time < fmtr_ctx->fmtr_min_time)
		fmtr_ctx->fmtr_min_time = tmp_fmtr_time;
	if (tmp_fmtr_time > fmtr_ctx->fmtr_max_time)
		fmtr_ctx->fmtr_max_time = tmp_fmtr_time;
#ifdef SL_LOG_PERFORMANCE
	DBG_MFPLOG(fmtr_ctx->sess_name, MSG, MOD_MFPLIVE,
			"SL FMTR Average fmtr exec time(ms): %f \
			Current: %f MIN: %f MAX: %f",
			fmtr_ctx->fmtr_time_consumed/
			fmtr_ctx->fmtr_call_count, tmp_fmtr_time, fmtr_ctx->fmtr_min_time,
			fmtr_ctx->fmtr_max_time);
#endif
	mfuc->del(mfuc);
	fmtr_ctx->sess_chunk_ctr++;
clean_return:
	if(fmtr_ctx->err_flag == 1){
		SLFMTR_ERRORCALLBACK(2);
	}	
	ref_cont->release_ref_cont(ref_cont);
	pthread_mutex_unlock(&fmtr_ctx->lock);
	return rc;
}


static int16_t processMfu(sl_fmtr_t *fmtr_state, mfu_contnr_t const* mfuc,
		mfu_discont_ind_t const** discont_ind) {

        int32_t rv = 1;
	if (!slTestEOS(mfuc)) {
	    if (fmtr_state->publ_type != SL_FILE){
		handleEOS(fmtr_state, mfuc);
		if(fmtr_state->publ_type == SL_DVR)
		    publishManifest(fmtr_state);
	    }
	    else
		rv = handlePublishRequest(fmtr_state);
	    DBG_MFPLOG(fmtr_state->sess_name, MSG, MOD_MFPLIVE,
				"SL FMTR: EOS received");
	    return rv;
	}
	if (fmtr_state->eos_flag == 1)
		return -1;

	uint32_t stream_id = mfuc->mfubbox->mfub->stream_id;
	if ((stream_id >= MFU2MOOF_MAX_CHANNELS))
		return -1;

	uint32_t mfu_track_count = mfuc->rawbox->desc_count;
	uint32_t i = 0;
	// detect audio presence and get index
	int32_t aud_idx = -1;

	/* Check whether the traks in the first MFU is greater than 
	 * the traks in the later MFU's. Because based on the first MFU, 
	 * tracks are being initialized.*/
	if(fmtr_state->publ_type == SL_FILE &&
	   stream_id == SL_AUDIO_TRACK_ID &&
	   ((mfu_track_count > fmtr_state->track_count[stream_id])&&
	    (fmtr_state->track_count[stream_id] != 0))) {
	    return -E_VPE_AV_INTERLEAVE_ERR;
	}

	for (i = 0; i < mfu_track_count; i++) {
	    if (strncmp(&(mfuc->datbox[i]->cmnbox->name[0]),
		"adat", 4) == 0)
		aud_idx = i;
	}
	if ((aud_idx >= 0) && (!fmtr_state->abitrate_computed_flag)) {
	    uint32_t frag_duration = \
		mfuc->mfubbox->mfub->duration;
		fmtr_state->abitrate = \
			(mfuc->datbox[aud_idx]->cmnbox->len * 8/ frag_duration); 
		fmtr_state->abitrate = (fmtr_state->abitrate / 1000) * 1000;
	    DBG_MFPLOG(fmtr_state->sess_name, MSG, MOD_MFPLIVE,
		       "SL FMTR Audio Bitrate is %u",
		       fmtr_state->abitrate);
	    fmtr_state->abitrate_computed_flag = 1;
	}
	    
        // Only the audio track of stream ID=SL_AUDIO_TRACK_ID is considered
	// Assumption: Every MFU has 1 video and 1 audio (video in 1st pos)
	if(!glob_enable_stream_ha){
	if ((aud_idx >= 0) && (stream_id != SL_AUDIO_TRACK_ID))
	    mfu_track_count--;
	}
	// preparing uuid values
	uint64_t mfu_duration = mfuc->mfubbox->mfub->duration * 10000000;

	sl_live_uuid1_t **uuid1 = NULL; 
	sl_live_uuid2_t **uuid2 = NULL; 

	if (fmtr_state->publ_type == SL_LIVE || fmtr_state->publ_type == SL_DVR) {
		// uuid1, uuid2 null terminated arrays
		uuid1 = (sl_live_uuid1_t**)slFmtrCalloc(mfu_track_count + 1,
				sizeof(sl_live_uuid1_t));
		uuid2 = (sl_live_uuid2_t**)slFmtrCalloc(mfu_track_count + 1,
				sizeof(sl_live_uuid2_t));

		mfu2moof_track_state_t* tracks = fmtr_state->tracks[stream_id]; 
		for (i = 0; i < mfu_track_count; i++) {

			uuid1[i] = createSlLiveUuid1();
			uint64_t curr_base_ts = 0;
			if ((tracks != NULL) && (tracks[i].num_of_chunks > 0)) {
				curr_base_ts =
					tracks[i].base_ts[tracks[i].num_of_chunks - 1];
			}
			uuid1[i]->timestamp = curr_base_ts;
			uuid1[i]->duration = mfu_duration; //(A)
			uuid2[i] = createSlLiveUuid2(1);
			uuid2[i]->timestamp[0] = curr_base_ts + mfu_duration; //(B)
			uuid2[i]->duration[0] = mfu_duration;

			//(A) and (B) : exact values will be updated by util_mfu2moof
		}
	}
	uint32_t seq_num = fmtr_state->seq_num[stream_id];
	// using the mfu to moof conversion utility

	mfu2moof_ret_t **ret = util_mfu2moof(seq_num, uuid1, uuid2, mfuc);
	if (fmtr_state->publ_type == SL_LIVE || fmtr_state->publ_type == SL_DVR) {
		// cleaning uuid related allocations
		for (i = 0; i < mfu_track_count; i++) {
			if (uuid1[i] != NULL)
				uuid1[i]->delete_sl_live_uuid1(uuid1[i]);
			if (uuid2[i] != NULL)
				uuid2[i]->delete_sl_live_uuid2(uuid2[i]);
		}
		free(uuid1);
		free(uuid2);
	}
	uint16_t rc = 1;
	i = 0;
	if (ret != NULL) {

		if (correctSmallDurationDiffs(fmtr_state, mfuc, ret) < 0) {
			DBG_MFPLOG(fmtr_state->sess_name, ERROR, MOD_MFPLIVE,
					"SL FMTR correctSmallDurationDiffs failed");
			fmtr_state->err_flag = 1;
			return -1;
		}
		ref_count_mem_t* ref_ret = createRefCountedContainer(ret,
				destroyMfu2Moof);
		if (ref_ret == NULL) {
			fmtr_state->err_flag = 1;
			return -1;
		}
		ref_ret->hold_ref_cont(ref_ret);

		if (fmtr_state->tracks[stream_id] == NULL) {
			// first mfu of this channel
			fmtr_state->tracks[stream_id] =	slFmtrCalloc(mfu_track_count,
					sizeof(mfu2moof_track_state_t));
			fmtr_state->track_count[stream_id] = mfu_track_count;
			for(; i < mfu_track_count; i++) {
				mfu2moof_track_state_t *t_state = 
					fmtr_state->tracks[stream_id] + i;
				int32_t rc1, rc2;
				// init track state
				rc1 = initTrackState(ret[i], t_state, fmtr_state, stream_id);
				// init track write location
				rc2 = initTrackWriteLocation(fmtr_state->store_url,
						t_state->bit_rate);
				if ((rc1 < 0) || (rc2 < 0)){
					free(fmtr_state->tracks[stream_id]);
					fmtr_state->track_count[stream_id] = 0;
					rc = -3;
					fmtr_state->err_flag = 1;
					ref_ret->release_ref_cont(ref_ret);
					goto return_clean;
				}
				// write track media data to the corr. loc
				if (writeTrackMoof(fmtr_state, t_state, ref_ret, i) < 0) {
					DBG_MFPLOG(fmtr_state->sess_name, ERROR, MOD_MFPLIVE,
							"SL FMTR WriteTrackMoof failed-1");
					fmtr_state->err_flag = 1;
				}

				t_state->mnf_intv_chunk_count++;
			}
		} else {
			NKN_ASSERT(mfu_track_count == fmtr_state->track_count[stream_id]);
			for(; i < mfu_track_count; i++) {
				mfu2moof_track_state_t *t_state = 
					fmtr_state->tracks[stream_id] + i;
				mfu_discont_ind_t const* discont = NULL;
				if (discont_ind != NULL)
					discont = discont_ind[i];
				// update track state
				if (updateTrackState(ret[i], t_state, discont,fmtr_state) < 0) {
					DBG_MFPLOG(fmtr_state->sess_name, ERROR, MOD_MFPLIVE,
							"SL FMTR updateTrackState failed");
					fmtr_state->err_flag = 1;
				} else
					// write track media data to the corr. loc
					if (writeTrackMoof(fmtr_state, t_state, ref_ret, i) < 0) {
						DBG_MFPLOG(fmtr_state->sess_name, ERROR, MOD_MFPLIVE,
								"SL FMTR WriteTrackMoof failed-2");
						fmtr_state->err_flag = 1;
					}

				t_state->mnf_intv_chunk_count++;
			}
		}
		fmtr_state->seq_num[stream_id]++;
		ref_ret->release_ref_cont(ref_ret);
	} else {
		DBG_MFPLOG(fmtr_state->sess_name, ERROR, MOD_MFPLIVE,
				"SL FMTR util_mfu2moof failed");
		fmtr_state->err_flag = 1;
		return -2;
	}
	if ((fmtr_state->err_flag == 0) && (fmtr_state->publ_type != SL_FILE))
		rc = handlePublishRequest(fmtr_state);

return_clean:
	return rc;
}

static int32_t slTestEOS(mfu_contnr_t const *mfuc)
{
	mfub_t const* mfub_data = mfuc->mfubbox->mfub;
	if (mfub_data->offset_vdat == 0xffffffffffffffff &&
			mfub_data->offset_adat == 0xffffffffffffffff) {
		return 0;
	}

	return 1;
}

/* (X)
   ASSERT Note: MFU2MOOF FMTR expects, all MFUs corresponding to the same
   STREAM ID(Channel) will always have same number of tracks and tracks shall 
   appear in same order within the MFU. When at a point, where the above
   expectation cannot be guaranteed, then MFU need to be updated to include
   some form of track identification context. And required changes need to be
   made in this fmtr*/


static mfu2moof_track_state_t const* getTrackState(sl_fmtr_t const*
		fmtr_state, uint32_t m_type, uint32_t track_num) {

	// @in track_num should start from 1 (not zero)

	int32_t i, traversed = 0;
	uint32_t j;
	for (i = 0; i < MFU2MOOF_MAX_CHANNELS; i++)
		for (j = 0; j < fmtr_state->track_count[i]; j++) {
			if (fmtr_state->tracks[i][j].media_type == m_type)
				traversed++;
			if ((traversed - 1) == (int32_t)track_num)
				return &(fmtr_state->tracks[i][j]);
		}
	return NULL;
}


static void slFmtrDelete(sl_fmtr_t* fmtr_state) {

	uint32_t i = 0;
	for (; i < MFU2MOOF_MAX_CHANNELS; i++) {
		uint32_t j = 0;
		for (; j < fmtr_state->track_count[i]; j++) {

			if (fmtr_state->tracks[i][j].codec_data != NULL)
				free(fmtr_state->tracks[i][j].codec_data);
			if (fmtr_state->tracks[i][j].sps != NULL)
				free(fmtr_state->tracks[i][j].sps);
			if (fmtr_state->tracks[i][j].adts != NULL)
				free(fmtr_state->tracks[i][j].adts);
			if (fmtr_state->tracks[i][j].exact_duration != NULL)
				free(fmtr_state->tracks[i][j].exact_duration);
			if (fmtr_state->tracks[i][j].base_ts != NULL)
				free(fmtr_state->tracks[i][j].base_ts);
		}
		if (j > 0)
			free(fmtr_state->tracks[i]);
	}
	fmtr_state->io_cb->io_hdlr_delete(fmtr_state->io_cb);
	free(fmtr_state->sess_name);
	free(fmtr_state->store_url);
	pthread_mutex_destroy(&fmtr_state->lock);
	free(fmtr_state);
	return;
}


static int32_t initTrackState(mfu2moof_ret_t const* ret_track,
		mfu2moof_track_state_t* t_state, sl_fmtr_t *fmtr_state,
		uint32_t stream_id) {

	int8_t rc = 1;
	t_state->media_type = ret_track->media_type;
	strncpy(&(t_state->mdat_type[0]), &(ret_track->mdat_type[0]), 5);
	t_state->num_of_chunks = t_state->tot_num_of_chunks = 1;
	if (t_state->media_type == 1)
		t_state->bit_rate = fmtr_state->abitrate + stream_id;
	else
		t_state->bit_rate = ret_track->bit_rate;
	//t_state->bit_rate += fmtr_state->reset_ctr;
	printf("Bit rate : %u\n", t_state->bit_rate);
	t_state->codec_data_len = ret_track->codec_info_size;
	t_state->base_ts = NULL;
	t_state->exact_duration = NULL;
	t_state->mnf_intv_chunk_count = 0;
	t_state->exact_duration =
		slFmtrCalloc(fmtr_state->dvr_chunk_count, sizeof(uint32_t));
	if (t_state->exact_duration == NULL) {
		perror("malloc failed");
		fmtr_state->err_flag = 1;
		return -1;
	}
	t_state->base_ts =
		slFmtrCalloc(fmtr_state->dvr_chunk_count, sizeof(uint64_t));
	if (t_state->base_ts == NULL) {
		perror("malloc failed");
		fmtr_state->err_flag = 1;
		return -1;
	}
	t_state->exact_duration[0] = ret_track->exact_duration;
	t_state->base_ts[0] = ret_track->exact_duration;

	t_state->sps = NULL;
	t_state->adts = NULL;

	if (t_state->codec_data_len > 0) {
		t_state->codec_data = slFmtrMalloc(t_state->codec_data_len);
		if (t_state->codec_data != NULL)
			memcpy(t_state->codec_data,
					ret_track->codec_specific_data, 
					t_state->codec_data_len);
		else {
			t_state->codec_data_len = 0;
			rc = -1;
		}
	} else
		t_state->codec_data = NULL;
	if (strncmp(&(t_state->mdat_type[0]), "H264", 4) == 0) {

		uint32_t delim_size = 0;
		if ((t_state->codec_data[0] == 0x00) && 
				(t_state->codec_data[1] == 0x00) && 
				(t_state->codec_data[2] == 0x00) && 
				(t_state->codec_data[3] == 0x01)) 
			delim_size = 4;
		else if ((t_state->codec_data[0] == 0x00) && 
				(t_state->codec_data[1] == 0x00) && 
				(t_state->codec_data[2] == 0x01)) 
			delim_size = 3;
		delim_size++; // NAL byte
		t_state->sps = parseSPS(t_state->codec_data + delim_size,
				t_state->codec_data_len);
	} else if (strncmp(&(t_state->mdat_type[0]), "AACL", 4) == 0) {
		t_state->adts = parseADTS((uint8_t const *)t_state->codec_data);
	}
	t_state->strm_state = STRM_ACTIVE;

	return rc;
}


static int32_t updateTrackState(mfu2moof_ret_t const* ret_track,
		mfu2moof_track_state_t* t_state, 
		mfu_discont_ind_t const* discont_ind, sl_fmtr_t const *fmtr_state) {

	if (t_state->num_of_chunks < fmtr_state->dvr_chunk_count) {
		t_state->num_of_chunks++;
	} else {

		uint64_t del_chunk_ts = t_state->base_ts[0]- t_state->exact_duration[0];

		memcpy(t_state->exact_duration,
				t_state->exact_duration + 1, (t_state->num_of_chunks - 1) * 4);
		memcpy(t_state->base_ts,
				t_state->base_ts + 1, (t_state->num_of_chunks - 1) * 8);

		// schedule old chunk delete task
		char m_type[6];
		char ql_dir[512], frag_info_dir[512];
		uint32_t bitrate = t_state->bit_rate;
		if (t_state->media_type == 0)
			strncpy(&(m_type[0]), "video", 6);
		else
			strncpy(&(m_type[0]), "audio", 6);

		snprintf(&(ql_dir[0]), 512,"%s/QualityLevels(%d)/Fragments(%s=%lu)",
				fmtr_state->store_url, bitrate,&(m_type[0]), del_chunk_ts);
		snprintf(&(frag_info_dir[0]),
				512,"%s/QualityLevels(%d)/FragmentInfo(%s=%lu)",
				fmtr_state->store_url, bitrate,&(m_type[0]),
				del_chunk_ts);
		struct timeval now;
		gettimeofday(&now, NULL);
		now.tv_sec += SL_CHUNK_KEEP_INTERVAL;
		char* url1 = strdup(&(frag_info_dir[0]));
		char* url2 = strdup(&(ql_dir[0]));
		fmtr_state->sched_timed_event(&now, SL_RemoveHandler, (void*)url1);
		fmtr_state->sched_timed_event(&now, SL_RemoveHandler, (void*)url2);
	}
	t_state->exact_duration[t_state->num_of_chunks - 1] =
		ret_track->exact_duration;
	if (t_state->num_of_chunks == 1)
		t_state->base_ts[t_state->num_of_chunks - 1] +=
			ret_track->exact_duration;
	else if ((fmtr_state->publ_type == SL_FILE) && (discont_ind != NULL) &&
			(discont_ind->discontinuity_flag == 1)) {
		t_state->base_ts[t_state->num_of_chunks - 1] =
			(discont_ind->timestamp * 10000000)/90000;
		t_state->base_ts[t_state->num_of_chunks - 1] +=
			ret_track->exact_duration;
		printf("Detected discontinuity: %u : %u : %lu : %lu\n",
				t_state->num_of_chunks,
				ret_track->exact_duration,
				t_state->base_ts[t_state->num_of_chunks - 2],
				t_state->base_ts[t_state->num_of_chunks - 1]);
	} else
		t_state->base_ts[t_state->num_of_chunks - 1] =
			t_state->base_ts[t_state->num_of_chunks - 2] +
			ret_track->exact_duration;

	t_state->tot_num_of_chunks++;
	return 1;
}


static int32_t createStoreDir(char const* store_url) {

	// create store directory
	int32_t rc = mkdir(store_url,
			S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	if ((rc < 0) && (errno != EEXIST)) {
		perror("mkdir failed 1: ");
		return -1;
	}
	return 1;
}


static int32_t initTrackWriteLocation(char const* store_url, uint32_t bitrate) {

	char ql_dir[512];
	int32_t rc = 0;
	rc = snprintf(&(ql_dir[0]), 512,"%s", store_url);
	snprintf(&(ql_dir[rc]), 512-rc, "/QualityLevels(%d)",
		 bitrate);
	//create QualityLevel Directory
	rc = createStoreDir(&(ql_dir[0]));
	return rc;
}


static int32_t handleEOS(sl_fmtr_t *fmtr_state, mfu_contnr_t const* mfuc) {

	int32_t err = 0;
	uint32_t stream_id = mfuc->mfubbox->mfub->stream_id;
	if ((stream_id >= MFU2MOOF_MAX_CHANNELS))
		return -1;

	mfu2moof_track_state_t* tracks = fmtr_state->tracks[stream_id]; 
	uint32_t mfu_track_count = fmtr_state->track_count[stream_id];
	uint32_t i = 0;
	for (; i < mfu_track_count; i++) {

		if (tracks != NULL) {
			uint64_t curr_base_ts = 0;
			// calc base time
			curr_base_ts = getTotalDuration(fmtr_state); 

			// get track bit rate & media type
			uint32_t bit_rate = tracks[i].bit_rate;
			uint8_t media_type = tracks[i].media_type;

			char m_type[6];
			char ql_dir[512];
			if (media_type == 0)
				strncpy(&(m_type[0]), "video", 6);
			else
				strncpy(&(m_type[0]), "audio", 6);

			//printf("M Type: %d Start time: %lu\n", media_type, curr_base_ts);

			//prepare write location
			snprintf(&(ql_dir[0]), 512,"%s/QualityLevels(%d)/Fragments(%s=%lu)",
					fmtr_state->store_url, bit_rate,&(m_type[0]), curr_base_ts);

			int8_t* eos_signal = slFmtrCalloc(8, sizeof(int8_t));
			io_complete_ctx_t* io_ctx = slFmtrCalloc(1,
					sizeof(io_complete_ctx_t));
			ref_count_mem_t* ref_cont = NULL;
			if (eos_signal != NULL)
				ref_cont = createRefCountedContainer(eos_signal,
						deleteCharPtr);
			if ((io_ctx == NULL) || (eos_signal == NULL) ||
						(ref_cont == NULL)) {

				fmtr_state->err_flag = 1;
				err = -1;
				break;
			}
			uint32_t mfra_len_nb = htonl(8);
			memcpy(eos_signal, &mfra_len_nb, 4);
			memcpy(eos_signal + 4, "mfra", 4);

			io_ctx->io_return_ctx = NULL;
			io_ctx->app_ctx = ref_cont;

			ref_cont->hold_ref_cont(ref_cont);
			if (SL_WriteNewFile(fmtr_state->sess_name,
						(uint8_t*)&(ql_dir[0]), (uint8_t*)eos_signal, 8,
						fmtr_state->io_cb, fmtr_state->io_end_hdlr,
						io_ctx) < 0) {
				ref_cont->release_ref_cont(ref_cont);
				free(io_ctx);
				DBG_MFPLOG(fmtr_state->sess_name, ERROR, MOD_MFPLIVE,
						"SL FMTR iow_handler file write(mfra) failed");
				fmtr_state->err_flag = 1;
				err = -1;
				break;
			}
		}
	}
	fmtr_state->eos_flag = 1;

	return err;
}


static int32_t writeTrackMoof(sl_fmtr_t *fmtr_state, 
		mfu2moof_track_state_t const* t_state,
		ref_count_mem_t* ref_ret, uint32_t idx) {

	mfu2moof_ret_t* ret = ((mfu2moof_ret_t**)ref_ret->mem)[idx];
	uint64_t start_time = t_state->base_ts[t_state->num_of_chunks - 1] -
		t_state->exact_duration[t_state->num_of_chunks - 1];
	char m_type[6];
	char ql_dir[512], frag_info_dir[512];
	uint32_t bitrate = t_state->bit_rate;
	int32_t err = 0;

	if (t_state->media_type == 0)
		strncpy(&(m_type[0]), "video", 6);
	else
		strncpy(&(m_type[0]), "audio", 6);

	//prepare write location
	snprintf(&(ql_dir[0]), 512,"%s/QualityLevels(%d)/Fragments(%s=%lu)",
			fmtr_state->store_url, bitrate,&(m_type[0]), start_time);
	snprintf(&(frag_info_dir[0]),
			512,"%s/QualityLevels(%d)/FragmentInfo(%s=%lu)",
			fmtr_state->store_url, bitrate,&(m_type[0]),
			start_time);

	struct timeval now1, now2;
	gettimeofday(&now1, NULL);
	uint8_t* moof_data = ret->moof_ptr;
	uint32_t moof_len = ret->moof_len;
	uint32_t moof_box_size = nkn_vpe_swap_uint32(*(uint32_t*)(moof_data));

	io_complete_ctx_t* io_ctx_1 = slFmtrCalloc(1, sizeof(io_complete_ctx_t));
	if (io_ctx_1== NULL) {
		fmtr_state->err_flag = 1;
		NKN_ASSERT(0);
		err = -E_VPE_PARSER_NO_MEM;
		goto return_clean;
	}
	io_ctx_1->app_ctx = (void*)ref_ret;

	io_complete_ctx_t* io_ctx_2 = slFmtrCalloc(1, sizeof(io_complete_ctx_t));
	if (io_ctx_2 == NULL) {
	       	fmtr_state->err_flag = 1;
		NKN_ASSERT(0);
		err = -E_VPE_PARSER_NO_MEM;
		goto return_clean;
	}
	io_ctx_2->app_ctx = (void*)ref_ret;

	ref_ret->hold_ref_cont(ref_ret);
	AO_fetch_and_add1(&glob_sl_fmtr_tot_write_pending);
	AO_fetch_and_add1(&glob_sl_fmtr_tot_write_begin);
	if (SL_WriteNewFile(fmtr_state->sess_name, 
				(uint8_t const*)&(frag_info_dir[0]), moof_data,
				moof_box_size, fmtr_state->io_cb,
				fmtr_state->io_end_hdlr, io_ctx_1) <0) {
		ref_ret->release_ref_cont(ref_ret);
		free(io_ctx_1);
		fmtr_state->err_flag = 1;
		AO_fetch_and_sub1(&glob_sl_fmtr_tot_write_pending);
		AO_fetch_and_sub1(&glob_sl_fmtr_tot_write_complete);
	} else {
		ref_ret->hold_ref_cont(ref_ret);
		AO_fetch_and_add1(&glob_sl_fmtr_tot_write_begin);
		AO_fetch_and_add1(&glob_sl_fmtr_tot_write_pending);
		if (SL_WriteNewFile(fmtr_state->sess_name,
					(uint8_t const*)&(ql_dir[0]), moof_data, moof_len,
					fmtr_state->io_cb, fmtr_state->io_end_hdlr, io_ctx_2) <0) {
			ref_ret->release_ref_cont(ref_ret);
			free(io_ctx_2);
			fmtr_state->err_flag = 1;
			AO_fetch_and_sub1(&glob_sl_fmtr_tot_write_pending);
			AO_fetch_and_sub1(&glob_sl_fmtr_tot_write_complete);
		}
	}

	gettimeofday(&now2, NULL);
	int32_t time_con_tmp = diffTimevalToMs(&now2, &now1);
	if (fmtr_state->fmtr_min_moof_write_time == 0)
		fmtr_state->fmtr_min_moof_write_time = time_con_tmp;
	if (fmtr_state->fmtr_max_moof_write_time == 0)
		fmtr_state->fmtr_max_moof_write_time = time_con_tmp;
	if (time_con_tmp < fmtr_state->fmtr_min_moof_write_time)
		fmtr_state->fmtr_min_moof_write_time = time_con_tmp;
	if (time_con_tmp > fmtr_state->fmtr_max_moof_write_time)
		fmtr_state->fmtr_max_moof_write_time = time_con_tmp;
	fmtr_state->fmtr_moof_write_time_consumed += time_con_tmp; 
	fmtr_state->fmtr_moof_write_count++;
#ifdef SL_LOG_PERFORMANCE
	DBG_MFPLOG(fmtr_state->sess_name, MSG, MOD_MFPLIVE,
			"SL FMTR NFS MOOF write time: %d Average: %f MIN: %f MAX: %f",
			time_con_tmp,
			(fmtr_state->fmtr_moof_write_time_consumed/
			 fmtr_state->fmtr_moof_write_count),
			fmtr_state->fmtr_min_moof_write_time,
			fmtr_state->fmtr_max_moof_write_time);
#endif

return_clean:
	if (fmtr_state->err_flag == 1)
		return err;
	return 1;
}


static uint32_t util_getMfuStreamId(char const *mfu) {

	mfub_t* mfub_data;
	mfub_data = (mfub_t*)(mfu + 8);
	return ntohs(mfub_data->stream_id);
}


static uint32_t util_getMfuDuration(char const *mfu) {

	mfub_t* mfub_data;
	mfub_data = (mfub_t*)(mfu + 8);
	return ntohl(mfub_data->duration);
}

static uint32_t getActiveQualityLevelCount(sl_fmtr_t const*
		fmtr_state, uint32_t m_type) {

	int32_t i,  count = 0;
	uint32_t j;
	for (i = 0; i < MFU2MOOF_MAX_CHANNELS; i++)
		for (j = 0; j < fmtr_state->track_count[i]; j++)
			if (fmtr_state->tracks[i][j].media_type == m_type 
			&& fmtr_state->tracks[i][j].strm_state == STRM_ACTIVE)
				count++;
	return count;
}

static uint32_t getQualityLevelCount(sl_fmtr_t const*
		fmtr_state, uint32_t m_type) {

	int32_t i,  count = 0;
	uint32_t j;
	for (i = 0; i < MFU2MOOF_MAX_CHANNELS; i++)
		for (j = 0; j < fmtr_state->track_count[i]; j++)
			if (fmtr_state->tracks[i][j].media_type == m_type)
				count++;
	return count;
}


static uint32_t getChunkCount(sl_fmtr_t const* fmtr_state, 
		uint32_t m_type) {

	int32_t i;
	uint32_t j;
	uint32_t min_chunk_count = 0,  chunk_count = 0;
	for (i = 0; i < MFU2MOOF_MAX_CHANNELS; i++){
	    for (j = 0; j < fmtr_state->track_count[i]; j++){
	    	if(fmtr_state->tracks[i][j].strm_state == STRM_ACTIVE){
		    if (fmtr_state->tracks[i][j].media_type == m_type) {
		    	chunk_count = fmtr_state->tracks[i][j].num_of_chunks;
		    if ((min_chunk_count == 0) || (min_chunk_count >= chunk_count))
		    	min_chunk_count = chunk_count;
		    }
	    	}
	    }
	}
	return min_chunk_count;
}


static uint64_t getTotalDuration(sl_fmtr_t const* fmtr_state) {

	/* Assumption: The total duration of all tracks within this fmtr 
	   session will be same */
    int32_t i;
    uint32_t j;
	for (i = 0; i < MFU2MOOF_MAX_CHANNELS; i++)
		for (j = 0; j < fmtr_state->track_count[i]; j++)
			return fmtr_state->tracks[i][j].base_ts[
				fmtr_state->tracks[i][j].num_of_chunks - 1];
	return 0;
}


static int32_t handlePublishRequest(sl_fmtr_t *fmtr_state) {

	uint32_t i, j;
	uint32_t tot_tracks = 0, avl_ident = 0;
	//printf("---\n");
	uint32_t req_chunks = fmtr_state->chunk_wait_count;
	for (i = 0; i < MFU2MOOF_MAX_CHANNELS; i++)
		for (j = 0; j < fmtr_state->track_count[i]; j++) {
			if(fmtr_state->tracks[i][j].strm_state == STRM_ACTIVE){
			tot_tracks++;
#if 0
			DBG_MFPLOG(fmtr_state->sess_name, MSG, MOD_MFPLIVE,
				"stream %u, track %u intvchunk %u, req chunks %u",
				i, j, fmtr_state->tracks[i][j].mnf_intv_chunk_count, req_chunks);
#endif
			if (fmtr_state->tracks[i][j].mnf_intv_chunk_count >= req_chunks
				&& fmtr_state->tracks[i][j].tot_num_of_chunks > 1)
				avl_ident++;
			}
			/*
			printf("strm %u ", i);
			if(j == 0)
				printf("VIDEO ");
			else
				printf("AUDIO ");
			
			printf("numChunks %u TotNumChunks %u mnfIntvCC %u(%u) avl_ident %u\n",
			fmtr_state->tracks[i][j].num_of_chunks,
			fmtr_state->tracks[i][j].tot_num_of_chunks,
			fmtr_state->tracks[i][j].mnf_intv_chunk_count, req_chunks, avl_ident);*/
		}
#if 0
	DBG_MFPLOG(fmtr_state->sess_name, MSG, MOD_MFPLIVE,
		"tot_tracks %u, avl_ident %u",
		tot_tracks, avl_ident);
#endif
	int32_t rc = 0;
	if (tot_tracks == avl_ident) {
		//printf("called pubManifest\n");
		rc = publishManifest(fmtr_state);
		if (rc < 0) {
			DBG_MFPLOG(fmtr_state->sess_name, ERROR, MOD_MFPLIVE,
					"SL FMTR publish manifest failed");
			fmtr_state->err_flag = 1;
		} else{
			//printf("called clearFmtrManifestState\n");		
			clearFmtrManifestState(fmtr_state);
		}
	}
	return rc;
}


static int32_t publishManifest(sl_fmtr_t* fmtr_state) {

	xmlTextWriterPtr writer;
	xmlBufferPtr buf;
	int32_t rc = 0;

	rc = checkChunkIntvlConsistency(fmtr_state);
	if (rc) {
	    return rc;
	}

	buf = xmlBufferCreate();
	if (buf == NULL) {
		printf("Error creating the xml buffer\n");
		return -1;
	}
	writer = xmlNewTextWriterMemory(buf, 0);
	if (writer == NULL) {
		printf("Error creating the xml writer\n");
		xmlBufferFree(buf);
		return -2;
	}

	rc = xmlTextWriterSetIndent(writer, 1);
	if (rc < 0) {
		printf("Error setting indent\n");
	}

	rc = xmlTextWriterStartDocument(writer, "1.0", "utf-8", NULL);
	if (rc < 0) {
		printf("Error creating start document\n");
		goto return_clean;
	}

	rc = xmlTextWriterStartElement(writer, BAD_CAST "SmoothStreamingMedia");
	if (rc < 0) {
		printf("Error : SmoothStreamingMedia\n");
		goto return_clean;
	}

	rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "MajorVersion",
			BAD_CAST "2");
	if (rc < 0) {
		printf("Error : SSM MajorVersion\n");
		goto return_clean;
	}
	rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "MinorVersion",
			BAD_CAST "0");
	if (rc < 0) {
		printf("Error : SSM MinorVersion\n");
		goto return_clean;
	}
	if (fmtr_state->publ_type == SL_FILE)
		rc = xmlTextWriterWriteFormatAttribute(writer,
				BAD_CAST "Duration", "%lu", getTotalDuration(fmtr_state)); 
	else
		rc = xmlTextWriterWriteFormatAttribute(writer,
				BAD_CAST "Duration", "%lu", (unsigned long)0); 

	if (rc < 0) {
		printf("Error : SSM Duration\n");
		goto return_clean;
	}
	if (fmtr_state->publ_type == SL_LIVE || fmtr_state->publ_type ==  SL_DVR) {
		/*End of Stream to be handled in case of DVR only*/
		if(fmtr_state->eos_flag){
			rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "IsLive",
					"%s", "FALSE");
		}
		else{
			rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "IsLive",
					"%s", "TRUE");
		}
		if (rc < 0) {
			printf("Error : SSM IsLive\n");
			goto return_clean;
		}
	}
	if (fmtr_state->publ_type == SL_LIVE || fmtr_state->publ_type == SL_DVR){
		uint64_t dvr_wnd_len;
		if(fmtr_state->publ_type == SL_DVR){
			dvr_wnd_len =
				fmtr_state->chunk_duration_approx * fmtr_state->dvr_chunk_count;
			rc = xmlTextWriterWriteFormatAttribute(writer,
					BAD_CAST "DVRWindowLength", "%lu", dvr_wnd_len*10000);
		}
		/*
		   else
		   dvr_wnd_len =
		   fmtr_state->chunk_duration_approx * 1;
		 */
		if (rc < 0) {
			printf("Error : SSM DVRWindowLength\n");
			goto return_clean;
		}
	}

	uint32_t i = 0;
	for (; i < 2; i++) {// 1 : 0 : represents video media type and 1=>audio

		uint32_t ql_count = getQualityLevelCount(fmtr_state, i);
		if (ql_count == 0)
			continue;
		uint32_t active_ql_count = getActiveQualityLevelCount(fmtr_state, i);
		
		rc = xmlTextWriterStartElement(writer, BAD_CAST "StreamIndex");
		if (rc < 0) {
			printf("Error : StreamIndex\n");
			goto return_clean;
		}
		if (i == 0)
			rc = xmlTextWriterWriteAttribute(writer,
					BAD_CAST "Type", BAD_CAST "video");
		else
			rc = xmlTextWriterWriteAttribute(writer,
					BAD_CAST "Type", BAD_CAST "audio");
		if (rc < 0) {
			printf("Error : SI Type\n");
			goto return_clean;
		}
		uint32_t total_chunks = getChunkCount(fmtr_state, i);
		if (fmtr_state->publ_type == SL_LIVE)
			rc = xmlTextWriterWriteFormatAttribute(writer,
					BAD_CAST"Chunks", "%u", 0);
		else
			rc = xmlTextWriterWriteFormatAttribute(writer,
					BAD_CAST"Chunks", "%u", total_chunks);
		if (rc < 0) {
			printf("Error : SI Chunks\n");
			goto return_clean;
		}
		if(i == 1){
			rc = xmlTextWriterWriteFormatAttribute(writer,
					BAD_CAST "QualityLevels",
					"%u", 1);
		}else{
			rc = xmlTextWriterWriteFormatAttribute(writer,
					BAD_CAST "QualityLevels",
					"%u", active_ql_count);
		}	
		if (rc < 0) {
			printf("Error : SI QualityLevels\n");
			goto return_clean;
		}
		if (i == 0)
			rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "Url", BAD_CAST
					"QualityLevels({bitrate})/Fragments(video={start time})");
		else
			rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "Url", BAD_CAST
					"QualityLevels({bitrate})/Fragments(audio={start time})");
		if (rc < 0) {
			printf("Error : SI Url\n");
			goto return_clean;
		}
		uint32_t j = 0, bitrate = 0;
		for (; j < ql_count; j++) {

			mfu2moof_track_state_t const* track =
				getTrackState(fmtr_state, i, j);
			if(track->strm_state == STRM_ACTIVE){
			rc = xmlTextWriterStartElement(writer,
					BAD_CAST "QualityLevel");
			if (rc < 0) {
				printf("Error : QualityLevel \n");
				goto return_clean;
			}
			rc = xmlTextWriterWriteFormatAttribute(writer,
					BAD_CAST "Index",
					"%u", j);
			if (rc < 0) {
				printf("Error : QI Index \n");
				goto return_clean;
			}
			bitrate = track->bit_rate;
			rc = xmlTextWriterWriteFormatAttribute(writer,
					BAD_CAST "Bitrate",
					"%u", bitrate);
			if (rc < 0) {
				printf("Error : QI Bitrate \n");
				goto return_clean;
			}
			rc = xmlTextWriterWriteFormatAttribute(writer,
					BAD_CAST "FourCC",
					"%s", &(track->mdat_type[0]));
			if (rc < 0) {
				printf("Error : QI FourCC \n");
				goto return_clean;
			}
			if (strncmp(&(track->mdat_type[0]), "H264", 4) == 0) {
				// H264 video only attributes
				rc = xmlTextWriterWriteFormatAttribute(writer,
						BAD_CAST "Width", "%u",
						track->sps->
						get_picture_width(track->sps));
				if (rc < 0) {
					printf("Error : QI Width \n");
					goto return_clean;
				}
				rc = xmlTextWriterWriteFormatAttribute(writer,
						BAD_CAST "Height", "%u",
						track->sps->
						get_picture_height(track->sps));
				if (rc < 0) {
					printf("Error : QI Height \n");
					goto return_clean;
				}
			}
			if (strncmp(&(track->mdat_type[0]), "AACL", 4) == 0) {

				rc = xmlTextWriterWriteFormatAttribute(writer,
						BAD_CAST "SamplingRate", "%u",
						track->adts->sample_freq);
				if (rc < 0) {
					printf("Error : QI SamplingRate \n");
					goto return_clean;
				}
				rc = xmlTextWriterWriteFormatAttribute(writer,
						BAD_CAST "Channels", "%u",
						track->adts->channel_count);
				if (rc < 0) {
					printf("Error : QI Channels \n");
					goto return_clean;
				}
				rc = xmlTextWriterWriteFormatAttribute(writer,
						BAD_CAST "BitsPerSample", "%u",
						16);
				if (rc < 0) {
					printf("Error : QI BitsPerSample \n");
					goto return_clean;
				}
				rc = xmlTextWriterWriteFormatAttribute(writer,
						BAD_CAST "PacketSize", "%u", 4); //TODO
				if (rc < 0) {
					printf("Error : QI PacketSize \n");
					goto return_clean;
				}
				rc = xmlTextWriterWriteFormatAttribute(writer,
						BAD_CAST "AudioTag", "%u", 255); // TODO
				if (rc < 0) {
					printf("Error : QI Audio Tag \n");
					goto return_clean;
				}
			}
			if (track->codec_data_len > 0) {
				uint32_t codec_data_len_hex = 
					track->codec_data_len * 2 + 1;
				char codec_data_hex[codec_data_len_hex];
				uint32_t k = 0;
				for (; k < track->codec_data_len; k++)
					snprintf(&(codec_data_hex[2 * k]),
							(codec_data_len_hex) -
							(2*k), "%02x",
							(unsigned char)
							track->codec_data[k]);
				rc = xmlTextWriterWriteFormatAttribute(writer,
						BAD_CAST "CodecPrivateData",
						"%s",
						BAD_CAST &(codec_data_hex[0]));
				if (rc < 0) {
					printf("Error : QI CodecPrivateData\n");
					goto return_clean;
				}
			}
			rc = xmlTextWriterEndElement(writer);
			if (rc < 0) {
				printf("Error : QI\n");
				goto return_clean;
			}
			if(i == 1)
				break;
			}
		}
		mfu2moof_track_state_t const *track = NULL;
		for(j = 0; j < ql_count; j++){
			track =	getTrackState(fmtr_state, i, j);
			if(track->strm_state == STRM_ACTIVE)
				break;
		}
		assert(track != NULL);
		for (j = 0; j < total_chunks; j++) {

			rc = xmlTextWriterStartElement(writer, BAD_CAST "c");
			if (rc < 0) {
				printf("Error : C\n");
				goto return_clean;
			}
			rc = xmlTextWriterWriteFormatAttribute(writer,
					BAD_CAST "n", "%u", j);
			if (rc < 0) {
				printf("Error : C n\n");
				goto return_clean;
			}
			rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "t", "%lu",
					track->base_ts[j] - track->exact_duration[j]);
			if (rc < 0) {
				printf("Error : C t\n");
				goto return_clean;

			}
			rc = xmlTextWriterEndElement(writer);
			if (rc < 0) {
				printf("Error : C End\n");
				goto return_clean;
			}
		}
		rc = xmlTextWriterEndElement(writer);
		if (rc < 0) {
			printf("Error : SI End\n");
			goto return_clean;
		}
	}

	rc = xmlTextWriterEndElement(writer);
	if (rc < 0) {
		printf("Error : SSM End\n");
		goto return_clean;
	}
	rc = xmlTextWriterEndDocument(writer);
	if (rc < 0) {
		printf("Error at xmlTextWriterEndDocument\n");
		goto return_clean;
	}

return_clean:
	xmlFreeTextWriter(writer);

	if (rc < 0) {
		xmlBufferFree(buf);
		return rc;
	}

	char url_cont[512];
	int32_t pos = snprintf(&url_cont[0], 512, "%s/",
			fmtr_state->store_url);
	snprintf(&url_cont[0] + pos, 512-pos, "%s", "Manifest");
	struct timeval now1, now2;
	gettimeofday(&now1, NULL);

	io_complete_ctx_t* io_ctx = slFmtrCalloc(1,
			sizeof(io_complete_ctx_t));
	ref_count_mem_t* ref_cont = createRefCountedContainer(buf->content,
			deleteCharPtr);
	if ((io_ctx == NULL) || (ref_cont == NULL)) {
		fmtr_state->err_flag = 1;
		return -1;
	}
	io_ctx->io_return_ctx = NULL;
	io_ctx->app_ctx = ref_cont;

	ref_cont->hold_ref_cont(ref_cont);
	if (SL_WriteNewFile(fmtr_state->sess_name, (uint8_t*)&(url_cont[0]),
				(uint8_t*)buf->content, strlen((char*)buf->content),
				fmtr_state->io_cb, fmtr_state->io_end_hdlr, io_ctx) < 0) {
		ref_cont->release_ref_cont(ref_cont);
		free(io_ctx);
		DBG_MFPLOG(fmtr_state->sess_name, ERROR, MOD_MFPLIVE,
				"SL FMTR iow_handler file write(Manifest) failed");
		fmtr_state->err_flag = 1;
		return -1;
	}

	gettimeofday(&now2, NULL);
	uint32_t tmp_fmtr_write_time = diffTimevalToMs(&now2, &now1);
#ifdef SL_LOG_PERFORMANCE
	DBG_MFPLOG(fmtr_state->sess_name, MSG, MOD_MFPLIVE,
			"SL FMTR NFS Manifest write time: %u", tmp_fmtr_write_time);
#endif
	tmp_fmtr_write_time = diffTimevalToMs(&now2, &now1);
	if (tmp_fmtr_write_time < fmtr_state->fmtr_min_moof_write_time)
		fmtr_state->fmtr_min_moof_write_time = tmp_fmtr_write_time;
	if (tmp_fmtr_write_time > fmtr_state->fmtr_max_moof_write_time)
		fmtr_state->fmtr_max_moof_write_time = tmp_fmtr_write_time; 
	fmtr_state->fmtr_moof_write_time_consumed += tmp_fmtr_write_time; 
	fmtr_state->fmtr_moof_write_count++;

	return rc;
}


static int32_t diffTimevalToMs(struct timeval const* from, 
		struct timeval const * val) {
	//if -ve, return 0
	double d_from = from->tv_sec + ((double)from->tv_usec)/1000000;
	double d_val = val->tv_sec + ((double)val->tv_usec)/1000000;
	double diff = d_from - d_val;
	if (diff < 0)
		return 0;
	return (int32_t)(diff * 1000);
}


static void* slFmtrMalloc(uint32_t size) {

#ifdef SL_FMTR_NORMAL_ALLOC
	return malloc(size);
#else
	return nkn_malloc_type(size, mod_mfp_sl_fmtr);
#endif
}


static void* slFmtrCalloc(uint32_t num_blocks, uint32_t block_size) {

#ifdef SL_FMTR_NORMAL_ALLOC
	return calloc(num_blocks, block_size);
#else
	return nkn_calloc_type(num_blocks, block_size, mod_mfp_sl_fmtr);
#endif
}


static int32_t SL_FwriteHandler(void* ctx, uint8_t const* file_path,
		uint8_t const* buf, uint32_t len) {

	FILE* fid = NULL;

	fid = fopen((char*)file_path, "wb");
	if (fid == NULL)
		return -1;
	int32_t rc = fwrite((char*)buf, len, 1, fid);
	if (rc != 1) {
		fclose(fid);
		return -1;
	}
	fclose(fid);
	return 0;
}


static int32_t SL_WriteHandler(uint8_t const* file_path,
		uint8_t const* buf, uint32_t len) {

	int32_t fid;

	fid = open((char*)file_path, O_WRONLY | O_CREAT | O_ASYNC,
			S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); 
	if (fid <= 0)
		return -1;
	uint32_t rc = write(fid, buf, len);
	if (rc != len) {
		close(fid);
		return -1;
	}
	close(fid);
	return 0;
}


static void removeHandler(void* ctx, char const* dir_path,
		char const* file_name) {

	if (file_name != NULL) {
		if (strncmp(file_name, "Fragment", 8) == 0) {
			char file_uri[FILENAME_MAX];
			snprintf(file_uri,FILENAME_MAX, "%s/%s", dir_path, file_name);
			remove(file_uri);
		}
	} else {
		if (strstr(dir_path, "QualityLevel") != NULL)
			remove(dir_path);
	}
}


static void SL_RemoveHandler(void* ctx) { 

	char* str = (char*) ctx;
	remove((char const*)ctx);
	free(str);
	return;
}


int32_t SL_WriteNewFile(char const* sess_name, uint8_t const* path,
		uint8_t* buf, uint32_t size,
		io_handler_t const* io_cb, io_complete_hdlr_fptr io_complete_hdlr,
		io_complete_ctx_t* io_ctx) {

	io_open_ctx_t open_ctx;
	int32_t err = 0;

	open_ctx.path = path;
	open_ctx.flags = O_CREAT | O_WRONLY | O_TRUNC | O_SYNC;
	open_ctx.mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	struct timeval now1, now2;
	gettimeofday(&now1, NULL);
	file_handle_t* fh = io_cb->io_open(&open_ctx);
	gettimeofday(&now2, NULL);
	double open_time = diffTimevalToMs(&now2, &now1);
	if (min_open_time == 0)
		min_open_time = open_time;
	if (min_open_time > open_time)
		min_open_time = open_time;
	if (max_open_time < open_time)
		max_open_time = open_time;
#ifdef SL_LOG_PERFORMANCE
	DBG_MFPLOG(sess_name, MSG, MOD_MFPLIVE,
		   "SL FMTR Open time: Current: %f MIN: %f MAX: %f",
		   open_time, min_open_time, max_open_time);
#endif
	if (fh == NULL) {
        DBG_MFPLOG(sess_name, MSG, MOD_MFPLIVE,
			   "SL FMTR Open: File Open Failed, path %s", path);
		err = -E_VPE_PARSER_INVALID_OUTPUT_PATH;
		NKN_ASSERT(0);
		return err;
	}
	io_ctx->fh = fh;
	if (io_cb->io_write(fh, buf, 1, size, 0, io_complete_hdlr, io_ctx) < 0) {
		io_cb->io_close(fh);
	        DBG_MFPLOG(sess_name, SEVERE, MOD_MFPLIVE,
			   "SL FMTR Write: Write Failed");
		err = -E_VPE_PARSER_WRITE_FAILE;
		NKN_ASSERT(0);
		return err;
	}
	return 1;
}


static void SL_AIO_CompleteHdlr(sigval_t sig_val) {

	io_complete_ctx_t* c_ctx = (io_complete_ctx_t*)sig_val.sival_ptr;
	ref_count_mem_t* ref_cont = (ref_count_mem_t*)c_ctx->app_ctx;
	struct aiocb* aio_ctx = (struct aiocb*)c_ctx->io_return_ctx;
	file_handle_t* fh = (file_handle_t*)c_ctx->fh;
	int err1 = 0;

	int32_t err = aio_error(aio_ctx);
	if (err != 0) {
		DBG_MFPLOG("ASYNCIO", SEVERE, MOD_MFPLIVE,
				"SL FMTR AIO: Error no: %d", err); 
		if (err == ENOSPC) {
			DBG_MFPLOG("ASYNCIO", SEVERE, MOD_MFPLIVE,
					"SL FMTR AIO: insufficient space in output"
					" device, free up space and restart"
					" session");
			NKN_ASSERT(0);
		} else {
			perror("aio error: ");
			err1 = errno;
			NKN_ASSERT(0);
		}
	}


	ssize_t rc = aio_return(aio_ctx);
	io_handler_t* io_h = createAIO_Hdlr();
	if (io_h != NULL) {
		io_h->io_flush(fh);
		io_h->io_advise(fh, 0, 0, POSIX_FADV_DONTNEED);
		io_h->io_close(fh);
		io_h->io_hdlr_delete(io_h);
	       	AO_fetch_and_sub1(&glob_sl_fmtr_tot_write_pending);
		AO_fetch_and_add1(&glob_sl_fmtr_tot_write_complete);
	}
 done:
	free(aio_ctx);
	free(c_ctx);
	if (ref_cont != NULL)
		ref_cont->release_ref_cont(ref_cont);
}


static void SL_IO_CompleteHdlr(sigval_t sig_val) {

	io_complete_ctx_t* c_ctx = (io_complete_ctx_t*)sig_val.sival_ptr;
	ref_count_mem_t* ref_cont = (ref_count_mem_t*)c_ctx->app_ctx;
	file_handle_t* fh = (file_handle_t*)c_ctx->fh;

	io_handler_t* io_h = createIO_Hdlr();
	if (io_h != NULL) {
		io_h->io_flush(fh);
		io_h->io_advise(fh, 0, 0, POSIX_FADV_DONTNEED);
		io_h->io_close(fh);
		io_h->io_hdlr_delete(io_h);
	}
	free(c_ctx);
	if (ref_cont != NULL)
		ref_cont->release_ref_cont(ref_cont);
}


static void destroyMfu2Moof(void *arg) {

	mfu2moof_ret_t** ret = (mfu2moof_ret_t**)arg;
	destroyMfu2MoofRet(ret);
}


static void deleteCharPtr(void *arg) {

	int8_t* dat = (int8_t*)arg;
	free(dat);
}


static int32_t correctSmallDurationDiffs(sl_fmtr_t* fmtr_state,
		mfu_contnr_t const* mfuc, mfu2moof_ret_t** ret) {

	int32_t rc = 0;
	uint32_t stream_id = mfuc->mfubbox->mfub->stream_id;
	mfu2moof_track_state_t *t_state = fmtr_state->tracks[stream_id];
	uint32_t tot_chunks = 0;
	uint32_t chunk_idx = 0;
	if (t_state != NULL)
		tot_chunks = t_state[0].tot_num_of_chunks;

	uint8_t identified = 0, j = 0;
	for (; j < MFU2MOOF_MAX_CHANNELS; j++) {
		if (fmtr_state->tracks[j] != NULL) {
			if (fmtr_state->tracks[j][0].strm_state == STRM_ACTIVE) {
				if (fmtr_state->tracks[j][0].tot_num_of_chunks > tot_chunks) {
					uint32_t idx_diff =
						fmtr_state->tracks[j][0].tot_num_of_chunks - tot_chunks;
					chunk_idx = fmtr_state->tracks[j][0].num_of_chunks-idx_diff;
					identified = 1;
					break;
				}
			}
		}
	}

	if (identified == 1) {

		mfu2moof_track_state_t* ref_t_state = fmtr_state->tracks[j];
		uint32_t track_count = fmtr_state->track_count[j];
		uint32_t i = 0;
		for (; i < track_count; i++) {

			//check only for video
			if (fmtr_state->tracks[j][i].media_type == 0) {

				if (fmtr_state->tracks[j][i].exact_duration[chunk_idx] != 
						ret[i]->exact_duration) {

					uint32_t diff = 0;
					if (fmtr_state->tracks[j][i].exact_duration[chunk_idx] >
							ret[i]->exact_duration)
						diff = fmtr_state->tracks[j][i].
							exact_duration[chunk_idx] - ret[i]->exact_duration;
					else
						diff = ret[i]->exact_duration -
							fmtr_state->tracks[j][i].exact_duration[chunk_idx];

					if (diff < 100000) { // less than 10ms
						DBG_MFPLOG(fmtr_state->sess_name, MSG,
								MOD_MFPLIVE, "SL FMTR Detected difference in \
								duration across profiles P1: %d P2: %d \
								P1 duration: %d P2 duration: %d", stream_id, j,
								ret[i]->exact_duration,
								fmtr_state->tracks[j][i].
								exact_duration[chunk_idx]);
						if (updateUUID(ret[i], fmtr_state->tracks[j][i].
									exact_duration[chunk_idx]) < 0) {
							DBG_MFPLOG(fmtr_state->sess_name, ERROR,
									MOD_MFPLIVE, "SL FMTR updateUUID failed");
						}
					} else { 
						DBG_MFPLOG(fmtr_state->sess_name, ERROR,
								MOD_MFPLIVE, "SL FMTR Detected difference in \
								duration across profiles P1: %d P2: %d \
								P1 duration: %d P2 duration: %d", stream_id, j,
								ret[i]->exact_duration,
								fmtr_state->tracks[j][i].
								exact_duration[chunk_idx]);
						rc = -1;
						NKN_ASSERT(0);
					}
				}
			}
		}
	}
	return rc;
}


static int32_t checkChunkIntvlConsistency(sl_fmtr_t const* fmtr_state) {

    uint32_t m_type = 0;//video
    uint32_t chunk_count = getChunkCount(fmtr_state, m_type);
    uint32_t ql_count = getQualityLevelCount(fmtr_state, m_type);
    uint32_t i, j, k;
    int32_t err = 0;
    for (i = 0; i < chunk_count; i++) {
	uint32_t exact_duration = 0;
	for (j = 0; j < MFU2MOOF_MAX_CHANNELS; j++) {
		for (k = 0; k < fmtr_state->track_count[j]; k++) {
			if (fmtr_state->tracks[j][k].media_type == m_type && 
				fmtr_state->tracks[j][k].strm_state == STRM_ACTIVE) {
				if (exact_duration == 0)
					exact_duration =
						fmtr_state->tracks[j][k].exact_duration[i];
				else {
					if (fmtr_state->tracks[j][k].exact_duration[i] != 
							exact_duration) {
						DBG_MFPLOG(fmtr_state->sess_name, ERROR,
								MOD_MFPLIVE,
								"SL FMTR Chunk duration NOT Same in all\
								rates Expected duration %d, duration in \
								profile %d is %d", exact_duration, j, 
								fmtr_state->tracks[j][k].exact_duration[i]);
						err = -E_VPE_INCONSISENT_SEG_DURATION;
						NKN_ASSERT(0);
						goto done;
					}
				}
			}
		}
	}
	}
 done:
    return err;
}

static void setErrorState(sl_fmtr_t *fmtr_state, uint32_t group_id){
	uint32_t track;

	for( track = 0; track < fmtr_state->track_count[group_id]; track++) {
		fmtr_state->tracks[group_id][track].strm_state = STRM_DEAD;
	}
	return;
}


static void resetFormatterState(sl_fmtr_t* fmtr_state) {

	pthread_mutex_lock(&fmtr_state->lock);

	fmtr_state->reset_ctr++;
	uint32_t i = 0;
	for (; i < MFU2MOOF_MAX_CHANNELS; i++) {
		uint32_t j = 0;
		for (; j < fmtr_state->track_count[i]; j++) {

			/*
			char file_uri[FILENAME_MAX];
			snprintf(file_uri,FILENAME_MAX, "%s/QualityLevels(%d)",
					fmtr_state->store_url,
					fmtr_state->tracks[i][j].bit_rate);
			traverse_tree_dir(file_uri, 0, removeHandler, NULL);
			*/

			fmtr_state->tracks[i][j].media_type = 0;
			fmtr_state->tracks[i][j].bit_rate = 0;
			fmtr_state->tracks[i][j].mnf_intv_chunk_count = 0;
			fmtr_state->tracks[i][j].num_of_chunks = 0;
			fmtr_state->tracks[i][j].tot_num_of_chunks = 0;
			if (fmtr_state->tracks[i][j].base_ts != NULL)
				free(fmtr_state->tracks[i][j].base_ts);
			fmtr_state->tracks[i][j].base_ts = NULL;
			if (fmtr_state->tracks[i][j].exact_duration != NULL)
				free(fmtr_state->tracks[i][j].exact_duration);
			fmtr_state->tracks[i][j].exact_duration = NULL;
			if (fmtr_state->tracks[i][j].codec_data != NULL)
				free(fmtr_state->tracks[i][j].codec_data);
			fmtr_state->tracks[i][j].codec_data = NULL;
			fmtr_state->tracks[i][j].codec_data_len = 0;
			if (fmtr_state->tracks[i][j].sps != NULL)
				free(fmtr_state->tracks[i][j].sps);
			fmtr_state->tracks[i][j].sps = NULL;
			if (fmtr_state->tracks[i][j].adts != NULL)
				free(fmtr_state->tracks[i][j].adts);
			fmtr_state->tracks[i][j].adts = NULL;
		}
		if (j > 0) {
			free(fmtr_state->tracks[i]);
			fmtr_state->tracks[i] = NULL;
		}
		fmtr_state->track_count[i] = 0;
		fmtr_state->seq_num[i] = 0;

		fmtr_state->abitrate_computed_flag = 0;
		fmtr_state->abitrate = 0;
		fmtr_state->eos_flag = 0;
		fmtr_state->err_flag = 0;
		fmtr_state->sess_chunk_ctr = 0;
	}

	printf("Formatter state reset\n");
	DBG_MFPLOG(fmtr_state->sess_name, MSG, MOD_MFPLIVE,
			"SL FMTR RESET STATE");
	pthread_mutex_unlock(&fmtr_state->lock);
	return;
}


#ifdef FMTR_UNIT_TEST

#define FMTR_PUB_AFTER 5
static void destroyMfuData(void *mfu_data); 

int main(int32_t argc, char** argv) {

	if (argc < 2) {

		printf("Usage: ./sltest <mfu file name>\n");
		return 0;
	}

	FILE* fp = fopen(argv[1], "rb");
	if (fp == NULL) {
		perror("File open failed: ");
		exit(-1);
	}

	uint32_t offset = 0;
	int32_t rc = 0;
	char *store_dir = "sl_test";
	sl_fmtr_t* fmtr_state = createSLFormatter(store_dir, 0, 2, 5, NULL, NULL);
	char*file_data = NULL;
	uint32_t i = 0;
	while (1) {

		mfub_t mfub;
		rc = fseek(fp, offset + 8, SEEK_SET);
		rc = fread((void*)&mfub, sizeof(mfub_t), 1, fp);
		if (rc <= 0) {
			break;
		}
		uint32_t mfu_size = ntohl(mfub.mfu_size);
		rc = fseek(fp, offset, SEEK_SET);
		file_data = slFmtrMalloc(mfu_size);
		rc = fread(file_data, mfu_size, 1, fp);
		offset += mfu_size;
		mfu_data_t *mfu_data =
		    malloc(sizeof(mfu_data_t));
		if (mfu_data == NULL)
		    return -1;
		mfu_data->sl_fmtr_ctx = fmtr_state;
		mfu_data->data = file_data; 
		mfu_data->data_size = mfu_size; 
		if (i == FMTR_PUB_AFTER) {
			mfub_t* mfub = (mfub_t*)(mfu_data->data + 8);
			mfub->offset_vdat = 0xffffffffffffffff;
			mfub->offset_adat = 0xffffffffffffffff;
		}
		ref_count_mem_t *ref_cont =
			createRefCountedContainer((void*)mfu_data, destroyMfuData);
		ref_cont->hold_ref_cont(ref_cont);
		rc = fmtr_state->handle_mfu(ref_cont);
		i++;
	}

	fmtr_state->delete_ctx(fmtr_state);

	fclose(fp);

	return 0;
}


static void destroyMfuData(void *mfu_data) {

	mfu_data_t *mfu_cont = (mfu_data_t*)mfu_data;
	free(mfu_cont->data);
	free(mfu_cont);
}


#endif

#ifdef ADTS_TEST 

int main() {

	uint8_t adts_val[7] = { 0xFF, 0xF9, 0x50, 0x80, 0x35, 0x5F, 0xFC };
	adts_data_t* adts;

	adts = parseADTS(&(adts_val[0]));
	printf("Id: %u\n", adts->id);
	printf("mpeg_layer: %u\n", adts->mpeg_layer);
	printf("prot_ind: %u\n", adts->prot_ind);
	printf("profile_code: %u\n", adts->profile_code);
	printf("Sampling freq: %u\n", adts->sample_freq);
	printf("Channel Count: %u\n", adts->channel_count);
	printf("frame_length: %u\n", adts->frame_length);
	printf("frame_count: %u\n", adts->frame_count);
	return 0;
}

#endif

