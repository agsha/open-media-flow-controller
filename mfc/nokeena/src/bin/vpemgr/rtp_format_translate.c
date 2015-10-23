#include <netinet/in.h>
#include "rtp_format.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "assert.h"
#include "math.h"
#include "nkn_vpe_bitio.h"

#define MAX_FORMAT //should be deleted later
//#define _HACK_ //temp till Muthu Sirs format is finalzed
#define HACK1
//#define NKN_DEBUG_PRINT


#ifdef MAX_FORMAT
#include "rtsp_vpe_nkn_fmt.h"
#endif
#define USE_MP4BOX

void base642bin(char *, int32_t, uint8_t *);
uint32_t _read32(uint8_t*,int32_t);
uint16_t _read16(uint8_t*,int32_t);
#define nkn_vfs_fwrite(buf, n1, n2, fd) fwrite(buf, n1, n2, fd)
#define nkn_vfs_fread(buf, n1, n2, fd) fread(buf, n1, n2, fd)


/*
Description:
  Requires an open cache formatted file and returns a outfile.mp4 file as an ouput 
  Extracts all the raw video and audio streams and muxes them into an mp4 file
*/

uint32_t _read32(uint8_t *buf, int32_t pos)
{
    return (buf[pos]<<24)|(buf[pos+1]<<16)|(buf[pos+2]<<8)|(buf[pos+3]);
}

uint16_t _read16(uint8_t *buf, int32_t pos)
{
    return (buf[pos]<<8)|(buf[pos+1]);
}


int32_t read_cache_translate(char* infile, char* hostname){
    rtp_formatizer_t *rtp;
    char* data,outfile[100],*tc,outfilewp[100],infilename[100];
    int32_t sdp_size,meta_size,fsize,i,ret,offset;
    uint8_t* udata;
    char cmd1[300],cmd[100];
    FILE *fout;
    //FILE *fin;
    int32_t fin;
    uint32_t frame_rate;
    char temp[100]; 


    /*
      Check if it is a *.mp4 file. If extension is not correct exit.
      Note: This should be handled outside the VPE manager. Hack for now. 
    */
    if(strstr(infile,".mp4") == NULL)
	return VPE_ERROR;


    strcpy(infilename,infile);
    strcat(infilename,".nknc");
    strcpy(temp, "rbc");
    //fin = fopen(infilename, "rb");
    fin = open(infilename, O_RDONLY | O_SYNC);
    if(fin == -1){
#ifdef NKN_DEBUG_PRINT
	printf("Input File %s Not FOUND",infile);
#endif
	return VPE_ERROR;
    }

    /*Form the outfile*/
    tc = strrchr(infile,'/');
    strcpy(outfilewp,tc+1);
    //    strcat(outfilewp,".mp4");
    strcpy(outfile,VPE_OUTPUT_PATH);
    //    strcat(outfile,tc+1);
    strcat(outfile,outfilewp);
	//    strcat(outfile,".mp4");


    rtp = (rtp_formatizer_t *)malloc(sizeof(rtp_formatizer_t));
    rtp->fin = fin;
    ret = VPE_SUCCESS;
    
    /*Get the size of the file*/
    //fseek(fin,0,SEEK_END);
    fsize = lseek(fin, 0, SEEK_END);
    //fsize = (int32_t)(ftell(fin));
    if (fsize == -1) {
#ifdef NKN_DEBUG_PRINT
	printf("Input File seek failed", infile);
#endif
	return VPE_ERROR;
    }

    /*Seek to SDP parsing result*/
    //fseek(fin,32+1+7+sizeof(rl_sdp_info_t),SEEK_SET);
    lseek(fin, 32+1+7+sizeof(rl_sdp_info_t), SEEK_SET);

    /*Read the SDP headers*/
    //fread(&sdp_size,4,sizeof(uint8_t),fin);
    read(fin, &sdp_size, 4 * sizeof(uint8_t));

    /*parse the SDP files*/
    data = (char*)malloc(sizeof(char)*sdp_size);
    //fread(data,sdp_size,sizeof(char),fin);
    read(fin, data, sdp_size * sizeof(char));
    if(parse_sdp_box(data,sdp_size,rtp) == VPE_ERROR){
	free(data);
	ret = VPE_ERROR;
	goto exitcode;
    }
    free(data);

    /*Read the metadata*/
    //fseek(fin,32+1+7,SEEK_SET);
    lseek(fin, 32+1+7, SEEK_SET);
    meta_size = sizeof(rl_sdp_info_t);
    udata = (uint8_t*)malloc(meta_size);
    //fread(udata,meta_size,sizeof(uint8_t),fin);
    read(fin, udata, meta_size * sizeof(uint8_t));

    /*Assign channel ids and track ids*/
    map_track_channel_ids(udata,meta_size,rtp);
    free(udata);

    /*Move to the RTP data from header*/
    offset = rvnf_header_s+sdp_size;
    rtp->rtp_data_size =fsize-offset;//rvnf_header_s-sdp_size-4-sizeof(char*);
    //fseek(rtp->fin,offset,SEEK_SET);
    lseek(rtp->fin, offset, SEEK_SET);

    /*Create the media trak files*/
    rtp_format_convert(rtp);
    

#ifdef USE_MP4BOX
    sprintf(cmd1,"/./opt/nkn/bin/MP4Box ");
    for(i=0;i<rtp->num_audio_streams;i++){
        sprintf(cmd," -add %s ",rtp->afname[i]);
        strcat(cmd1,cmd);
    }

    for(i=0;i<rtp->num_video_streams;i++){
	rtp->video[i].pkt.frame_rate = ceil(rtp->video[i].pkt.num_frames/rtp->duration);
        sprintf(cmd," -add %s -fps %d ",rtp->vfname[i],rtp->video[i].pkt.frame_rate);
        strcat(cmd1,cmd);
    }
    //    sprintf(cmd," -hint  %s",outfile);
    sprintf(cmd," %s",outfile);
    strcat(cmd1,cmd);
    system(cmd1);

#else    
    sprintf(cmd1,"/./opt/nkn/bin/ffmpeg ");
    //cmd+=strlen(cmd);
    for(i=0;i<rtp->num_audio_streams;i++){
        sprintf(cmd," -i %s -acodec copy ",rtp->afname[i]);
	//cmd+=strlen(cmd);
	strcat(cmd1,cmd);
    }
    if(rtp->num_video_streams>0)
	frame_rate = rtp->video[0].pkt.frame_rate= ceil(rtp->video[0].pkt.num_frames/rtp->duration);
    for(i=0;i<rtp->num_video_streams;i++){
        sprintf(cmd," -i %s -vcodec copy ",rtp->vfname[i]);
	strcat(cmd1,cmd);
    }
    if(frame_rate)
	sprintf(cmd,"-r %d %s",frame_rate,outfile);
    else
	sprintf(cmd,"%s",outfile);
    strcat(cmd1,cmd);
    system(cmd1);
#endif


#if 1 
    /*Delete all temp files*/
    for(i=0;i<rtp->num_audio_streams;i++){
        sprintf(cmd,"rm  %s",rtp->afname[i]);
	system(cmd);
    }
    for(i=0;i<rtp->num_video_streams;i++){
        sprintf(cmd,"rm %s",rtp->vfname[i]);
	system(cmd);
    }
#endif
    /*Now ingest them into the cache*/
    /*First check if they are created*/
    if((fout = fopen(outfile,"rb")) == NULL)
	goto exitcode;
    else
	fclose(fout);

#ifdef HACK1
	sprintf(cmd1,"/opt/nkn/bin/cache_add.sh -r  %s %s", outfile, infile + strlen("/nkn/mnt/fuse") );
#else
    sprintf(cmd1,"/opt/nkn/bin/cache_add.sh -r -H %s  %s /%s",hostname,outfile,outfilewp);
#endif
    system(cmd1);

 exitcode:
    /*Free up all the malloced data*/
    //fclose(rtp->fin);
    close(rtp->fin);
    free(rtp);
    rtp = NULL;
    return ret;
}

int32_t map_track_channel_ids(uint8_t* data,int32_t size,rtp_formatizer_t *rtp){
    int32_t trak_id;
    int32_t i,j;
    uint8_t cid;
    rl_sdp_info_t* sdpinfo;
    sdpinfo = (rl_sdp_info_t*)(data);    
    for(j=0;j<sdpinfo->num_trackID;j++){
	sscanf(sdpinfo->track[j].trackID,"trackID=%d",&trak_id);
	cid = sdpinfo->track[j].channel_id;
	switch(sdpinfo->track[j].track_type){
            case AUDIO_TRAK:
                for(i=0;i<rtp->num_audio_streams;i++){
                    if(rtp->audio[i].trak_id == trak_id)
#ifdef HACK1
			rtp->audio[i].cid = 1;
#else
                        rtp->audio[i].cid = cid;
#endif
                }
                break;
            case VIDEO_TRAK:
                for(i=0;i<rtp->num_video_streams;i++){
                    if(rtp->video[i].trak_id == trak_id)
#ifdef HACK1
			rtp->video[i].cid = 0;
#else
                        rtp->video[i].cid = cid;
#endif
			rtp->video[i].pkt.num_frames = 0;
			rtp->video[i].pkt.frame_rate = 0;
                }
                break;
            default:
                break;
	}

    }
    return VPE_SUCCESS;
}


/**************************************************************
Description:
      This wrapper performs the following functions:
Inputs:
      flat file of rtp packets.
      audio and video stream PTs
Outputs:
      Video and audio raw files (*.h264,*.aac,...)
Algorithm: 
      while(feof(infile)){
       if(audio seq num == prev_aud_seq_num+1)
         aac_rtp_dump into aac file
       if(video seq num == prev_vid_seq_num+1)
         h264_rtp_dump into h264 file
      } 
      Include code to get all this packed into one mp4 stream file.

Assumption: Only one media track per channel!!!!

******************************************************************/

//#define VPE_DUMP_FILE "/nkn/vpe/dump_file"
int32_t rtp_format_convert(rtp_formatizer_t *rtp){
    uint8_t *data, *data_init;
    int32_t datalen = 0,i,j;
    uint32_t cid,rtpsize,pos = 0,count = 0;
    int32_t seek_box_size = 0;
    uint8_t *seek_data;
    int32_t temp=0;
    FILE *fout_data;

    if(!rtp)//||rtp->fin==NULL))
	return VPE_ERROR;

    /*Malloc for audio and video streams*/
    if(allocate_rtp_streams(rtp)!=VPE_SUCCESS)
	return VPE_ERROR;

    /*Write the SPS PPS in the file*/
    // fout_data = fopen(VPE_DUMP_FILE, "wb");
    //if(fout_data == NULL)
    //printf("Error in opening dump file \n");

    for(i=0;i<rtp->num_video_streams;i++){
        if(rtp->video[i].codec_type == VID_H264)
            write_h264_ps(rtp->video[i].sdp,rtp->video[i].fout);
	rtp->video[i].pkt.prev_time = 0;
    }

    data_init = data = (uint8_t*)malloc(sizeof(uint8_t)*rtp->rtp_data_size);
    datalen = rtp->rtp_data_size;

    //fread(data,datalen,1,rtp->fin);
    temp = read(rtp->fin, data, datalen);
#if 0
    if(temp != datalen) {
      assert(0);
    }
    
    {

        uint8_t *zero_data;
        zero_data = (uint8_t*)calloc(1, 100);
        if( !memcmp(data + 16384, zero_data, 100) ) {
	    /* hit error case */
	    lseek(rtp->fin, -datalen, SEEK_CUR);
	    read(rtp->fin, data, datalen);
	}
    }
#endif
    //nkn_vfs_fwrite(data,datalen,sizeof(uint8_t),fout_data);   
    //fclose(fout_data);
    /*Skip the seek boxes*/

    pos = 4;
    seek_box_size = _read32(data,pos);
    seek_box_size = ntohl(seek_box_size);
    pos+=4;
#ifdef _HACK_
    pos+=4; ///Remove MFC
#endif
    data+=pos;
    seek_data = data;
    while(datalen>0){
	/*'$' separated RTP packets*/
	count++;
	pos =0;	
#ifdef NKN_DEBUG_PRINT
	printf("dataleft = %u\n",datalen);
	if(data[pos]!=0x24)
	    printf("$ missing here\n");
#endif
	pos++;
	cid = data[pos];
	pos++;
	rtpsize= _read16(data,pos);
	pos+=2;
#ifndef _HACK_
	rtpsize = ntohs(rtpsize);
	pos+=4; //Skip timestamp
#endif
	for(i = 0;i <rtp->num_audio_streams;i++){
	    if(cid == rtp->audio[i].cid){
		switch(rtp->audio[i].codec_type){
		    case AUD_AAC:
			rtp->audio[i].pkt.data = data+pos;
			rtp->audio[i].pkt.size = rtpsize;
			rtp->audio[i].pkt.fout = rtp->audio[i].fout;
			//			aac_add_adts(&rtp->audio[i]);
			aac_rtp_dump(&rtp->audio[i]);
			break;
		    default:
			break;

		}
		break;
	    }
	}
	for(j=0;j <rtp->num_video_streams;j++){
	    if(cid == rtp->video[j].cid){
                switch(rtp->video[j].codec_type){
                    case VID_H264:
                        rtp->video[j].pkt.data = data+pos;
                        rtp->video[j].pkt.size = rtpsize;
                        rtp->video[j].pkt.fout = rtp->video[j].fout;
                        h264_rtp_dump(&rtp->video[j].pkt);
                        break;
                    default:
                        break;

                }
                break;
	    }
	}
	pos+=rtpsize;
	if((uint32_t)datalen<pos+1){	    
	    int iiii = 0;
	    break;
	}
	data+=pos;
	/*Skip the seek boxes*/
	if((int32_t)(data-seek_data)>= seek_box_size){
	    int32_t pos1 ;
	    pos1 = 4;
	    seek_box_size = _read32(data,pos1);
	    seek_box_size = ntohl(seek_box_size);
	    pos1+=4;	    
	    data+=pos1;
	    seek_data = data;
	    pos+=pos1;
	}
	datalen-=pos;
    }

#ifdef NKN_DEBUG_PRINT
    printf("Deleting All temp Files\n");
#endif
    /*Close all the raw files*/
    for(j=0;j <rtp->num_video_streams;j++){
	if( rtp->video[j].fout!=NULL)
	    fclose(rtp->video[j].fout);
    }
    for(j=0;j <rtp->num_audio_streams;j++){
        if( rtp->audio[j].fout!=NULL)
            fclose(rtp->audio[j].fout);
    }

    if(data_init) {
	free(data_init);
    }
    return VPE_SUCCESS;
}




int32_t get_sdp_size(uint8_t* data){
    int32_t pos =CACHE_FIXED_HDR_SIZE,sdp_size;
    //data+=CACHE_FIXED_HDR_SIZE;
    sdp_size = _read32(data,pos);
    return sdp_size;
}


int32_t  write_h264_ps(char *sdp,FILE *fid){

    char *dpos,*data,*temp;
    uint8_t *out;
    int32_t word,len1,len2;
    data = sdp;
    while( (dpos = strstr(data,"="))!=NULL){
	//	len = (int32_t)(dpos-data);
	temp = strtok(data,"=");
	//Can have a , here . Account for it
	if(temp[0]==','){
	    //  len-=1;
	    temp++;
	}

	word = NALU_DELIMIT;
	word = htonl(word);
	nkn_vfs_fwrite(&word,1,sizeof(int32_t),fid);
	len1 = strlen(temp);
	len2 = 3*(len1/4);
	len2 += len1%4;//len1 - (len2*4/3);
	if(len1%4 != 0)
	    len2-=1;
	
	out = (uint8_t*)calloc(sizeof(uint8_t)*len2,1);
	base642bin(temp,len1,out);
	nkn_vfs_fwrite(out,len2,1,fid);
	
	data = dpos+2;
	free(out);	
	out=NULL;
    }

    return VPE_SUCCESS;

}


int32_t parse_sdp_box(char* data, int32_t size,rtp_formatizer_t *rtp){

    int32_t pos= 0,data_left,pt,len;
    char* temp,codec_type[50],*temp1,*temp2,*orig_data,*outer_data;
    size_t tlen;
    rtp->num_video_streams = rtp->num_audio_streams = 0;
    data_left = size;
    orig_data = data;
    //by suma
    data= strstr(data,"a=range:npt=0-");
    data+=strlen("a=range:npt=0-");
    sscanf(data,"%f",&rtp->duration);
    data = orig_data;

    data = strstr(data,"m=");
    while(data !=NULL){
	data+=2;
	outer_data = data;
	if(strncmp(data,"video",5) == 0){
	    pos+=5+1;
	    data+=pos;
	    temp = strstr(data,"trackID");
	    temp+=strlen("trackID");
	    sscanf(temp,"=%d",&rtp->video[rtp->num_video_streams].trak_id);
	    temp = strstr(data,"a=rtpmap:");
	    if(temp == NULL)
		return VPE_ERROR;
	    temp+=2+6+1;
	    sscanf(temp,"%d %s",&pt,codec_type);
	    temp1 = strtok(codec_type,"/");
	    len = strlen(temp1);
	    if(strncmp(temp1,"H264",423) == 0)
		rtp->video[rtp->num_video_streams].codec_type = VID_H264;
	    else{
		rtp->video[rtp->num_video_streams].codec_type = VID_UNKNOWN;
		return VPE_ERROR;		
	    }

	    sscanf(&codec_type[len+1],"%d",&rtp->video[rtp->num_video_streams].timescale);
	    temp1 = strstr(data,"sprop-parameter-sets");
	    temp2 = temp1 + strlen("sprop-parameter-sets")+1;
            temp1 = strstr(temp2,"\r\n");
            tlen = (size_t)(temp1-temp2);
            rtp->video[rtp->num_video_streams].sdp = (char*)calloc(1, tlen+1);//strlen(temp1));
            strncpy(rtp->video[rtp->num_video_streams].sdp,temp2/*1*/,tlen);//strlen(temp1));
	    rtp->num_video_streams++;
	    data = outer_data;
	}
	if(strncmp(data,"audio",5) == 0){
	    pos+=5+1;
	    data+=pos;
	    rtp->audio[rtp->num_audio_streams].codec_type =AUD_AAC;
            temp = strstr(data,"trackID");
            temp+=strlen("trackID");
            sscanf(temp,"=%d",&rtp->audio[rtp->num_audio_streams].trak_id);
            temp = strstr(data,"trackID");
            temp+=strlen("trackID");
            sscanf(temp,"%d",&rtp->video[rtp->num_video_streams].trak_id);

	    temp = strstr(data,"mpeg4-generic/");
	    temp+=strlen("mpeg4-generic/");
	    sscanf(temp,"%d/%c",&rtp->audio[rtp->num_audio_streams].timescale,&rtp->audio[rtp->num_audio_streams].adts.num_channels);
	    rtp->audio[rtp->num_audio_streams].adts.is_mpeg2 =0;
	    rtp->audio[rtp->num_audio_streams].adts.object_type = 2-1;
	    rtp->audio[rtp->num_audio_streams].adts.sr_index = get_aac_sr(rtp->audio[rtp->num_audio_streams].timescale);
	    if( rtp->audio[rtp->num_audio_streams].adts.sr_index == 245)
		return VPE_ERROR;
	    if((temp = strstr(data,"sizeLength=")) == NULL)
		temp = strstr(data,"sizelength=");
	    if(temp == NULL)
		return VPE_ERROR;	    
	    else{
		temp+=strlen("sizeLength=");
		sscanf(temp,"%d",&rtp->audio[rtp->num_audio_streams].ind.sizelen);
	    }

            if((temp = strstr(data,"indexLength=")) == NULL)
		temp =  strstr(data,"indexlength=");
            if(temp == NULL)
		    return VPE_ERROR;
            else{
                temp+=strlen("indexLength=");
                sscanf(temp,"%d",&rtp->audio[rtp->num_audio_streams].ind.indexlen);
            }
	    if((temp = strstr(data,"indexDeltaLength="))==NULL)
		temp = strstr(data,"indexdeltalength=");
            if(temp == NULL)
                return VPE_ERROR;
            else{
                temp+=strlen("indexDeltaLength=");
                sscanf(temp,"%d",&rtp->audio[rtp->num_audio_streams].ind.deltalen);
            }

	    rtp->num_audio_streams++;
	    data = outer_data;
	}
	data = strstr(data,"m=");
    }
    return VPE_SUCCESS;
}

uint8_t get_aac_sr(int32_t samp_freq){

    int32_t sf_tbl [] = {
	96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
	16000, 12000, 11025, 8000, 7350, 0, 0, 0,-1
    };
    int32_t i = 0;
    while(sf_tbl[i]){
	if(samp_freq == sf_tbl[i])
	    break;	
	i++;
    }
    if(sf_tbl[i] == -1)
	return 245;
    return i;
}

int32_t allocate_rtp_streams(rtp_formatizer_t *rtp){
    int32_t i;
    char fname[30];
    for(i=0;i<rtp->num_audio_streams;i++){

	sprintf(fname,"%s%d",VPE_OUTPUT_PATH,i);
	//	sprintf(fname,"%d",i);
	strcat(fname,"\0");
        switch(rtp->audio[i].codec_type){
            case AUD_AAC:
		strcat(fname,".aac");
                break;
  
        }
	rtp->audio[i].fout = fopen(fname,"wb");
	rtp->audio[i].fname = fname;
	strcpy(rtp->afname[i],fname);
    }

    for(i=0;i<rtp->num_video_streams;i++){
        sprintf(fname,"%s%d",VPE_OUTPUT_PATH,i);
	switch(rtp->video[i].codec_type){
	    case VID_H264:
		strcat(fname,".h264");
		break;
	    default:
		break;
	}
        rtp->video[i].fout = fopen(fname,"wb");
	rtp->video[i].fname = fname;
        strcpy(rtp->vfname[i],fname);
    }
    return VPE_SUCCESS;
}


int32_t aac_rtp_dump(stream_state_t* audio){//pkt_format_data_t *pktdata, indinfo_t ind){
    uint8_t *buf,*pld,*auh;
    uint32_t pkt_size,offset,numNALs,i;
    uint16_t word16,bits16;

    FILE* fout;
    pkt_format_data_t *pktdata;
    bitstream_state_t *bs;
    pktdata = &audio->pkt;
    buf = pktdata->data;
    pkt_size = pktdata->size;
    fout = pktdata->fout;
    pld = buf+12;
    bits16 = _read16(pld,0);
    /*
      Each NAL would have to be preceeded by an ADTS header.
      NAL boundaries are indentified as follows:
      x---RTP header(12)---x---AU header(y) --x--NAL1 NAL2 .... NALk--x
      AU header
      |- Let s = sizeLength,d = indexDeltaLength, i = indexLength in bits obtained from SDP.
      |-> x--AU hdr size y in bits (16 bits) --x--NAL1len(s)SeqnumofNAL1(d)--x...x--NALklen(s)SeqnumofNALk(d)--x
    */
    word16=bits16/8;
    numNALs = (int32_t)(bits16/(audio->ind.sizelen+audio->ind.deltalen));
    //    numNALs/=8;
    auh = buf+12+2;
    //    auh+=2; //account for size of auh
    bs = bio_init_bitstream(auh,word16);
    offset = 12+2+word16;
    pkt_size-=offset;
    pld = buf+offset;
    for(i=0;i<numNALs;i++){
	audio->adts.sample_len = bio_read_bits(bs,audio->ind.sizelen+audio->ind.deltalen);
	audio->adts.sample_len>>=audio->ind.deltalen;
	//	audio->adts.sample_len/=8;
	aac_add_adts(audio);
	nkn_vfs_fwrite(pld,audio->adts.sample_len,sizeof(uint8_t),fout);
	pld+=audio->adts.sample_len;
	auh+=((audio->ind.sizelen+audio->ind.deltalen)/8);
    }
    return VPE_SUCCESS;
}


int32_t aac_add_adts(stream_state_t* audio){
    FILE* fout;
    uint8_t buf[7];
    bitstream_state_t *bs;
    fout = audio->pkt.fout;
    //    audio->adts.sample_len = audio->pkt.size;
    buf[0] =  0xFF;
    buf[1] = 0xF0|(audio->adts.is_mpeg2<<3)|0x01;
    bs = bio_init_bitstream(&buf[2],5);
    bio_write_int(bs,audio->adts.object_type,2);
    bio_write_int(bs,audio->adts.sr_index,4);
    bio_write_int(bs,0,1);
    bio_write_int(bs,audio->adts.num_channels,3);
    bio_write_int(bs,0,4);
    bio_write_int(bs,audio->adts.sample_len+7,13);
    bio_write_int(bs,0x7FF,11);
    bio_write_int(bs,0,2);
    nkn_vfs_fwrite(&buf[0],7,sizeof(uint8_t),fout);
    return VPE_SUCCESS;    
}

int32_t h264_rtp_dump(pkt_format_data_t *pktdata){
    /*
      Core level function that blindly extracts the h.264 payload and dumps into the file specified.
      Does not check for sequence numbers:
      1. Special handling required for SPS/PPs can be a part of separate function.
      2. For each NAL - being represented by the NAL octet in the RTP header, add  Annex B ompliance - 0x00000001

    */

    uint8_t *buf,*pld,uword;
    uint32_t pkt_size,pos=0,word32,ts;
    static uint32_t second_ts = 0;
    FILE* fout;


    buf = pktdata->data;
    pkt_size = pktdata->size;
    fout = pktdata->fout;
    ts = _read32(buf,4);
    //if(ts == 511281197)
    //{
    //    ts =511281197;
    //    ts= ts;
    //}
#if 0
    if(pktdata->prev_time==0 && (ts!= pktdata->prev_time)){
	pktdata->frame_rate = 90000/(ts-pktdata->prev_time);
	pktdata->prev_time = ts;
    }
#endif
#if 0//by suma
    if(pktdata->prev_time==0 && pktdata->frame_rate ==0)
	pktdata->prev_time = ts;
    if(pktdata->prev_time!=ts &&pktdata->frame_rate ==0){
	second_ts = ts;
	pktdata->frame_rate = 90000/(ts-pktdata->prev_time);
    }
    if(ts<second_ts && pktdata->frame_rate && (ts>pktdata->prev_time)){
        second_ts = ts;
        pktdata->frame_rate = 90000/(ts-pktdata->prev_time);
    }
#else

    if(pktdata->prev_time != ts)
	{
	    pktdata->num_frames++;

	}
    pktdata->prev_time = ts;
    //nkn_vfs_fwrite(ts,1,sizeof(uint32_t),fout_data);
    //nkn_vfs_fwrite(pktdata5->prev_time,1,sizeof(uint32_t),fout_data);
#endif

    pld = buf+12;
    uword = pld[pos]&0x1F;
    if(uword == 0)
	return VPE_ERROR;
    if((uword <=MAX_SINGLE_NALU) && (uword >=MIN_SINGLE_NALU)){
	/*single NALU*/
	word32 = NALU_DELIMIT;
	word32 = htonl(word32); 
	nkn_vfs_fwrite(&word32,1,sizeof(uint32_t),fout);
        nkn_vfs_fwrite(pld,pkt_size-12,sizeof(uint8_t),fout);
    }
   else if(uword ==STAPA){
        /*STAPS */
       uint32_t wsize;
       uint16_t NALsize;
      
       pos++;
       wsize = pkt_size-12-1;
       while(wsize){
	   NALsize = _read16(pld,pos);
	  
	   pos+=2;
	  
	   word32 = NALU_DELIMIT;
	   word32 = htonl(word32);
	   nkn_vfs_fwrite(&word32,1,sizeof(uint32_t),fout);
	   nkn_vfs_fwrite(&pld[pos],NALsize,sizeof(uint8_t),fout);
	   pos+=NALsize;
	  
	   wsize= wsize - NALsize - 2;
       }
      

    }   
   else if(uword == FUA){
       /*Fragmented unit*/
       uint8_t S,nal_h=0;
       uint32_t wsize;
       nal_h = pld[pos]>>5;
       nal_h<<=5;
       pos++;
       nal_h|=(pld[pos]&0x1F);
       uword = pld[pos++];
       S = uword >> 7;
       if(S){
	   word32 = NALU_DELIMIT;
	   word32 = htonl(word32);
	   nkn_vfs_fwrite(&word32,1,sizeof(uint32_t),fout);
	   nkn_vfs_fwrite(&nal_h,1,1,fout);
       }
       wsize = pkt_size-12-2;
       nkn_vfs_fwrite(&pld[pos],wsize,sizeof(uint8_t),fout);
    }
   else
       return VPE_ERROR;

    return VPE_SUCCESS;
}



void base642bin(char *sdp, int32_t num_bytes, uint8_t *out){
    int32_t data_left,pos=0, result_bytes = 0;
    uint32_t ind1,ind2,ind3,ind4,word;
    uint8_t byt1,byt2,byt3,*iout;
    char base64table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    data_left = num_bytes;
    iout = out;
    while(data_left){
        if(data_left >=4){
            //(uint32_t)(((sdp[pos++]) - 0x41)&0x3f);
            ind1 = (uint32_t)(strchr(base64table,sdp[pos++])-base64table);
            ind2 = (uint32_t)(strchr(base64table,sdp[pos++])-base64table);
            ind3 = (uint32_t)(strchr(base64table,sdp[pos++])-base64table);
            ind4 = (uint32_t)(strchr(base64table,sdp[pos++])-base64table);



            word = (ind1<<18)|(ind2<<12)|(ind3<<6)|ind4;
            *out++ = (uint8_t)((word>>16)&0xFF);
            *out++ = (uint8_t)((word>>8)&0xFF);
            *out++ = (uint8_t)(word&0xFF);
            data_left-=4;
        }
        else{
            switch(data_left){
                case 1:
                    // *out = (uint8_t)(strchr(base64table,sdp[pos])-base64table);//(((uint8_t)(sdp[pos]) - 0x41)&0x3f)<<6;
                    //*out<<=6;
                    //data_left-=1;
                    break;
                case 2:
                    /*12 bits: 6+2 4+4 zeros*/
                    byt1 = (uint8_t)(strchr(base64table,sdp[pos++])-base64table);
                    byt2 = (uint8_t)(strchr(base64table,sdp[pos])-base64table);
                    *out = (byt1<<2)|((byt2>>4)&0x3);
                    // out++;
                    //*out = (byt2&0xF);
                    //*out<<=4;
                    data_left-=2;
                    break;
		case 3:
                    /*18 bits: 6+2 4+4 2+6 zeros*/
                    byt1 = (uint8_t)(strchr(base64table,sdp[pos++])-base64table);
                    byt2 = (uint8_t)(strchr(base64table,sdp[pos++])-base64table);
                    byt3 = (uint8_t)(strchr(base64table,sdp[pos])-base64table);
                    *out++ = (byt1<<2)|((byt2>>4)&0x3);
                    *out = (byt2&0xF);
                    *out = (*out<<4)|(byt3>>2);
                    //              out++;
                    //*out = byt3&0x3;
                    //*out<<=6;
                    data_left-=3;
                    break;
                default:
                    break;
            }
        }
    }
    result_bytes = (int32_t)(out-iout);
    out = iout;
}


