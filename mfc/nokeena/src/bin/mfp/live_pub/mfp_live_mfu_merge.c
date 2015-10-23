#include "mfp_live_mfu_merge.h"
#include "nkn_memalloc.h"

#define MFU_FIXED_SIZE 16
#define FIXED_BOX_SIZE 8


static void clear_collected_mfus(mfu_data_t* mfus, uint32_t num_mfus);

static void deletemfuContext(mfu_contnr_t* mfucont); 

static void clear_mfucs_list( fmtr_mfu_list_t *fmt);
static void copy_mfu(mfu_data_t *,
		     mfu_data_t *);

static int32_t  write32_byte_swap( uint8_t *data,
                                   uint32_t pos,
                                   uint32_t val);

//static void clear_fmtr_mfu_list(fmtr_mfu_list_t *);

static int32_t mfu_mux_ts2mfu_desc_init(ts2mfu_desc_t*);

static int32_t mfu_demux_write_rwfg_box(ts2mfu_desc_t* ts2mfu_desc,
                                        uint8_t* rwfgbox_pos);


static int32_t  mfu_merge_write_vdat_box(uint8_t*,
                                         fmtr_mfu_list_t* );

static int32_t  mfu_merge_write_adat_box(uint8_t*,
                                         fmtr_mfu_list_t* );

static void parse_mfu_data(mfu_data_t *mfu,
                           mfu_demux_data_t *mfud);


static int32_t mfu_mux_data(fmtr_mfu_list_t *fmt);


static int32_t mfu_merge_fill_fields(fmtr_mfu_list_t *fmt,
                                     ts2mfu_desc_t* op_ts2mfu_desc);



static int32_t mfu_demux_mux_data(fmtr_mfu_list_t *fmt);


//#define AGG_SANITY_CHK

#ifdef AGG_SANITY_CHK
static void dump_input_fmts(fmtr_mfu_list_t *fmt,
			    uint32_t j){
    uint32_t len;
    char fname[500]={'\0'};
    FILE* fid;
    mfu_data_t * mfudata = &fmt->mfus[j];
    len = snprintf(NULL,0,"Sess%s_Stream%u_duration%u_mfu%u.mfu",fmt->sess_name,fmt->sess_id,fmt->target_duration,j);
    snprintf(fname,len+1,"Sess%s_Stream%u_duration%u_mfu%u.mfu",fmt->sess_name,fmt->sess_id,fmt->target_duration,j);
    fid = fopen(fname, "wb");
    fwrite(mfudata->data, mfudata->data_size,1,fid);
    fclose(fid);
    return;
}
#endif




int32_t mfp_live_accum_mfus(mfu_data_t *mfu,
			    fmtr_mfu_list_t *fmt,
			    void* its2mfu_desc,
			    uint32_t duration,
			    mfu_data_t **out_mfu){

    ts2mfu_desc_t* ts2mfu_desc;
    ts2mfu_desc = (ts2mfu_desc_t*)its2mfu_desc;
    memcpy(&fmt->mfus[fmt->num_mfus], mfu, sizeof(mfu_data_t));
    copy_mfu(&fmt->mfus[fmt->num_mfus], mfu);
    fmt->ts2mfu_desc[fmt->num_mfus] = ts2mfu_desc;
    //    DBG_MFPLOG ("MFUDEL", MSG, MOD_MFPLIVE," Adding to list %p, mfu number %u target duration %u",fmt->ts2mfu_desc[fmt->num_mfus], fmt->num_mfus,fmt->target_duration);
#ifdef AGG_SANITY_CHK
    dump_input_fmts(fmt,fmt->num_mfus);
#endif
    fmt->num_mfus++;
    fmt->duration_acced+=duration;

    if(fmt->duration_acced < fmt->target_duration)
	return MFU_AGG_WAIT;

    /*Sufficient Data: Create an output MFU*/
    mfu_demux_mux_data(fmt);
    /*Copy the output MFU structure*/
    *out_mfu = nkn_calloc_type(1, sizeof(mfu_data_t),mod_vpe_mfp_merge_t);
    copy_mfu(*out_mfu,fmt->out_mfu);
    free(fmt->out_mfu->data);
    fmt->out_mfu->data = NULL;


    /*Clear formatter structure*/
    clear_collected_mfus(fmt->mfus, fmt->num_mfus);
    clear_mfucs_list(fmt);
    //clear_fmtr_mfu_list(fmt);
    

    return 1;
}


int32_t clear_fmtr_vars( fmtr_mfu_list_t *fmt){
    clear_collected_mfus(fmt->mfus, fmt->num_mfus);
    //clear_mfucs_list(fmt);
    clear_fmtr_mfu_list(fmt);
    return 1;
}


static void clear_mfucs_list( fmtr_mfu_list_t *fmt){
    uint32_t j ;
    mfu_contnr_t* mfuc;
    for(j=0; j< fmt->num_mfus; j++){
	mfuc = fmt->mfud[j].mfuc;
	if(mfuc!=NULL){
	    deletemfuContext(mfuc);
	}
    }
    return;
}


static void copy_mfu(mfu_data_t * out_mfu,
		     mfu_data_t *imfu){
    mfu_data_t* omfu;
    omfu = out_mfu;
    omfu->data_size = imfu->data_size;
    omfu->data = nkn_calloc_type(1,omfu->data_size*sizeof(uint8_t),mod_vpe_mfp_merge_t2);
    memcpy(omfu->data,imfu->data,omfu->data_size);
    return;
}


static void clear_collected_mfus(mfu_data_t* mfus, uint32_t num_mfus){
    uint32_t j;
    for(j=0; j<num_mfus;j++){
	mfu_data_t* mfu = mfus+j;
	if(mfu->data){
	    free(mfu->data);
	    mfu->data = NULL;
	}
	mfu->data_size = 0;
    }
    return;
}

void clear_fmtr_mfu_list(fmtr_mfu_list_t *fmt){
    uint32_t j;
    /*    for(j=0; j<fmt->num_mfus;j++){
	DBG_MFPLOG ("MFUDEL", MSG, MOD_MFPLIVE,"Deleting in clear_fmtr_mfu_list %p",fmt->ts2mfu_desc[0]);
	ts2mfu_desc_free(fmt->ts2mfu_desc[fmt->num_mfus]);
    }
    */
    fmt->duration_acced = 0;
    fmt->num_mfus = 0;
}


static int32_t mfu_demux_mux_data(fmtr_mfu_list_t *fmt){
    /*Call Jegadish's parser and get the structures*/
    uint32_t j ;
    for(j=0; j< fmt->num_mfus; j++){
	parse_mfu_data(&fmt->mfus[j],&fmt->mfud[j]);
    }
    /*Mulitplex the MFUs back into one*/
    mfu_mux_data(fmt);
    return 1;

}


static int32_t mfu_mux_data(fmtr_mfu_list_t *fmt){
    int32_t ret;
    ts2mfu_desc_t* ts2mfu_desc, *op_ts2mfu_desc;
    uint32_t mfu_data_size,vdat_box_size,j;
    mfub_t *mfubox_header;
    uint8_t *rwfgbox_pos = NULL;
    uint8_t *vdatbox_pos = NULL;
    uint8_t* adatbox_pos = NULL; 
    uint8_t *mfubox_pos =NULL;
    uint32_t vdat_pos, adat_pos;

    mfubox_header = &fmt->mfud->mfub;
    op_ts2mfu_desc = (ts2mfu_desc_t*) nkn_calloc_type(1, sizeof(ts2mfu_desc_t),
						      mod_vpe_mfp_merge_t);
    /*MFU header handling*/
    op_ts2mfu_desc->mfub_box_size = sizeof(mfub_t);

    /*Get total number of Audio and Video Aus*/
    for(j=0; j< fmt->num_mfus; j++){
	ts2mfu_desc = fmt->ts2mfu_desc[j];
	if(ts2mfu_desc->is_video){
	    op_ts2mfu_desc->num_video_AU += ts2mfu_desc->num_video_AU;
	    if(!op_ts2mfu_desc->is_video)
		op_ts2mfu_desc->is_video = 1;
	}
	if (ts2mfu_desc->is_audio){
	    op_ts2mfu_desc->num_audio_AU+= ts2mfu_desc->num_audio_AU;
	    if(!op_ts2mfu_desc->is_audio)
		op_ts2mfu_desc->is_audio = 1;
	}
    }
    /*Allocate the Required memory*/
    ret = mfu_mux_ts2mfu_desc_init(op_ts2mfu_desc);

    /*Sizes calculation*/
    if(op_ts2mfu_desc->is_video){
	op_ts2mfu_desc->mfu_raw_box->videofg_size = (fmt->ts2mfu_desc[0])->mfu_raw_box->videofg_size;
	for(j=1; j< fmt->num_mfus; j++){
	    ts2mfu_desc = fmt->ts2mfu_desc[j];
	    op_ts2mfu_desc->mfu_raw_box->videofg_size+= 3*(4*ts2mfu_desc->num_video_AU);
	}
	op_ts2mfu_desc->mfu_raw_box->videofg.unit_count = op_ts2mfu_desc->num_video_AU;
	op_ts2mfu_desc->mfu_raw_box->videofg.timescale = mfubox_header->timescale;
	op_ts2mfu_desc->mfu_raw_box_size = op_ts2mfu_desc->mfu_raw_box->videofg_size + 4;
	for(j=0;j< fmt->num_mfus; j++){
	    ts2mfu_desc = fmt->ts2mfu_desc[j];
	    op_ts2mfu_desc->vdat_size+=ts2mfu_desc->vdat_size;
	    op_ts2mfu_desc->adat_size+=ts2mfu_desc->adat_size;
	}
    }
    if(op_ts2mfu_desc->is_audio){
	op_ts2mfu_desc->mfu_raw_box->audiofg.timescale = mfubox_header->timescale;
	op_ts2mfu_desc->mfu_raw_box->audiofg.unit_count = op_ts2mfu_desc->num_audio_AU;
	op_ts2mfu_desc->mfu_raw_box->audiofg_size = (fmt->ts2mfu_desc[0])->mfu_raw_box->audiofg_size;
	for(j=1; j< fmt->num_mfus; j++){
            ts2mfu_desc = fmt->ts2mfu_desc[j];
            op_ts2mfu_desc->mfu_raw_box->audiofg_size+= 3*(4*ts2mfu_desc->num_audio_AU);
        }
        op_ts2mfu_desc->mfu_raw_box_size += op_ts2mfu_desc->mfu_raw_box->audiofg_size + 4;
    }

    /*Fill in the fields*/
    mfu_merge_fill_fields(fmt, op_ts2mfu_desc);

    /*Create the MFU*/
    mfu_data_size = MFU_FIXED_SIZE + op_ts2mfu_desc->mfub_box_size + op_ts2mfu_desc->mfu_raw_box_size;
    mfu_data_size += (op_ts2mfu_desc->is_video)*(op_ts2mfu_desc->vdat_size+FIXED_BOX_SIZE)+(op_ts2mfu_desc->is_audio)*(op_ts2mfu_desc->adat_size+FIXED_BOX_SIZE); 
    fmt->out_mfu->data = (uint8_t *)nkn_malloc_type(mfu_data_size,mod_vpe_mfp_ts2mfu_t);

    /*Raw Box Handling */
    rwfgbox_pos=fmt->out_mfu->data + op_ts2mfu_desc->mfub_box_size + (1*(BOX_NAME_SIZE + BOX_SIZE_SIZE));
    mfu_demux_write_rwfg_box(op_ts2mfu_desc,rwfgbox_pos);


    /*Write Vdat Box*/
    if (op_ts2mfu_desc->is_video) {
        vdat_pos =  op_ts2mfu_desc->mfub_box_size +
            op_ts2mfu_desc->mfu_raw_box_size + (2 *(BOX_NAME_SIZE + BOX_SIZE_SIZE));
        vdatbox_pos = fmt->out_mfu->data + vdat_pos;
        mfu_merge_write_vdat_box(vdatbox_pos,fmt);
        vdat_box_size = BOX_NAME_SIZE + BOX_SIZE_SIZE;
    }

    /*Write Adat Box*/
    if (ts2mfu_desc->is_audio) {
        adat_pos = op_ts2mfu_desc->mfub_box_size + 
	    op_ts2mfu_desc->mfu_raw_box_size+ 
	    op_ts2mfu_desc->vdat_size + 
	    vdat_box_size + (2*( BOX_NAME_SIZE+BOX_SIZE_SIZE));
        adatbox_pos =  fmt->out_mfu->data  + adat_pos;
        mfu_merge_write_adat_box(adatbox_pos,fmt);
    }


    /*Write MFU Header*/
    mfubox_pos = fmt->out_mfu->data;
    mfubox_header->offset_vdat = vdat_pos;
    mfubox_header->offset_adat = adat_pos;
    mfubox_header->mfu_size = mfu_data_size;
    mfubox_header->duration = fmt->duration_acced/90000;
    mfu_write_mfub_box(op_ts2mfu_desc->mfub_box_size, 
		       mfubox_header,
		       mfubox_pos);
    fmt->out_mfu->data_size = mfu_data_size;

    ts2mfu_desc_free(op_ts2mfu_desc);
    return 1;
}


static int32_t  mfu_merge_write_vdat_box(uint8_t* vdatbox_pos,
					 fmtr_mfu_list_t* fmt){
    uint32_t pos = 0,j, vdat_size=0;
    mfu_demux_data_t *mfud = NULL;
    mfu_data_t* mfu = NULL;
    uint32_t local_vdat_pos, local_vdat_size;
    /* writting box_name */
    vdatbox_pos[pos] = 'v';
    vdatbox_pos[pos+1] = 'd';
    vdatbox_pos[pos+2] = 'a';
    vdatbox_pos[pos+3] = 't';
    pos += BOX_NAME_SIZE;
    pos += BOX_SIZE_SIZE;
    /*Copy the vdat's*/
    for(j=0;j< fmt->num_mfus; j++){
	mfud =  &fmt->mfud[j];
	mfu = &fmt->mfus[j];
	/*find the vdat and copy it*/
	local_vdat_pos = mfud->mfub.offset_vdat;
	local_vdat_size =  ntohl(*(uint32_t*)(mfu->data+ local_vdat_pos + 4));
	vdat_size+= local_vdat_size;
	memcpy(&vdatbox_pos[pos],
	       mfu->data+ local_vdat_pos + 8,
	       local_vdat_size);
	pos+=local_vdat_size;
    }
    pos = BOX_NAME_SIZE;
    write32_byte_swap(vdatbox_pos, pos, vdat_size);
    return 1;
}


static int32_t  mfu_merge_write_adat_box(uint8_t* adatbox_pos,
                                         fmtr_mfu_list_t* fmt){
    uint32_t pos = 0,j, adat_size=0;
    mfu_demux_data_t *mfud = NULL;
    mfu_data_t* mfu = NULL;
    uint32_t local_adat_pos, local_adat_size;
    /* writting box_name */
    adatbox_pos[pos] = 'a';
    adatbox_pos[pos+1] = 'd';
    adatbox_pos[pos+2] = 'a';
    adatbox_pos[pos+3] = 't';
    pos += BOX_NAME_SIZE;
    pos += BOX_SIZE_SIZE;
    /*Copy the vdat's*/
    for(j=0;j< fmt->num_mfus; j++){
        mfud =  &fmt->mfud[j];
        mfu = &fmt->mfus[j];
        /*find the vdat and copy it*/
        local_adat_pos = mfud->mfub.offset_adat;
        local_adat_size =  ntohl(*(uint32_t*)(mfu->data+ local_adat_pos + 4));
        adat_size+= local_adat_size;
        memcpy(&adatbox_pos[pos],
               mfu->data+ local_adat_pos + 8,
               local_adat_size);
        pos+=local_adat_size;
    }
    pos = BOX_NAME_SIZE;
    write32_byte_swap(adatbox_pos, pos, adat_size);
    return 1;
}



static int32_t mfu_merge_fill_fields(fmtr_mfu_list_t *fmt,
				     ts2mfu_desc_t* op_ts2mfu_desc){
    uint32_t j;
    ts2mfu_desc_t* ts2mfu_desc;
    /*
    for(j=0; j< fmt->num_mfus; j++){
        ts2mfu_desc = fmt->ts2mfu_desc[j];
        if(ts2mfu_desc->is_video){
            op_ts2mfu_desc->num_video_AU += ts2mfu_desc->num_video_AU;
            if(!op_ts2mfu_desc->is_video)
                op_ts2mfu_desc->is_video = 1;
        }
        if (ts2mfu_desc->is_audio){
            op_ts2mfu_desc->num_audio_AU+= ts2mfu_desc->num_audio_AU;
            if(!op_ts2mfu_desc->is_audio)
                op_ts2mfu_desc->is_audio = 1;
        }
    }
    */
    /*Codec Specific Data*/
    if(op_ts2mfu_desc->is_video){
	uint32_t sdpos=0, sspos=0, cdpos=0;
	ts2mfu_desc = fmt->ts2mfu_desc[0];
	op_ts2mfu_desc->mfu_raw_box->videofg.codec_info_size = ts2mfu_desc->mfu_raw_box->videofg.codec_info_size;
	op_ts2mfu_desc->mfu_raw_box->videofg.codec_specific_data = 
	    (uint8_t*)nkn_calloc_type(1, ts2mfu_desc->mfu_raw_box->videofg.codec_info_size, mod_vpe_mfp_merge_t);
	memcpy(op_ts2mfu_desc->mfu_raw_box->videofg.codec_specific_data,
	       ts2mfu_desc->mfu_raw_box->videofg.codec_specific_data,
	       ts2mfu_desc-> mfu_raw_box->videofg.codec_info_size);
	/*Sample duration and Sizes*/
	for(j=0; j< fmt->num_mfus; j++){
	    ts2mfu_desc = fmt->ts2mfu_desc[j];
	    memcpy(op_ts2mfu_desc->mfu_raw_box->videofg.sample_duration+sdpos,
		   ts2mfu_desc->mfu_raw_box->videofg.sample_duration,
		   4*ts2mfu_desc->num_video_AU);
	    sdpos+=ts2mfu_desc->num_video_AU;
	    memcpy(op_ts2mfu_desc->mfu_raw_box->videofg.composition_duration+cdpos,
		   ts2mfu_desc->mfu_raw_box->videofg.composition_duration,
		   4*ts2mfu_desc->num_video_AU);
	    cdpos+=ts2mfu_desc->num_video_AU;
	    memcpy(op_ts2mfu_desc->mfu_raw_box->videofg.sample_sizes+sspos,
		   ts2mfu_desc->mfu_raw_box->videofg.sample_sizes,
		   4*ts2mfu_desc->num_video_AU);
	    sspos+=ts2mfu_desc->num_video_AU;
	}
    }

    if(op_ts2mfu_desc->is_audio){
        uint32_t sdpos=0, sspos=0, cdpos=0;
        ts2mfu_desc = fmt->ts2mfu_desc[0];
	op_ts2mfu_desc->mfu_raw_box->audiofg.codec_info_size = ts2mfu_desc->mfu_raw_box->audiofg.codec_info_size;
	op_ts2mfu_desc->mfu_raw_box->audiofg.codec_specific_data = nkn_calloc_type(1,ts2mfu_desc-> mfu_raw_box->audiofg.codec_info_size,mod_vpe_mfp_merge_t);
        memcpy(op_ts2mfu_desc->mfu_raw_box->audiofg.codec_specific_data,
               ts2mfu_desc->mfu_raw_box->audiofg.codec_specific_data,
               ts2mfu_desc-> mfu_raw_box->audiofg.codec_info_size);
        /*Sample duration and Sizes*/
        for(j=0; j< fmt->num_mfus; j++){
            ts2mfu_desc = fmt->ts2mfu_desc[j];
            memcpy(op_ts2mfu_desc->mfu_raw_box->audiofg.sample_duration+sdpos,
                   ts2mfu_desc->mfu_raw_box->audiofg.sample_duration,
                   4*ts2mfu_desc->num_audio_AU);
            sdpos+=ts2mfu_desc->num_audio_AU;
            memcpy(op_ts2mfu_desc->mfu_raw_box->audiofg.composition_duration+cdpos,
                   ts2mfu_desc->mfu_raw_box->audiofg.composition_duration,
                   4*ts2mfu_desc->num_audio_AU);
            cdpos+=ts2mfu_desc->num_audio_AU;
            memcpy(op_ts2mfu_desc->mfu_raw_box->audiofg.sample_sizes+sspos,
                   ts2mfu_desc->mfu_raw_box->audiofg.sample_sizes,
                   4*ts2mfu_desc->num_audio_AU);
            sspos+=ts2mfu_desc->num_audio_AU;
        }
    }

    return 1;
}



static int32_t mfu_demux_write_rwfg_box(ts2mfu_desc_t* ts2mfu_desc,
					uint8_t* rwfgbox_pos){
    uint32_t mfu_raw_box_size;
    mfu_rwfg_t *mfu_raw_box;
    int32_t ret;
    mfu_raw_box_size = ts2mfu_desc->mfu_raw_box_size;
    mfu_raw_box = ts2mfu_desc->mfu_raw_box;
    ts2mfu_desc->mfu_join_mode = 2;
    ts2mfu_desc->ignore_PTS_DTS = 2;
    ret = mfu_write_rwfg_box(mfu_raw_box_size,
			     mfu_raw_box,
			     rwfgbox_pos,
			     ts2mfu_desc);
    return 1;

}




static int32_t mfu_mux_ts2mfu_desc_init(ts2mfu_desc_t* ts2mfu_desc){
    uint32_t *temp = NULL, *temp_1 = NULL;
    ts2mfu_desc->mfub_box = (mfub_t*) nkn_calloc_type(1, sizeof(mfub_t),
						      mod_vpe_mfp_merge_t);
    ts2mfu_desc->mfu_raw_box = (mfu_rwfg_t*)nkn_calloc_type(1, sizeof(mfu_rwfg_t),
							    mod_vpe_mfp_merge_t);
    if(ts2mfu_desc->is_video){
	temp = (uint32_t*)nkn_malloc_type((4*ts2mfu_desc->num_video_AU)*3,
					  mod_vpe_mfp_ts2mfu_t);
	ts2mfu_desc->mfu_raw_box->videofg.sample_duration = temp;
	ts2mfu_desc->mfu_raw_box->videofg.composition_duration = temp +
            ts2mfu_desc->num_video_AU;
	ts2mfu_desc->mfu_raw_box->videofg.sample_sizes = temp +
	    (2*ts2mfu_desc->num_video_AU);
	ts2mfu_desc->mfu_raw_box->videofg.codec_info_size = 0;
	ts2mfu_desc->mfu_raw_box->videofg.codec_specific_data = NULL;
	ts2mfu_desc->vid_PTS = (uint32_t*) nkn_malloc_type(4*ts2mfu_desc->num_video_AU,
							   mod_vpe_mfp_ts2mfu_t);
	ts2mfu_desc->vid_DTS = (uint32_t*) nkn_malloc_type(4*ts2mfu_desc->num_video_AU,
							   mod_vpe_mfp_ts2mfu_t);
    }

    if(ts2mfu_desc->is_audio){
	temp_1 = (uint32_t*)nkn_calloc_type((4*ts2mfu_desc->num_audio_AU)*3, 1,
					    mod_vpe_mfp_merge_t);
        ts2mfu_desc->mfu_raw_box->audiofg.sample_duration = temp_1;
        ts2mfu_desc->mfu_raw_box->audiofg.composition_duration = temp_1 +
            ts2mfu_desc->num_audio_AU;
        ts2mfu_desc->mfu_raw_box->audiofg.sample_sizes = temp_1 +
            (2*ts2mfu_desc->num_audio_AU);
        ts2mfu_desc->mfu_raw_box->audiofg.codec_info_size = 0;
        ts2mfu_desc->mfu_raw_box->audiofg.codec_specific_data = NULL;
        ts2mfu_desc->aud_PTS = (uint32_t*) nkn_calloc_type(4*ts2mfu_desc->num_audio_AU, 1,
							   mod_vpe_mfp_merge_t);
    }
    return 1;

}



static void parse_mfu_data(mfu_data_t *mfu, 
			   mfu_demux_data_t *mfud){
    /*Parse MFU data*/
    //    
    mfud->mfuc = parseMFUCont(mfu->data);
    memcpy(&mfud->mfub, mfud->mfuc->mfubbox->mfub, sizeof(mfub_t));
    return;
}


static int32_t  write32_byte_swap( uint8_t *data,
				   uint32_t pos,
				   uint32_t val)
{
    data[pos] = val>>24;
    data[pos+1] = (val>>16)&0xFF;
    data[pos+2] = (val>>8)&0xFF;
    data[pos+3] = val&0xFF;
    return 1;
}



void* init_merge_mfu(uint32_t target_duration,
		     uint32_t max_mfus){
    uint32_t j;
    fmtr_mfu_list_t* fmt;
    //    fmt = (fmtr_mfu_list_t*)(fmtr);
    fmt = nkn_calloc_type(1, sizeof(fmtr_mfu_list_t),mod_vpe_mfp_merge_t);
    fmt->target_duration = target_duration*90;
    fmt->max_num_mfus = max_mfus;
    /*Internal structure allocation*/
    fmt->mfud = nkn_calloc_type(1, sizeof(mfu_demux_data_t)*fmt->max_num_mfus
				,mod_vpe_mfp_merge_t);
    fmt->mfus = nkn_calloc_type(1, sizeof(mfu_data_t)*fmt->max_num_mfus
                                ,mod_vpe_mfp_merge_t);
    fmt->out_mfu = nkn_calloc_type(1, sizeof(mfu_data_t)*fmt->max_num_mfus
				   ,mod_vpe_mfp_merge_t);
    fmt->ts2mfu_desc = nkn_calloc_type(1,sizeof( ts2mfu_desc_t *)*fmt->max_num_mfus,
				       mod_vpe_mfp_merge_t);
    return (void*)fmt;
}


void delete_merge_mfu(void*  ts2mfu_desc){
    //    DBG_MFPLOG ("MFUDEL", MSG, MOD_MFPLIVE,"Deleting in delete_merge_mfu %p",ts2mfu_desc);
    ts2mfu_desc_free( (ts2mfu_desc_t*) ts2mfu_desc);
    return;
}


static void deletemfuContext(mfu_contnr_t* mfucont) {

    if (mfucont->mfubbox != NULL)
	mfucont->mfubbox->del(mfucont->mfubbox);
    uint32_t desc_count = 0;
    if (mfucont->rawbox != NULL) {
	desc_count = mfucont->rawbox->desc_count;
	mfucont->rawbox->del(mfucont->rawbox);
    }
    if (mfucont->datbox != NULL) {
	uint32_t i = 0;
	for (; i < desc_count; i++)
	    mfucont->datbox[i]->del(mfucont->datbox[i]);
	free(mfucont->datbox);
    }
    free(mfucont);
}



void set_sessid_name(fmtr_mfu_list_t* fmt,
		     char* sess_name,
		     uint32_t sess_id){


    strncpy(fmt->sess_name, sess_name, strlen(sess_name));
    fmt->sess_id = sess_id;
    return;


}
