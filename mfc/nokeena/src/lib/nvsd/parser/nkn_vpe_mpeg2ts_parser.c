/*******************************************************************/
/*
  Author:Karthik Narayanan
  Date:24-June 2009
  Memory Requirements:
  Stack Depth:
  Description:
  Contains the following functions:
  1.gen_trickplay_hdr: Used for trickplay for MPEG2TS
  Inputs:
  Index File Pointer: Pointer to the index file data from 1st 
  packet's data
  IndexFile Size: sizeof(indexfile)-sizeof(indexfile_header). 
  Present packet number:self explanatory.
  Speed: +ve for FF & -ve for rewind. Indicates number of I 
  frames to skip.
  Mode: 1-if the trickplay is called for the first time.
  0: for subsequent trickplay calls.
  Returns:
  Num pkts to dump and corresponding pkt numbers.
  When it cannot seek to the next required key frame, it 
  returns -1 for num pkts.
*/
/*******************************************************************/
#include "nkn_vpe_mpeg2ts_parser.h"
#include "nkn_vpe_h264_parser.h"
#include "nkn_vpe_mpeg2ps_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/******************************************************************/
/*Fucntion Name: gen_trickplay_hdr                               */
/*Description: Generates the meatdata file for MPEG2TS Parsing   */
/*			and returns the metadata file                        */
/*Inputs:   *ts filename,metadata file name                      */
/*Return Values:                 */
/*****************************************************************/
int32_t gen_trickplay_hdr(char *fname,char *meta)
{
    uint32_t first_pkt_num,last_pkt_num;
    uint8_t buf[MPEG2_TS_PKT_SIZE],prev_frame_type_pending;
    int32_t ii;
    int32_t ret;//,first_video_packet=0,last_video_packet=0;
    int64_t fpos;
    static int num_pkts_bw_IDRs=0,prev_IDR=0,prev_IDR_temp=0,IDR_flag=0,num_pkts_bw_IDRs_temp=0;	
    FILE *fid;
    Misc_data_t* extra_data;
    FILE *fmeta;//=fopen("metadata.dat","ab");
    tplay_index_record* tply_dump,*tply_dump_temp;
    tplay_index_record* tplay_record,*tply_init;//=(tplay_index_record*)malloc(sizeof(tplay_index_record));
    Ts_t* ts=(Ts_t*)malloc(sizeof(Ts_t));

    extra_data=(Misc_data_t*)malloc(sizeof(Misc_data_t));
    fid=fopen(fname,"rb");
    fmeta=fopen(meta,"wb");
    if(fmeta==NULL)
	return -1;
    tply_dump=(tplay_index_record*)malloc(sizeof(tplay_index_record)*MAX_PKTS_BW_IDR);//Allocate 3000 packets of memory - Between IDR frames
    if(tply_dump==NULL)
	return VPE_ERR;
    if(Ts_alloc(ts)==VPE_ERR)
	return VPE_ERR;
    ts->vsinfo->num_pids=0;
    ts->asinfo->num_pids=0;
    ts->num_pids=0;
    ts->num_pkts=0;
    ts->pas->prev_cc=0x0F;

    tply_init=tply_dump;
    extra_data->first_PTS=-1;
    extra_data->last_PTS=0;
    extra_data->frame_type_pending=0;
	
    tply_dump->next_tplay_pkt=0;
    tply_dump->prev_tplay_pkt=0;
    fseek(fid,0,SEEK_END);
    fpos=ftell(fid);
    fseek(fid,0,SEEK_SET);



    while(ts->num_pkts<fpos/MPEG2_TS_PKT_SIZE)
	{
	    /*Read packets & generate heaer file */
	    prev_frame_type_pending=extra_data->frame_type_pending;
	    tplay_record=tply_dump;
	    tplay_record->PTS=0;
	    ret = fread(buf,MPEG2_TS_PKT_SIZE,sizeof(uint8_t),fid); 
	    ret = handle_pkt_tplay(buf, ts,tplay_record,extra_data);
	    if(extra_data->frame_type_pending){
		//handle corner case of packet identity not known but is Video
		//Note the packets to be dumped.
		//Change the trickplay records for all those packets if it is an I frame:
		//Keep accumulating packets till the frame type is decided.			
		if(prev_frame_type_pending==0){
		    //transition to take place for video packets only.
		    tply_dump_temp=tply_dump;//(tplay_index_record*)malloc(sizeof(tplay_index_record)*100);
		    first_pkt_num=ts->num_pkts;
		    num_pkts_bw_IDRs_temp=0;
		}			
		if(tplay_record->type==TPLAY_RECORD_PIC_IFRAME||tplay_record->type==TPLAY_RECORD_PIC_NON_IFRAME){
		    last_pkt_num=ts->num_pkts;	
		    extra_data->frame_type_pending=0;
		    //This has to be the last video packet. 
		    if(tplay_record->type==TPLAY_RECORD_PIC_IFRAME){
			//It is an I frame - so dump till the last 
			int32_t i;
			num_pkts_bw_IDRs+=num_pkts_bw_IDRs_temp;
			IDR_flag=1;					
			tply_dump=tply_init;				
			tplay_record->prev_tplay_pkt=prev_IDR;	
			for(i=0;i<num_pkts_bw_IDRs;i++){
			    tply_dump->next_tplay_pkt=first_pkt_num;
			    tply_dump++;				
			}
			prev_IDR_temp=ts->num_pkts;
			fwrite(tply_init,num_pkts_bw_IDRs,sizeof(tplay_index_record),fmeta);					
			num_pkts_bw_IDRs=1;
			//Bunch of new Packets which have to be refreshed. 
			tply_dump=tply_dump_temp;
			for(i=0;i<(int32_t)(last_pkt_num-first_pkt_num);i++){
			    tply_dump->type=TPLAY_RECORD_PIC_NON_IFRAME;
			    tply_dump++;
			}
			memcpy(tply_init,tply_dump_temp,(last_pkt_num-first_pkt_num)*sizeof(tplay_index_record));
		    }
		    else{
			//non-I
			int i;
			if(IDR_flag){ //if this is the 1st Non-IDR packet
			    prev_IDR=prev_IDR_temp;
			    IDR_flag=0;
			}
			tply_dump_temp->type=TPLAY_RECORD_PIC_NON_IFRAME;
			for(i=1;i<(int32_t)(last_pkt_num-first_pkt_num);i++){
			    if(tply_dump_temp->type==TPLAY_RECORD_VIDEO_UNDEF){							
				tply_dump_temp->type=TPLAY_RECORD_PIC_NON_IFRAME;
			    }
			    tply_dump_temp++;
			}
			num_pkts_bw_IDRs+=num_pkts_bw_IDRs_temp;				
		    }				
		    tply_dump++;
		}
		else{
		    tply_dump++;
		    num_pkts_bw_IDRs_temp++;
		}			
		tplay_record->prev_tplay_pkt=prev_IDR;	
		ts->num_pkts++;		
	    }
	    else{
		if(tplay_record->type!=TPLAY_RECORD_PIC_IFRAME){
		    num_pkts_bw_IDRs++;
		    if(IDR_flag&&(tplay_record->type==TPLAY_RECORD_PIC_NON_IFRAME)){
			//1st non-IDR packet after IDR packets
			prev_IDR=prev_IDR_temp;
			IDR_flag=0;
		    }
		    tply_dump++;
		}
		else{
		    if(ts->start_of_pid){
			int i;
			tply_dump=tply_init;				
			tplay_record->prev_tplay_pkt=prev_IDR;	
			for(i=0;i<num_pkts_bw_IDRs;i++){
			    tply_dump->next_tplay_pkt=ts->num_pkts;
			    tply_dump++;				
			}
			prev_IDR_temp=ts->num_pkts;
			fwrite(tply_init,num_pkts_bw_IDRs,sizeof(tplay_index_record),fmeta);
#ifdef _DEBUG_	
			fclose(fmeta);
#endif
				
			num_pkts_bw_IDRs=1;
			tply_dump=tply_init;
			tply_dump->pkt_num=tplay_record->pkt_num;
			tply_dump->next_tplay_pkt=tplay_record->next_tplay_pkt;
			tply_dump->prev_tplay_pkt=tplay_record->prev_tplay_pkt;
			tply_dump->PTS=tplay_record->PTS;
			tply_dump->size=tplay_record->size;
			tply_dump->start_offset=tplay_record->start_offset;
			tply_dump->type=tplay_record->type;
			tply_dump++;
		    }
		    else{
			num_pkts_bw_IDRs++;
			tply_dump++;
		    }
		    IDR_flag=1;
		}
		tplay_record->prev_tplay_pkt=prev_IDR;	
		ts->num_pkts++;
	    }
	}
	
    tply_dump=tply_init;
    for(ii=0;ii<num_pkts_bw_IDRs;ii++){
	tply_dump->next_tplay_pkt=prev_IDR_temp;
	tply_dump++;				
    }
    fwrite(tply_init,num_pkts_bw_IDRs,sizeof(tplay_index_record),fmeta);
    tply_dump--;
    printf("Number of packets dumped %d\n",tply_dump->pkt_num);
    fwrite(extra_data,1,sizeof(Misc_data_t),fmeta);
    fclose(fmeta);
    fclose(fid);
    free(tply_init);
    free(ts);
    return VPE_OK;
}

/*Handles Ts Packet on the basis of feature requested*/
int handle_pkt_tplay(uint8_t *pkt,Ts_t* ts, tplay_index_record* tply, Misc_data_t*misc)
{	
    uint32_t pid=0;
    uint8_t adaptation_field=0,
	adaptation_field_length=190, payload_unit_start_indicator =
	0,payload_start_byte=0;
    uint8_t continuity_count = 0,random_access_indicator=0;
    int32_t stream_type =0;
    int64_t PCR_base = 0,PCR_ext = 0;
    int frame_type = 0;
    static int frame_num=0;
    if(pkt[0] != 0x47)return VPE_SYNC_BYTE_ERR;
    pid = (int)(pkt[1])<<8;
    pid=(pid|(int)(pkt[2]))&0x1FFF;
    payload_unit_start_indicator=(pkt[1]>>6)&0x01; //indicates start of PES pkt
    ts->start_of_pid=payload_unit_start_indicator;
	
    tply->type=TPLAY_RECORD_REQD_INFO;

    /*Get and Log the present PID*/

    stream_type=handle_pid(ts,pid);
	
    /*Adaptation Field*/
    adaptation_field=(pkt[3]>>4)&0x03;
    /*Adapatation Field Present:*/
    if(adaptation_field==2||adaptation_field==3)
	{
	    adaptation_field_length=pkt[4];
	    if(adaptation_field_length==0)
		adaptation_field_length=1; //accnt for 1 stuffing byte
	    else{
		random_access_indicator=(pkt[5]>>6)&0x01;
		if((pkt[5]>>4)&0x01){
		    PCR_base=(((int64_t)(pkt[6]))<<24)|(((int64_t)(pkt[7]))<<16)|(((int64_t)(pkt[8]))<<8)|(((int64_t)(pkt[9])));
		    PCR_base<<=1;
		    PCR_base|=(int64_t)(pkt[10]>>7);
		    PCR_ext=(int64_t)(pkt[10]&0x01);
		    PCR_ext<<=8;
		    PCR_ext|=pkt[11];
		}
			
		payload_start_byte++;//1 byte for adaptation field length
	    }
	}
    else
	adaptation_field_length=0;
	
    continuity_count=pkt[3]&0xF;	
    payload_start_byte+=4+adaptation_field_length;
    if(pid==0x0000)
	{	
	    /*Tables arrive here:*/
	    uint8_t* section,section_pos=0;
	    uint16_t section_len,pgm_map_pid,pgm_num;
	    if(payload_unit_start_indicator)
		payload_start_byte+=pkt[payload_start_byte]+1; //pointer_access+1 is pyld start
		
	    if(pkt[payload_start_byte]!=0x0)
		return -2;
		
	    section=&pkt[payload_start_byte];
	    if((section[5]&0x01) != 1) {
		return 1; //Skip  PAT if continuity_counter==0
	    }
	    section_pos++;
	    section_len=(section[section_pos]<<8|section[section_pos+1])&0x0FFF;

	    section_pos=8;
	    section_len-=5;
	    while(section_len>4){
		pgm_num=section[section_pos++]<<8;
		pgm_num|=section[section_pos++];
		if(pgm_num==0x00)
		    section_pos+=2;
		else
		    {
			//PID Information: Either PMT or NTK Info. 
			pgm_map_pid=((section[section_pos]<<8)|section[section_pos+1])&0xFFF;
			section_pos+=2;
			/*				New PID for PMT? 		*/
			/*Skip Version Number - assuming that change in PAT is not present*/
			if((ts->pas->prev_cc+1)%0x10!=(continuity_count)%0x10)
			    return 1; //The packets will have to be contiguous for now: Will change in streaming case
			ts->pas->prev_cc=continuity_count;
			stream_type=handle_pid(ts,pgm_map_pid);
		    }
		section_len-=4;

	    }	
	    tply->type=TPLAY_RECORD_REQD_INFO;

	}
    else if(pid==0x0001){
	tply->pkt_num=ts->num_pkts;
	tply->type=TPLAY_RECORD_REQD_INFO;
	tply->size=(uint8_t)(0x01ABCDEF);
	tply->start_offset=(uint8_t)(0x1ABCDEF);
	return VPE_OK;
		
    }
    else{
	if(stream_type==UNKWN_PID){
	    uint8_t table_id,*section,section_pos=0,strm_type;			
	    uint16_t section_len,pgm_num,pcr_pid,e_pid,es_info_len;
	    if(payload_unit_start_indicator)
		payload_start_byte+=pkt[payload_start_byte]+1; //pointer_access+1 is pyld start
	    table_id=pkt[payload_start_byte];
	    tply->type=TPLAY_RECORD_REQD_INFO;
	    if(table_id==0x02){		
		//PMT Here!!!
		section=&pkt[payload_start_byte];
		section_len=(section[1]<<8|section[2])&0x0FFF;
		pgm_num=(section[3]<<8)|section[4];
		if((section[5]&0x01) != 1) {
		    return 1; 
		}
		pcr_pid=(section[8]<<8|section[9])&0xFFF;
		//pgm_info_len=(section[10]<<8|section[11])&0xFFF;
		section_pos=12+(((section[10]<<8)|section[11])&0xFFF); //Skip the descriptor.
		section_len=section_len-9-((section[10]<<8|section[11])&0xFFF); //elem_data+CRC
		while(section_len>4){
		    strm_type=section[section_pos++];
		    e_pid=(section[section_pos]<<8|section[section_pos+1])&0x1FFF;
		    section_pos+=2;
		    es_info_len=(section[section_pos]<<8|section[section_pos+1])&0xFFF;
		    section_pos+=2+es_info_len;
		    section_len-=(5+es_info_len);
		    update_pid(ts,e_pid,strm_type);
		}
	    }
	    else{
		//New undectected payload - name as PVT and move on!
		ts->pmtinfo[ts->num_pids-1].stream_type=PVT_PID;								
		tply->pkt_num=ts->num_pkts;
		//tply->type=TPLAY_RECORD_JUNK;
		tply->size=(uint8_t)(0x1ABCDEF);
		tply->start_offset=(uint8_t)(0x1ABCDEF);
		return VPE_OK;
	    }
	}
	else if(stream_type==VIDEO_PID){
	    uint8_t strm_id,*PES,pes_position,PTS_DTS_flags,PES_hdr_len,*data,stream_num;
	    uint16_t PES_len,pts_temp;
	    uint32_t AU_type=0,data_pos,old_data_pos;
	    int64_t PTS=0;
	    if(random_access_indicator)
		frame_num+=0;
	    if(payload_unit_start_indicator){
				
		frame_num++;				
		PES=&pkt[payload_start_byte];
		pes_position=0;
		if( ((PES[pes_position]<<16)|(PES[pes_position+1]<<8)|(PES[pes_position+2])) !=0x01 ) {
		    return VPE_PES_ERR;
		}

		pes_position+=3;
		strm_id=PES[pes_position++];
		PES_len=PES[pes_position]<<8|PES[pes_position+1];
		pes_position+=2;
		/*Has to be Video Stream ID here*/
		if(strm_id>>4==0x0E ){
		    stream_num=strm_id&0x0F;
		    pes_position++;					
		    if(payload_unit_start_indicator){
			tply->pkt_num=ts->num_pkts;
			tply->type=6;//TPLAY_RECORD_RANDOM_ACCESS;				
		    }
		    PTS_DTS_flags=(PES[pes_position]>>6)&0x03;
		    pes_position++;
		    PES_hdr_len=PES[pes_position++];
		    data=&PES[pes_position+PES_hdr_len];
		    data_pos=pes_position+PES_hdr_len;

		    if(PTS_DTS_flags==0x02){
			PTS=(int64_t)((PES[pes_position++]>>1&0x07)<<30);
			pts_temp=(uint16_t)(PES[pes_position])<<7|(uint16_t)(PES[pes_position+1])>>1;
			PTS|=(int64_t)((pts_temp&0x7FFF)<<15);
			pes_position+=2;
			pts_temp=(uint16_t)(PES[pes_position])<<7|(uint16_t)(PES[pes_position+1])>>1;
			PTS|=(int64_t)(pts_temp&0x7FFF);
			pes_position+=2;
			tply->PTS=PTS;
			if(misc->first_PTS==-1)
			    misc->first_PTS=PTS;
			if(PTS>misc->last_PTS)
			    misc->last_PTS=PTS;
		    }
		    if(PTS_DTS_flags==0x03){
			PTS=(int64_t)((PES[pes_position++]>>1&0x07)<<30);
			pts_temp=(uint16_t)(PES[pes_position])<<7|(uint16_t)(PES[pes_position+1])>>1;
			PTS|=(int64_t)((pts_temp&0x7FFF)<<15);
			pes_position+=2;
			pts_temp=(uint16_t)(PES[pes_position])<<7|(uint16_t)(PES[pes_position+1])>>1;
			PTS|=(int64_t)(pts_temp&0x7FFF);
			pes_position+=2;
			tply->PTS=PTS;						
			if(misc->first_PTS==-1)
			    misc->first_PTS=PTS;
			if(PTS>misc->last_PTS)
			    misc->last_PTS=PTS;
		    }
					
		    /*Fill all tply fields!*/
		    misc->present_video_stream_num=stream_num;
		    if(ts->vsinfo->vstream[stream_num]->codecId==MPEG4_AVC){					
			while(AU_type==0){
			    old_data_pos=data_pos;
			    AU_type=handle_mpeg4AVC_bounded(data,&data_pos);
			    data+=(data_pos-old_data_pos);
			    if(data_pos-188<=0)
				break;
			}
			if(AU_type==SLICE_TYPE_UNKNOWN){
			    misc->frame_type_pending=1;
			}
			if(AU_type==IDR_SLICE){
			    frame_type=TPLAY_RECORD_PIC_IFRAME;
			    tply->pkt_num=ts->num_pkts;
			    misc->frame_type_pending=0;
			}
			else{
			    frame_type=TPLAY_RECORD_PIC_NON_IFRAME;
			    misc->frame_type_pending=0;
			}
		    }//AVC
		    if(ts->vsinfo->vstream[stream_num]->codecId==MPEG2_PES){
			AU_type=handle_mpeg2(data,&data_pos);
			if(AU_type==IDR_SLICE){
			    frame_type=TPLAY_RECORD_PIC_IFRAME;
			    misc->frame_type_pending=0;
			}
			if(AU_type==NON_IDR_SLICE){
			    frame_type=TPLAY_RECORD_PIC_NON_IFRAME;							
			    misc->frame_type_pending=0;
			}
			if(AU_type==SLICE_TYPE_UNKNOWN){
			    frame_type=TPLAY_RECORD_VIDEO_UNDEF;
			    misc->frame_type_pending=1;
			}
		    }//MPEG2 PES.
		}
		else//non-vid stream
		    return VPE_ERR;
	    }//PES Packet Start
	    else{
		//tply->pkt_num=ts->num_pkts;
		if(misc->frame_type_pending){
		    //Support only MPEG2 PS.
		    if(ts->vsinfo->vstream[misc->present_video_stream_num]->codecId==MPEG2_PES){
			data=&pkt[payload_start_byte];
			data_pos=payload_start_byte;
			AU_type=handle_mpeg2(data,&data_pos);
			if(AU_type==IDR_SLICE){
			    frame_type=TPLAY_RECORD_PIC_IFRAME;							
			    misc->frame_type_pending=0;
			}
			if(AU_type==NON_IDR_SLICE){
			    frame_type=TPLAY_RECORD_PIC_NON_IFRAME;
			    misc->frame_type_pending=0;
			}
			if(AU_type==SLICE_TYPE_UNKNOWN){
			    frame_type=TPLAY_RECORD_VIDEO_UNDEF;							
			    misc->frame_type_pending=1;
			}
		    }
		}
	    }//video packet in between
	}
	else if(stream_type==AUDIO_PID)	{
	    tply->type=TPLAY_RECORD_JUNK;
	}
	else{
	    //PVT stream here  - skip
	}

    }
    tply->pkt_num=ts->num_pkts;
    if(stream_type==VIDEO_PID)
	tply->type=frame_type;
    else{
	if(tply->type!=TPLAY_RECORD_REQD_INFO)
	    tply->type=TPLAY_RECORD_JUNK;
    }
    tply->size= (uint8_t)(0x1ABCDEF);
    tply->start_offset=(uint8_t)(0x1ABCDEF);
    return VPE_OK;
}
	
int32_t handle_pid(Ts_t* Ts,uint32_t PID)
{
    /*Check if its a new PID and update the PID structure*/
    int i,new_pid=1;
    int32_t stream_type;
    for(i=0; i < (int32_t)(Ts->num_pids); i++)
	{
	    if(PID==(Ts->pmtinfo[i]).pid)
		{
		    new_pid=0;
		    stream_type=Ts->pmtinfo[i].stream_type;
		}
	}
    if(new_pid)
	{
	    //Ts->pids[Ts->num_pids]=PID;
	    Ts->pmtinfo[Ts->num_pids].pid=PID;
	    Ts->pmtinfo[Ts->num_pids].stream_type=UNKWN_PID;		
	    stream_type=UNKWN_PID; 
	    Ts->num_pids++;
	}
    return  stream_type;
}

int32_t update_pid(Ts_t* Ts,uint16_t pid,uint8_t strm)
{
    int32_t strm_type,i,new_pid=1;
    /*Refer Table 2-29 in H.222 Amendment 3*/
    if(strm==0x01||strm==0x10||strm==0x1b||strm==0x02)
	strm_type=VIDEO_PID;
    else if(strm==0x03||strm==0x4||strm==0x0F||strm==0x11)
	strm_type=AUDIO_PID;
    else 
	strm_type=PVT_PID;
    for(i=0;i< (int32_t)(Ts->num_pids);i++) {
	if(pid==(Ts->pmtinfo[i]).pid){
	    new_pid=0;
	    if(strm_type!=Ts->pmtinfo[i].stream_type) {
		Ts->pmtinfo[i].stream_type = strm_type;
		new_pid = 1;
	    }
	}
    }
    if(new_pid){
	Ts->pmtinfo[Ts->num_pids].pid=pid;
	Ts->pmtinfo[Ts->num_pids].stream_type=strm_type;
	Ts->num_pids++;		
    }


    //Handle Only Video for Now
    if(strm_type==VIDEO_PID&&new_pid){
	Ts->vsinfo->vstream[Ts->vsinfo->num_pids]=(VStream_t*)malloc(sizeof(VStream_t));
	if(Ts->vsinfo->vstream[Ts->vsinfo->num_pids]==NULL)
	    return VPE_ERR;		
	Ts->vsinfo->vstream[Ts->vsinfo->num_pids]->pid=pid;
	if(strm==0x01||strm==0x02)
	    Ts->vsinfo->vstream[Ts->vsinfo->num_pids]->codecId=MPEG2_PES;
	else if(strm==0x10)
	    Ts->vsinfo->vstream[Ts->vsinfo->num_pids]->codecId=MPEG4_VISUAL;
	else//if(strm==0x1b)
	    Ts->vsinfo->vstream[Ts->vsinfo->num_pids]->codecId=MPEG4_AVC;
	Ts->vsinfo->num_pids++;
    }

    //Populate Audio Later!
    return 0;
}

int Ts_alloc(Ts_t* ts1)
{
	
    ts1->pmtinfo=(PMT_t*)malloc(sizeof(PMT_t)*MAX_PIDS); 
    ts1->vsinfo=(VStrmInfo_t*)malloc(sizeof(VStrmInfo_t)); 
    ts1->asinfo=(AStrmInfo_t*)malloc(sizeof(AStrmInfo_t));
    ts1->pas=(PAS_t*)malloc(sizeof(PAS_t));
    if(ts1==NULL||(ts1->pmtinfo==NULL)||(ts1->vsinfo==NULL)||(ts1->asinfo==NULL)||(ts1->pas==NULL))
	return VPE_ERR;
    //If ts1 assigned:
    ts1->vsinfo->vstream=(VStream_t**)malloc(sizeof(VStream_t**)*MAX_PIDS_VIDEO);
    ts1->asinfo->astream=(AStream_t**)malloc(sizeof(AStream_t**)*MAX_PIDS_AUDIO);
    if((ts1->vsinfo->vstream==NULL)||(ts1->asinfo->astream==NULL))
	return VPE_ERR;	
    return VPE_OK;
}

void Ts_reset(Ts_t *ts)
{
    int32_t i;
    for (i = 0; i < ts->vsinfo->num_pids; i++) { 
	free(ts->vsinfo->vstream[i]);
    }
    
    for (i = 0; i < ts->asinfo->num_pids; i++) {
	free(ts->asinfo->astream[i]);
    }
    
    ts->vsinfo->num_pids=0;
    ts->asinfo->num_pids=0;
    ts->num_pids=0;
    ts->num_pkts=0;
    ts->pas->prev_cc=0x0F;
    ts->num_pids = 0;

}

void Ts_free(Ts_t *ts)
{
  int32_t i =0;
    if (ts) {
	free(ts->pmtinfo);
	for(i=0; i<ts->vsinfo->num_pids; i++) {
	  free(ts->vsinfo->vstream[i]);
	}
	free(ts->vsinfo->vstream);
	free(ts->asinfo->astream);
	free(ts->vsinfo);
	free(ts->asinfo);
	free(ts->pas);
	free(ts);
    }
}

int Tplay_malloc(Tplay_index_t* tr)
{
    tr->curr_index=(tplay_index_record*)malloc(sizeof(tplay_index_record));
    tr->prev_index=(tplay_index_record*)malloc(sizeof(tplay_index_record));
    if(tr->curr_index==NULL||tr->prev_index==NULL)
	return VPE_ERR;
    return VPE_OK;
}

int32_t check_mpeg2ts_index_file(FILE * fid)
{
    int64_t fsize,bytes_consumed=0;
    uint32_t prev_PTS,prev_pkt_num,video=0;
    uint8_t *data,mem_flag=0;
    tplay_index_record *tply;
    FILE* flog=fopen("Ts_parse_log","w");
    tply=(tplay_index_record*)malloc(sizeof(tplay_index_record));
    fseek(fid,0,SEEK_END);
    fsize=ftell(fid);
    fsize-=(TPLY_HDR_SIZE+TPLY_EXTRA_INFO);
    fseek(fid,0,SEEK_SET);
    prev_PTS=0;
    prev_pkt_num=-1;
    if(fsize%sizeof(tplay_index_record)!=0)
	fprintf(flog,"Warning::File does not contain integral number of packets\n");

    data=(uint8_t*)malloc(sizeof(uint8_t)*(fsize));
    if(data==NULL){
	mem_flag=1;
	fread(tply,1,sizeof(tplay_index_record),fid);
	bytes_consumed+=sizeof(tplay_index_record);
    }
    else{
	fread(data,fsize,sizeof(uint8_t),fid);
	tply=(tplay_index_record*)(data);
    }
    while(bytes_consumed<fsize){
	if(mem_flag){
	    //check pkt numbers, PTS,Log video packets
	    if(tply->pkt_num!=prev_pkt_num+1)
		fprintf(flog,"Packet number %d Missing\n",prev_pkt_num+1);
	    prev_pkt_num=tply->pkt_num;
	    if(tply->PTS!=0){
		prev_PTS=tply->PTS;
	    }
	    if(tply->type==TPLAY_RECORD_PIC_NON_IFRAME||tply->type==TPLAY_RECORD_PIC_IFRAME)
		video++;	
	    fread(tply,1,sizeof(tplay_index_record),fid);
	}
	else{
	    //check pkt numbers, PTS,Log video packets
	    if(tply->pkt_num!=prev_pkt_num+1){
		fprintf(flog,"Packet number %d Missing\n",prev_pkt_num+1);
	    }
	    else{
		//packet number correct:
		fprintf(flog,"Pkt Num: %d Previous pktNum %d Next Pktnum:%d\n",tply->pkt_num,tply->prev_tplay_pkt,tply->next_tplay_pkt);					
		if(0){//(tply->type!=TPLAY_RECORD_PIC_IFRAME){
		    //check next frame & prev trickplay pkt
		    if(tply->pkt_num<tply->prev_tplay_pkt)
			fprintf(flog,"Previous pkt error: pktNum:%d prev: %d \n",tply->pkt_num,tply->prev_tplay_pkt);					
		    if(tply->pkt_num>tply->next_tplay_pkt)
			fprintf(flog,"Next pkt error: pktNum:%d next: %d \n ",tply->pkt_num,tply->next_tplay_pkt);
		}
	    }

	    prev_pkt_num=tply->pkt_num;
	    if(tply->PTS!=0){
		prev_PTS=tply->PTS;
	    }
	    if(tply->type==TPLAY_RECORD_PIC_NON_IFRAME||tply->type==TPLAY_RECORD_PIC_IFRAME)
		video++;	
	    tply++;		
	}
	bytes_consumed+=sizeof(tplay_index_record);
    }
    if(mem_flag==0)
	free(data);
    fprintf(flog,"PTS mismatch in Packet number %d\n",video);
    fclose(flog);

    return VPE_OK;
}
