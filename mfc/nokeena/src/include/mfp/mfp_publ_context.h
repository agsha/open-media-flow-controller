#ifndef MFP_PUBL_CONTEXT_HH
#define MFP_PUBL_CONTEXT_HH

#include <stdint.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include "mfp_limits.h"
#include "mfp_accum_intf.h"
#include "nkn_vpe_ha_state.h"
#include "nkn_vpe_sl_fmtr.h"
#include "sync_serial/mfp_sync_serial.h"
#include "nkn_vpe_fmtr.h"
#include "mfp_err.h"
#include "mfp_live_conf.h"
#include "mfp_safe_io.h"

#include "nkn_authmgr.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#define ENCAP_SYNC_TIME_OFFSET 10
#define ENCAP_IN_SYNC 1

//#define STREAM_CONT_CHECK

/*** Required ENUM declarations */
typedef enum source_type {
	LIVE_SRC,
	FILE_SRC,
	CONTROL_SRC
} SOURCE_TYPE;


#if defined (INC_SEGD)
typedef enum accum_fmtr_type {
    APPLE,
    SL,
    NONE
} ACCUM_FMTR_TYPE;
#endif

typedef enum pub_schemes {
	SMOOTH_STREAMING,
	FLASH_STREAMING,
	MOBILE_STREAMING
} PUB_SCHEMES;

typedef enum transport_type {
	RTP,
	TS,
	RAW_UDP,
	ISMV_MBR,
	ISMV_SBR,
	FLV,
	MP4,
	UDP_IP,
	RTP_UDP
} ENCAPSULATION_TYPE;

typedef enum media_type {
	VIDEO,
	AUDIO,
	MUX,
	SVR_MANIFEST,
	CLIENT_MANIFEST,
	ZERI_MANIFEST,
	ZERI_INDEX
} MEDIA_TYPE;

typedef enum encoding_type {
	H264,
	AAC
} ENCODING_TYPE;

#if 0
typedef enum stream_state {
	LIVE,
	DEAD
} STREAM_STATE;
#endif

typedef enum mfp_op_state {
	PUB_SESS_STATE_INIT,
	PUB_SESS_STATE_SYNC,
	PUB_SESS_STATE_RUNNING,
	PUB_SESS_STATE_IDLE,
	PUB_SESS_STATE_REMOVE,
	PUB_SESS_STATE_EEXIST,
	PUB_SESS_STATE_REMOVED,
	/* read only states for ctrl pmf response handler */
	PUB_SESS_STATE_STOP, 
	PUB_SESS_STATE_PAUSE,
	PUB_SESS_STATE_RESTART,
	PUB_SESS_STATE_STATUS,
	PUB_SESS_STATE_CONFIG,
	PUB_SESS_STATE_STATUS_LIST
} PUB_SESS_OP_STATE;

typedef enum mfp_cfg_types {
        MFP_CFG_ADD_WATCHDIR,
	MFP_CFG_DEL_WATCHDIR
} MFP_PMF_CFG_TYPES;

/*** source conf values : live or file */
typedef struct live_source {

	struct sockaddr_in to;
	struct in_addr source_if;
	int8_t is_multicast;
	int8_t is_ssm;
	int32_t fd; //array of sock fd corresponding to this session
} live_source_t;

typedef struct file_source {

	int8_t* file_url;
	// Other details related to file_url need to be filled in 
} file_source_t;

typedef union media_source {

	live_source_t live_src;
	file_source_t file_src;
} media_source_t;

typedef struct sync_prms{
    uint32_t first_time_recd;
    uint32_t sync_time; //in ms
    int8_t encap_in_sync; //0: If sync not present. else 1
    int8_t sync_search_started; //1: Search for sync point has started: Else 0
    uint32_t ref_accum_cntr;
    int8_t kill_all_streams;
    uint32_t stream_num;
    uint32_t last_mfu_seq_num[MAX_STREAM_PER_ENCAP];
#if defined (INC_SEGD) 
    uint32_t ms_last_mfu_seq_num[MAX_STREAM_PER_ENCAP];
    uint32_t ss_last_mfu_seq_num[MAX_STREAM_PER_ENCAP];
    uint32_t ze_last_mfu_seq_num[MAX_STREAM_PER_ENCAP];
#endif
}sync_ctx_params_t;

typedef struct tag_sess_sched_info {
        uint64_t st_time;
        uint64_t duration; /*seconds*/
}sess_sched_info_t;

typedef struct tag_cfg_info {
        MFP_PMF_CFG_TYPES type;
        char val[256];
} pmf_cfg_info_t;

typedef enum file_pump_mode{
	TS_INPUT = 0,
	UPCP_INPUT
}FILE_PUMP_MODE_E;

/*** every stream in a session will have 1 stream_parm */
typedef struct stream_param {

	media_source_t media_src;
	uint32_t bit_rate;
	uint8_t profile_id;
	uint16_t apid;
	uint16_t vpid;
#ifdef STREAM_CONT_CHECK
	uint16_t apid_cont_ctr;
	uint16_t vpid_cont_ctr;
#endif
        uint32_t file_source_mode;
        uint8_t inp_file_name[MAX_FILEPUM_INPFILE_NAME];
	FILE_PUMP_MODE_E file_pump_mode;
	ENCAPSULATION_TYPE encaps_type;
	MEDIA_TYPE med_type;
	ENCODING_TYPE enc_type;	
	ENCAPSULATION_TYPE tport_type;

	int32_t* related_idx; // stream idx(index) related to this
	//stream
	int32_t n_related_streams;
	/* The stream_param struct for audio would have the corresponding
	   video stream_id in the related_ids array and vice versa */
	sync_ctx_params_t syncp;
	int32_t return_code;

	stream_state_e stream_state;
} stream_param_t;

typedef enum formatter_status{
	FMTR_INIT_FAILURE = -3,
	FMTR_INPUT_DATA_ERROR = -2,
	FMTR_OUTPUT_IO_ERROR = -1,
	FMTR_OFF = 0,
	FMTR_ON = 1
} formatter_status_e;

/*** Publish type conf values from user */
typedef struct silver_stream_parm {//user conf values for silver stream publish

	formatter_status_e status;
	uint32_t key_frame_interval;
	uint8_t dvr;
	uint32_t dvr_seconds;
	size_t disk_usage;
	mfp_safe_io_ctx_t *sioc;
} silver_parm_t;

typedef struct flash_parm_tt {//user conf values for flash stream publish

	formatter_status_e status;
	uint32_t fragment_duration;
	uint32_t segment_duration;
	zeri_stream_type_t stream_type;
	uint32_t dvr_seconds;
	uint32_t encryption; //0 - off, 1 - on
	size_t disk_usage;
	mfp_safe_io_ctx_t *sioc;
	int8_t*  common_key;
	int8_t*  license_server_url;
	int8_t*  license_server_cert;
	int8_t*  transport_cert;
	int8_t*  packager_cert;
	int8_t*  credential_pwd;
	int8_t*  policy_file;
	int8_t*  content_id;
	uint32_t orig_mod_req;
} flash_parm_t;

typedef struct mobile_stream_parm {//user conf values for mobile stream publish

	formatter_status_e status;
	uint32_t key_frame_interval;
	size_t disk_usage;
	mfp_safe_io_ctx_t *sioc;
	uint32_t segment_start_idx;
	uint8_t min_segment_child_plist;
	uint8_t segment_idx_precision;
	uint32_t segment_rollover_interval;
	uint32_t dvr_window_length; //in minutes
	uint8_t dvr;
	char store_url[500];
	char delivery_url[500];
        uint8_t encr_flag;
	char kms_addr[256];
	uint32_t kms_port;
	AUTHTYPE kms_type;
	uint32_t key_rotation_interval;
	uint32_t iv_len;
	char key_delivery_base_uri[512];
	/* Other required fields need to be added : 
	   Depending on the conf values collected from user */
} mobile_parm_t;


struct mfp_publish_context;

typedef void (*deleteHandlerPC)(struct mfp_publish_context*);


typedef struct tag_pub_status_t {

    char *name;
    char *instance;

    char *num_multicast_strms_name;
    uint32_t num_multicast_strms;

    char *num_unicast_strms_name;
    uint32_t num_unicast_strms;

    char *tot_input_bytes_name;
    uint64_t tot_input_bytes;

    char *tot_output_bytes_name;
    uint64_t tot_output_bytes;

    char *num_apple_fmtr_name;
    uint32_t num_apple_fmtr;

    char *num_sl_fmtr_name;
    uint32_t num_sl_fmtr;

    char *num_zeri_fmtr_name;
    uint32_t num_zeri_fmtr;


}pub_status_t;

typedef struct tag_global_op_cfg {
    size_t disk_usage;
    mfp_safe_io_ctx_t *sioc;
}global_op_cfg_t;

typedef struct tag_sync_params{
   uint32_t pts_value[MAX_SYNC_IDR_FRAMES][MAX_STREAM_PER_ENCAP];
   uint32_t dts_value[MAX_SYNC_IDR_FRAMES][MAX_STREAM_PER_ENCAP];
   uint32_t valid_index_entry[MAX_SYNC_IDR_FRAMES][MAX_STREAM_PER_ENCAP];
   uint32_t idr_index[MAX_STREAM_PER_ENCAP];
   uint32_t use_pts;
   uint32_t use_pts_decision_valid;
} sync_params_t;
/*** consolidated publish request structure */
typedef struct mfp_publish_context {

	PUB_SESS_OP_STATE op;
	uint8_t encaps_count;

	SOURCE_TYPE src_type;
	uint32_t duration;

	char **sess_name_list;
	uint32_t n_sess_names;

	sess_sched_info_t ssi;
        pmf_cfg_info_t cfg;
	stream_param_t* stream_parm;


	/*Every encaps in this array point to a particular position in the
	  stream_parm array */
	stream_param_t** encaps;
	uint32_t* streams_per_encaps;
	uint8_t stream_count;
	uint8_t active_fd_count;// used in session cleanup

	/* Specific configuration parameters of the publish schemes are
	   specified in each of the structure below */
	silver_parm_t ss_parm;
	flash_parm_t fs_parm;
	mobile_parm_t ms_parm;
	global_op_cfg_t op_cfg;
	int8_t* name;

	int8_t* ss_store_url;
	int8_t* ss_delivery_url;

	int8_t* fs_store_url;
	int8_t* fs_delivery_url;

	int8_t* ms_store_url;
	int8_t* ms_delivery_url;

	/* Every received stream is passed to a particular accumulator.
	   The state of those accumulators and their function handlers are
	   declared here */
	void** accum_ctx;
	accum_interface_t** accum_intf;

	/* A session level id for the chunks produced in this session.
	   Value increases by 1 for every chunk produced. */
	uint32_t sess_chunk_ctr;

	/* Every accumulated stream is passed to a particular formatter.
       	The state of those formatters and their function handlers are
	declared here */
	sl_fmtr_t* sl_publ_fmtr;
	sync_serial_t* sl_sync_ser;
	deleteHandlerPC delete_publish_ctx;
	pub_status_t stats;

	sync_params_t sync_params;
	uint32_t formatters_created_flag;
    
} mfp_publ_t;


/**
 * creates a new publisher context
 * @param encaps_count - the number of encapsulations in the PMF file
 * @param src_type - the source type as defined in the PMF file (can
 * be LIVE/FILE)
 * @return - returns the address of a valid publisher context, NULL on
 * error
 */
mfp_publ_t* newPublishContext(uint8_t encaps_count, 
		SOURCE_TYPE src_type); 

uint32_t newPublishContextStats(mfp_publ_t *pub_ctx);


/**
 * allocates memory for a session name list (STATUS request Control
 * PMF's)
 * @param pub - publisher context
 * @param n_sess_names - number of session names
 * @return returns 0 on success and a negative number on error
 */
int32_t allocSessNameList(mfp_publ_t *pub, uint32_t n_sess_names);

uint32_t create_pub_dbg_counter(
	mfp_publ_t* pub_ctx,
	char **name,
	char *instance,
	char *string,
	void *count_var,
	uint32_t size);

int32_t initControlPMF(mfp_publ_t *pub_ctx,
		       const char *const name,
		       PUB_SESS_OP_STATE op);
#endif
