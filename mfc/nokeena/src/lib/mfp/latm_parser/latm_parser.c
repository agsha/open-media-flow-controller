#include "common.h"
//#include "aac.h"
#include "get_bits.h"

#if defined(UNIT_TEST)
#define A_(x)   x
#define B_(x)   x
#define C_(x)  	x
//#define DBG_MFPLOG(dId,dlevel,dmodule,fmt,...) //
#else
#define A_(x)   //x
#define B_(x)   //x
#define C_(x)   //x
#endif

static int check_payload_mem(
	struct LATMContext *latmctx);


/**
 * Inititalize GetBitContext.
 * @param buffer bitstream buffer, must be FF_INPUT_BUFFER_PADDING_SIZE bytes larger than the actual read bits
 * because some optimized bitstream readers read 32 or 64 bit at once and could read over the end
 * @param bit_size the size of the buffer in bits
 */
void init_get_bits(GetBitContext *s, const uint8_t *buffer,
                                 int bit_size)
{
    int buffer_size = (bit_size+7)>>3;
    if (buffer_size < 0 || bit_size < 0) {
        buffer_size = bit_size = 0;
        buffer = NULL;
    }

    s->buffer       = buffer;
    s->size_in_bits = bit_size;
    s->size_in_bits_plus8 = bit_size + 8;
    s->buffer_end   = buffer + buffer_size;
    s->index        = 0;
}


static inline uint32_t latm_get_value(GetBitContext *b)
{
    int length = get_bits(b, 2);

    return get_bits_long(b, (length+1)*8);
}



static int read_stream_mux_config(struct LATMContext *latmctx,
                                  GetBitContext *gb)
{
    int ret, audio_mux_version = get_bits(gb, 1);

    uint32_t prog, lay;
    uint32_t usesameConfig;
    uint32_t cur_strmCnt;
	
    latmctx->audio_mux_version_A = 0;
    if (audio_mux_version)
        latmctx->audio_mux_version_A = get_bits(gb, 1);

    if (!latmctx->audio_mux_version_A) {

        if (audio_mux_version)
            latm_get_value(gb);                 // taraFullness

	latmctx->streamCnt = 0;
    latmctx->allStreamSameTimeFraming = get_bits(gb, 1);   // allStreamSameTimeFraming
    latmctx->numSubFrames = get_bits(gb, 6);              // numSubFrames
    B_(printf("numSubFrames %u ", latmctx->numSubFrames+1));
    latmctx->numPrograms = get_bits(gb, 4); 

	// for each program (which there is only on in DVB)
	for(prog = 0; prog <= latmctx->numPrograms; prog++){

        // for each layer (which there is only on in DVB)
        latmctx->numLayer[prog] = get_bits(gb, 3);

		for(lay = 0; lay <= latmctx->numLayer[prog]; lay++){

			latmctx->progSIndx[latmctx->streamCnt] = prog; 
			latmctx->laySIndx[latmctx->streamCnt] = lay;
			latmctx->streamID[prog][lay] = latmctx->streamCnt++;

			if(!prog && !lay){
				usesameConfig = 0;
			} else{
				usesameConfig = get_bits(gb, 1);
			}

			if(!usesameConfig){
		        // for all but first stream: use_same_config = get_bits(gb, 1);
		        if (!audio_mux_version) {
		            latmctx->m4a_dec_si[lay] = initAscInfo();
			    	parseAscInfo(gb, latmctx->m4a_dec_si[lay]); 
		        } else {
		            int ascLen = latm_get_value(gb);
		            ret = gb->index;
		            latmctx->m4a_dec_si[lay] = initAscInfo();
				    parseAscInfo(gb,latmctx->m4a_dec_si[lay]); 
				    ret = gb->index - ret;
		            ascLen -= ret;
		            skip_bits_long(gb, ascLen);
		        }
			}
			cur_strmCnt = latmctx->streamID[prog][lay];
	        latmctx->frame_length_type[cur_strmCnt] = get_bits(gb, 3);
	        switch (latmctx->frame_length_type[cur_strmCnt]) {
		        case 0:
					 // latmBufferFullness
		            latmctx->latmBufferFullness[cur_strmCnt] = get_bits(gb, 8);      
					if(!latmctx->allStreamSameTimeFraming){
						m4a_decoder_specific_info_t *curlayer, *prevlayer;
						curlayer = latmctx->m4a_dec_si[lay];
						prevlayer = NULL;
						if(lay > 0)
							prevlayer = latmctx->m4a_dec_si[lay -1];
						if((curlayer && (curlayer->base_object_type == 6 ||
							curlayer->base_object_type == 20))&&
						   (prevlayer && (prevlayer->base_object_type == 8 ||
							prevlayer->base_object_type == 24))){
							latmctx->coreFrameOffset = get_bits(gb, 6);
						}
					}
		            break;
		        case 1:
		            latmctx->frame_length[cur_strmCnt] = get_bits(gb, 9);
		            break;
		        case 3:
		        case 4:
		        case 5:
		            latmctx->CELPframeLengthTableIndex[cur_strmCnt] = get_bits(gb, 6);       
		            break;
		        case 6:
		        case 7:
		            latmctx->HVXCframeLengthTableIndex[cur_strmCnt] = get_bits(gb, 1);       
		            break;
        	}
		}
	}
	uint32_t tmp;
	if (get_bits(gb, 1)) {                  // other data
            if (audio_mux_version) {
                latmctx->otherDataLengthBits = latm_get_value(gb);             // other_data_bits
            } else {
	            latmctx->otherDataLengthBits = 0;
                int esc;
                do {
  		    latmctx->otherDataLengthBits = latmctx->otherDataLengthBits << 3;
                    esc = get_bits(gb, 1);
                    tmp = get_bits(gb, 8);
		    latmctx->otherDataLengthBits += tmp;
                } while (esc && gb->index < gb->size_in_bits_plus8);
            }
        }

        if (get_bits(gb, 1))                     // crc present
            skip_bits(gb, 8);                    // config_crc
    }

    return VPE_SUCCESS;
}

static int fill_frame_len(struct LATMContext *latmctx, 
	uint32_t strmCnt,
	uint32_t subFrameIdx){

	uint32_t t_index;
	uint32_t code;
	
	uint32_t HVXC_framelen_type7[4][2]= { 
		{40, 80},
		{28, 40},
		{2,  25},
		{0,  3}};

	uint32_t CELP_framelen_type4[62] = {
	154,170,186,147,156,165,114,120,126,132,138,142,146,154,166,174,182,190,198,
	206,210,214,110,114,118,120,122,186,218,230,242,254,266,278,286,294,318,342,
	358,374,390,406,422,136,142,148,154,160,166,170,174,186,198,206,214,222,230,
	238,216,160,280,338
	};

	uint32_t CELP_framelen_type3[2][62]={
   {156,172,188,149,158,167,116,122,128,134,140,144,148,156,168,176,184,192,200,
	208,212,216,112,116,120,122,124,188,220,232,244,256,268,280,288,296,320,344,
	360,376,392,408,424,138,144,150,156,162,168,172,176,188,200,208,216,224,232,
	240,218,162,282,340},
   {134,150,166,127,136,145,94 ,100,106,112,118,122,126,134,146,154,162,170,178,
    186,190,194,90 ,94 ,98 ,100,102,166,174,186,198,210,222,234,242,250,276,298,
    314,330,346,362,378,92 ,98 ,104,110,116,122,126,130,142,154,162,170,178,186,
    194,172,116,238,296}};

	uint32_t CELP_framelen_type5[4][62]={
   {156,172,188,149,158,167,116,122,128,134,140,144,148,156,168,176,184,192,200,
   	208,212,216,112,116,120,122,124,188,220,232,244,256,268,280,288,296,320,344,
   	360,376,392,408,424,138,144,150,156,162,168,172,176,188,200,208,216,224,232,
   	240,218,162,282,340},
   {23,23,23,23,23,23,23,23,
    23,23,23,23,23,23,23,23,
    23,23,23,23,23,23,23,23,
    23,23,23,23,40,40,40,40,
    40,40,40,40,40,40,40,40,
    40,40,40,40,40,40,40,40,
    40,40,40,40,40,40,40,40,
    40,40,40,40,40,40},
   {8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,
    8,8,8,8,8,8},
   {2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,
    2,2,2,2,2,2}};
   

	switch(latmctx->frame_length_type[strmCnt]){
		case 1:
			   latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt] =
					   	8 * (latmctx->frame_length[strmCnt] + 20);
			   break;
		case 3:
				t_index = latmctx->HVXCframeLengthTableIndex[strmCnt];
				code = latmctx->MuxSlotLengthCoded[subFrameIdx][strmCnt];
				latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt] = 
					CELP_framelen_type3[code][t_index];
				break;
				
		case 4:
				t_index = latmctx->HVXCframeLengthTableIndex[strmCnt];
				code = latmctx->MuxSlotLengthCoded[subFrameIdx][strmCnt];
				latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt] = 
					CELP_framelen_type4[t_index];
				break;

		case 5:
				t_index = latmctx->HVXCframeLengthTableIndex[strmCnt];
				code = latmctx->MuxSlotLengthCoded[subFrameIdx][strmCnt];
				latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt] = 
					CELP_framelen_type5[code][t_index];
				break;

		case 6:
				if(!latmctx->HVXCframeLengthTableIndex[strmCnt])
					latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt] = 40;
				else
					latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt] = 80;					
				break;

		case 7:
				t_index = latmctx->HVXCframeLengthTableIndex[strmCnt];
				code = latmctx->MuxSlotLengthCoded[subFrameIdx][strmCnt];
				latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt] = 
					HVXC_framelen_type7[code][t_index];
				//assert(code != 3 && t_index != 0);
				//if(code == 3 && t_index == 0)
				break;

		default:
				break;
			
	}

	return VPE_SUCCESS;
}


static int read_payload_length_info(
	struct LATMContext *latmctx, 
	GetBitContext *gb,
	uint32_t subFrameIdx)
{
    uint8_t tmp;
    uint32_t prog, lay;
    uint32_t strmCnt;
    uint32_t chunkCnt;
    uint32_t strmIndx;
    
    if(latmctx->allStreamSameTimeFraming){
	    for(prog = 0; prog <= latmctx->numPrograms; prog++){
	    	for(lay = 0; lay <= latmctx->numLayer[prog]; lay++){
		    strmCnt = latmctx->streamID[prog][lay];
		    if (latmctx->frame_length_type[strmCnt] == 0) {
		        latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt] = 0;
		        do {
		            tmp = get_bits(gb, 8);
		            latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt] += tmp;
		        } while (tmp == 255);
			latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt] <<= 3; //bytes to bits
		    } else if (latmctx->frame_length_type[strmCnt] == 3 ||
		               latmctx->frame_length_type[strmCnt] == 5 ||
		               latmctx->frame_length_type[strmCnt] == 7) {
		        	latmctx->MuxSlotLengthCoded[subFrameIdx][strmCnt] = get_bits(gb, 2);
				fill_frame_len(latmctx, strmCnt, subFrameIdx);
		    } else if (latmctx->frame_length_type[strmCnt] == 1 ||
		    	   latmctx->frame_length_type[strmCnt] == 4 ||
		    	   latmctx->frame_length_type[strmCnt] == 6 ){
		    		fill_frame_len(latmctx, strmCnt, subFrameIdx);
		    }
	    	}
	    }
    }else{
    	latmctx->numChunks = get_bits(gb, 4);
		for(chunkCnt = 0; chunkCnt <= latmctx->numChunks; chunkCnt++){
			strmIndx = get_bits(gb, 4);
			prog = latmctx->progCIndx[chunkCnt] = latmctx->progSIndx[strmIndx];
			lay = latmctx->layCIndx[chunkCnt] = latmctx->laySIndx[strmIndx];
			strmCnt = latmctx->streamID[prog][lay];
			if (latmctx->frame_length_type[strmCnt] == 0) {
				latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt] = 0;
				do {
				   tmp = get_bits(gb, 8);
				   latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt] += tmp;
				} while (tmp == 255);
				latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt] <<= 3; //bytes to bits
				latmctx->AuEndFlag[strmCnt] = get_bits(gb,1);
			} else if  (latmctx->frame_length_type[strmCnt] == 3 ||
		      		latmctx->frame_length_type[strmCnt] == 5 ||
		      		latmctx->frame_length_type[strmCnt] == 7) {
					latmctx->MuxSlotLengthCoded[subFrameIdx][strmCnt]=get_bits(gb,2);
					fill_frame_len(latmctx, strmCnt, subFrameIdx);
		    } else if (latmctx->frame_length_type[strmCnt] == 1 ||
	    		   latmctx->frame_length_type[strmCnt] == 4 ||
	    		   latmctx->frame_length_type[strmCnt] == 6 ){
	    			fill_frame_len(latmctx, strmCnt, subFrameIdx);
		    }
		}
	}
    return VPE_SUCCESS;
}

static int print_debug_log(
	struct LATMContext * latmctx, 
	GetBitContext *gb, 
	uint32_t strmCnt,
	uint32_t subFrameIdx){
	
	uint32_t i;
	
	B_(printf("numPrograms %u ", latmctx->numPrograms+1));
	B_(printf("numChunks %u \n", latmctx->numChunks+1));
	B_(printf("numLayer "));
	for(i = 0; i < latmctx->numPrograms+1; i++){
		B_(printf("%u ", latmctx->numLayer[i]+1));
	}
	B_(printf("\nStrmCnt %u PayloadLen %u ", strmCnt, latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt]));
	B_(printf("gb-pos %u(%u)\n\n",  gb->index, gb->size_in_bits_plus8));
	return VPE_SUCCESS;
}

static int read_payload_mux(
	struct LATMContext *latmctx, 
	GetBitContext *gb,
	uint32_t subFrameIdx){

	uint32_t prog, lay;
	uint32_t strmCnt;
	uint32_t chunkCnt;
	uint32_t i;
	//uint8_t *base_addr;
	uint32_t payload_len_bits;
	uint32_t payload_len_bytes;
	

	if(latmctx->allStreamSameTimeFraming){
		  for(prog = 0; prog <= latmctx->numPrograms; prog++){
		      for(lay = 0; lay <= latmctx->numLayer[prog]; lay++){
			  	strmCnt = latmctx->streamID[prog][lay];
				payload_len_bits = latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt];
				payload_len_bytes = (payload_len_bits + 7) >> 3;

				if(payload_len_bits){
					//base_addr = latmctx->latm_payload[subFrameIdx][strmCnt].payload = (uint8_t *)malloc(
					//	payload_len_bytes * sizeof(uint8_t));
					
					latmctx->latm_payload[subFrameIdx][strmCnt].payload_filllen = 0;

					if(((uint32_t)get_bits_left(gb)) >= payload_len_bits){
						//memcpy(base_addr, gb->buffer + (gb->index>>3), payload_len_bytes);
						latmctx->audio_vector[subFrameIdx][strmCnt].data = (uint8_t *)gb->buffer;
						latmctx->audio_vector[subFrameIdx][strmCnt].offset = (gb->index>>3) ;
						latmctx->audio_vector[subFrameIdx][strmCnt].length = payload_len_bytes;
						latmctx->latm_payload[subFrameIdx][strmCnt].payload_filllen = payload_len_bits;
						gb->index += payload_len_bits;
					}else{
						B_(printf("LAYERS: not enough payload bits\n"));
						print_debug_log(latmctx, gb, strmCnt, subFrameIdx);
					}
				}
		      }
		  }
	}else{
			for(chunkCnt = 0; chunkCnt <= latmctx->numChunks; chunkCnt++){
				prog = latmctx->progCIndx[chunkCnt];
				lay = latmctx->layCIndx[chunkCnt];
				strmCnt = latmctx->streamID[prog][lay];
				payload_len_bits = latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt];
				payload_len_bytes = (payload_len_bits + 7) >> 3;

				if(payload_len_bits){
					//base_addr = latmctx->latm_payload[subFrameIdx][strmCnt].payload = (uint8_t *)malloc(
					//	payload_len_bytes * sizeof(uint8_t));
					latmctx->latm_payload[subFrameIdx][strmCnt].payload_filllen = 0;
					if(((uint32_t)get_bits_left(gb)) >= payload_len_bits){
						//memcpy(base_addr, gb->buffer + (gb->index>>3), payload_len_bytes);
						latmctx->audio_vector[subFrameIdx][strmCnt].data = (uint8_t *) gb->buffer;
						latmctx->audio_vector[subFrameIdx][strmCnt].offset = (gb->index>>3);
						latmctx->audio_vector[subFrameIdx][strmCnt].length = payload_len_bytes;
						latmctx->latm_payload[subFrameIdx][strmCnt].payload_filllen = payload_len_bits;
						gb->index += payload_len_bits;
					}else{
						B_(printf("CHUNKS: not enough payload bits\n"));
						print_debug_log(latmctx, gb, strmCnt, subFrameIdx);
					}
				}
			}
	}
	return VPE_SUCCESS;
}



static int read_audio_mux_element(struct LATMContext *latmctx,
                                  GetBitContext *gb)
{
    int err  = VPE_SUCCESS;
	uint32_t i;
	static uint32_t AudioMuxElementCnt;
	
    uint8_t use_same_mux = get_bits(gb, 1);
    if (!use_same_mux) {
        err = read_stream_mux_config(latmctx, gb);
		if(err == VPE_ERROR){
			printf("failed to parse stream mux config");
			return err;
		}
    } 
    if (latmctx->audio_mux_version_A == 0) {
		for( i = 0; i < latmctx->numSubFrames + 1; i++){
	        err = read_payload_length_info(latmctx, gb, i);
			if(err == VPE_ERROR){
				printf("failed to parse payload length info");
				return err;
			}
			err = read_payload_mux(latmctx, gb, i);
			if(err == VPE_ERROR){
				printf("failed to parse payload");
				return err;
			}
		}
    }
	err = check_payload_mem(latmctx);
	if(err == VPE_ERROR){
		printf("payload check failed");
		return err;
	}

	//NKN_ASSERT(gb->index == gb->size_in_bits);
    C_(printf("MuxElement Rd %u OthersDataBits %u Total %u\n", gb->index, 
		latmctx->otherDataLengthBits, 
		gb->size_in_bits));
	C_(print_hex_buffer((uint8_t *)(gb->buffer + (gb->index >> 3)), 32, (uint8_t *)"others"));

	return err;
}

static int check_payload_mem(
	struct LATMContext *latmctx){
	
	uint32_t prog, lay;
	uint32_t strmCnt;
	uint32_t chunkCnt;
	uint32_t payload_len;
#if defined(UNIT_TEST)
	dbg_res_t *res = (dbg_res_t *)calloc(sizeof(dbg_res_t), 1);
#else
	dbg_res_t *res = (dbg_res_t *)nkn_calloc_type(1, sizeof(dbg_res_t), mod_vpe_mfp_latm_parser);
#endif
	uint32_t subFrameIdx;
	
	for( subFrameIdx = 0; subFrameIdx <= latmctx->numSubFrames; subFrameIdx++){
		if(latmctx->allStreamSameTimeFraming){
		  B_(printf("numPrograms %u ",latmctx->numPrograms));
		  for(prog = 0; prog <= latmctx->numPrograms; prog++){
			  B_(printf("numLayer %u ",latmctx->numLayer[prog]));
			  for(lay = 0; lay <= latmctx->numLayer[prog]; lay++){
			  	res->num_valid_strms++;
				strmCnt = latmctx->streamID[prog][lay];
				payload_len = latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt];
			  	//res->num_valid_strms++;
				if(payload_len){
					if(payload_len == latmctx->latm_payload[subFrameIdx][strmCnt].payload_filllen){
						B_(printf("strmCnt %u, payloadlen %u[%u]\n", strmCnt, payload_len,
							latmctx->latm_payload[subFrameIdx][strmCnt].payload_filllen));
						res->num_payloadrd_pass++;
					}else{
						res->num_payloadrd_fail++;
					}
				}else{
					res->num_payload_size0++;
					//res->num_payloadrd_pass++;
				}
			  }
		  	}
		 }else{
			  for(chunkCnt = 0; chunkCnt <= latmctx->numChunks; chunkCnt++){
				  B_(printf("numChunks %u ",latmctx->numChunks));
				  prog = latmctx->progCIndx[chunkCnt];
				  lay = latmctx->layCIndx[chunkCnt];
				  strmCnt = latmctx->streamID[prog][lay];
				  payload_len = latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt];
				  res->num_valid_strms++;
				  if(payload_len){
					  if(payload_len == latmctx->latm_payload[subFrameIdx][strmCnt].payload_filllen){
						  B_(printf("strmCnt %u, payloadlen %u[%u]\n", strmCnt, payload_len,
							  latmctx->latm_payload[subFrameIdx][strmCnt].payload_filllen));
						  res->num_payloadrd_pass++;
					  }else{
						  res->num_payloadrd_fail++;
					  }			  	
				  } else{
					  res->num_payload_size0++;
					  //res->num_payloadrd_pass++;
				  }
			  }
		 }
  	 }

	 A_(printf("Stats: NumValidStreams %u ", res->num_valid_strms));
	 A_(printf("PLPass %u PLFail %u PLSizeZero %u\n", 
	 	res->num_payloadrd_pass, res->num_payloadrd_fail,
	 	res->num_payload_size0));
     if((res->num_valid_strms != (res->num_payloadrd_pass + res->num_payload_size0))){
#if !defined(UNIT_TEST)
//	 	NKN_ASSERT(0);
#endif
		return VPE_ERROR;
     }
	 latmctx->mux_element_rpt = res;

	 return VPE_SUCCESS;
                                  
}

void print_hex_buffer(uint8_t *data, uint32_t count, uint8_t *name){
	uint32_t i;
	C_(printf("%s", name));
	for(i = 0; i < count; i++){
		if(!(i % 16)) { C_(printf("\n")); }
		C_(printf("%02x ", *(data + i)));
	}
	C_(printf("\n"));
	return;
}


static int assign_PTS_DTS(
		struct LATMContext *latmctx,
		uint32_t PTS_value, 
		uint32_t DTS_value){
		latmctx->PTS_Value[latmctx->TotTimestampCnt] = PTS_value;
		latmctx->DTS_Value[latmctx->TotTimestampCnt] = DTS_value;
		latmctx->TotTimestampCnt++;	
		return VPE_SUCCESS;
}

static int calc_sample_duration(struct LATMContext *latmctx){
	uint32_t i;
	C_(printf("TotTimestampCnt %u\n",latmctx->TotTimestampCnt));
	for(i = 0; i < latmctx->TotTimestampCnt-1; i++){
		latmctx->sampleDuration[i] = latmctx->DTS_Value[i+1] - latmctx->DTS_Value[i];
		latmctx->compositeDuration[i] = latmctx->PTS_Value[i] - latmctx->DTS_Value[i];	
		C_(printf("cnt %u SamDur %u ComDurn %u DTS %u PTS %x\n", 
			i, latmctx->sampleDuration[i], latmctx->compositeDuration[i],
			latmctx->DTS_Value[i], latmctx->PTS_Value[i]));
	}
	if(i>0)
	latmctx->sampleDuration[i] = latmctx->sampleDuration[i -1];
	latmctx->compositeDuration[i] = latmctx->PTS_Value[i] - latmctx->DTS_Value[i];	
	C_(printf("cnt %u SamDur %u ComDurn %u DTS %u PTS %x\n", 
		i, latmctx->sampleDuration[i], latmctx->compositeDuration[i],
		latmctx->DTS_Value[i], latmctx->PTS_Value[i]));
	return VPE_SUCCESS;
}

static int32_t free_asc_info(struct LATMContext *latmctx){

	uint32_t prog, lay;

	for(prog = 0; prog <= latmctx->numPrograms; prog++){
		for(lay = 0; lay <= latmctx->numLayer[prog]; lay++){
			if(latmctx->m4a_dec_si[lay] != NULL){
				delete_asc_info(latmctx->m4a_dec_si[lay]);
				latmctx->m4a_dec_si[lay] = NULL;
			}
		}
	}

	return VPE_SUCCESS;
}


static int32_t free_payload_mem(struct LATMContext *latmctx){

	uint32_t prog, lay;
	uint32_t strmCnt;
	uint32_t payload_len;
	uint32_t subFrameIdx;
	
	for(subFrameIdx = 0; subFrameIdx < MAX_NUM_SUBFRAMES; subFrameIdx++){
		for(strmCnt = 0; strmCnt < MAX_NUM_STREAMS; strmCnt++){
			if(latmctx->MuxSlotLengthBits[subFrameIdx][strmCnt]){
				if(latmctx->latm_payload[subFrameIdx][strmCnt].payload != NULL){
					free(latmctx->latm_payload[subFrameIdx][strmCnt].payload);
					latmctx->latm_payload[subFrameIdx][strmCnt].payload = NULL;
				}
			}
		}
	}

	
	return VPE_SUCCESS;
}


int32_t free_latmctx_mem(
	struct LATMContext **latmctx,
	uint32_t num_latmmux_elements){

	uint32_t cnt;

	for(cnt = 0; cnt < num_latmmux_elements; cnt++){

		if(latmctx[cnt] != NULL){
			free_asc_info(latmctx[cnt]);
			free_payload_mem(latmctx[cnt]);
			
			if(latmctx[cnt]->mux_element_rpt != NULL){
			   //DBG_MFPLOG("LATM", MSG, MOD_MFPLIVE, "free latmctx[%d]->mux_element_rpt", cnt);
			   free(latmctx[cnt]->mux_element_rpt);
			   latmctx[cnt]->mux_element_rpt = NULL;
			}

			if(latmctx[cnt]->mux_element != NULL){
				//DBG_MFPLOG("LATM", MSG, MOD_MFPLIVE, "free latmctx[%d]->mux_element", cnt);
				free(latmctx[cnt]->mux_element);
				latmctx[cnt]->mux_element = NULL;
			}

			//DBG_MFPLOG("LATM", MSG, MOD_MFPLIVE, "free latmctx[%d]", cnt);
			free(latmctx[cnt]);
			latmctx[cnt] = NULL;
		}
	}
	
	return VPE_SUCCESS;
}



int parse_loas_latm(
	uint8_t *data, 
	uint32_t *slve_pkts, 
	uint32_t slve_npkts,
	struct LATMContext **latmctx,
	uint32_t *num_latmmux_elements){

	uint32_t pkt_cnt;
	uint32_t pos;
	uint8_t payload_unit_start_indicator;
	uint16_t rpid;
	uint8_t	continuity_counter, nxt_continuity_counter;
	uint8_t adaptation_field_control;	

	uint64_t PTS_value;  
	uint64_t DTS_value;		
	uint32_t payload_pos;
	uint32_t init_pos;
	uint32_t first_frame_start = 0;
	uint32_t latm_chunk_cnt = 0;

	GetBitContext	    gb;

	uint32_t payload_len;
	uint32_t mux_element_start_pkt_idx;
	nxt_continuity_counter = 0;
	
	for( pkt_cnt = 0; pkt_cnt < slve_npkts; pkt_cnt++){
	    payload_unit_start_indicator = 0;
		PTS_value = 0;
		DTS_value = 0;
		init_pos = slve_pkts[pkt_cnt];
		pos = slve_pkts[pkt_cnt];
		pos = parse_TS_header(data, pos, &payload_unit_start_indicator, 
			&rpid, &continuity_counter, &adaptation_field_control);
		if(pos == 0xffffffff){
			//DBG_MFPLOG("LATM", ERROR, MOD_MFPLIVE, "error parsing TS header");
			free_latmctx_mem(latmctx, ++latm_chunk_cnt);
			return VPE_ERROR;			
		}
		if(payload_unit_start_indicator && !first_frame_start){
			first_frame_start = 1;
		}		
		if(!first_frame_start)
			continue;
		if(pkt_cnt == 0){
			//assign same for first packet in chunk - to avoid false alarm
			nxt_continuity_counter = continuity_counter;			
		}
		if(continuity_counter != nxt_continuity_counter){
			//DBG_MFPLOG("LATM", ERROR, MOD_MFPLIVE, "audio - missing continuity counter");
			free_latmctx_mem(latmctx, ++latm_chunk_cnt);
			return VPE_ERROR;			
		}
		//calc next continuity counter 
		nxt_continuity_counter = (continuity_counter+1) % 16;

		
		if(payload_unit_start_indicator == 1){
			pos = parse_PES_header(data, pos, &PTS_value, &DTS_value, &payload_pos); 
			if(pos == 0xffffffff){
				//DBG_MFPLOG("LATM", ERROR, MOD_MFPLIVE, "error parsing PES header");
				free_latmctx_mem(latmctx, ++latm_chunk_cnt);
				return VPE_ERROR;			
			}				
		}	
		//new frame starts if previous frame length is 0 and PSI flag is 1
		if((payload_unit_start_indicator == 1) && (latmctx[latm_chunk_cnt] == NULL)){
#if defined (UNIT_TEST)
			latmctx[latm_chunk_cnt] = (struct LATMContext *)calloc(sizeof(struct LATMContext), 1);
#else
			latmctx[latm_chunk_cnt] = (struct LATMContext *)nkn_calloc_type(1, sizeof(struct LATMContext), 
						mod_vpe_mfp_latm_parser);
			//DBG_MFPLOG("LATM", MSG, MOD_MFPLIVE, "calloc latmctx[%d]", latm_chunk_cnt);
#endif
			if(latmctx[latm_chunk_cnt] == NULL){
				//DBG_MFPLOG("LATM", ERROR, MOD_MFPLIVE, "error allocating mem for latmctx in main");
				free_latmctx_mem(latmctx, ++latm_chunk_cnt);
				return VPE_ERROR;
			}
		} else if (payload_unit_start_indicator == 1){
			DBG_MFPLOG("LATM", ERROR, MOD_MFPLIVE,"PSI flag is 1 before previous EOF");
			free_latmctx_mem(latmctx, ++latm_chunk_cnt);
			return VPE_ERROR;		
		}
		if(latmctx[latm_chunk_cnt] == NULL){
			//DBG_MFPLOG("LATM", ERROR, MOD_MFPLIVE, "PSI flag is NULL at start of LATM frame");
			free_latmctx_mem(latmctx, ++latm_chunk_cnt);
			return VPE_ERROR;			
		}
		if(!latmctx[latm_chunk_cnt]->mux_element_length){
			payload_len = TS_PKT_SIZE - (pos - init_pos);
			pos = loas_parse(data, pos, payload_len, &latmctx[latm_chunk_cnt]->mux_element_length);
			mux_element_start_pkt_idx = pkt_cnt;
			latmctx[latm_chunk_cnt]->mux_element_wr_pos = 0;
			//latmctx[latm_chunk_cnt]->mux_element_length = 0;
#if defined (UNIT_TEST)
			latmctx[latm_chunk_cnt]->mux_element = (uint8_t *)malloc(latmctx[latm_chunk_cnt]->mux_element_length * sizeof(uint8_t));
#else
			latmctx[latm_chunk_cnt]->mux_element = (uint8_t *)nkn_malloc_type(
				latmctx[latm_chunk_cnt]->mux_element_length * sizeof(uint8_t),
				mod_vpe_mfp_latm_parser);
			//DBG_MFPLOG("LATM", MSG, MOD_MFPLIVE, "malloc latmctx[%d]->mux_element", latm_chunk_cnt);
#endif
			if(latmctx[latm_chunk_cnt]->mux_element == NULL){
				printf("unable to allocate mem for latmctx[latm_chunk_cnt]->mux_element");
				free_latmctx_mem(latmctx, ++latm_chunk_cnt);
				return VPE_ERROR;
			}
			latmctx[latm_chunk_cnt]->mux_element_wr_pos = 0;
			memcpy(latmctx[latm_chunk_cnt]->mux_element + latmctx[latm_chunk_cnt]->mux_element_wr_pos,
			       data + pos,
			       payload_len - 3);
			latmctx[latm_chunk_cnt]->mux_element_wr_pos += (payload_len - 3);
			latmctx[latm_chunk_cnt]->mux_element_length =  latmctx[latm_chunk_cnt]->mux_element_length - (payload_len - 3);
			       
		}else{
			payload_len = TS_PKT_SIZE - (pos - init_pos);
			memcpy(latmctx[latm_chunk_cnt]->mux_element + latmctx[latm_chunk_cnt]->mux_element_wr_pos,
			       data + pos,
			       payload_len);
			latmctx[latm_chunk_cnt]->mux_element_wr_pos += payload_len;
			latmctx[latm_chunk_cnt]->mux_element_length =  latmctx[latm_chunk_cnt]->mux_element_length - payload_len;
		}
		if(payload_unit_start_indicator){
			assign_PTS_DTS(latmctx[latm_chunk_cnt], PTS_value, DTS_value);
		}
		
		if(!latmctx[latm_chunk_cnt]->mux_element_length){
			//one complete AudioMuxElement
			init_get_bits(&gb, 
				latmctx[latm_chunk_cnt]->mux_element, 
				latmctx[latm_chunk_cnt]->mux_element_wr_pos<<3);
			A_(printf("----- AudioMuxElement# %d-------\n", latm_chunk_cnt));
			read_audio_mux_element(latmctx[latm_chunk_cnt], &gb);
			calc_sample_duration(latmctx[latm_chunk_cnt]);
			latm_chunk_cnt++;
		}
	}

	*num_latmmux_elements = latm_chunk_cnt;

	if(latmctx[latm_chunk_cnt] && latmctx[latm_chunk_cnt]->mux_element_length > 0){
		//one incomplete AudioMuxElement
		/*init_get_bits(&gb, latmctx[latm_chunk_cnt]->mux_element, latmctx[latm_chunk_cnt]->mux_element_wr_pos);
		A_(printf("----- Part AudioMuxElement# %d-------\n", latm_chunk_cnt));
		read_audio_mux_element(latmctx[latm_chunk_cnt], &gb);
		calc_sample_duration(latmctx[latm_chunk_cnt]);
		latm_chunk_cnt++;*/
		if(latmctx[latm_chunk_cnt]->mux_element != NULL){
			//DBG_MFPLOG("LATM", MSG, MOD_MFPLIVE, "free latmctx[%d]->mux_element", latm_chunk_cnt);
			free(latmctx[latm_chunk_cnt]->mux_element);
			latmctx[latm_chunk_cnt]->mux_element = NULL;
		}

		if(latmctx[latm_chunk_cnt] != NULL){
			//DBG_MFPLOG("LATM", MSG, MOD_MFPLIVE, "free latmctx[%d]", latm_chunk_cnt);
			free(latmctx[latm_chunk_cnt]);
			latmctx[latm_chunk_cnt] = NULL;
		}
		return (slve_npkts - mux_element_start_pkt_idx);
	}else{
		return 0;
	}

	
}
 
