/*
 * nkn_vpemgr.c
 * Fqueue based offline vpe manager task scheduler
 * Responsible for scheduling offline vpe based tasks such as chunking for smoothflow, transcoding, multi-rate generation, etc
 * This module interfaces with an Interpretor (Python/Perl) to call on modules that perform the task of pre-processing media data
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <alloca.h>
#include <poll.h>
#include <uuid/uuid.h>

#include <pthread.h>
#include <semaphore.h>
#include <queue.h>
#include <unistd.h>
#include <math.h>

#include "nkn_common_config.h"

#include "nkn_mgmt_agent.h"
#include "nkn_cfg_params.h"
#include "http_header.h"
#include "fqueue.h"
#include "ssp_python_plugin.h"
#include "nkn_mem_counter_def.h"
#include "nkn_vpemgr_defs.h"
#include "rtp_format.h"

//need come from CLI
#define IDLE_CORE_NUM   5

sem_t idle_core_num;
pthread_mutex_t task_queue_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * task_type is one of
 *  VPE_IDENTIFIER_SMOOTHFLOW
 *  VPE_IDENTIFIER_RTSP
 *  VPE_IDENTIFIER_GENERAL_TRANSCODE
 *  VPE_IDENTIFIER_YOUTUBE_TRANSCODE
 *
 * for all task, we will generate checksum of remapped_uri of orirate
 * to detect whether this task is the same as one of the tasks in processing queue.
 *
 * For one task_type
 * if checksum of incoming task is the same as one of the tasks in processing queue
 *    this incoming task will be omitted
 * else
 *    this incoming task will be processed
 *
 * note: why we use the remapped_uri of orirate for the checksum?
 * the rampped_uri of orirate  and remapped_uri of lowrate are 1to1 match.
 * In youtube transcode, the remapped_uri_lowrate will need some process to generate, but
 * remapped_uri_orirate can be read directly from fqueue
 * As a result, we will use uri_orirate
 *
 */

typedef struct vpe_task_attr{
    int task_type; //
    u_int32_t cksum_remapped_uri;
    TAILQ_ENTRY(vpe_task_attr) entries;
}vpe_task_attr_t;

TAILQ_HEAD(, vpe_task_attr) tailq_head;




int vpemgr_debug_output;

#if 1
#define DBG(_fmt, ...) { \
	if (vpemgr_debug_output) { \
	    fprintf(stdout, (_fmt), __VA_ARGS__); \
	    fprintf(stdout, "\n"); \
	} \
}
#else
#define DBG(_fmt, ...)
#endif

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

/* Local Macros */
#define MAX_LINE_SIZE		4096
#define VPEMGR_QUEUE_FILE 	"/tmp/VPEMGR.queue"

static int do_not_create_daemon = 0;

/* Local Variables */
static const char *input_queue_filename = VPEMGR_QUEUE_FILE;
static int input_queuefile_maxsize = 4096;


/* Local Function Prototypes */
int 	pid_file_exist(void);
void	create_pid_file (void);
void	exit_vpemgr (void);
void	termination_handler (int nSignum);
void	print_usage (char *cpProgName);
int 	getoptions(int argc, char *argv[]);
void 	daemonize(void);

void    process_qelement(void *qe);
void 	request_handler(void);
int     process_sf_qelement(const fqueue_element_t *qe_t);
int     process_rtsp_qelement(const fqueue_element_t *qe_t);
int     process_generic_transcode_qelement(const fqueue_element_t *qe_t);
int     process_youtube_transcode_qelement(const fqueue_element_t *qe_t);

static u_int32_t compute_cksum(const char *p, int len);
int generate_vpe_task_attri(int task_type, const char *remapped_uri_data, 
			    int remapped_uri_len, vpe_task_attr_t* cur_task_attr);
static int findString (const char *pcMainStr, const char *pcSubStr, int length);


/*****************************************************************************
*	Function : main()
*	Purpose : main logic
*	Parameters : argc, argv
*	Return Value : int
*****************************************************************************/
int main (int argc, char *argv[])
{
    if (getoptions(argc, argv)) {
    	exit(4);
    }
    mime_hdr_startup(); // Init mime_hdr structures (one time process init)

    /* Fix for Bug 1565.
       We dont need to maintain pid, since this will run under PM and it will handle the PID's
       if (pid_file_exist()) {
       fprintf(stdout, "error : [%s] already running ...\n", argv[0]);
       exit(5);
       }
    */
    /* Install signal handler */
    //#define DELLLLLL
#ifdef DELLLLLL
    //char filename[]="/var/home/root/sample_h264_1mbit.mp4";
    char filename[]="/nkn/mnt/fuse/sample_h264_1mbit.mp4";

	//    strcpy(filename,"~/sample_h264_300kbit.mp4.nknc");
    //    strcat(filename,uriData);

    read_cache_translate(filename,(char *)"hello");
    return 0;
#endif



    if (signal(SIGINT, termination_handler) == SIG_IGN)
    	signal(SIGINT, SIG_IGN);
    if (signal(SIGHUP, termination_handler) == SIG_IGN)
    	signal(SIGHUP, SIG_IGN);
    if (signal(SIGTERM, termination_handler) == SIG_IGN)
    	signal(SIGTERM, SIG_IGN);
    if (signal(SIGQUIT, termination_handler) == SIG_IGN)
    	signal(SIGQUIT, SIG_IGN);
    signal(SIGUSR1, termination_handler);

    if (!do_not_create_daemon) {
    	daemonize();
    }

    /* Set the atexit function */
    if (atexit(exit_vpemgr) != 0) {
    	fprintf(stdout, "error : failed to set exit routine, hence exiting... \n");
    	exit(5);
    }

    /* Fix for Bug 1565.
    // Create the pid file
    create_pid_file();
    */

    request_handler();

    exit(0);
}

/*****************************************************************************
*	Function : request_handler()
*	Purpose : Process fqueue_element_t requests
*	Parameters : none
*	Return Value : 0 on success and non-zero on failure
*****************************************************************************/
void request_handler(void)
{
    int rv=0;
    fhandle_t fh;
    struct pollfd pfd;
    pthread_t pid;
    fqueue_element_t *qe;

    //initial sem
    rv = sem_init(&idle_core_num, 1, IDLE_CORE_NUM);
    // Initialize the tail queue
    TAILQ_INIT(&tailq_head);

    // Create input fqueue
    do {
    	rv = create_fqueue(input_queue_filename, input_queuefile_maxsize);
	if (rv) {
	    fprintf(stdout, "%s:%d Unable to create input queuefile [%s], rv=%d\n",
	    	    __FUNCTION__, __LINE__, input_queue_filename, rv);
	    sleep(10);
	}
    } while (rv);

    // Open input fqueue
    do {
    	fh = open_queue_fqueue(input_queue_filename);
	if (fh < 0) {
	    fprintf(stdout, "%s:%d Unable to open input queuefile [%s]\n",
	    	__FUNCTION__, __LINE__, input_queue_filename);
	    sleep(10);
	}
    } while (fh < 0);

    // Process input fqueue elements
    while (1) {

	//allocate memory for qe of one task
	qe = (fqueue_element_t *) malloc(sizeof(fqueue_element_t));

	// Remove each queue element and forward it to the processor
    	rv = dequeue_fqueue_fh_el(fh, qe, 1);

	if (rv == -1) {
	    if (qe) {
		free(qe);
	    }
	    continue;
	} else if (rv) {
	    fprintf(stdout, "%s:%d dequeue_fqueue_fh_el(%s) failed, rv=%d\n",
	    	__FUNCTION__, __LINE__, input_queue_filename, rv);
	    poll(&pfd, 0, 100);
	    if (qe) {
		free(qe);
	    }
	    continue;
	}

	// Schedule queue element for parsing/processing
	pthread_create(&pid/*[parral_task]*/, NULL, (void*)process_qelement,(void*)qe);
    }//while
    sem_destroy(&idle_core_num);
}

/*****************************************************************************
*	Function : process_qelement()
*	Purpose : Extract request data from fqueue_element_t and send/process it.
*	Parameters : fqueue_element_t *
*	Return Value :  void
*****************************************************************************/
void  process_qelement(void *qe)
{
    int rv;

    const char *name;
    int name_len;
    const char *uriData;
    int uriLen;
    const char *vpeId;
    int vpeIdLen;
    char *mapBuf;

    mime_header_t httpreq;
    fqueue_element_t *qe_t;
    qe_t = (fqueue_element_t *) qe;

    rv = init_http_header(&httpreq, 0, 0);
    if (rv) {
    	DBG("%s:%d init_http_header() failed, rv=%d", __FUNCTION__, __LINE__, rv);
	rv = 1;
	goto exit;
    }

    rv = get_attr_fqueue_element(qe_t, T_ATTR_URI, 0, &name, &name_len, &uriData, &uriLen);
    if (rv) {
    	DBG("%s:%d get_attr_fqueue_element(T_ATTR_URI) failed, rv=%d", __FUNCTION__, __LINE__, rv);
	rv = 3;
	goto exit;
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "vpe_identifier", 14, &vpeId, &vpeIdLen);
    if (!rv) {
	/* Dont need to deserialize since I have only one value pair. In
	future it may be better to use http_header and marshall multiple name value pairs
	rv = deserialize(hostData, hostLen, &httpreq, 0, 0);
	if (rv) {
	    DBG("%s:%d deserialize() failed, rv=%d", __FUNCTION__, __LINE__, rv);
	    rv = 4;
	    goto exit;
	}
	*/
    } else {
    	DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"http_header\" failed, rv=%d",
	    __FUNCTION__, __LINE__, rv);
    }

    mapBuf = (char *)alloca(VPE_IDENTIFIER_MAX_LEN);
    memset(mapBuf, 0, VPE_IDENTIFIER_MAX_LEN);
    snprintf(mapBuf, VPE_IDENTIFIER_MAX_LEN, "%d", VPE_IDENTIFIER_SMOOTHFLOW);

    if ( !strcmp(vpeId, mapBuf) ) {
	// Initiate Smoothflow Processing
	process_sf_qelement(qe_t);

    } else {
	memset(mapBuf, 0, VPE_IDENTIFIER_MAX_LEN);
	snprintf(mapBuf, VPE_IDENTIFIER_MAX_LEN, "%d", VPE_IDENTIFIER_RTSP);
	if ( !strcmp(vpeId, mapBuf) ) {
	    // Initiate RTSP Processing
	    process_rtsp_qelement(qe_t);

	} else {
	    memset(mapBuf, 0, VPE_IDENTIFIER_MAX_LEN);
	    snprintf(mapBuf, VPE_IDENTIFIER_MAX_LEN, "%d", VPE_IDENTIFIER_GENERAL_TRANSCODE);
	    if (!strcmp(vpeId, mapBuf)) {
		// Initiate Generic Transcode Processing
		process_generic_transcode_qelement(qe_t);

	    } else {
		memset(mapBuf, 0, VPE_IDENTIFIER_MAX_LEN);
		snprintf(mapBuf, VPE_IDENTIFIER_MAX_LEN, "%d", VPE_IDENTIFIER_YOUTUBE_TRANSCODE);
		if (!strcmp(vpeId, mapBuf)) {
		    // Initiate youtube Transcode Processing
		    process_youtube_transcode_qelement(qe_t);
		} else {
		    // add new things to support new task
		}
	    }

	}
    }


exit:
    shutdown_http_header(&httpreq);
    if (qe_t) {
	free(qe_t);
    }
    pthread_exit(NULL);
}


/*****************************************************************************
*	Function : process_sf_qelement()
*	Purpose : Extract request data from fqueue_element_t and send/process it.
*	Parameters : fqueue_element_t *
*	Return Value :  void
*****************************************************************************/
int process_sf_qelement(const fqueue_element_t *qe_t)
{
    int rv;

    const char *name;
    int name_len;
    const char *uriData;
    int uriLen;
    const char *hostData;
    int hostLen;
    const char *stateQuery;
    int stateQueryLen;

    mime_header_t httpreq;

    char pythoncmd[] = "python /nkn/plugins/pyVpePublish.py";
    char *cmd;
    int cmd_len = 0;
    vpe_task_attr_t *cur_task_attr, *temp_task_1, *temp_task_2;
    int do_process = 1;

    cur_task_attr =(vpe_task_attr_t *) malloc(sizeof(vpe_task_attr_t));

    rv = init_http_header(&httpreq, 0, 0);
    if (rv) {
    	DBG("%s:%d init_http_header() failed, rv=%d", __FUNCTION__, __LINE__, rv);
	rv = 1;
	goto exit;
    }

    rv = get_attr_fqueue_element(qe_t, T_ATTR_URI, 0, &name, &name_len, &uriData, &uriLen);
    if (rv) {
    	DBG("%s:%d get_attr_fqueue_element(T_ATTR_URI) failed, rv=%d", __FUNCTION__, __LINE__, rv);
	rv = 3;
	goto exit;
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "host_name", 9, &hostData, &hostLen);
    if (!rv) {
	/* Dont need to deserialize since I have only one value pair. In
	future it may be better to use http_header and marshall multiple name value pairs
	rv = deserialize(hostData, hostLen, &httpreq, 0, 0);
	if (rv) {
	    DBG("%s:%d deserialize() failed, rv=%d", __FUNCTION__, __LINE__, rv);
	    rv = 4;
	    goto exit;
	}
	*/
    } else {
    	DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"http_header\" failed, rv=%d",
	    __FUNCTION__, __LINE__, rv);
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "uri_name", 8, &uriData, &uriLen);
    if (!rv) {
        // No deserialize right now
    } else {
        DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"uri_name\" failed, rv=%d",
            __FUNCTION__, __LINE__, rv);
    }


    rv = get_nvattr_fqueue_element_by_name(qe_t, "state_query_param", 17, &stateQuery, &stateQueryLen);
    if (!rv) {
	// No deserialize right now
    } else {
        DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"state_query_param\" failed, rv=%d",
            __FUNCTION__, __LINE__, rv);
    }

    //generate the cksum
    generate_vpe_task_attri(VPE_IDENTIFIER_SMOOTHFLOW, uriData, uriLen, cur_task_attr);

    // check whether this task is already in processing tailq
    pthread_mutex_lock(&task_queue_lock);

    if (TAILQ_EMPTY(&tailq_head)) { // empty tailq
	TAILQ_INSERT_TAIL(&tailq_head, cur_task_attr, entries);
	do_process = 1;
    } else { //not empty tailq
	do_process = 1;

	TAILQ_FOREACH(temp_task_1, &tailq_head, entries) {
	    if((temp_task_1->task_type == cur_task_attr->task_type) &&
	       (temp_task_1->cksum_remapped_uri == cur_task_attr->cksum_remapped_uri)) {
		do_process = 0; //found the task in already in processing tailq
		break;
	    }
	}
	if (do_process) {//task is not in tailq, insert this task
	    TAILQ_INSERT_TAIL(&tailq_head, cur_task_attr, entries);
	}
    }
    pthread_mutex_unlock(&task_queue_lock);

    //trigger the python script
    if (do_process) {

	while (sem_wait(&idle_core_num) == -1 && errno == EINTR);

	cmd_len = strlen(pythoncmd) + hostLen        + 3 + ceil(log10(hostLen))       + 3
	                            + uriLen         + 3 + ceil(log10(uriLen))        + 3
	                            + stateQueryLen  + 3 + ceil(log10(stateQueryLen)) + 3;
	cmd = (char*)malloc(cmd_len);
	memset(cmd, 0, cmd_len);
	sprintf(cmd,"%s '%s' '%d' '%s' '%d' '%s' '%d'",pythoncmd, hostData, hostLen,
		                                       uriData, uriLen, stateQuery, stateQueryLen);

	system(cmd);
	free(cmd);
	sem_post(&idle_core_num);
	//	pyVpePublish(hostData, hostLen, uriData, uriLen, stateQuery, stateQueryLen);
    }

    //task finished, remove from tailq
    if (do_process){
	pthread_mutex_lock(&task_queue_lock);

	for (temp_task_1 = TAILQ_FIRST(&tailq_head); temp_task_1 != NULL; temp_task_1 = temp_task_2) {
	    temp_task_2 = TAILQ_NEXT(temp_task_1, entries);
	    if ((temp_task_1->task_type == cur_task_attr->task_type) &&
		(temp_task_1->cksum_remapped_uri == cur_task_attr->cksum_remapped_uri)) {
		TAILQ_REMOVE(&tailq_head, temp_task_1, entries);
		break;
	    }
	}
	pthread_mutex_unlock(&task_queue_lock);
    }

exit:
    if (cur_task_attr) {
	free(cur_task_attr);
    }
    shutdown_http_header(&httpreq);
    return rv;
}


/*****************************************************************************
*	Function : process_rtsp_qelement()
*	Purpose : Extract request data from fqueue_element_t and send/process it.
*	Parameters : fqueue_element_t *
*	Return Value :  void
*****************************************************************************/
int process_rtsp_qelement(const fqueue_element_t *qe_t)
{
    int rv;

    const char *name;
    int name_len;
    const char *uriData;
    int uriLen;
    const char *hostData;
    int hostLen;
    const char *stateQuery;
    int stateQueryLen;
    char filename[200];
    mime_header_t httpreq;

    vpe_task_attr_t *cur_task_attr, *temp_task_1, *temp_task_2;
    int do_process = 1;

    cur_task_attr =(vpe_task_attr_t *) malloc(sizeof(vpe_task_attr_t));

    rv = init_http_header(&httpreq, 0, 0);
    if (rv) {
    	DBG("%s:%d init_http_header() failed, rv=%d", __FUNCTION__, __LINE__, rv);
	rv = 1;
	goto exit;
    }

    rv = get_attr_fqueue_element(qe_t, T_ATTR_URI, 0, &name, &name_len, &uriData, &uriLen);
    if (rv) {
    	DBG("%s:%d get_attr_fqueue_element(T_ATTR_URI) failed, rv=%d", __FUNCTION__, __LINE__, rv);
	rv = 3;
	goto exit;
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "host_name", 9, &hostData, &hostLen);
    if (!rv) {
	/* Dont need to deserialize since I have only one value pair. In
	future it may be better to use http_header and marshall multiple name value pairs
	rv = deserialize(hostData, hostLen, &httpreq, 0, 0);
	if (rv) {
	    DBG("%s:%d deserialize() failed, rv=%d", __FUNCTION__, __LINE__, rv);
	    rv = 4;
	    goto exit;
	}
	*/
    } else {
    	DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"http_header\" failed, rv=%d",
	    __FUNCTION__, __LINE__, rv);
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "uri_name", 8, &uriData, &uriLen);
    if (!rv) {
        // No deserialize right now
    } else {
        DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"uri_name\" failed, rv=%d",
            __FUNCTION__, __LINE__, rv);
    }

    generate_vpe_task_attri(VPE_IDENTIFIER_RTSP, uriData, uriLen, cur_task_attr);

    // check whether this task is already in processing tailq
    pthread_mutex_lock(&task_queue_lock);

    if (TAILQ_EMPTY(&tailq_head)) { // empty tailq
	TAILQ_INSERT_TAIL(&tailq_head, cur_task_attr, entries);
	do_process = 1;
    } else { //not empty tailq
	do_process = 1;

	TAILQ_FOREACH(temp_task_1, &tailq_head, entries) {
	    if((temp_task_1->task_type == cur_task_attr->task_type) &&
	       (temp_task_1->cksum_remapped_uri == cur_task_attr->cksum_remapped_uri)) {
		do_process = 0; //found the task in already in processing tailq
		break;
	    }
	}
	if (do_process) {//task is not in tailq, insert this task
	    TAILQ_INSERT_TAIL(&tailq_head, cur_task_attr, entries);
	}
    }

    pthread_mutex_unlock(&task_queue_lock);

    //strart the conversion task
    if(do_process){

	while (sem_wait(&idle_core_num) == -1 && errno == EINTR);

	strcpy(filename,uriData);
	if(read_cache_translate(filename,(char*)hostData) == VPE_ERROR){
	    DBG("NKN FILE CONVERSION FAILED, rv=%d",VPE_ERROR);
	    sem_post(&idle_core_num);
	    goto exit;
	}
	sem_post(&idle_core_num);
    }

    //task finished, remove from tailq
    if (do_process){
	pthread_mutex_lock(&task_queue_lock);

	for (temp_task_1 = TAILQ_FIRST(&tailq_head); temp_task_1 != NULL; temp_task_1 = temp_task_2) {
	    temp_task_2 = TAILQ_NEXT(temp_task_1, entries);
	    if ((temp_task_1->task_type == cur_task_attr->task_type) &&
		(temp_task_1->cksum_remapped_uri == cur_task_attr->cksum_remapped_uri)) {
		TAILQ_REMOVE(&tailq_head, temp_task_1, entries);
		break;
	    }
	}
	pthread_mutex_unlock(&task_queue_lock);
    }

exit:
    if (cur_task_attr) {
	free(cur_task_attr);
    }
    shutdown_http_header(&httpreq);
    return rv;
}


/*****************************************************************************
*	Function : pid_file_exist()
*	Purpose : Check for presence of pid file
*	Parameters : void
*	Return Value : 1 if exists else 0
*****************************************************************************/
int pid_file_exist(void)
{
    FILE	*fpPidFile;
    fpPidFile = fopen(VPEMGR_PID_FILE, "r");
    if (fpPidFile != NULL) {
    	fclose(fpPidFile);
    	return 1;
    } else {
    	return 0;
    }
}

/*****************************************************************************
*	Function : create_pid_file()
*	Purpose : create the pid file to track the process
*	Parameters : void
*	Return Value : void
*****************************************************************************/
void create_pid_file(void)
{
    FILE	*fpPidFile;
    pid_t	pidSelf;

    /* Get the pid */
    pidSelf = getpid();

    /* Write this to the pid file */
    fpPidFile = fopen(VPEMGR_PID_FILE, "w");
    if (fpPidFile != NULL) {
    	fprintf(fpPidFile, "%d", pidSelf);
    	fclose(fpPidFile);
    } else {
	fprintf(stdout, "error : Failed to create the pid file <%s>\n", VPEMGR_PID_FILE);
	exit(2);
    }
    return;
}

/*****************************************************************************
*	Function : exit_vpemgr()
*	Purpose : function called when app is exiting. Primary activity is to
*		remove the pid file
*	Parameters : cpProgName - the argv[0] value
*	Return Value : void
*****************************************************************************/
void exit_vpemgr(void)
{
    /* unlink the pid file */
    unlink(VPEMGR_PID_FILE);
    return;
}

/*****************************************************************************
*	Function : termination_handler()
*	Purpose : signal handler for SIGTERM, SIGINT and SIGHUP
*	Parameters : int signum
*	Return Value : void
*****************************************************************************/
void termination_handler(int nSignum)
{
    if (nSignum == SIGUSR1) {
	vpemgr_debug_output = vpemgr_debug_output ? 0 : 1;
	fprintf(stdout, "VPEMGR debug logging [%s]\n", vpemgr_debug_output ?  "enabled" : "disabled");
	fflush(stdout);
	return;
    }

    /* Call the exit routine */
    exit_vpemgr();
    exit(3);
}

/*****************************************************************************
*	Function : daemonize()
*	Purpose : Create daemon process
*	Parameters : none
*	Return Value : void
*****************************************************************************/
void daemonize(void)
{
    if (fork() != 0) {
    	/* Terminate parent */
    	exit(0);
    }
    if (setsid() == -1) {
    	exit(0);
    }
    signal(SIGHUP, SIG_IGN);
    if (fork() != 0) {
    	/* Terminate parent */
    	exit(0);
    }

    /* Do not chdir when this processis managed by the PM
     * if (chdir("/") != 0) exit(0);
     */
}

/*****************************************************************************
*	Function : print_usage()
*	Purpose : print the usage of this utility namely command line options
*	Parameters : cpProgName - the argv[0] value
*	Return Value : void
*****************************************************************************/
void print_usage(char *cpProgName)
{
    fprintf(stdout, "usage : %s [-D-I:-h-i:-v]\n", cpProgName);
    fprintf(stdout, "\t -D : Do not create daemon process \n");
    fprintf(stdout, "\t -I : Input queue filename (default: %s)\n", input_queue_filename);
    fprintf(stdout, "\t -h : Help\n");
    fprintf(stdout, "\t -i : Input queue maxsize (default: %d)\n", input_queuefile_maxsize);
    fprintf(stdout, "\t -v : verbose output (default: no)\n");
    return;
}

/*****************************************************************************
*	Function : getoptions()
*	Purpose : Parse command line arguments
*	Parameters : argc, argv - passed as is from main
*	Return Value : 0 on success and non-zero on failure
*****************************************************************************/
int getoptions(int argc, char *argv[])
{
    char 	chTemp;

    input_queue_filename = VPEMGR_QUEUE_FILE;

    /* Using getopt get the parameters */
    while ((chTemp = getopt(argc, argv, "DI:hi:v")) != -1) {
    	switch (chTemp) {
	case 'D':
	    do_not_create_daemon = 1;
	    break;
	case 'I':
	    input_queue_filename = optarg;
	    break;
	case 'i':
	    input_queuefile_maxsize = atoi(optarg);
	    break;
	case 'v':
	    vpemgr_debug_output = 1;
	    break;
	case 'h':
	case '?':
	    print_usage(argv[0]) ;
	    return 1;
	    break;
	default:
	    abort ();
         }
    }
    return 0;
}

/* Start of BZ 8382 */

/*
 * Function : generate_vpe_task_attri
 * Purpose : generate the attribute(task_type and cksum of uri) for current vpe task
 * Parameters : task_type, uri_data, uri_len, cur_task_attr
 * Return Value : 0 on success
 *
 */
int
generate_vpe_task_attri(int task_type, const char *uri_data, int uri_len, vpe_task_attr_t* cur_task_attr)
{

    cur_task_attr->task_type = task_type;

    cur_task_attr->cksum_remapped_uri = compute_cksum(uri_data, uri_len);

    return 0;
}



/*
 * Function : compute_cksum
 * Purpose : compute the cksum of a string in char *p with len stringlength
 * Parameters :  string in *p, stringlength  len
 * Return Value : cksum value
 *
 */
static u_int32_t
compute_cksum(const char *p, int len)
{
    int n;
    u_int32_t cksum = 0;

    for (n = 0; n < len; n++) {
	cksum += p[n];
	if (cksum & 1) {
	    cksum = (cksum >> 1) + 0x8000;
	} else {
	    cksum = (cksum >> 1);
	}
	cksum += p[n];
	cksum &= 0xffff;
    }
    return cksum;
}

/*
 * Function: findStr
 * Purpose: Search for a substring within a string
 * Return values:
 * pos >= 0 : Match; -1: No match
 */
static int
findString (const char *pcMainStr, const char *pcSubStr, int length)
{
    int t;
    const char *p, *p2;

    for(t=0; t<length; t++) {
	p = &pcMainStr[t];
	p2 = pcSubStr;

	while(*p2 && *p2==*p) {
	    p++;
	    p2++;
	}
	if(!*p2) return t; // return pos of match
    }
    return -1; // no match found
}

/*
 * Function : process_generic_transcode_qelement()
 * Purpose : Extract request data from fqueue_element_t and send/process it.
 *           this function extracts the transcode parameter
 * Parameters : fqueue_element_t *
 * Return Value :  void
 *
 */
int process_generic_transcode_qelement(const fqueue_element_t *qe_t)
{
    int rv;

    const char *name;
    int name_len;
    const char *uriData;
    int uriLen;
    const char *hostData;
    int hostLen;
    const char *containerType;
    int containerTypeLen;
    const char *transRatioData;
    /*
       transcodeData will provide the reduce bitrate transcode ratio,for example
       "10%", "20%", or "30%", means the bitrate reduces 10%, 20%, or 30%
    */
    int transRatioLen;
    const char *transComplexData;
    int transComplexLen;
    mime_header_t httpreq;

    char pythoncmd[] = "python /nkn/plugins/pyGenericTranscode.py";
    char *cmd;
    int cmd_len = 0;
    int remapped_uri_len;
    vpe_task_attr_t *cur_task_attr, *temp_task_1, *temp_task_2;
    int do_process = 1;

    cur_task_attr =(vpe_task_attr_t *) malloc(sizeof(vpe_task_attr_t));

    rv = init_http_header(&httpreq, 0, 0);
    if (rv) {
	DBG("%s:%d init_http_header() failed, rv=%d", __FUNCTION__, __LINE__, rv);
	rv = 1;
	goto exit;
    }

    rv = get_attr_fqueue_element(qe_t, T_ATTR_URI, 0, &name, &name_len, &uriData, &uriLen);
    if (rv) {
	DBG("%s:%d get_attr_fqueue_element(T_ATTR_URI) failed, rv=%d", __FUNCTION__, __LINE__, rv);
	rv = 3;
	goto exit;
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "host_name", 9, &hostData, &hostLen);
    if (!rv) {
	/* Dont need to deserialize since I have only one value pair. In
	   future it may be better to use http_header and marshall multiple name value pairs
	   rv = deserialize(hostData, hostLen, &httpreq, 0, 0);
	   if (rv) {
	       DBG("%s:%d deserialize() failed, rv=%d", __FUNCTION__, __LINE__, rv);
	       rv = 4;
	       goto exit;
	   }
	*/
    } else {
	DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"http_header\" failed, rv=%d",
	    __FUNCTION__, __LINE__, rv);
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "uri_name", 8, &uriData, &uriLen);
    if (!rv) {
	// No deserialize right now
    } else {
	DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"uri_name\" failed, rv=%d",
	    __FUNCTION__, __LINE__, rv);
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "container_type", 14, &containerType, &containerTypeLen);
    if (!rv) {
	// No deserialize right now
    } else {
	DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"container_type\" failed, rv=%d",
	    __FUNCTION__, __LINE__, rv);
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "transcode_ratio_param", 21, &transRatioData, &transRatioLen);
    if (!rv) {
	// No deserialize right now
    } else {
	DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"transcode_ratio_param\" failed, rv=%d",
	    __FUNCTION__, __LINE__, rv);
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "transcode_complex_param", 23, &transComplexData, &transComplexLen);
    if (!rv) {
	// No deserialize right now
    } else {
	DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"transcode_complex_param\" failed, rv=%d",
	    __FUNCTION__, __LINE__, rv);
    }

    if ( (remapped_uri_len = findString(uriData, "?", uriLen)) == -1 ) {
	remapped_uri_len = uriLen;
	printf("\nURL does not contain any query params\n");
    }

    generate_vpe_task_attri(VPE_IDENTIFIER_GENERAL_TRANSCODE, uriData, remapped_uri_len, cur_task_attr);

    // check whether this task is already in processing tailq
    pthread_mutex_lock(&task_queue_lock);

    if (TAILQ_EMPTY(&tailq_head)) { // empty tailq
	TAILQ_INSERT_TAIL(&tailq_head, cur_task_attr, entries);
	do_process = 1;
    } else { //not empty tailq
	do_process = 1;

	TAILQ_FOREACH(temp_task_1, &tailq_head, entries) {
	    if((temp_task_1->task_type == cur_task_attr->task_type) &&
	       (temp_task_1->cksum_remapped_uri == cur_task_attr->cksum_remapped_uri)) {
		do_process = 0; // found the task in already in processing tailq
		break;
	    }
	}
	if (do_process) {// task is not in tailq, insert this task
	    TAILQ_INSERT_TAIL(&tailq_head, cur_task_attr, entries);
	}
    }

    pthread_mutex_unlock(&task_queue_lock);

    // trigger the generic transcoder python script
    if(do_process) {
	while (sem_wait(&idle_core_num) == -1 && errno == EINTR);

	cmd_len = strlen(pythoncmd) + hostLen          + 3 + ceil(log10(hostLen))   + 3
	                            + uriLen           + 3 + ceil(log10(uriLen))    + 3
	    	                    + containerTypeLen + 3 + transRatioLen          + 3
	                            + transComplexLen  + 3;
	cmd = (char*)malloc(cmd_len);
	memset(cmd, 0, cmd_len);
	sprintf(cmd, "%s '%s' '%d' '%s' '%d' '%s' '%s' '%s'", pythoncmd, hostData, hostLen,
		                                              uriData, uriLen, containerType,
		                                              transRatioData, transComplexData);

	system(cmd); //replace the pyVpeGenericTranscode
	free(cmd);
	sem_post(&idle_core_num);

    }

    if (do_process){ //task finished, remove from tailq

	pthread_mutex_lock(&task_queue_lock);

	for (temp_task_1 = TAILQ_FIRST(&tailq_head); temp_task_1 != NULL; temp_task_1 = temp_task_2) {
	    temp_task_2 = TAILQ_NEXT(temp_task_1, entries);
	    if ((temp_task_1->task_type == cur_task_attr->task_type) &&
		(temp_task_1->cksum_remapped_uri == cur_task_attr->cksum_remapped_uri)) {
		TAILQ_REMOVE(&tailq_head, temp_task_1, entries);
		break;
	    }
	}

	pthread_mutex_unlock(&task_queue_lock);
    }

 exit:
    if (cur_task_attr) {
	free(cur_task_attr);
    }
    shutdown_http_header(&httpreq);
    return rv;
}

/*
 * Function : process_youtube_transcode_qelement()
 * Purpose : Extract request data from fqueue_element_t and send/process it.
 *           this function extracts the transcode parameter
 * Parameters : fqueue_element_t *
 * Return Value :  void
 *
 */
int process_youtube_transcode_qelement(const fqueue_element_t *qe_t)
{
    int rv;

    const char *name;
    int name_len;
    const char *uriData;
    int uriLen;
    const char *video_id_uri_query;
    int video_id_uri_queryLen;
    const char *hostData;
    int hostLen;
    const char *format_tag;
    int format_tagLen;
    const char *remapUriData;
    int remapUriLen;
    const char *nsuriPrefix;
    int nsuriPrefixLen;
    /*
    transcodeData will provide the reduce bitrate transcode ratio,for example
    "10%", "20%", or "30%", means the bitrate reduces 10%, 20%, or 30%
    */
    const char *transRatioData;
    int transRatioLen;
    const char *transComplexData;
    int transComplexLen;
    mime_header_t httpreq;
    char pythoncmd[] = "python /nkn/plugins/pyYoutubeTranscode.py";
    char *cmd;
    int cmd_len = 0;
    vpe_task_attr_t *cur_task_attr, *temp_task_1, *temp_task_2;
    int do_process = 1;

    cur_task_attr =(vpe_task_attr_t *) malloc(sizeof(vpe_task_attr_t));

    rv = init_http_header(&httpreq, 0, 0);
    if (rv) {
	DBG("%s:%d init_http_header() failed, rv=%d", __FUNCTION__, __LINE__, rv);
	rv = 1;
	goto exit;
    }

    rv = get_attr_fqueue_element(qe_t, T_ATTR_URI, 0, &name, &name_len, &uriData, &uriLen);
    if (rv) {
	DBG("%s:%d get_attr_fqueue_element(T_ATTR_URI) failed, rv=%d", __FUNCTION__, __LINE__, rv);
	rv = 3;
	goto exit;
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "host_name", 9, &hostData, &hostLen);
    if (!rv) {
	/* Dont need to deserialize since I have only one value pair. In
	   future it may be better to use http_header and marshall multiple name value pairs
	   rv = deserialize(hostData, hostLen, &httpreq, 0, 0);
	   if (rv) {
	       DBG("%s:%d deserialize() failed, rv=%d", __FUNCTION__, __LINE__, rv);
	       rv = 4;
	       goto exit;
	   }
	*/
    } else {
	DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"http_header\" failed, rv=%d",
	    __FUNCTION__, __LINE__, rv);
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "video_id_uri_query", 18, &video_id_uri_query, &video_id_uri_queryLen);
    if (!rv) {
	// No deserialize right now
    } else {
	DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"video_id_uri_query\" failed, rv=%d",
	    __FUNCTION__, __LINE__, rv);
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "format_tag", 10, &format_tag, &format_tagLen);
    if (!rv) {
	// No deserialize right now
    } else {
	DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"seek_name\" failed, rv=%d",
	    __FUNCTION__, __LINE__, rv);
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "internal_cache_name", 19, &remapUriData, &remapUriLen);
    if (!rv) {
	// No deserialize right now
    } else {
	DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"internal_cache_name\" failed, rv=%d",
	    __FUNCTION__, __LINE__, rv);
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "ns_uri_prefix", 13, &nsuriPrefix, &nsuriPrefixLen);
    if (!rv) {
	// No deserialize right now
    } else {
	DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"ns_uri_prefix\" failed, rv=%d",
	    __FUNCTION__, __LINE__, rv);
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "transcode_ratio_param", 21, &transRatioData, &transRatioLen);
    if (!rv) {
	// No deserialize right now
    } else {
	DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"transcode_ratio_param\" failed, rv=%d",
	    __FUNCTION__, __LINE__, rv);
    }

    rv = get_nvattr_fqueue_element_by_name(qe_t, "transcode_complex_param", 23, &transComplexData, &transComplexLen);
    if (!rv) {
	// No deserialize right now
    } else {
	DBG("%s:%d get_nvattr_fqueue_element_by_name() for \"transcode_complex_param\" failed, rv=%d",
	    __FUNCTION__, __LINE__, rv);
    }

    generate_vpe_task_attri(VPE_IDENTIFIER_YOUTUBE_TRANSCODE, remapUriData, remapUriLen, cur_task_attr);

    // check whether this task is already in processing tailq
    pthread_mutex_lock(&task_queue_lock);

    if (TAILQ_EMPTY(&tailq_head)) { // empty tailq
	TAILQ_INSERT_TAIL(&tailq_head, cur_task_attr, entries);
	do_process = 1;
    } else { //not empty tailq
	do_process = 1;

	TAILQ_FOREACH(temp_task_1, &tailq_head, entries) {
	    if((temp_task_1->task_type == cur_task_attr->task_type) &&
	       (temp_task_1->cksum_remapped_uri == cur_task_attr->cksum_remapped_uri)) {
		do_process = 0; //found the task in already in processing tailq
		break;
	    }
	}

	if (do_process) {//task is not in tailq, insert this task
	    TAILQ_INSERT_TAIL(&tailq_head, cur_task_attr, entries);
	}
    }

    pthread_mutex_unlock(&task_queue_lock);

    if (do_process) {
	// sem_wait(&idle_core_num); //fix the GDB and sem_wait issue
	while (sem_wait(&idle_core_num) == -1 && errno == EINTR);

	cmd_len = strlen(pythoncmd) + hostLen               + 3 + ceil(log10(hostLen))               + 3
	                            + video_id_uri_queryLen + 3 + ceil(log10(video_id_uri_queryLen)) + 3
	                            + format_tagLen         + 3 + ceil(log10(format_tagLen))         + 3
	                            + remapUriLen           + 3 + ceil(log10(remapUriLen))           + 3
	                            + nsuriPrefixLen        + 3 + ceil(log10(nsuriPrefixLen))        + 3
	                            + transRatioLen         + 3 + transComplexLen+ 3;
	cmd =(char*)malloc(cmd_len);
	memset(cmd, 0, cmd_len);
	sprintf(cmd,"%s '%s' '%d' '%s' '%d' '%s' '%d' '%s' '%d' '%s' '%d' '%s' '%s'",
		pythoncmd, hostData, hostLen, video_id_uri_query, video_id_uri_queryLen,
		format_tag, format_tagLen, remapUriData, remapUriLen,
		nsuriPrefix, nsuriPrefixLen, transRatioData, transComplexData);

	system(cmd);
	free(cmd);
	sem_post(&idle_core_num);
    }

    if (do_process){ //task finished, remove from tailq
	pthread_mutex_lock(&task_queue_lock);

	for (temp_task_1 = TAILQ_FIRST(&tailq_head); temp_task_1 != NULL; temp_task_1 = temp_task_2) {
	    temp_task_2 = TAILQ_NEXT(temp_task_1, entries);
	    if ((temp_task_1->task_type == cur_task_attr->task_type) &&
		(temp_task_1->cksum_remapped_uri == cur_task_attr->cksum_remapped_uri)) {
		TAILQ_REMOVE(&tailq_head, temp_task_1, entries);
		break;
	    }
	}

	pthread_mutex_unlock(&task_queue_lock);
    }

 exit:
    if (cur_task_attr) {
	free(cur_task_attr);
    }
    shutdown_http_header(&httpreq);
    return rv;
}

/* End of BZ 8382 */


void rtsp_cfg_so_callback (char * s)
{
}


/*
 * End of nkn_vpemgr.c
 */

/* Function is called in libnkn_memalloc, but we don't care about this. */
//extern int nkn_mon_add(const char *name_tmp, char *instance, void *obj,
//		       uint32_t size);
int nkn_mon_add(const char *name_tmp __attribute((unused)),
		const char *instance __attribute((unused)),
		void *obj __attribute((unused)),
		uint32_t size __attribute((unused)))
{
	return 0;
}
