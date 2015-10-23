/*******************************************************************/
/*
  Author:Karthik Narayanan
  Date:24-June 2009
  Description:
  Contains the following functions:
  1.get_first_idr_pkts: Used for trickplay - seeks to the nearest
  I frame and returns the corresponding packets to be returned.
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

#include "trickplay.h"
#include "glib.h"
#include <stdlib.h>
#include <string.h>



pkt_list_t* nkn_vpe_get_first_idr_pkts(uint8_t *index_file_pointer, uint64_t index_file_size, uint32_t pkt_num,int16_t speed,uint8_t mode){

    uint64_t fpos;
    uint8_t *index_file_start;
    tplay_index_record* tply;
    pkt_list_t *pktlist;
    uint32_t *pkt_ptr;
    int32_t i, num_pkts;
    int32_t n_pkts;

    n_pkts = 0;
    pktlist=(pkt_list_t*)malloc(sizeof(pkt_list_t));
    memset(pktlist, 0, sizeof(pkt_list_t));

    index_file_start = index_file_pointer;

    //goto present packet number.
    fpos=(uint64_t)(sizeof(tplay_index_record)*pkt_num);
    if(fpos>index_file_size){
	pktlist->num_pkts = -1;
	return pktlist;
    }
    index_file_pointer+=fpos;
    if(mode){
	//Might not be in an I-frame.
	//Seek to next I frame.
	tply=(tplay_index_record*)(index_file_pointer);
	if(speed>0){
	    if(tply->next_tplay_pkt<pkt_num){
		pktlist->num_pkts = -1;
		return pktlist;
	    }
	    index_file_pointer=index_file_start+(sizeof(tplay_index_record)*(tply->next_tplay_pkt-1));
	}
	else{
	    if(tply->prev_tplay_pkt==0){
		pktlist->num_pkts = -1;
		return pktlist;
	    }
	    index_file_pointer=index_file_start+(sizeof(tplay_index_record)*(tply->prev_tplay_pkt-1));
	}
	tply=(tplay_index_record*)(index_file_pointer);
	//The previous index frame will not change till the next non-I frame.
	pktlist->num_pkts=0;
	while(tply->type!=TPLAY_RECORD_PIC_NON_IFRAME){
	    //Note the IDR packets
	    if((uint8_t*)(tply)>index_file_start+index_file_size){
		pktlist->num_pkts=-1;
		return pktlist;
	    }

	    if(tply->type==TPLAY_RECORD_PIC_IFRAME)
		pktlist->num_pkts++;
	    tply++;
	}
	pkt_ptr=(uint32_t*)malloc(sizeof(uint32_t)*pktlist->num_pkts);
	pktlist->pkt_nums=pkt_ptr;
	tply=(tplay_index_record*)(index_file_pointer);
	num_pkts=0;//pktlist->num_pkts;
	while(num_pkts<=pktlist->num_pkts && ((uint8_t*)(tply)<index_file_start+index_file_size)){
	    //check for eof
	    if(tply->type==TPLAY_RECORD_PIC_IFRAME){
		num_pkts++;
		*(pktlist->pkt_nums)=tply->pkt_num;
		pktlist->pkt_nums++;
	    }
	    tply++;
	}
	pktlist->pkt_nums=pkt_ptr;
	return pktlist;
    }
    else{
	//have already serviced the 1st I frame : Now keep jumping I frames.
	if(speed<0){
	    //Rewind
	    uint32_t prev_pkt;
	    uint8_t *prev_tply_pkt;
	    tply=(tplay_index_record*)(index_file_pointer);
	    if(tply->prev_tplay_pkt==0||tply->prev_tplay_pkt==pkt_num){
		pktlist->num_pkts = -1;
		return pktlist;
	    }
	    prev_tply_pkt=index_file_pointer;
	    for(i=0;i<speed;i++)
		{
		    prev_pkt=tply->prev_tplay_pkt;
		    prev_tply_pkt=index_file_start+(sizeof(tplay_index_record)*prev_pkt);
		    tply=(tplay_index_record*)(prev_tply_pkt);
		}
	    index_file_pointer=prev_tply_pkt;
	}
	else{
	    //FF
	    uint32_t next_pkt;
	    uint8_t *next_tply_pkt;
	    tply=(tplay_index_record*)(index_file_pointer);
	    next_tply_pkt=index_file_pointer;
	    for(i=0;i<speed;i++)
		{
		    next_pkt=tply->next_tplay_pkt;
		    next_tply_pkt=index_file_start+(sizeof(tplay_index_record)*next_pkt);
		    tply=(tplay_index_record*)(next_tply_pkt);
		    if(tply->next_tplay_pkt<pkt_num){
			pktlist->num_pkts = -1;
			return pktlist;
		    }
		}
	    index_file_pointer=next_tply_pkt;
	}
	if(index_file_pointer > (index_file_start + index_file_size)||index_file_pointer<index_file_start) {
	    pktlist->num_pkts = -1;
	    return pktlist;
	}

	if(tply->type!=TPLAY_RECORD_PIC_IFRAME)
	    return NULL;
	//The previous index frame will not change till the next non-I frame.
	pktlist->num_pkts=0;
	while(tply->type!=TPLAY_RECORD_PIC_NON_IFRAME){
	    //Note the IDR packets
	    if(tply->type==TPLAY_RECORD_PIC_IFRAME)
		pktlist->num_pkts++;
	    tply++;
	}
	pkt_ptr=(uint32_t*)malloc(sizeof(uint32_t)*pktlist->num_pkts);
	pktlist->pkt_nums=pkt_ptr;
	tply=(tplay_index_record*)(index_file_pointer);
	num_pkts=0;//pktlist->num_pkts;
	while(num_pkts<=pktlist->num_pkts){
	    //check for eof
	    if((uint8_t*)(tply) > (index_file_start + index_file_size)) 
		break;
	    if(tply->type==TPLAY_RECORD_PIC_IFRAME){
		num_pkts++;
		*(pktlist->pkt_nums)=tply->pkt_num;
		pktlist->pkt_nums++;
	    }
	    tply++;
	}
	pktlist->pkt_nums=pkt_ptr;
	return pktlist;
    }
}

#if 0
int64_t get_duration(int64_t start_time,int64_t end_time){

    uint8_t* index_start;
    int64_t start_time,end_time,time;
    tplay_index_record *tply;
    index_start=index_data_pointer;
    index_data_pointer=index_start+(sizeof(tplay_index_record)*(first_pkt-1));
    tply=(tplay_index_record*)(index_data_pointer);
    start_time = tply->PTS;
    index_data_pointer=index_start+(sizeof(tplay_index_record)*(last_pkt-1));
    tply=(tplay_index_record*)(index_data_pointer);
    end_time = tply->PTS;
    return (end_time-start_time)/90;

    return (end_time-start_time)/90;

    
}
#endif
