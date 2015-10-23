#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>

#define MAX_BUFFER_LEN 64535

static void exitClean(int signal_num); 
static void initSignalHandlers(void); 


#define VPE_ERROR (0)
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
	return VPE_ERROR;
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


uint32_t insert_new_track_info(ts_tracks_t *ts_tracks, uint16_t pid){

    uint32_t index = ts_tracks->num_tracks;
    ts_tracks->num_tracks++;	
    ts_tracks->track_info[index] = calloc(1, sizeof(track_info_t));
    if(ts_tracks->track_info[index] == NULL){
	printf("Unable to allocate mem for track info\n");
	exit(1);
    }
    ts_tracks->track_info[index]->pid = pid;
    ts_tracks->track_info[index]->first_instance = 1;
    ts_tracks->track_info[index]->prev_cont_counter = 0;
    ts_tracks->track_info[index]->cont_flag = 1;
    ts_tracks->track_info[index]->accum_count = 0;
    return(index);
}

uint32_t reset_track_info(ts_tracks_t *ts_tracks, uint32_t index){
    ts_tracks->track_info[index]->first_instance = 1;
    ts_tracks->track_info[index]->prev_cont_counter = 0;
    ts_tracks->track_info[index]->cont_flag = 1;
    ts_tracks->track_info[index]->accum_count = 0;
    return;
}

uint32_t lookup_index_for_pid(ts_tracks_t *ts_tracks, uint16_t pid){
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

void delete_track_info(ts_tracks_t * ts_tracks){

    uint32_t i;
    for(i = 0; i < ts_tracks->num_tracks ; i++){
	if(ts_tracks->track_info[i] != NULL){
	    free(ts_tracks->track_info[i]);
	    ts_tracks->track_info[i] = NULL;
	}
    }
    return;
}

struct timeval start, end;

void check_streams(void *argv);


int main (int32_t argc, char* argv[]) {

    if (argc == 0) {
	printf("Usage: ./<pgm_name> <num_stream> <mult_ip_addr0> <port_num0> <mult_ip_addr1> <port_num1> ...\n");
	return 0;
    }
    gettimeofday(&start, NULL);
    initSignalHandlers();
    uint32_t num_streams = atoi(argv[1]);
    uint32_t i;
    pthread_t *pthr[10];

    for (i = 0; i < num_streams; i++){
    	pthread_create(pthr[i], NULL, check_streams, (void *)(argv[i*2 + 2]));
    }
   
    return(0);
}


void check_streams(void *argv){

    long long tot_pkts_recvd = 0;
    int32_t fd;
    struct ip_mreq m_ip;
    
    char *ip_addr;
    char *port_no;
    char *inp;

    inp = (char *)argv;
	
    ts_tracks_t ts_tracks;
    ts_tracks.num_tracks = 0;
    uint32_t j;

    ip_addr = &inp[0];
    port_no = &inp[1];
    
    for(j = 0; j < NUM_TRACKS; j++){
	ts_tracks.track_info[j] = NULL;
    }

    struct sockaddr_in local_addr;
    int8_t buff[MAX_BUFFER_LEN];

    uint16_t port = (uint16_t)strtol(port_no, NULL, 0);
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
	//return -1;
	pthread_exit(NULL);
    }

    if (inet_pton(AF_INET, ip_addr, &(m_ip.imr_multiaddr)) == -1) {
	perror("inet_pton: Address conversion failed.\n");
	close(fd);
	pthread_exit(NULL);
    }
    local_addr.sin_family = PF_INET;
    local_addr.sin_port = htons(port);
    local_addr.sin_addr.s_addr = m_ip.imr_multiaddr.s_addr;

    int32_t rc = bind(fd, (struct sockaddr *)&local_addr,
	    sizeof(struct sockaddr_in));
    if (rc < 0) {
	perror("bind: ");
	close(fd);
	pthread_exit(NULL);
    }
    m_ip.imr_interface.s_addr = INADDR_ANY;
    rc = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
	    &m_ip, sizeof(struct ip_mreq));
    if (rc < 0) {
	perror("setsockopt: ");
	close(fd);
	pthread_exit(NULL);
    }
    uint32_t sock_len = sizeof(struct sockaddr_in);

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


    while (1) {

	rc = recvfrom(fd, buff, MAX_BUFFER_LEN, 0,
		(struct sockaddr *)&local_addr, &sock_len);
	if (rc < 0) {
	    perror("recvfrom failed: ");
	    close(fd);
	    pthread_exit(NULL);
	}
	tot_pkts_recvd += 1;
	char ip_src[16];
	inet_ntop(AF_INET, &(local_addr.sin_addr), &(ip_src[0]), 16);
	if(((uint32_t)tot_pkts_recvd) % 100 == 0)
	    printf("%lu Message received from:%s:%d  size: %d\n", tot_pkts_recvd, ip_src, ntohs(local_addr.sin_port), rc);


	//check continuity counter in all ts packets within network packet
	for( i = 0; i < rc / 188; i++){

	    //parse TS header
	    pos = parseTS_header(buff, i* 188, &psi, &pid, &cont_counter, &afc );
	    if(pos == VPE_ERROR){
		printf("Unable to find sync word\n");
		pthread_exit(NULL);
	    }

	    if(pid == PAT_PKT){
		uint8_t *data = buff + pos; 
		pmt_pid  = ((data[11] & 0x1f) << 8)  |  data[12];
		pmt_pcr_acquired |= 0x1;
	    }

	    if(pid == pmt_pid){
		uint8_t *data = buff + pos; 
		pcr_pid  = ((data[9] & 0x1f) << 8)  |  data[10];	    	
		pmt_pcr_acquired |= 0x2;
	    }

	    if(pid != PAT_PKT && pid != NULL_PKT && pid != pmt_pid && pid != pcr_pid && pmt_pcr_acquired == 0x3){
		//lookup trackinfo
		uint32_t index = lookup_index_for_pid(&ts_tracks, pid);
		if(index == 0xffffffff){
		    //if no, insert the node
		    index = insert_new_track_info(&ts_tracks, pid);
		}
		if(ts_tracks.track_info[index]->first_instance == 1){
		    ts_tracks.track_info[index]->prev_cont_counter = cont_counter;
		    ts_tracks.track_info[index]->first_instance = 0;
		} else{
		    //if yes. check continuity, add count
		    if((ts_tracks.track_info[index]->prev_cont_counter + 1)% 16 == cont_counter){
			ts_tracks.track_info[index]->prev_cont_counter = cont_counter;
			ts_tracks.track_info[index]->cont_flag = 1;
			ts_tracks.track_info[index]->accum_count += 1;
		    } else{
			//if continuity missing, report error with stats
			printf("pid: %d Continuity counter missing after %u packets\n", pid, ts_tracks.track_info[index]->accum_count);
			reset_track_info(&ts_tracks, index);
			//exit(1);
		    }
		}
	    }

	}
    }	
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

    gettimeofday(&end, NULL);
    printf("******************\n");
    printf("Summary: Total Duration(in secs): %ld \n",
	    end.tv_sec - start.tv_sec);
    printf("******************\n");    
}



