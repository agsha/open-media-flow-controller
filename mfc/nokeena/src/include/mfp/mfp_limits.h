#ifndef _MFP_LIMITS_H_
#define  _MFP_LIMITS_H_


/*Generic error codes*/
#define VPE_WARNING (-1)
#define VPE_SUCCESS (1)
#define VPE_ERROR   (0)
#define VPE_REDO 3


//MAX MFP SESSIONS SUPPORTED
#define MAX_MFP_SESSIONS_SUPPORTED (1000)
//MAX PROFILES PER STREAM
#define MAX_STREAM_PER_ENCAP 20
//sync params
#define MAX_SYNC_IDR_FRAMES (20)
//MAX FILE NAME
#define MAX_FILE_NAME_LEN 250
//MAX PUB NAME LEN
#define MAX_PUB_NAME_LEN 40
//MAX URI LEN
#define MAX_URL_LEN 250
//FILE PUMP
#define MAX_FILEPUM_INPFILE_NAME (1024)
//Accum buffer limit
#define STD_TSBITSTREAMS_OVERHEAD (200) //kbps
#define BUFFER_SIZE_SCALE	  (1.5)
#define MAX_NUM_NWINPT_PKTS	  (50)
//DTS vs PTS
#define TIMESTAMP_DELTA		 (500)

//for file, max ts pkt limit
#define MAX_NUM_INPT_PKTS 2000000 
#define MAX_NUM_INPT_PKTS_CHUNK 20000
//Apple formatter
#define SECS_TO_MILLISECS	(1000)
#define MIN_TO_SECONDS		(60)

//network related
#define BYTES_IN_PACKET (188)
#define NUM_PKTS_IN_NETWORK_CHUNK (7)

//seq-num, mfu duration data pair
#define MAX_DPELEMENT_DEPTH	(10)

#endif
