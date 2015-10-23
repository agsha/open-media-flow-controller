#include "rtp_mpeg4_es.h"
#include "nkn_vpe_bitio.h"
#include "rtp_packetizer_common.h"
#include "nkn_memalloc.h"

const uint32_t sampling_freq_tbl [] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 
    16000, 12000, 11025, 8000, 7350, 0, 0, 0
};

m4a_decoder_specific_info_t*
m4a_init_decoder_specific_info()
{
    /* need to free the GA object when cleaning if (if alloc'ed) inside the decoder
     * specific info reader 
     */
    return (m4a_decoder_specific_info_t*)nkn_calloc_type(1,
							 sizeof(m4a_decoder_specific_info_t),
							 mod_vpe_m4a_generic);
}

void
m4a_cleanup_decoder_specific_info(m4a_decoder_specific_info_t *dc)
{
    SAFE_FREE(dc->GA_si);
    SAFE_FREE(dc);
}

int32_t
m4a_read_GA_specific_config(bitstream_state_t *bs, m4a_decoder_specific_info_t *si) 
{
    uint32_t frame_length_flag, depends_on_core_coder, extn_flag;

    frame_length_flag = 0;
    depends_on_core_coder = 0;
    extn_flag = 0;
    
    frame_length_flag = bio_read_bits(bs, 1);
    depends_on_core_coder = bio_read_bits(bs, 1);
    if(depends_on_core_coder) {
	 si->GA_si->core_coder_delay = bio_read_bits(bs, 14);
    }

    extn_flag = bio_read_bits(bs, 1);
    if(!si->channel_config) {
	/* read program config element */
	/* not supported */
    }
    
    if(si->base_object_type == 6 ||
       si->base_object_type == 20 ) {
	si->GA_si->layer_num = bio_read_bits(bs, 3);
    }

    if(extn_flag) {
	if(si->base_object_type == 22) {
	    si->GA_si->num_sub_frames = bio_read_bits(bs, 5);
	    si->GA_si->layer_length = bio_read_bits(bs, 11);
	}
        if(si->base_object_type == 17 || si->base_object_type == 19 ||
	   si->base_object_type == 20 || si->base_object_type == 23 ) {
	    si->GA_si->aac_section_data_resilience_flag = bio_read_bits(bs, 1);
	    si->GA_si->aac_scale_factore_data_resilience_flag = bio_read_bits(bs, 1);
	    si->GA_si->aac_spectral_data_resilience_flag = bio_read_bits(bs, 1);
	}

	si->GA_si->extn_flag3 = bio_read_bits(bs, 1);
    }
       
    return 0;
}

int32_t
m4a_read_decoder_specific_info(bitstream_state_t *bs, m4a_decoder_specific_info_t *m4a_dec_si)
{
    m4a_dec_si->base_object_type = bio_read_bits(bs, 5);
    m4a_dec_si->sampling_freq_idx = bio_read_bits(bs, 4);
    if(m4a_dec_si->sampling_freq_idx == 0x0F) {
	m4a_dec_si->sampling_freq = bio_read_bits(bs, 24);
    } else {
	m4a_dec_si->sampling_freq = sampling_freq_tbl[m4a_dec_si->sampling_freq_idx];
    }

    m4a_dec_si->channel_config = bio_read_bits(bs, 4);
    m4a_dec_si->sbr_present_flag = -1;
    if(m4a_dec_si->base_object_type == 5) {
	uint32_t extn_audio_type;
	extn_audio_type = m4a_dec_si->base_object_type;
	m4a_dec_si->sbr_present_flag = 1;
	m4a_dec_si->extn_sampling_freq_idx = bio_read_bits(bs, 4);
	if(m4a_dec_si->extn_sampling_freq_idx == 0x0F) {
	    m4a_dec_si->extn_sampling_freq = bio_read_bits(bs, 24);
	}
	m4a_dec_si->base_object_type = bio_read_bits(bs, 5);
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
		    nkn_calloc_type(1,
				    sizeof(m4a_GA_specific_config_t),
				    mod_vpe_m4a_generic);
	    m4a_read_GA_specific_config(bs, m4a_dec_si);
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
		ep_config = bio_read_bits(bs ,2);
		if(ep_config == 2 ||
		   ep_config == 3) {
		    // not supported
		}
		if(ep_config == 3) {
		    /* read direct mapping */
		    bio_read_bits(bs, 2);
		}
	    }
    }
    
    if(m4a_dec_si->extn_audio_object_type != 5 && bio_get_remaining_bytes(bs) >= 2) {
	uint32_t sync_extn_type;

	/* read sync extn type */
	sync_extn_type = bio_read_bits(bs, 11);
	if(sync_extn_type == 0x2b7) {
	    m4a_dec_si->extn_audio_object_type = bio_read_bits(bs, 5);
	    if(m4a_dec_si->extn_audio_object_type == 5) {
		m4a_dec_si->sbr_present_flag = bio_read_bits(bs, 1);
		if(m4a_dec_si->sbr_present_flag == 1) {
		    m4a_dec_si->extn_sampling_freq_idx = bio_read_bits(bs, 4);
		    if(m4a_dec_si->extn_sampling_freq_idx == 0x0f) {
			m4a_dec_si->extn_sampling_freq = bio_read_bits(bs, 24);
		    }
		}
	    }
	}
    }
    
    return 0;
}
