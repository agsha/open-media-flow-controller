/*
 *
 * Filename:  mp4_feature.cpp
 * Date:      2009/03/23
 * Module:    Parser for hinted MP4 files
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include "nkn_vpe_types.h"
#include "string.h"
#include "rtp_avc_packetizer.h"
#include "nkn_vpe_bufio.h"
#include "rtp_packetizer_common.h"

//#define DUMP_SDP

int32_t 
rtp_packetize_avc(Sample_rtp_info_t* rtp_sample_info)
{
    //Each sample is an AU with a bunch of NALS.
    int32_t tot_size, nal_size, AUstart = 1,AUend = 0,AUsize,num_NALs,mode,*org_pkt_size;
    uint8_t *nalu,*nalu_ptr,*sample_data,*pkt_start;
    tot_size = rtp_sample_info->sample_size;
    sample_data = rtp_sample_info->sample_data;
    rtp_sample_info->pkt_builder->num_rtp_pkts =0;
    if(!rtp_sample_info->pkt_builder->avc1)
	return MP4_RET_ERROR;
    AUsize = rtp_sample_info->sample_size;
    num_NALs = get_num_NALs_AU(sample_data,rtp_sample_info->pkt_builder->avc1->NALUnitLength,AUsize);
    rtp_sample_info->pkt_builder->header.timestamp = (int32_t)(rtp_sample_info->sample_time*rtp_sample_info->ft);
    if(1){//AUsize >MTU_SIZE){
	int32_t *nalu_sizes,i,total_size = 0;
	nalu_sizes = (int32_t*)malloc(sizeof(int32_t)*num_NALs);
	rtp_sample_info->pkt_builder->num_rtp_pkts=0;
	nalu_ptr = sample_data;//+rtp_sample_info->pkt_builder->avc1->NALUnitLength;
	for(i=0;i<num_NALs;i++){
	    nal_size = get_nal_size(nalu_ptr,rtp_sample_info->pkt_builder->avc1->NALUnitLength);
	    if(nal_size>MTU_SIZE){
		int32_t num_pkts;
		num_pkts = (nal_size/MTU_SIZE)+1;
		total_size+=num_pkts*(RTP_HEADER_SIZE+FU_HEADER+FU_IN)+nal_size;
		rtp_sample_info->pkt_builder->num_rtp_pkts+=num_pkts;
	    }
	    else{
		total_size+=nal_size+RTP_HEADER_SIZE;
		rtp_sample_info->pkt_builder->num_rtp_pkts+=1;
	    }
	    nalu_sizes[i]=nal_size;
	    nalu_ptr+=nal_size+rtp_sample_info->pkt_builder->avc1->NALUnitLength;
	}
	rtp_sample_info->pkt_builder->rtp_pkts = (uint8_t*)malloc(sizeof(uint8_t)*total_size);
	rtp_sample_info->pkt_builder->rtp_pkt_size = (int32_t*)malloc(sizeof(int32_t)*rtp_sample_info->pkt_builder->num_rtp_pkts);
        org_pkt_size = rtp_sample_info->pkt_builder->rtp_pkt_size;
	pkt_start = rtp_sample_info->pkt_builder->rtp_pkts;
	nalu = sample_data;
	rtp_sample_info->pkt_builder->header.marker = 0;
	for(i=0;i<num_NALs;i++){
	   if(nalu_sizes[i]>MTU_SIZE){
	       int32_t num_pkts,j;
	       uint8_t * nalu_fua;
	       nalu_fua = nalu; 
	       num_pkts = nalu_sizes[i]/MTU_SIZE + 1;
	       rtp_sample_info->pkt_builder->fu_a.fu_indicator.F = 0;
	       rtp_sample_info->pkt_builder->fu_a.fu_header.R = 0;
	       nalu_sizes[i]-=1;
	       for(j=0;j<num_pkts;j++){
		   mode = FU_A;
		   if(j == num_pkts-1){
		       nal_size =  nalu_sizes[i]%1436;
		       rtp_sample_info->pkt_builder->fu_a.fu_header.E = 1;
                       if(i==num_NALs-1)
                           rtp_sample_info->pkt_builder->header.marker = 1;
		   }
		   else
		       nal_size = 1436;

		   if(j==0){
		       nalu_fua+=rtp_sample_info->pkt_builder->avc1->NALUnitLength;
		       rtp_sample_info->pkt_builder->fu_a.fu_indicator.NRI = (*nalu_fua>>5)&0x03;
		       rtp_sample_info->pkt_builder->fu_a.fu_indicator.type = 28;
		       rtp_sample_info->pkt_builder->fu_a.fu_header.S = 1;
		       rtp_sample_info->pkt_builder->fu_a.fu_header.E = 0;
		       rtp_sample_info->pkt_builder->fu_a.fu_header.type = (*nalu_fua)&0x1F;
		       //		       nal_size-=1;
		       nalu_fua+=1;
		   }
		   build_rtp_avc_pkt(rtp_sample_info->pkt_builder,mode,nal_size,nalu_fua,1,1);
		   rtp_sample_info->pkt_builder->rtp_pkts+=*rtp_sample_info->pkt_builder->rtp_pkt_size;
		   rtp_sample_info->pkt_builder->rtp_pkt_size++;
		   rtp_sample_info->pkt_builder->fu_a.fu_header.S = 0;
		   nalu_fua+=nal_size;
	       }
	       nalu_sizes[i]+=1;
	       nalu += rtp_sample_info->pkt_builder->avc1->NALUnitLength;
	    }
	    else{
		/*Single Unit Mode*/
		mode = SINGLE_NAL;
		if(i==num_NALs-1)
		    rtp_sample_info->pkt_builder->header.marker = 1;
		nal_size = nalu_sizes[i];
		nalu+=rtp_sample_info->pkt_builder->avc1->NALUnitLength; 
		build_rtp_avc_pkt(rtp_sample_info->pkt_builder,mode,nal_size,nalu,1,1);
		rtp_sample_info->pkt_builder->rtp_pkts+=*rtp_sample_info->pkt_builder->rtp_pkt_size;
		rtp_sample_info->pkt_builder->rtp_pkt_size++;
	    }
	   nalu+=nalu_sizes[i];
	}
	if(nalu_sizes != NULL)
	    free(nalu_sizes);
	rtp_sample_info->pkt_builder->rtp_pkts =  pkt_start ;
	rtp_sample_info->pkt_builder->rtp_pkt_size = org_pkt_size;
    }
    else{	
	//Only one packet per sample
	



	rtp_sample_info->pkt_builder->num_rtp_pkts = 1;
	nalu_ptr = sample_data;
	if(num_NALs == 1){
	    mode = SINGLE_NAL;
	    nal_size = get_nal_size(sample_data,rtp_sample_info->pkt_builder->avc1->NALUnitLength);
	    rtp_sample_info->pkt_builder->rtp_pkts = (uint8_t*)malloc(sizeof(uint8_t)*(RTP_HEADER_SIZE+nal_size));
	    nalu = sample_data+rtp_sample_info->pkt_builder->avc1->NALUnitLength;
	    rtp_sample_info->pkt_builder->header.marker = 1;
	    build_rtp_avc_pkt(rtp_sample_info->pkt_builder,mode,nal_size,nalu,1,1);
	}
	else{
	    int32_t i,total_size=0,*nalu_sizes;
	    mode = STAP_A;
	    nalu = sample_data;
	    nalu_ptr=nalu;
	    nalu_sizes = (int32_t*)malloc(sizeof(int32_t)*num_NALs);
	    for(i=0;i<num_NALs;i++){
                nal_size = get_nal_size(nalu_ptr,rtp_sample_info->pkt_builder->avc1->NALUnitLength);
		total_size+=nal_size+2;
		nalu_sizes[i]=nal_size;
		nalu_ptr+=rtp_sample_info->pkt_builder->avc1->NALUnitLength+nal_size;
	    }
	    rtp_sample_info->pkt_builder->rtp_pkts = (uint8_t*)malloc(sizeof(uint8_t)*(RTP_HEADER_SIZE+1+total_size));
	    *rtp_sample_info->pkt_builder->rtp_pkt_size = 0;
	    nalu_ptr = sample_data;
	    for(i=0;i<num_NALs;i++){
		nal_size = nalu_sizes[i];
		nalu=nalu_ptr+rtp_sample_info->pkt_builder->avc1->NALUnitLength;
		if(i==num_NALs-1){
		    AUend=1;
		    rtp_sample_info->pkt_builder->header.marker = 1;
		}
		else
		    rtp_sample_info->pkt_builder->header.marker = 0;
		build_rtp_avc_pkt(rtp_sample_info->pkt_builder,mode,nal_size,nalu,AUend,AUstart);
		nalu_ptr=nalu+nal_size;
		AUstart =0;
	    }
	    if(nalu_sizes != NULL)
		free(nalu_sizes);
	}


    }
    

    return 1;
}


int32_t 
build_rtp_avc_pkt(rtp_packet_builder_t* pkt_builder,int32_t mode,int32_t nal_size,uint8_t *nalu,int32_t AUEnd, uint32_t AUStart)
{

    uint8_t * data,val;
    AVC_NAL_octet present_octet;
    uint8_t* pkt_data;// = pkt_builder->rtp_pkts;
    data = nalu;
    pkt_builder->header.sequence_num++;
    if(mode == STAP_A){
	// Form the STAP header
	if(AUStart){
	    pkt_builder->octet.F = 0;
	    pkt_builder->octet.NRI = 0;
	    pkt_builder->octet.type = 24;
	    pkt_builder->header.marker = 1;
	}
	val = *nalu;
	pkt_builder->octet.F|=((val>>7)&0x01);
	present_octet.NRI = (val>>5)&0x07;
	pkt_builder->octet.NRI = _MAX(present_octet.NRI, pkt_builder->octet.NRI);
	//Insert the nal size and the payload
	if(!pkt_builder->rtp_pkts){
	    pkt_data = pkt_builder->rtp_pkts;
	    pkt_data+=*pkt_builder->rtp_pkt_size;
	    if(AUStart){
		//Skip the header and STAP header
		pkt_data+=RTP_HEADER_SIZE+STAP_HEADER_SIZE;
		*pkt_builder->rtp_pkt_size+=RTP_HEADER_SIZE+STAP_HEADER_SIZE;		
	    }
	    *pkt_data = (uint8_t)((nal_size>>8)&0xFF);
	    pkt_data++;
	    *pkt_data = (uint8_t)(nal_size&0xFF);
	    pkt_data++;
	    memcpy(pkt_data,nalu,nal_size);
	    *pkt_builder->rtp_pkt_size+=2+nal_size;
	}
	if(AUEnd){
	    //This signals the end of packet. Maintian this state outside the call.
	    uint8_t *header,*pkt;
	    header = (uint8_t*)malloc(sizeof(uint8_t)*RTP_HEADER_SIZE);
	    pkt_builder->header.payload_type = 0x61;
	    form_rtp_header(pkt_builder->header, header);
	    pkt= pkt_data-*pkt_builder->rtp_pkt_size;
	    memcpy(pkt,header,RTP_HEADER_SIZE);
	    pkt+= RTP_HEADER_SIZE;	
	    //Copy the STAP header:
	    *pkt =  (pkt_builder->octet.F<<7)|((pkt_builder->octet.NRI&0x03)<<5)|(pkt_builder->octet.type&0x1F); 
	    if(header != NULL)
		free(header);
	}
    }
    else if(mode == SINGLE_NAL){
        if(pkt_builder->rtp_pkts!=NULL){
            pkt_data = pkt_builder->rtp_pkts;
            if(AUStart){
                //Skip the header and STAP header
                pkt_data+=RTP_HEADER_SIZE;
                *pkt_builder->rtp_pkt_size=RTP_HEADER_SIZE;
            }
            memcpy(pkt_data,nalu,nal_size);
            *pkt_builder->rtp_pkt_size+=nal_size;
        }
        if(AUEnd){
            //This signals the end of packet. Maintian this state outside the call.
            uint8_t *header,*pkt;
            header = (uint8_t*)malloc(sizeof(uint8_t)*RTP_HEADER_SIZE);
            pkt_builder->header.payload_type = 0x61;
            form_rtp_header(pkt_builder->header, header);
            pkt= pkt_data-RTP_HEADER_SIZE;//*pkt_builder->rtp_pkt_size;
            memcpy(pkt,header,RTP_HEADER_SIZE);
            pkt+= RTP_HEADER_SIZE;
	    if(header != NULL)
		free(header);
        }
    }
    else{
	//Fragmented unit
	//Will come one by one:
	if(pkt_builder->rtp_pkts!=NULL){
	    uint8_t *header,FUI,FUH;
	    AVC_NAL_octet fu_ind;
	    AVC_FUA fu_hdr;

	    /*Form RTP packet header for FU A*/
            header = (uint8_t*)malloc(sizeof(uint8_t)*RTP_HEADER_SIZE);
	    pkt_data = pkt_builder->rtp_pkts;
            form_rtp_header(pkt_builder->header, header);
            memcpy(pkt_data,header,RTP_HEADER_SIZE);
            pkt_data+= RTP_HEADER_SIZE;
	    if(header != NULL)
		free(header);

	    /*Add the FU headers*/
	    fu_ind = pkt_builder->fu_a.fu_indicator;
	    fu_hdr = pkt_builder->fu_a.fu_header;
	    FUI =  (fu_ind.F<<7)|(fu_ind.NRI<<5)|(fu_ind.type&0x1F);
	    FUH = (fu_hdr.S<<7)|(fu_hdr.E<<6)|(fu_hdr.R<<5)|(fu_hdr.type&0x1F);
	    *pkt_data++ = FUI;
	    *pkt_data++ = FUH;
            memcpy(pkt_data,nalu,nal_size);
	    pkt_data+=nal_size; 
	    *pkt_builder->rtp_pkt_size=nal_size+RTP_HEADER_SIZE+2;
	}
    }



    return 1;
}


void 
form_rtp_header(rtp_header_t h, uint8_t *header)
{
    int32_t temp;
    h.version = 2;
    h.padding = 0;
    h.extension = 0;
    h.CSRC_count = 0;
    h.SSRC = 0x6952;  

    *header = (h.version<<6)|(h.padding<<5)|(h.extension<<4)|(h.CSRC_count&0x0F);
    header++;
    *header = (h.marker<<7)|(h.payload_type&0x7F);
    header++;
    *header = h.sequence_num>>8;
    header++;
    *header = h.sequence_num&0xFF;
    header++;
    temp = (h.timestamp>>24)|((h.timestamp>>8)&0xFF00)|((h.timestamp&0xFF00)<<8)|((h.timestamp&0xFF)<<24);
    memcpy(header,&temp,4);
    header+=4;
    temp = (h.SSRC>>24)|((h.SSRC>>8)&0xFF00)|((h.SSRC&0xFF00)<<8)|((h.SSRC&0xFF)<<24);
    memcpy(header,&temp,4);
    header+=4;
}

void 
read_avc1_box(AVC_conf_Record_t *avc,uint8_t *data, uint8_t data_size)
{

    int32_t box_size,bytes_left,i;
    box_size = _read32(data,0);
    bytes_left = box_size;
    bytes_left-=8;
    data+=8;

    avc->version = *data++;
    avc->AVCProfileIndication = *data++;
    avc->profile_compatibility = *data++;
    avc->AVCLevelIndication = *data++;
    avc->NALUnitLength = (*data&0x03)+1;
    data++;
    avc->numOfSequenceParameterSets = *data&0x1f;
    data++;
    avc->sps = (param_sets_t*)malloc(sizeof(param_sets_t)*avc->numOfSequenceParameterSets);
    for(i=0;i< avc->numOfSequenceParameterSets;i++){
	avc->sps->length = _read16(data,0);
	avc->sps->param_set = (uint8_t*)malloc(sizeof(uint8_t)*avc->sps->length);
	data+=2;
	memcpy(avc->sps->param_set,data,avc->sps->length);
	data+=avc->sps->length;
    }

    avc->numOfPictureParameterSets = *data&0x1f;
    data++;
    avc->pps = (param_sets_t*)malloc(sizeof(param_sets_t)*avc->numOfPictureParameterSets);
    for(i=0;i<avc->numOfPictureParameterSets;i++){
        avc->pps->length = _read16(data,0);
        avc->pps->param_set = (uint8_t*)malloc(sizeof(uint8_t)*avc->pps->length);
        data+=2;
        memcpy(avc->pps->param_set,data,avc->pps->length);
        data+=avc->pps->length;
    }
}

void cleanup_avc_packet_builder(rtp_packet_builder_t* bldr){

    AVC_conf_Record_t *avc;
    int32_t i;
    avc = bldr->avc1;
    for(i=0;i< avc->numOfSequenceParameterSets;i++){
	if(avc->sps->param_set)
	    SAFE_FREE(avc->sps->param_set);
    }
    if(avc->sps)
	SAFE_FREE(avc->sps);
    for(i=0;i<avc->numOfPictureParameterSets;i++){
	if(avc->pps->param_set)
	    SAFE_FREE(avc->pps->param_set);
    }
    if(avc->pps)
	SAFE_FREE(avc->pps);

    if(avc)
	SAFE_FREE(avc);
}




int32_t
build_sdp_avc_info(rtp_packet_builder_t* builder, char** sdp,int32_t *sdp_size,int32_t height,int32_t width)
{
    char *buf;
    char pl_name[50], media_type[50], temp[256];
    int32_t i;
    param_sets_t  *tset;
    int32_t tval = 97;

#ifdef DUMP_SDP
    FILE *fid;
    fid = fopen("sdp","wb");
#endif 


    *sdp = (char*)calloc(1, 6000);
    buf = *sdp;
    strcpy(pl_name, "video");
    strcpy(media_type, "H264");

    //    sprintf(buf, "m=%s 0 RTP/AVP %d\r\nb=AS:%d\r\nb=TIAS:%d\r\na=maxprate:%d\r\na=rtpmap:%d %s/%d\r\na=control:trackID=%d\r\n", pl_name,tval, 784,800,115,tval,media_type,90000, 13);
#if 0
    snprintf(buf, 6000, "m=%s 0 RTP/AVP %d\r\nb=AS:%d\r\na=rtpmap:%d %s/%d\r\na=control:trackID=%d\r\n", pl_name,tval, 771,tval,media_type,90000, 13);
    snprintf(temp, 256, "a=fmtp:97 profile-level-id=%X%X%X; ",builder->avc1->AVCProfileIndication,builder->avc1->profile_compatibility,builder->avc1->AVCLevelIndication);
    strcat(buf,temp);
    snprintf(temp, 256, "packetization-mode=%d; sprop-parameter-sets=",1);
    strcat(buf,temp);
#else
    snprintf(buf, 6000, "m=%s 0 RTP/AVP %d\r\na=rtpmap:%d %s/%d\r\na=control:trackID=%d\r\n", pl_name,tval,tval,media_type,90000, 13);
    //snprintf(temp, 256, "a=framesize:%d %d-%d\r\n",tval,640,480);
    snprintf(temp, 256, "a=framesize:%d %d-%d\r\n",tval,width,height);
    strcat(buf,temp);

    snprintf(temp, 256, "a=fmtp:97 packetization-mode=%d;profile-level-id=%X%X%X;",1,builder->avc1->AVCProfileIndication,builder->avc1->profile_compatibility,builder->avc1->AVCLevelIndication);
    strcat(buf,temp);
    snprintf(temp,256, "sprop-parameter-sets=");
    strcat(buf,temp);
#endif

    tset = builder->avc1->sps;
    for(i=0;i<builder->avc1->numOfSequenceParameterSets;i++){
	uint8_t *sps_data;
	int32_t rem,sps_len_64;
	char* sps64;
	sps_data = tset->param_set;
	sps_len_64 = tset->length*8/6;
	rem = tset->length%3;
	if(rem == 2)
	    sps_len_64+=3;
	if(rem == 1)
            sps_len_64+=2;

	sps64 = (char*)calloc(1,sps_len_64);
	if(sps64!=NULL){
	    bin2base64(sps_data,tset->length,sps64);
	    strcat(buf,sps64);
	    if(i!=builder->avc1->numOfSequenceParameterSets-1)
		snprintf(temp, 256, "=,");
	    else{
		if(builder->avc1->numOfPictureParameterSets)
		    snprintf(temp, 256,"=,");
		else
		    snprintf(temp,256, "=");
	    }
	    strcat(buf,temp);
	}
	tset++;
	if(sps64 != NULL)
	    free(sps64);
    }
    tset = builder->avc1->pps;
    for(i=0;i<builder->avc1->numOfPictureParameterSets;i++){
        uint8_t *sps_data;
        int32_t rem,sps_len_64;
        char* sps64;
        sps_data = tset->param_set;
        sps_len_64 = tset->length*8/6;
        rem = tset->length%3;
        if(rem == 2)
            sps_len_64+=3;
        if(rem == 1)
            sps_len_64+=2;
        sps64 = (char*)calloc(1,sps_len_64);
        if(sps64!=NULL){
            bin2base64(sps_data,tset->length,sps64);
            strcat(buf,sps64);
            if(i== builder->avc1->numOfPictureParameterSets-1)
                snprintf(temp, 256, "==\r\n");
            else
                snprintf(temp, 256, "=,");
            strcat(buf,temp);
        }
        tset++;
	if(sps64 != NULL)
	    free(sps64);
    }

    // sprintf(temp, "a=framesize:%d %d-%d\r\n",tval,640,480);

    //strcat(buf,temp);

    //strcat(buf,temp);

    *sdp_size = strlen(buf);


#ifdef DUMP_SDP
    fwrite(buf,*sdp_size,1,fid);
    fclose(fid);
#endif

    return 1;
}
