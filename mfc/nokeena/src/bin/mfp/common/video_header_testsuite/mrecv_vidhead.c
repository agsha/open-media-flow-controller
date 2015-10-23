#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>

#include "mfp_video_header.h"
#include "mfp_limits.h"
#include "nkn_vpe_mfp_ts2mfu.h"
#include "mfp_live_file_pump.h"
#include "mfp_live_accum_ts_v2.h"

#define MAX_BUFFER_LEN 64535

static void exitClean(int signal_num); 
static void initSignalHandlers(void); 

struct timeval start_test, end_test;
unsigned long tot_pkts_recvd = 0;
int32_t fd;
struct ip_mreq m_ip;

#define VPE_ERROR1 (0)
#define NUM_TRACKS (10)

typedef struct track_info_tt{
    uint16_t pid;
    uint32_t first_instance;
    uint8_t prev_cont_counter;
    uint32_t cont_flag;
    uint32_t accum_count;
}track_info_t;

typedef struct ts_tracks{
    uint32_t num_tracks;
    track_info_t *track_info[NUM_TRACKS];
}ts_tracks_t;

typedef struct track_stat{
    uint32_t pid;
    uint32_t prev_pts;
    uint32_t prev_dts;
    uint32_t sample_cnt;
}track_stat_t;

static track_stat_t video_trak;
static track_stat_t audio1_trak;
static track_stat_t audio2_trak;



static uint16_t get_pkt_pid(uint8_t* data){
    uint16_t pid = 0;
    pid = data[1]&0x1F;
    pid<<=8;
    pid|=data[2];
    return pid;
}

static uint16_t parse_pat_for_pmt(uint8_t *data){
    uint16_t pid = 0, pos = 0,program_number,program_map_PID=0;;
    uint8_t adaf_field;
    /*Get adaptation field*/

    adaf_field = (data[3]>>4)&0x3;
    pos+= 4;
    if(adaf_field == 0x02 || adaf_field == 0x03)
	pos+=1+data[pos]; //skip adaptation filed.

    pos+=1; //pointer field
    /*Table 2-25 -- Program association section*/
    /*skip 8 bytes till last_section_number (included)*/
    pos += 8;
    for(;;){
        program_number =0;
        program_number = data[pos];
        program_number<<=8;
        program_number|=data[pos+1];
        pos+=2;
        if(program_number == 0) {
            pos+=2;
        } else {
            program_map_PID = data[pos]&0x1F;
            program_map_PID<<=8;
            program_map_PID|=data[pos+1];
            pos+=2;
            break;
        }
    }
    (printf("PAT found\n"));
    return program_map_PID;
}


static void get_av_pids_from_pmt(uint8_t *data, uint16_t* vpid,
				 uint16_t *apid,  uint16_t* apid2){

    uint16_t pos = 0,program_info_length=0,pid, elementary_PID=0,
	ES_info_length=0, section_len = 0;
    uint8_t adaf_field, num_aud = 0, stream_type;

    *vpid = *apid =0;
    /*Get adaptation field*/
    adaf_field = (data[3]>>4)&0x3;
    pos+= 4;
    if(adaf_field == 0x02 || adaf_field == 0x03)
	pos+=1+data[pos]; //skip adaptation filed.

    pos+=1; //pointer field
    /*Table 2-28 -- Transport Stream program map section*/
    section_len = (data[6] & 0x0f) << 8;
    section_len |= data[7];

    /*skip 10 bytes till PCR_PID (included)*/
    pos += 10;

    /*skip the data descriptors*/
    program_info_length = data[pos]&0x0F;
    program_info_length<<=8;
    program_info_length|=data[pos+1];
    pos+=program_info_length;
    pos+=2;

    /*parse for PIDs*/
    for(;;){
	stream_type = data[pos++];
	elementary_PID = ES_info_length = 0;
	elementary_PID = data[pos]&0x1F;
	elementary_PID<<=8;
	elementary_PID |= data[pos+1];
	pos+=2;
	ES_info_length =  data[pos]&0xF;
	ES_info_length<<=8;
	ES_info_length|= data[pos+1];
	pos+=2;
	pos+= ES_info_length;
        
	switch(stream_type){
	    case 0x0F:
	    case 0x11:
		/*Audio: 0x0F for ISO/IEC 13818-7*/
		if(*apid == 0)
		    *apid = elementary_PID;
		else
		    *apid2 = elementary_PID;
		break;
	    case 0x1B:
		/*Video: 0x1B for H.264*/
		*vpid = elementary_PID;
		break;
	    default:
		break;
	}
	if (pos >= section_len) {
	    break;
	}

	if(pos>=180 /*||(*vpid&&*apid)*/)
	    break;
    }

}



static int8_t get_spts_pids(uint8_t*  data,  uint32_t len,
			    uint16_t* vpid,  uint16_t *apid,
			    uint16_t* apid2, uint16_t pmt_pid){

    int8_t ret = 0;
    uint32_t i, n_ts_pkts = 0;
    uint16_t pid;
    n_ts_pkts = len/188;
    for(i=0;i<n_ts_pkts;i++){
	pid = get_pkt_pid(data + i*188);
	if(pid == pmt_pid){
	    get_av_pids_from_pmt(data+i*188,vpid,apid,apid2);
	    ret = 1;
	}
    }

    return ret;

}


static uint16_t get_pmt_pid(uint8_t* data,uint32_t len){

    uint32_t i, n_ts_pkts=0;
    uint16_t pid, pmt_pid = 0;
    n_ts_pkts = len/188;
    for(i=0;i<n_ts_pkts;i++){
	pid = get_pkt_pid(data + i*188);
	if(!pid){
	    pmt_pid = parse_pat_for_pmt(data + i*188);
	    break;
	}
    }

    return pmt_pid;
}

static void parse_spts_pmt(uint8_t* data,uint32_t len,mux_st_elems_t *mux){
    int8_t ret;
    if(!mux->pmt_pid){
	mux->pmt_pid =  get_pmt_pid((uint8_t*)data,len);
	if(!mux->pmt_pid)
	    return;
    }
    if(!mux->pmt_parsed){
	ret = get_spts_pids((uint8_t*)data,len,
		&mux->video_pid,&mux->audio_pid1,
		&mux->audio_pid2,mux->pmt_pid);
	if(ret)
	    mux->pmt_parsed = 1;
    }
}

static uint32_t parseTS_header(uint8_t *data, uint32_t pos, 
	uint8_t *payload_unit_start_indicator,
	uint16_t* rpid, 
	uint8_t	*continuity_counter,
	uint8_t *adaptation_field_control) {

    uint8_t  sync_byte;
    uint32_t adaptation_field_len = 0;
    uint16_t pid;

    sync_byte = *(data+pos);
    if (sync_byte != 0x47) {
	//DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "sync byte - not found, got %x \n",sync_byte);
	return VPE_ERROR1;
    }

    pos++;//skip sync byte
    *payload_unit_start_indicator = ((*(data+pos))&0x40)>>6;
    pid = data[pos]&0x1F;
    pid = (pid<<8)|data[pos+1];
    *rpid = pid;    
    pos += 2;//skip transport_priority,pid
    *adaptation_field_control = ((*(data+pos))&0x30)>>4;
    *continuity_counter = (*(data+pos)) & 0x0f;
    pos++;

    if ((*adaptation_field_control == 0x3) ||
	    (*adaptation_field_control == 0x2)) { 
	adaptation_field_len = *(data+pos);
	pos++;
	pos += adaptation_field_len;
    }
    return(pos);
}


static uint32_t insert_new_track_info(ts_tracks_t *ts_tracks, uint16_t pid){

    uint32_t index1 = ts_tracks->num_tracks;
    ts_tracks->num_tracks++;	
    ts_tracks->track_info[index1] = calloc(1, sizeof(track_info_t));
    if(ts_tracks->track_info[index1] == NULL){
	printf("Unable to allocate mem for track info\n");
	exit(1);
    }
    ts_tracks->track_info[index1]->pid = pid;
    ts_tracks->track_info[index1]->first_instance = 1;
    ts_tracks->track_info[index1]->prev_cont_counter = 0;
    ts_tracks->track_info[index1]->cont_flag = 1;
    ts_tracks->track_info[index1]->accum_count = 0;
    return(index1);
}

static uint32_t reset_track_info(ts_tracks_t *ts_tracks, uint32_t index1){
    ts_tracks->track_info[index1]->first_instance = 1;
    ts_tracks->track_info[index1]->prev_cont_counter = 0;
    ts_tracks->track_info[index1]->cont_flag = 1;
    ts_tracks->track_info[index1]->accum_count = 0;
    return 0;
}

static uint32_t lookup_index1_for_pid(ts_tracks_t *ts_tracks, uint16_t pid){
    uint32_t ret = 0xffffffff;
    uint32_t i;

    for(i = 0; i < ts_tracks->num_tracks; i++){
	if(ts_tracks->track_info[i]->pid == pid){
	    ret = i;
	    break;
	}
    }
    return(ret);
}

static void delete_track_info(ts_tracks_t * ts_tracks){

    uint32_t i;
    for(i = 0; i < ts_tracks->num_tracks ; i++){
	if(ts_tracks->track_info[i] != NULL){
	    free(ts_tracks->track_info[i]);
	    ts_tracks->track_info[i] = NULL;
	}
    }
    return;
}



static void initSignalHandlers(void) {

    struct sigaction action_cleanup;
    memset(&action_cleanup, 0, sizeof(struct sigaction));
    action_cleanup.sa_handler = exitClean;
    action_cleanup.sa_flags = 0;
    sigaction(SIGINT, &action_cleanup, NULL);
    sigaction(SIGTERM, &action_cleanup, NULL);
}


static void exitClean(int signal_num) {

    gettimeofday(&end_test, NULL);
    int32_t rc = setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, 
	    &m_ip, sizeof(struct ip_mreq));
    if (rc < 0) {
	perror("setsockopt: ");
    }
    printf("******************\n");
    printf("Summary: Total Duration(in secs): %ld Total pkts recvd: %lu\n",
	    end_test.tv_sec - start_test.tv_sec, tot_pkts_recvd);
    printf("******************\n");

    close(fd);
}





static char * convert_slicetype_to_string(slice_type_e type){
	char *res;
	switch(type){
		case 1:
			res = (char*)"I Slice";
			break;
		case 2:
			res = (char*)"P Slice";
			break;
		case 3:
			res = (char*)"B Slice";
			break;
		case 0:
			res = (char*)"Not Assigned";
			break;
		case -1:
			res = (char*)"NeedMoreData";
			break;
	}
	return (res);
}

static void get_trak_stat(track_stat_t *trak, uint32_t pid, uint32_t pts){
	if(trak->prev_pts){
		printf("pid#%u sample_durn %d\n",pid, pts - trak->prev_pts);
		trak->prev_pts = pts;
	} else {
		trak->prev_pts = pts;
	}
	return;
}

static int ts_parser_video_header_wrapper(
	unsigned char * data, 
	unsigned int len, 
	unsigned int apid, 
	unsigned int vpid)
{

	ts_pkt_infotable_t *pkt_tbl;
	
	unsigned int num_ts_pkts = len /BYTES_IN_PACKET;
	
	unsigned int *pkt_offsets;
	pkt_offsets = (unsigned int *) malloc(num_ts_pkts * sizeof(int));
	if(pkt_offsets == NULL){
		printf("error alloting memory to pkt_offsets\n");
		return (-1);	
	}
	
	unsigned int i;
	for(i = 0; i < num_ts_pkts; i++){
		pkt_offsets[i] = i * BYTES_IN_PACKET;
	}
	
	create_index_table(
				data, 
				pkt_offsets, 
				num_ts_pkts, 
				apid, 
				vpid, 
				&pkt_tbl);
	
	unsigned int cnti;
	
	for( cnti = 0; cnti < num_ts_pkts; cnti++){
		/*printf("pkt# %u  framestart %u Type %s pid#%u pts %u\n",
		cnti, pkt_tbl[cnti].frame_start,
		convert_slicetype_to_string(pkt_tbl[cnti].frame_type),
		pkt_tbl[cnti].pid, pkt_tbl[cnti].pts);*/
		if( pkt_tbl[cnti].pts){
		if(pkt_tbl[cnti].pid == video_trak.pid){
			get_trak_stat(&video_trak,pkt_tbl[cnti].pid, pkt_tbl[cnti].pts);
		} else if(pkt_tbl[cnti].pid == audio1_trak.pid){
			get_trak_stat(&audio1_trak,pkt_tbl[cnti].pid, pkt_tbl[cnti].pts);
		} else if(pkt_tbl[cnti].pid == audio2_trak.pid){
			get_trak_stat(&audio2_trak,pkt_tbl[cnti].pid, pkt_tbl[cnti].pts);
		}
			}
	}	
	
	slice_type_e ft;
	unsigned int frame_type_num;
	
	for( cnti = 0; cnti < num_ts_pkts; cnti++){
		if(pkt_tbl[cnti].frame_start){
			ft = find_frame_type(
					num_ts_pkts, 
					cnti,
					vpid, 
					pkt_tbl,
					&frame_type_num);
			printf("pktindex1 %u --> %s\n", cnti, convert_slicetype_to_string(ft));
		}
	}
	
	
	
	
	if(pkt_tbl){
		free(pkt_tbl);
		pkt_tbl = NULL;
	}	
	if(pkt_offsets){
		free(pkt_offsets);
		pkt_offsets = NULL;
	}
	
	
	return 0;
}




int main (int32_t argc, char* argv[]) {

	if (argc <= 3) {
		printf("Usage: ./<pgm_name> <mult_ip_addr> <port_num> <local_if_ip> <Optional: dis_file_dump>\n");
		return 0;
	}
	gettimeofday(&start_test, NULL);
	initSignalHandlers();
	ts_tracks_t ts_tracks;
	ts_tracks.num_tracks = 0;
	uint32_t j;
	for(j = 0; j < NUM_TRACKS; j++){
		ts_tracks.track_info[j] = NULL;
	}
	uint32_t disable_file_dump = 0; 
	if(argc > 4){
		disable_file_dump =	atoi(argv[4]);
	}

	struct sockaddr_in local_addr;
	int8_t buff[MAX_BUFFER_LEN];

	uint16_t port = (uint16_t)strtol(argv[2], NULL, 0);
	if (port == 0) {
		printf("Invalid port number.\n");
		exit(-1);
	}

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("socket: ");
		exit(-1);
	}

	int32_t set_reuse = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
				(char *)&set_reuse,
				sizeof(set_reuse)) < 0) {
		perror("setsockopt : reuseaddr: ");
		return -1;
	}

	if (inet_pton(AF_INET, argv[1], &(m_ip.imr_multiaddr)) == -1) {
		perror("inet_pton: Multicast Address conversion failed.\n");
		close(fd);
		exit(-1);
	}
	local_addr.sin_family = PF_INET;
	local_addr.sin_port = htons(port);
	local_addr.sin_addr.s_addr = m_ip.imr_multiaddr.s_addr;

	int32_t rc = bind(fd, (struct sockaddr *)&local_addr,
			sizeof(struct sockaddr_in));
	if (rc < 0) {
		perror("bind: ");
		close(fd);
		exit(-1);
	}

	//m_ip.imr_interface.s_addr = INADDR_ANY;
	if (inet_pton(AF_INET, argv[3], &(m_ip.imr_interface.s_addr)) == -1) {
		perror("inet_pton: Local IP Address conversion failed.\n");
		close(fd);
		exit(-1);
	}

	rc = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
			&m_ip, sizeof(struct ip_mreq));
	if (rc < 0) {
		perror("setsockopt: ");
		close(fd);
		exit(-1);
	}
	uint32_t sock_len = sizeof(struct sockaddr_in);
	FILE* fp;

	if(disable_file_dump == 0){
		char file_name[500];
		snprintf(&file_name[0], 499, "ts_dump_%d", fd);
		fp = fopen(file_name, "wb");
		printf("file name: %s\n", file_name);
		if (fp == NULL) {
			printf("TS Dump : File Open failed.\n");
			abort();
		}
	}

	uint32_t i;
	uint16_t pid;
	uint8_t psi;
	uint8_t cont_counter;
	uint8_t afc;
	uint32_t pos;
	uint32_t pmt_pid = 0xffff;
	uint32_t pcr_pid = 0xffff;
	uint32_t pmt_pcr_acquired = 0;

#define PAT_PKT (0)
#define NULL_PKT (0x1fff)

	mux_st_elems_t mux;

	while (1) {
		rc = recvfrom(fd, buff, MAX_BUFFER_LEN, 0,
				(struct sockaddr *)&local_addr, &sock_len);
		if (rc < 0) {
			perror("recvfrom failed: ");
			close(fd);
			exit(-1);
		}
		parse_spts_pmt((uint8_t*)buff,rc,&mux);
		if(mux.pmt_parsed) {
			printf ("Started Receiving Data and parsed PAT, PMT, VPID, APID \n");
		    break;
		}
	}

	if(mux.video_pid)
		video_trak.pid = mux.video_pid;
	if(mux.audio_pid1)
		audio1_trak.pid = mux.audio_pid1;
	if(mux.audio_pid2)
		audio2_trak.pid = mux.audio_pid2;
	
	while (1) {

		rc = recvfrom(fd, buff, MAX_BUFFER_LEN, 0,
				(struct sockaddr *)&local_addr, &sock_len);
		if (rc < 0) {
			perror("recvfrom failed: ");
			close(fd);
			exit(-1);
		}
		tot_pkts_recvd += 1;
		char ip_src[16];
		inet_ntop(AF_INET, &(local_addr.sin_addr), &(ip_src[0]), 16);
		if(((uint32_t)tot_pkts_recvd) % 100 == 0)
			printf("%lu Message received from:%s:%d  size: %d\n",
					tot_pkts_recvd, ip_src, ntohs(local_addr.sin_port), rc);
		if(disable_file_dump == 0){
			if (fwrite(buff, 1, rc, fp) != (uint32_t)rc) {
				printf("TS Dump : File write failed\n");
				abort();
			}
		} else
			continue;


		//check continuity counter in all ts packets within network packet
		for( i = 0; i < (uint32_t)rc / 188; i++){

			//parse TS header
			pos = parseTS_header((uint8_t*)buff, i* 188, &psi,
					&pid, &cont_counter, &afc );
			if(pos == VPE_ERROR1){
				printf("Unable to find sync word\n");
				exit(1);
			}

			if(pid == PAT_PKT){
				uint8_t *data = (uint8_t*)buff + pos; 
				pmt_pid  = ((data[11] & 0x1f) << 8)  |  data[12];
				pmt_pcr_acquired |= 0x1;
			}

			if(pid == pmt_pid){
				uint8_t *data = (uint8_t*)buff + pos; 
				pcr_pid  = ((data[9] & 0x1f) << 8)  |  data[10];	    	
				pmt_pcr_acquired |= 0x2;
			}

			if(pid != PAT_PKT && pid != NULL_PKT &&
					pid != pmt_pid && pid != pcr_pid &&
					pmt_pcr_acquired == 0x3){
				//lookup trackinfo
				uint32_t index1 = lookup_index1_for_pid(&ts_tracks, pid);
				if(index1 == 0xffffffff){
					//if no, insert the node
					index1 = insert_new_track_info(&ts_tracks, pid);
				}
				if(ts_tracks.track_info[index1]->first_instance == 1){
					ts_tracks.track_info[index1]->prev_cont_counter =
						cont_counter;
					ts_tracks.track_info[index1]->first_instance = 0;
				} else{
					//if yes. check continuity, add count
					if((ts_tracks.track_info[index1]->prev_cont_counter + 1)% 16
							== cont_counter){
						ts_tracks.track_info[index1]->prev_cont_counter =
							cont_counter;
						ts_tracks.track_info[index1]->cont_flag = 1;
						ts_tracks.track_info[index1]->accum_count += 1;
					} else{
						//if continuity missing, report error with stats
						printf("pid: %d Continuity counter missing after %u \
								packets\n", pid,
								ts_tracks.track_info[index1]->accum_count);
						reset_track_info(&ts_tracks, index1);
						//exit(1);
					}
				}
			}

		}

		ts_parser_video_header_wrapper((unsigned char *)buff, rc, mux.audio_pid1, mux.video_pid);
		
	}

	if(disable_file_dump == 0){
		fclose(fp);
	}
}

