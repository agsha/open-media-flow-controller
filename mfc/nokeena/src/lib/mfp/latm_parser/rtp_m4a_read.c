#include "common.h"
#include "get_bits.h"


const uint32_t asc_sampling_freq_tbl [] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 
    16000, 12000, 11025, 8000, 7350, 0, 0, 0
};

m4a_decoder_specific_info_t*
initAscInfo()
{
    /* need to free the GA object when cleaning if (if alloc'ed) inside the decoder
     * specific info reader 
     */
    return (m4a_decoder_specific_info_t*)calloc(1, sizeof(m4a_decoder_specific_info_t));
}

void
delete_asc_info(m4a_decoder_specific_info_t *dc)
{
    free(dc->ascData);
    free(dc->GA_si);
    free(dc->ascData);
    free(dc);
}

static int32_t
parseGASpecificConfig(GetBitContext *gb, m4a_decoder_specific_info_t *si) 
{
    uint32_t frame_length_flag, depends_on_core_coder, extn_flag;

    frame_length_flag = 0;
    depends_on_core_coder = 0;
    extn_flag = 0;
    
    frame_length_flag = get_bits(gb, 1);
    depends_on_core_coder = get_bits(gb, 1);
    if(depends_on_core_coder) {
	 si->GA_si->core_coder_delay = get_bits(gb, 14);
    }

    extn_flag = get_bits(gb, 1);
    if(!si->channel_config) {
	/* read program config element */
	/* not supported */
    }
    
    if(si->base_object_type == 6 ||
       si->base_object_type == 20 ) {
	si->GA_si->layer_num = get_bits(gb, 3);
    }

    if(extn_flag) {
	if(si->base_object_type == 22) {
	    si->GA_si->num_sub_frames = get_bits(gb, 5);
	    si->GA_si->layer_length = get_bits(gb, 11);
	}
        if(si->base_object_type == 17 || si->base_object_type == 19 ||
	   si->base_object_type == 20 || si->base_object_type == 23 ) {
	    si->GA_si->aac_section_data_resilience_flag = get_bits(gb, 1);
	    si->GA_si->aac_scale_factore_data_resilience_flag = get_bits(gb, 1);
	    si->GA_si->aac_spectral_data_resilience_flag = get_bits(gb, 1);
	}

	si->GA_si->extn_flag3 = get_bits(gb, 1);
    }
       
    return 0;
}

int32_t
parseAscInfo(GetBitContext *gb, m4a_decoder_specific_info_t *m4a_dec_si)
{
	m4a_dec_si->ascLen = gb->index;
    m4a_dec_si->base_object_type = get_bits(gb, 5);
    m4a_dec_si->sampling_freq_idx = get_bits(gb, 4);
    if(m4a_dec_si->sampling_freq_idx == 0x0F) {
	m4a_dec_si->sampling_freq = get_bits(gb, 24);
    } else {
	m4a_dec_si->sampling_freq = asc_sampling_freq_tbl[m4a_dec_si->sampling_freq_idx];
    }
    //printf("SampleFreq %u ", m4a_dec_si->sampling_freq);
    m4a_dec_si->channel_config = get_bits(gb, 4);
    m4a_dec_si->sbr_present_flag = -1;
    if(m4a_dec_si->base_object_type == 5) {
		uint32_t extn_audio_type;
		extn_audio_type = m4a_dec_si->base_object_type;
		m4a_dec_si->sbr_present_flag = 1;
		m4a_dec_si->extn_sampling_freq_idx = get_bits(gb, 4);
		if(m4a_dec_si->extn_sampling_freq_idx == 0x0F) {
		    m4a_dec_si->extn_sampling_freq = get_bits(gb, 24);
		}
		m4a_dec_si->base_object_type = get_bits(gb, 5);
    } else {
		m4a_dec_si->extn_audio_object_type = 0;
    }

	switch(m4a_dec_si->base_object_type) {
		case 1:
		case 2:
		case 3:
		case 4:
		case 6:
		case 7:
		case 17:
		case 19:
		case 20:
		case 21:
		case 22:
		case 23:
		    m4a_dec_si->GA_si = (m4a_GA_specific_config_t*)
			    calloc(1,  sizeof(m4a_GA_specific_config_t));
		    parseGASpecificConfig(gb, m4a_dec_si);
		    break;
		case 8:
		    /* CELP not supported */
		    break;
		case 9:
		    /* HVXC not supported */
		    break;
		case 12:
		    /* TTS not supported */
		    break;
		case 14:
		case 15:
		case 16:
		    /* Structured Audio Specific Config not supported */
		    break;
		case 24:
		    /* Error Resilient CELP, not supported */
		    break;
		case 26:
		case 27:
		    /* Parametric Specific Config, not supported */
		case 28:
		    /* SSC not supported */
		    break;
		case 32:
		case 33:
		case 34:
		    /* MPEG1/2 sepcific config, not supported */
		    break;
		default:
		    /* reseverd */
		    break;
    }

	switch (m4a_dec_si->base_object_type) {
		case 17:
		case 19:
		case 20:
		case 21:
		case 22:
		case 23:
		case 24:
		case 25:
		case 26:
		case 27:
		    {
			int32_t ep_config;
			ep_config = 0;
			/* read ep config */
			ep_config = get_bits(gb ,2);
			if(ep_config == 2 ||
			   ep_config == 3) {
			    // not supported
			}
			if(ep_config == 3) {
			    /* read direct mapping */
			    get_bits(gb, 2);
			}
		    }
	}
    
    if(m4a_dec_si->extn_audio_object_type != 5 && get_bits_left(gb)>= 2) {
	uint32_t sync_extn_type;

	/* read sync extn type */
	sync_extn_type = show_bits(gb, 11);
	if(sync_extn_type == 0x2b7) {
 	    sync_extn_type = get_bits(gb, 11);
	    m4a_dec_si->extn_audio_object_type = get_bits(gb, 5);
	    if(m4a_dec_si->extn_audio_object_type == 5) {
		m4a_dec_si->sbr_present_flag = get_bits(gb, 1);
		if(m4a_dec_si->sbr_present_flag == 1) {
		    m4a_dec_si->extn_sampling_freq_idx = get_bits(gb, 4);
		    if(m4a_dec_si->extn_sampling_freq_idx == 0x0f) {
			m4a_dec_si->extn_sampling_freq = get_bits(gb, 24);
		    }
		}
	    }
	}
    }
	uint32_t start_index = m4a_dec_si->ascLen >> 3;
    m4a_dec_si->ascLen = gb->index - m4a_dec_si->ascLen;
     m4a_dec_si->ascLen =  ((m4a_dec_si->ascLen + 7) >> 3); //convert bits to bytes, roof 
	if(m4a_dec_si->ascLen){
#if defined (UNIT_TEST)
		m4a_dec_si->ascData = (uint8_t *)malloc(m4a_dec_si->ascLen * sizeof(uint8_t));
#else
		m4a_dec_si->ascData = (uint8_t *)nkn_malloc_type(m4a_dec_si->ascLen * sizeof(uint8_t), 
		mod_vpe_mfp_latm_parser);
		//DBG_MFPLOG("LATM", MSG, MOD_MFPLIVE, "alloc asc");

#endif
		if(m4a_dec_si->ascData == NULL){
			printf("Unable to allocate mem\n");
		}
		memcpy(m4a_dec_si->ascData, gb->buffer + start_index, m4a_dec_si->ascLen);
		//printf("ASC Size %u\n", m4a_dec_si->ascLen);
		print_hex_buffer(m4a_dec_si->ascData, m4a_dec_si->ascLen, (uint8_t *)"ASC");
	}
    return 0;
}

