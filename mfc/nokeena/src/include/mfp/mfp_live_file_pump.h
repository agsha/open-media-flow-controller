#ifndef _MFP_LIVE_FILE_PUMP_H_
#define _MFP_LIVE_FILE_PUMP_H_

#include <pthread.h>
#include "mfp_publ_context.h"
#include "nkn_vpe_mfp_ts2mfu.h"
#include "mfp_live_event_handler.h"
#include "nkn_debug.h"

#define MAX_FDS_PROCESSED (10)

#define BITS_PER_Kb (1024)
#define NUM_BITS_IN_BYTES (8)
#define MS_IN_SEC (1000)
#define UPCP_HEADER (35)

#define MAX_PID_EST  (6)
#define NUM_NETWORK_PKTS (15)
#define MAX_PKTS_REQD_FOR_ESTIMATE (NUM_NETWORK_PKTS * NUM_PKTS_IN_NETWORK_CHUNK)
#define EST_PKT_DEPTH (MAX_PKTS_REQD_FOR_ESTIMATE*TS_PKT_SIZE)
#define NUM_UPCP_PKTS_IN_105TSPKTS (NUM_NETWORK_PKTS)

extern pthread_t file_pump_thread;


typedef enum file_pump_err_tt{
	eSuccess = 0,
	eNoMem,
	eEOF,
	eUnknownError
} file_pump_err_t;


typedef struct FILE_PUMP_CTXT_TT file_pump_ctxt_t;
typedef struct sched_entry_tt sched_entry_t;

typedef void* (*start_fp)(void *);
typedef file_pump_err_t (*insert_fd_entry)(file_pump_ctxt_t *,
	int32_t ,uint32_t, FILE_PUMP_MODE_E , mfp_publ_t *, sess_stream_id_t* );
	


//linked list of scheduled fds
struct sched_entry_tt{
	int32_t fd;
	mfp_publ_t *pub_ctx;
	sess_stream_id_t* id;
	uint32_t num_pkts_processed;
	uint32_t bitrate;
	FILE_PUMP_MODE_E fp_mode;
	uint32_t num_pkts_per_second;
	uint32_t sched_weight;
	sched_entry_t *next;
};



struct FILE_PUMP_CTXT_TT{
	uint32_t num_fds;
	sched_entry_t *rrsched_entry; //round robin sched entry
	sched_entry_t *origin_rrsched_entry; 

	//function pointers
	start_fp start;
	insert_fd_entry insert_entry;

	pthread_mutex_t file_pump_mutex;
	pthread_mutexattr_t file_pump_mutex_attr;
	pthread_cond_t file_pump_cond_var;
	pthread_condattr_t file_pump_cont_attr;
	
};

typedef struct pid_ppty_map_tt{
	uint32_t pid;
	uint32_t min_pkt_time;
	uint32_t max_pkt_time;
	uint32_t max_duration;
	uint32_t entry_valid;
}pid_ppty_map_t;

extern file_pump_ctxt_t * new_file_pump_context(void);



#endif
