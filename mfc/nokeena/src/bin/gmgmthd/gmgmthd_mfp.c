/*
 *
 * Filename:  gmgmthd_mfp.c
 * Date:      2011/03/09
 * Author:    Varsha Rajagopalan
 *
 * (C) Copyright 2011 Juniper Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#include "time.h"
#include "md_client.h"
#include "mdc_wrapper.h"
#include "gmgmthd_main.h"
#include "random.h"
#include "proc_utils.h"
#include "file_utils.h"
#include "clish_module.h"
#include "string.h"
//#include <sys/inotify.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpathInternals.h>
#include <libxml/xmlschemastypes.h>
#include "nkn_mgmt_defs.h"
#if 0
#define INOTIFY_EVENT_SIZE sizeof(struct inotify_event)
#define EVENT_BUFLEN ((N_FD * INOTIFY_EVENT_SIZE) + 1024)
#define N_FD 1024
int32_t dir_watch_fd, dir_watch_wd;
#endif

#define MAX_CHAR 256
/* ------------------------------------------------------------------------- */
/** Local prototypes & definitions
 */
static const char *mfp_status_node_binding = "/nkn/nvsd/mfp/status-time";
typedef struct mfp_status_timer_data_st {
    int                     index;          /* Index holder : Never reset or modify */
    lew_event               *poll_event;
    lew_event_handler       callback;
    void                    *context_data; /* Points to iteself */
    lt_dur_ms               poll_frequency;
} mfp_status_timer_data_t;

typedef struct mfp_watch_dir_st {
    int	 index;
    lew_event *poll_event;
    lew_event_handler callback;
    void *context_data;
    lt_dur_ms   poll_frequency;
//  int fail_count;
//  int inotify_watch_fd;
//  int inotify_watch_wd;
} mfp_watch_dir_t;

static mfp_watch_dir_t  g_watch_dir;
static mfp_status_timer_data_t  g_mfp_get_status;

/* 
 * MFP PMF generation  related functions
 */
int
mfp_generate_stream_pmf_file( const char *streamname, tstring **ret_output);

int
mfp_generate_encap_live_spts_media_profile( FILE *fp,
	const char *streamname, tstring **ret_output);

int
mfp_generate_encap_file_spts_media_profile( FILE *fp,
	const char *streamname, tstring **ret_output);

int
mfp_generate_encap_ssp_media_sbr( FILE *fp,
	const char *streamname, tstring **ret_output);

int
mfp_generate_encap_ssp_media_mbr( FILE *fp,
	const char *streamname, tstring **ret_output);

int
mfp_generate_output_parameter( FILE *fp,
	const char *streamname, tstring **ret_output);
int
mfp_check_if_valid_mount( tstring * t_storage, uint32 *mount_error, uint8_t *mount_type);

int
mfp_escape_sequence_space_in_address( tstring *t_address);

int
mfp_generate_encap_file_mp4( FILE *fp, const char *streamname,
	tstring **ret_output);

int
mfp_action_stop( const char *streamname, tstring **ret_output);
int
mfp_action_remove( const char *streamname, tstring **ret_output);
int
mfp_action_status( const char *streamname, tstring **ret_output);
int
mfp_action_restart( const char *streamname, tstring **ret_output);
/*
 * Getting the session status at regular intervals
 */

int
mfp_status_delete_poll_timer(mfp_status_timer_data_t *context);
int
mfp_status_context_init(void);
int
mfp_get_status( int32 *file_published);
int
mfp_poll_timeout_handler(int fd, short event_type,
	void *data, lew_context *ctxt);
int
mfp_set_poll_timer(mfp_status_timer_data_t *context);
static int
mfp_status_timer_context_allocate(lt_dur_ms  poll_frequency,
	mfp_status_timer_data_t **ret_context);
int
mfp_parse_status_file(const char *filename);
static int 
mfp_get_list_of_sessions(tstr_array **sess_names, tbool isfile);
int
mfp_config_handle_change(const bn_binding_array *arr, uint32 idx,
	                        bn_binding *binding, void *data);
#if 1
int
mfp_watch_dir_context_init(void);
int
mfp_directory_watch(int fd, short event_type,
	void *data, lew_context *ctxt);
int
mfp_set_directory_watch_timer(mfp_watch_dir_t *context);
#endif
/* ------------------------------------------------------------------------- */


int
mfp_generate_stream_pmf_file( const char *streamname,
				tstring **ret_output)
{
    int err = 0;
    char *node_name = NULL;
    char filename[256] ;
    char dest_filename[256] ;
    tstring *t_media_encap_type = NULL;
    FILE  *fp = NULL;
    char  date_str[64];
    char *ret_string = NULL;
    tstring *t_file_mfpd = NULL;
    tstring *t_live_mfpd = NULL;
    char session_name[MAX_CHAR];
    char session_value[MAX_CHAR];


    if(!streamname || ! ret_output) {
    	ret_string = smprintf("error: unable to find the stream-name\n");
    	goto bail;
    }

    time_t cur_time = time(NULL);
    struct tm *loctime = localtime(&cur_time);
    strftime(date_str, sizeof(date_str), "%Y%m%d_%H:%M:%S", loctime);
    
    /*Creating the unique name for session*/
    snprintf( session_name, sizeof(session_name), "/nkn/nvsd/mfp/config/%s/name",streamname);
    snprintf( session_value, sizeof(session_value), "%s%s", streamname, date_str);

    err = mdc_set_binding(mfp_mcc, NULL, NULL,
		    bsso_modify,bt_string, session_value,
		    session_name);
    bail_error(err);
    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap", streamname);
    bail_null(node_name);

    err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
	    			&t_media_encap_type, node_name, NULL);
    if( t_media_encap_type && ts_length(t_media_encap_type) > 0) {
	/*If the prefix is file then it is File Media Source*/
	if( (ts_cmp_prefix_str( t_media_encap_type, "file_", false)) == 0) {
	    /* File media source*/
	    /*BUG FIX: 6869
	     * Add a check to see if the file-mfpd deamon is running.If it is not
	     * running, start the deamon
	     */
	    err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
	    			&t_file_mfpd, "/pm/monitor/process/file_mfpd/state", NULL);
	    if(ts_equal_str(t_file_mfpd, "stopped", false)){
		lc_log_basic(LOG_NOTICE, _("Starting the mod-file-publisher\n"));
		err = mdc_send_action_with_bindings_str_va(
			mfp_mcc, NULL, NULL, "/pm/actions/restart_process", 1,
			"process_name", bt_string, "file_mfpd");
		bail_error(err);
		/*
		 * Adding sleep to allow the deamon to start and then publish the PMF file
		 */
		sleep(5);
	    }


	    /* Create a file media source PMF*/
	    snprintf( filename, sizeof(filename), "/var/log/file_%s.xml", session_value);
	    snprintf( dest_filename, sizeof(dest_filename), "/nkn/vpe/file/file_%s.xml", session_value);
	    fp = fopen( filename, "w");
	    if(!fp) {
		lc_log_basic(LOG_NOTICE, "Failed to open file:%s", filename);
		goto bail;
	    }
	    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<PubManifest version=\"1.0\" name=\"%s\">\r\n<FileMediaSource>\r\n", session_value);
	    if( ts_has_suffix_str( t_media_encap_type, "ssp_media_sbr", false)) {
		err = mfp_generate_encap_ssp_media_sbr(fp, streamname, ret_output);
		if(err) {
		    goto bail;
		}
	    }else if (ts_has_suffix_str( t_media_encap_type, "ssp_media_mbr", false)) {
		err = mfp_generate_encap_ssp_media_mbr(fp, streamname, ret_output);
		if(err) {
		    goto bail;
		}
	    }else if (ts_has_suffix_str( t_media_encap_type, "spts_media_profile", false)) {
		err = mfp_generate_encap_file_spts_media_profile(fp, streamname, ret_output);
		if(err) {
		    goto bail;
		}
	    }else if (ts_has_suffix_str( t_media_encap_type, "file_mp4", false)) {
		/*MP4 package*/
		err = mfp_generate_encap_file_mp4(fp, streamname, ret_output);
		if(err) {
		    goto bail;
		}
	    }else {
		/* Error: unknown encapsulation type specified*/
		ret_string = smprintf("Error:Unknown encap type specified\r\n");
		fclose(fp);
		fp = NULL;
		err = lf_remove_file(filename);
		goto bail;
	    }
	    fprintf(fp, "</FileMediaSource>\r\n");
	    /*Output stream parameters*/
	    err = mfp_generate_output_parameter(fp, streamname, ret_output);
	    if(err){
		    goto bail;
	    }
	    fprintf(fp, "</PubManifest>\r\n");
	    fclose(fp);
	    fp = NULL;

	    err = lf_copy_file( filename, dest_filename, fco_ensure_dest_dir);
	    bail_error(err);
	    err = lf_remove_file(filename);
	    bail_error(err);
	    ret_string = smprintf("Stream PMF created for session:%s\r\n", streamname); 
	    //ret_string = smprintf("Stream PMF created:%s\r\n",dest_filename); 


	} else if( (ts_cmp_prefix_str( t_media_encap_type, "live_", false)) == 0){
	    /* Live media source*/
	    /*BUG FIX: 6869
	     * Add a check to see if the live-mfpd deamon is running.If it is not
	     * running, start the deamon
	     */
	    err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
	    			&t_live_mfpd, "/pm/monitor/process/live_mfpd/state", NULL);
	    if(ts_equal_str(t_live_mfpd, "stopped", false)){
		lc_log_basic(LOG_NOTICE, _("Starting the mod-live-publisher\n"));
		err = mdc_send_action_with_bindings_str_va(
			mfp_mcc, NULL, NULL, "/pm/actions/restart_process", 1,
			"process_name", bt_string, "live_mfpd");
		bail_error(err);
		/*
		 * Adding sleep to allow the deamon to start and then publish the PMF file
		 */
		sleep(5);
	    }
	    /* Create a live media source PMF*/
	    snprintf( filename, sizeof(filename), "/var/log/live_%s.xml", session_value);
	    snprintf( dest_filename, sizeof(dest_filename), "/nkn/vpe/live/live_%s.xml", session_value);
	    fp = fopen( filename, "w");
	    if(!fp) {
		ret_string = smprintf("Failed to open file:%s\r\n", filename);
		goto bail;
	    }
	    err = fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<PubManifest version=\"1.0\" name=\"%s\">\r\n<LiveMediaSource>\r\n", session_value);
	    /*Input stream parameters*/
	    if (ts_has_suffix_str( t_media_encap_type, "spts_media_profile", false)) {
		err = mfp_generate_encap_live_spts_media_profile(fp, streamname, ret_output);
		if(err) {
		    goto bail;
		}
	    }else {
		/* Error: unknown encapsulation type specified*/
		ret_string = smprintf(" Error:Unknown encap type specified\r\n");
		goto bail;
	    }
	    fprintf(fp, "</LiveMediaSource>\r\n");
	    /*Output stream parameters*/
	    err = mfp_generate_output_parameter(fp, streamname, ret_output);
	    if(err){
		    goto bail;
	    }
	    fprintf(fp, "</PubManifest>\r\n");
	    fclose(fp);
	    fp = NULL;
	    err = lf_copy_file( filename, dest_filename, fco_ensure_dest_dir);
	    bail_error(err);
	    err = lf_remove_file(filename);
	    bail_error(err);
	    ret_string = smprintf("Stream PMF created for :%s\r\n", streamname); 
	} else {
	    /* unknown media source error message*/
	    ret_string = smprintf("Error: Unknow media source specified.Valid-FileMediaSource/LiveMediaSource\r\n");
	    err = 1;
	}
    } else {
	/* unknown streamname message*/
	ret_string = smprintf("Stream:%s doesnt exist or mediasource type isnt set\r\n", streamname);
	err = 1;
    }


bail:
    if(err) {
	if(fp) {
	    fclose(fp);
	    fp = NULL;
	    err = lf_remove_file(filename);
	    err = 1;
	}
    }
    if(ret_string) {
	 ts_new_str_takeover(ret_output, &ret_string, -1, -1);
    }
    safe_free(node_name);
    ts_free(&t_media_encap_type);
    ts_free(&t_file_mfpd);
    ts_free(&t_live_mfpd);
    return err;
}

int
mfp_generate_encap_ssp_media_sbr( FILE *fp, const char *streamname,
				tstring **ret_output)
{
    int err = 0;
    char *node_name = NULL;
    tstring *t_srvr_url = NULL;
    tstring *t_bitrate = NULL;
    tstring *t_client_url = NULL;
    tstring *t_media_type = NULL;
    tstring *t_media_url = NULL;
    tstring *t_protocol = NULL;
    char *ret_string = NULL;


    if(!fp || !streamname) {
	/*Error*/
	ret_string = smprintf("Error:File or streamname is missing \r\n");
	goto bail;
    }

    fprintf(fp," <SSP_SBR_Encapsulation>\r\n");
    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/file_ssp_media_sbr/svr-manifest-url", streamname);
    bail_null(node_name);
    err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
	    			&t_srvr_url, node_name, NULL);
    safe_free(node_name);
    if (t_srvr_url  && ts_length(t_srvr_url) > 0) {
	/*
	 * BUG: 6874
	 * Check if the protocol specified is NFS,
	 * if NFS , then append the "/nkn/nfs" to the server-manifest-url"
	 */
	    /*
	     * Commenting out escape sequence as on Mar 01-2011 -
		err = mfp_escape_sequence_space_in_address(t_srvr_url);
		bail_error(err);
	     */
	node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/file_ssp_media_sbr/protocol", streamname);
	err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		&t_protocol, node_name, NULL);
	safe_free(node_name);
	if(t_protocol && (ts_equal_str(t_protocol, "nfs", false))) {
	    fprintf(fp,"<SSP_Media>\r\n<Type>svr-manifest</Type>\r\n<Src>/nkn/nfs/%s</Src>\r\n",
			ts_str(t_srvr_url));
	}else if(t_protocol && (ts_equal_str(t_protocol, "http", false))) {
	    fprintf(fp,"<SSP_Media>\r\n<Type>svr-manifest</Type>\r\n<Src>http://%s</Src>\r\n", ts_str(t_srvr_url));
	}else {
	    fprintf(fp,"<SSP_Media>\r\n<Type>svr-manifest</Type>\r\n<Src>%s</Src>\r\n",
			ts_str(t_srvr_url));
	}
	err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		&t_bitrate, NULL,
		"/nkn/nvsd/mfp/config/%s/media-encap/file_ssp_media_sbr/svr-manifest-bitrate",
				streamname);
	bail_error(err);
#if 0
	/* optional parameter -bitrate*/
	if(t_bitrate && ts_length(t_bitrate) >0 ) {
	    fprintf(fp, "<BitRate>%s</BitRate>\r\n", ts_str(t_bitrate));
	}
#endif
	fprintf(fp, "</SSP_Media>\r\n");
    }else {
	ret_string = smprintf("Error: Server manifest URL is not specified\r\n");
	/*Send error message saying the server url is not set*/
	goto bail;
    }
#if 0
    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/file_ssp_media_sbr/client-manifest-url", streamname);
    bail_null(node_name);
    err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
	    			&t_client_url, node_name, NULL);
    safe_free(node_name);
    if (t_client_url  && ts_length(t_client_url) > 0) {
	fprintf(fp,"<SSP_Media>\r\n<Type>client-manifest</Type>\r\n<Src>%s</Src>\r\n",
			ts_str(t_client_url));
	err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		&t_bitrate, NULL,
		"/nkn/nvsd/mfp/config/%s/media-encap/file_ssp_media_sbr/client-manifest-bitrate",
				streamname);
	bail_error(err);
	/* optional parameter -bitrate*/
	if(t_bitrate && ts_length(t_bitrate) >0 ) {
	    fprintf(fp, "<BitRate>%s</BitRate>\r\n", ts_str(t_bitrate));
	}
	fprintf(fp, "</SSP_Media>\r\n");
    }else {
	/*Send error message saying the client url is not set*/
	ret_string = smprintf("Error: Client manifest URL is not specified\r\n");
	goto bail;
    }

    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/file_ssp_media_sbr/type", streamname);
    bail_null(node_name);
    err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
	    			&t_media_type, node_name, NULL);
    safe_free(node_name);
    if (t_media_type  && ts_length(t_media_type) > 0) {
	fprintf(fp,"<SSP_Media>\r\n<Type>%s</Type>\r\n",
			ts_str(t_media_type));
	node_name =  smprintf("/nkn/nvsd/mfp/config/%s/media-encap/file_ssp_media_sbr/url", streamname);
	err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
	    			&t_media_url, node_name, NULL);
	safe_free(node_name);
	if(t_media_url && ts_length(t_media_url) >0 ) {
	    fprintf(fp, "<Src>%s</Src>\r\n", ts_str(t_media_url));
	}else{
	    /* Error:  url is not set for  audio/video/mux media-type*/
	    ret_string = smprintf("Error: Media type(audio/video/mux) url is not specified\r\n");
	    goto bail;
	}
	err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		&t_bitrate, NULL,
		"/nkn/nvsd/mfp/config/%s/media-encap/file_ssp_media_sbr/bitrate",
				streamname);
	bail_error(err);
	/* Removing the bitrate from XML file since it is not needed for file*/
	/* optional parameter -bitrate*/
	if(t_bitrate && ts_length(t_bitrate) >0 ) {
	    fprintf(fp, "<BitRate>%s</BitRate>\r\n", ts_str(t_bitrate));
	}
	fprintf(fp, "</SSP_Media>\r\n");
    }else {
	/*Send error message saying the media type -auidio/video/mux is not set*/
	ret_string = smprintf("Error: Media type(audio/video/mux) is not specified\r\n");
	goto bail;
    }
#endif
    fprintf(fp," </SSP_SBR_Encapsulation>\r\n");

bail:
    if(ret_string) {
	err = ts_new_str_takeover(ret_output, &ret_string, -1, -1);
	err = 1;
    }
    ts_free(&t_client_url);
    ts_free(&t_srvr_url);
    ts_free(&t_media_url);
    ts_free(&t_media_type);
    ts_free(&t_bitrate);
    ts_free(&t_protocol);
    return err;
}

int
mfp_generate_encap_ssp_media_mbr( FILE *fp, const char *streamname,
				tstring **ret_output)
{
    int err  = 0;
    char *node_name = NULL;
    tstring *t_srvr_url = NULL;
    tstring *t_bitrate = NULL;
    tstring *t_client_url = NULL;
    tstring *t_media_type = NULL;
    tstring *t_media_url = NULL;
    char *ret_string = NULL;


    if(!fp || !streamname) {
	/*Error*/
	ret_string = smprintf("Error: File or streamname is missing\r\n");
	goto bail;
    }

    fprintf(fp," <SSP_MBR_Encapsulation>\r\n");
    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/file_ssp_media_mbr/svr-manifest-url", streamname);
    bail_null(node_name);
    err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
	    			&t_srvr_url, node_name, NULL);
    safe_free(node_name);
    if (t_srvr_url  && ts_length(t_srvr_url) > 0) {
	fprintf(fp,"<SSP_Media>\r\n<Type>svr-manifest</Type>\r\n<Src>%s</Src>\r\n",
			ts_str(t_srvr_url));
	err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		&t_bitrate, NULL,
		"/nkn/nvsd/mfp/config/%s/media-encap/file_ssp_media_mbr/svr-manifest-bitrate",
				streamname);
	bail_error(err);
#if 0
	/* optional parameter -bitrate*/
	if(t_bitrate && ts_length(t_bitrate) >0 ) {
	    fprintf(fp, "<BitRate>%s</BitRate>\r\n", ts_str(t_bitrate));
	}
#endif
	fprintf(fp, "</SSP_Media>\r\n");
    }else {
	/*Send error message saying the server url is not set*/
	ret_string = smprintf("Error: Server URL value is not set\r\n");
	goto bail;
    }
#if 0
    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/file_ssp_media_mbr/client-manifest-url", streamname);
    bail_null(node_name);
    err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
	    			&t_client_url, node_name, NULL);
    safe_free(node_name);
    if (t_client_url  && ts_length(t_client_url) > 0) {
	fprintf(fp,"<SSP_Media>\r\n<Type>client-manifest</Type>\r\n<Src>%s</Src>\r\n",
			ts_str(t_client_url));
	err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		&t_bitrate, NULL,
		"/nkn/nvsd/mfp/config/%s/media-encap/file_ssp_media_mbr/client-manifest-bitrate",
				streamname);
	bail_error(err);
	/* optional parameter -bitrate*/
	if(t_bitrate && ts_length(t_bitrate) >0 ) {
	    fprintf(fp, "<BitRate>%s</BitRate>\r\n", ts_str(t_bitrate));
	}
	fprintf(fp, "</SSP_Media>\r\n");
    }else {
	/*Send error message saying the client url is not set*/
	ret_string = smprintf("Error: Client URL value is not set\r\n");
	goto bail;
    }

    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/file_ssp_media_mbr/type", streamname);
    bail_null(node_name);
    err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
	    			&t_media_type, node_name, NULL);
    safe_free(node_name);
    if (t_media_type  && ts_length(t_media_type) > 0) {
	fprintf(fp,"<SSP_Media>\r\n<Type>%s</Type>\r\n",
			ts_str(t_media_type));
	node_name =  smprintf("/nkn/nvsd/mfp/config/%s/media-encap/file_ssp_media_mbr/url", streamname);
	err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
	    			&t_media_url, node_name, NULL);
	safe_free(node_name);
	if(t_media_url && ts_length(t_media_url) >0 ) {
	    fprintf(fp, "<Src>%s</Src>\r\n", ts_str(t_media_url));
	}else{
	    /* Error:  url is not set for  audio/video/mux media-type*/
	    ret_string = smprintf("Error: Media Type(Audio/video/mux) URL value is not set\r\n");
	    goto bail;
	}
	err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		&t_bitrate, NULL,
		"/nkn/nvsd/mfp/config/%s/media-encap/file_ssp_media_mbr/bitrate",
				streamname);
	bail_error(err);
	/* optional parameter -bitrate*/
	if(t_bitrate && ts_length(t_bitrate) >0 ) {
	    fprintf(fp, "<BitRate>%s</BitRate>\r\n", ts_str(t_bitrate));
	}
	fprintf(fp, "</SSP_Media>\r\n");
    }else {
	/*Send error message saying the media type -auidio/video/mux is not set*/
	ret_string = smprintf("Error: Media Type(Audio/video/mux) is not set\r\n");
	goto bail;
    }
#endif
    fprintf(fp," </SSP_MBR_Encapsulation>\r\n");

bail:
    if(ret_string) {
	err = ts_new_str_takeover(ret_output, &ret_string, -1, -1);
	err = 1;
    }
    ts_free(&t_client_url);
    ts_free(&t_srvr_url);
    ts_free(&t_media_url);
    ts_free(&t_media_type);
    ts_free(&t_bitrate);
    return err;
}

int
mfp_generate_encap_file_spts_media_profile( FILE *fp, const char *streamname,
				tstring **ret_output)
{
    int err  = 0;
    char *node_name = NULL;
    tstr_array *profile_config = NULL;
    uint32 i = 0;
    const char *profile_id = NULL;
    tstring *t_src = NULL;
    tstring *t_dest = NULL;
    tstring *t_vpid = NULL;
    tstring *t_apid = NULL;
    tstring *t_bitrate = NULL;
    tstring *t_protocol = NULL;
    char *ret_string = NULL;

    if(!fp || !streamname) {
	/*Error*/
	ret_string = smprintf("Filename or streamname not present\r\n");
	goto bail;
    }
    node_name =  smprintf("/nkn/nvsd/mfp/config/%s/media-encap/file_spts_media_profile", streamname);
    bail_null(node_name);
    err = mdc_get_binding_children_tstr_array(mfp_mcc,
            NULL, NULL, &profile_config,
	    node_name, NULL);
    bail_error_null(err, profile_config);
    safe_free(node_name);

    /*If no profile is added, throw an error*/
    if(tstr_array_length_quick(profile_config)== 0){
	ret_string = smprintf("Error: No input profile configured\n");
	goto bail;
    }
    fprintf(fp, "<MP2TS_SPTS_UDP_Encapsulation>\r\n");
    for(;i < tstr_array_length_quick(profile_config); i++) {
	profile_id = tstr_array_get_str_quick(profile_config, i);
	node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/file_spts_media_profile/%s/source", streamname, profile_id);
	bail_null(node_name);
	/*get source*/
	err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
		&t_src, node_name, NULL);
	safe_free(node_name);
	if(t_src && (ts_length(t_src) > 0 )) {
#if 0
	    err = mfp_escape_sequence_space_in_address(t_src);
	    bail_error(err);
#endif
	    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/file_spts_media_profile/%s/protocol", streamname, profile_id);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_protocol, node_name, NULL);
	    safe_free(node_name);
	    if(t_protocol && (ts_equal_str(t_protocol, "nfs", false))) {
		fprintf(fp, "<SPTS_Media>\r\n<ProfileId>%s</ProfileId>\r\n<Src>/nkn/nfs/%s</Src>\r\n",
			profile_id, ts_str(t_src));
	    }else if(t_protocol && (ts_equal_str(t_protocol, "http", false))) {
		fprintf(fp, "<SPTS_Media>\r\n<ProfileId>%s</ProfileId>\r\n<Src>http://%s</Src>\r\n", profile_id, ts_str(t_src));
	    }else {

		fprintf(fp, "<SPTS_Media>\r\n<ProfileId>%s</ProfileId>\r\n<Src>%s</Src>\r\n",
			profile_id, ts_str(t_src));
	    }
	    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/file_spts_media_profile/%s/dest", streamname, profile_id);
	    err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
		    &t_dest, node_name, NULL);
	    safe_free(node_name);
	    if(t_dest && (ts_length(t_dest) > 0 )) {
		fprintf(fp, "<Dest>%s</Dest>\r\n", ts_str(t_dest));
	    }
	    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/file_spts_media_profile/%s/bitrate", streamname, profile_id);
	    err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		    &t_bitrate, NULL,
		    node_name, NULL);
	    safe_free(node_name);
	    if(t_bitrate && (ts_length(t_bitrate) > 0 )) {
		/*BUG FIX: 6694
		 * If the bitrate is 0 then dont send this tag to PMF file
		 */
		if( !(ts_equal_str(t_bitrate, "0", false)) ){
		    fprintf(fp, "<BitRate>%s</BitRate>\r\n", ts_str(t_bitrate));
		}
	    }
	    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/file_spts_media_profile/%s/vpid", streamname, profile_id);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_vpid, node_name, NULL);
	    safe_free(node_name);
	    if(t_vpid && (ts_length(t_vpid) > 0 )) {
		/*TODO: check if the value is valid value*/
		fprintf(fp, "<vpid>%s</vpid>\r\n", ts_str(t_vpid));
	    }
	    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/file_spts_media_profile/%s/apid", streamname, profile_id);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_apid, node_name, NULL);
	    safe_free(node_name);
	    if(t_apid && (ts_length(t_apid) > 0 )) {
		/*TODO: check if the value is valid value*/
		fprintf(fp, "<apid>%s</apid>\r\n", ts_str(t_apid));
	    }
	    fprintf(fp, "</SPTS_Media>\r\n");
	} else {
	    /* Profile source not set*/
	    ret_string = smprintf("Error:Profile source not set\r\n");
	    goto bail;
	}
	ts_free(&t_src);
	ts_free(&t_dest);
	ts_free(&t_vpid);
	ts_free(&t_apid);
	ts_free(&t_bitrate);
	ts_free(&t_protocol);
    }
    fprintf(fp, "</MP2TS_SPTS_UDP_Encapsulation>\r\n");

bail:
    if(ret_string){
	err = ts_new_str_takeover(ret_output, &ret_string, -1, -1);
	err = 1;
    }
    return err;
}

int
mfp_generate_encap_live_spts_media_profile( FILE *fp, const char *streamname,
				tstring **ret_output)
{
    int err  = 0;
    char *node_name = NULL;
    tstr_array *profile_config = NULL;
    uint32 i = 0;
    const char *profile_id = NULL;
    tstring *t_src = NULL;
    tstring *t_ip = NULL;
    tstring *t_vpid = NULL;
    tstring *t_apid = NULL;
    tstring *t_bitrate = NULL;
    char *ret_string = NULL;

    if(!fp || !streamname) {
	/*Error*/
	ret_string = smprintf("Error:Missing filename or Streamname\r\n");
	goto bail;
    }
    node_name =  smprintf("/nkn/nvsd/mfp/config/%s/media-encap/live_spts_media_profile", streamname);
    bail_null(node_name);
    err = mdc_get_binding_children_tstr_array(mfp_mcc,
            NULL, NULL, &profile_config,
	    node_name, NULL);
    bail_error_null(err, profile_config);
    safe_free(node_name);
    /*If no profile is added, throw an error*/
    if(tstr_array_length_quick(profile_config)== 0){
	ret_string = smprintf("Error: No input profile configured\n");
	goto bail;
    }

    fprintf(fp, "<MP2TS_SPTS_UDP_Encapsulation>\r\n");
    for(;i < tstr_array_length_quick(profile_config); i++) {
	profile_id = tstr_array_get_str_quick(profile_config, i);
	node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/live_spts_media_profile/%s/source", streamname, profile_id);
	/*get source*/
	err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		&t_src, node_name, NULL);
	safe_free(node_name);
	if(t_src && (ts_length(t_src) > 0 )) {
    	    tstring *t_srcport = NULL;
	    fprintf(fp, "<SPTS_Media>\r\n<ProfileId>%s</ProfileId>\r\n",
		    profile_id);
	    node_name =  smprintf("/nkn/nvsd/mfp/config/%s/media-encap/live_spts_media_profile/%s/srcport", streamname, profile_id);
	    err  = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		    &t_srcport, NULL,
		    node_name, NULL);
	    safe_free(node_name);
	    /* It is the source for the customer and for MFC it is the delivery point
	     * Hence changing the tag for the PMF
	     */
	    if(t_srcport && ts_length(t_srcport) > 0) {
		fprintf(fp, "<Dest>udp://@%s:%s</Dest>\r\n", ts_str(t_src), ts_str(t_srcport));
		ts_free(&t_srcport);

	    }else {
		ret_string = smprintf("Error:Profile Source Port is not set\r\n");
	    }
#if 1
	    //Adding the IP address field to PMF. 
	    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/live_spts_media_profile/%s/ipsource", streamname, profile_id);
	    err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
		    &t_ip, node_name, NULL);
	    safe_free(node_name);
	    if(t_ip && (ts_length(t_ip) > 0 )) {
		fprintf(fp, "<SourceIP>%s</SourceIP>\r\n", ts_str(t_ip));
	    }
#endif 
	    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/live_spts_media_profile/%s/bitrate", streamname, profile_id);
	    err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		    &t_bitrate, NULL,
		    node_name, NULL);
	    safe_free(node_name);
	    if(t_bitrate && (ts_length(t_bitrate) > 0 )) {
		fprintf(fp, "<BitRate>%s</BitRate>\r\n", ts_str(t_bitrate));
	    }else {
		ret_string = smprintf("Error: Bitrate not set for live spts profile:%s\r\n", profile_id);
		goto bail;
	    }
	    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/live_spts_media_profile/%s/vpid", streamname, profile_id);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_vpid, node_name, NULL);
	    safe_free(node_name);
	    if(t_vpid && (ts_length(t_vpid) > 0 )) {
		/*TODO: check if the value is valid value*/
		fprintf(fp, "<vpid>%s</vpid>\r\n", ts_str(t_vpid));
	    }
	    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap/live_spts_media_profile/%s/apid", streamname, profile_id);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_apid, node_name, NULL);
	    safe_free(node_name);
	    if(t_apid && (ts_length(t_apid) > 0 )) {
		/*TODO: check if the value is valid value*/
		fprintf(fp, "<apid>%s</apid>\r\n", ts_str(t_apid));
	    }

	    fprintf(fp, "</SPTS_Media>\r\n");
	} else {
	    /* Profile source not set*/
	    ret_string = smprintf("Error:Profile Source is not set\r\n");
	    goto bail;
	}
	ts_free(&t_src);
	ts_free(&t_ip);
	ts_free(&t_vpid);
	ts_free(&t_apid);
	ts_free(&t_bitrate);
    }
    fprintf(fp, "</MP2TS_SPTS_UDP_Encapsulation>\r\n");
bail:
    if(ret_string) {
	err = ts_new_str_takeover(ret_output, &ret_string, -1, -1);
	err = 1;
    }
    return err;
}
int
mfp_generate_output_parameter(FILE *fp, const char *streamname,
	tstring ** ret_output)
{
    int err = 0;
    int output_param = 0;
    node_name_t mobile_node = {0};
    node_name_t flash_node = {0};
//    char *mobile_node = NULL;
//    char *flash_node = NULL;
    char *smooth_node = NULL;
    char *stream_type = NULL;
    tbool t_mobile_status = false;
    tbool t_flash_status = false;
    tbool t_smooth_status = false;
    tstring *t_keyframe = NULL;
    tstring *t_start = NULL;
    tstring *t_rollover = NULL;
    tstring *t_precision = NULL;
    tstring *t_segplay = NULL;
    tstring *t_storage = NULL;
    tstring *t_delivery = NULL;
    tstring *t_media_type = NULL;
    tstring *t_dvrlength = NULL;
    tbool t_dvr = false;
    char *ret_string = NULL;
    tbool t_encrypt = false;
    tbool t_unique = false;
    tbool t_key_type = false;
    tstring *t_key_url = NULL;
    tstring *t_key_prefix = NULL;
    tstring *t_port = NULL;
    tstring *t_interval = NULL;
    tstring *t_external = NULL;
    tstring *t_http_reqd = NULL;
    tstring *t_content = NULL;
    tstring *t_policy = NULL;
    tstring *t_cred_pswd = NULL;
    tstring *t_pkg_cred = NULL;
    tstring *t_trans_cert = NULL;
    tstring *t_lic_cert = NULL;
    tstring *t_lic_url = NULL;
    tstring *t_common_key = NULL;
    tstring *t_segment = NULL;
    tstring *t_dvrsec = NULL;
    tstring *t_stream = NULL;

    if(!streamname || !fp){
	/* error */
	ret_string = smprintf("Error: streamname or filename is not present\r\n");
	goto bail;
    }

    stream_type = smprintf("/nkn/nvsd/mfp/config/%s/media-type", streamname);
    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
	    &t_media_type, stream_type, NULL);
    safe_free(stream_type);

    /* mobile streaming  output parameters*/

    snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/status", streamname);
    err = mdc_get_binding_tbool (mfp_mcc, NULL, NULL, NULL,
	    &t_mobile_status, mobile_node, NULL);
    bail_error(err);
    fprintf(fp, "<PubSchemes>\r\n");
    if(t_mobile_status) {
	fprintf(fp, "<MobileStreaming status=\"on\" ");
	output_param++;
    }else {
	fprintf(fp, "<MobileStreaming status=\"off\" ");
    }
    snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/keyframe", streamname);
    err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
	    &t_keyframe, NULL, mobile_node, NULL);
    if(t_keyframe && (ts_length(t_keyframe) > 0 )) {
	fprintf(fp, "KeyFrameInterval=\"%s\" ", ts_str(t_keyframe));
    }else {
	/*Error  keyframe not set*/
	ret_string = smprintf("Error:Apple HTTP Streaming HLS Key frame interval is not set\r\n");
	goto bail;
    }
    snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/segment-start", streamname);
    err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
	    &t_start, NULL, mobile_node, NULL);
    if(t_start && (ts_length(t_start) > 0 )) {
	fprintf(fp, "SegmentStartIndex=\"%s\" ", ts_str(t_start));
    }else {
	/*Error  segment start index  not set*/
    }
    snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/segment-rollover", streamname);
    err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
	    &t_rollover, NULL, mobile_node, NULL);
    /*
     * BUG:6653- Segment rollover interval is valid for live alone
     * dont put this attribute for file related PMF. 
     */
    if(ts_equal_str(t_media_type, "Live", false)) {
	if(t_rollover && (ts_length(t_rollover) > 0 )) {
	    fprintf(fp, "SegmentRolloverInterval=\"%s\" ", ts_str(t_rollover));
	}
    }
    snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/segment-precision", streamname);
    err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
	    &t_precision, NULL,	mobile_node, NULL);
    if(t_precision && (ts_length(t_precision) > 0 )) {
	fprintf(fp, "SegmentIndexPrecision=\"%s\" ", ts_str(t_precision));
    }else {
	/*Error  segment precision not set*/
    }
    snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/minsegsinchildplay", streamname);
    err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
	    &t_segplay, NULL, mobile_node, NULL);
    if(t_segplay && (ts_length(t_segplay) > 0 )) {
	fprintf(fp, "MinSegsInChildPlaylist=\"%s\" ", ts_str(t_segplay));
    }
    snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/dvr", streamname);
    err = mdc_get_binding_tbool (mfp_mcc, NULL, NULL, NULL,
	    &t_dvr, mobile_node, NULL);
    bail_error(err);
    if(t_dvr) {
	fprintf(fp, "DVR=\"on\" ");
    }else {
	if(ts_equal_str(t_media_type, "Live", false)) {
	    fprintf(fp, "DVR=\"off\" ");
	}
    }
    snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/storage_url", streamname);
    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
	    &t_storage, mobile_node, NULL);
    if(t_storage && (ts_length(t_storage) > 0 )) {
	/*
	 * TODO: check if the value is valid value
	 * check if the mount point is present.
	 * Append /nkn/nfs with the first part of the storage value 
	 * and then send "/nkn/nfs/<storage-url>" to the PMF file
	 */
	uint32 mount_error = 0;
	uint8_t type = 0;
	err = mfp_check_if_valid_mount(t_storage, &mount_error, &type);
	/*error*/
	if(mount_error) {
	    ret_string = smprintf("Error: %s:No such Mount point exists\n", ts_str(t_storage));
	    goto bail;
	}
	if (type) {
	    node_name_t mount_path_node = {0};
	    tstring *t_mount_path = NULL;

	    snprintf(mount_path_node, sizeof(mount_path_node), 
		    "/nkn/nfs_mount/config/%s/mountpath", ts_str(t_storage));
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_mount_path, mount_path_node, NULL);
	    bail_error_null(err, t_mount_path);
	    fprintf(fp, "StorageURL=\"%s/%s-apple\" ", ts_str(t_mount_path), streamname);
	} else {
	    fprintf(fp, "StorageURL=\"/nkn/nfs/%s/%s-apple\" ", ts_str(t_storage), streamname);
	}
	}else {
	    /* error*/
	    if(t_mobile_status){
		ret_string =  smprintf("Error:Apple HTTP Streaming HLS Storage URL not set\r\n");
		goto bail;
	    }else {
		fprintf(fp, "StorageURL=\"\" ");
	    }
	}
    snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/delivery_url", streamname);
    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
	    &t_delivery, mobile_node, NULL);
    if(t_delivery && (ts_length(t_delivery) > 0 )) {
		/*TODO: check if the value is valid value*/
#if 0
	    	err = mfp_escape_sequence_space_in_address(t_delivery);
	   	bail_error(err);
#endif
	    fprintf(fp, "DeliveryURL=\"%s\" ", ts_str(t_delivery));
    }else {
	/* error*/
	if(t_mobile_status){
	    ret_string =  smprintf("Error:Apple HTTP streaming HLS Delivery URL not set\r\n");
	    goto bail;
	}else {
	    fprintf(fp, "DeliveryURL=\"\" ");
	}
    }
    /* Release 11A Adding encryption parameter values */
    snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/encrypt/enable", streamname);
    err = mdc_get_binding_tbool (mfp_mcc, NULL, NULL, NULL,
	    &t_encrypt, mobile_node, NULL);
    bail_error(err);
    if(t_encrypt) {
	fprintf(fp, "Encryption=\"on\" ");
	snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/encrypt/unique_key", streamname);
	err = mdc_get_binding_tbool (mfp_mcc, NULL, NULL, NULL,
		&t_unique, mobile_node, NULL);
	bail_error(err);
	if(t_unique) {
	    fprintf(fp, "UniqueKeyPerProfile=\"on\" ");
	}else {
	    fprintf(fp, "UniqueKeyPerProfile=\"off\" ");
	}
	snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/encrypt/key_rotation", streamname);
	err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		&t_interval, NULL, mobile_node, NULL);
	if(t_interval && (ts_length(t_interval) > 0 )) {
	    fprintf(fp, "KeyRotationInterval=\"%s\" ", ts_str(t_interval));
	}//else ignore the attribute value.
	snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/encrypt/kms_native", streamname);
	err = mdc_get_binding_tbool (mfp_mcc, NULL, NULL, NULL,
		&t_key_type, mobile_node, NULL);
	bail_error(err);
	if(t_key_type) {
	    /*KMS -Native nodes */
	    snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/encrypt/kms_native_prefix", streamname);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_key_prefix, mobile_node, NULL);
	    if(t_key_prefix && (ts_length(t_key_prefix) > 0 )) {
		fprintf(fp, " KMS_Type=\"NATIVE\" KMS_FileNamePrefix=\"%s\" ", ts_str(t_key_prefix));
	    }else {
		ret_string = smprintf("Error: KMS File Prefix is not mentioned for KMS-Native\n");
		goto bail;
	    }
	    snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/encrypt/kms_storage_url", streamname);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_key_url, mobile_node, NULL);
	    if(t_key_url && (ts_length(t_key_url) > 0 )) {
		fprintf(fp, "KMS_FileStorageUrl=\"%s\" ", ts_str(t_key_url));
	    }else {
		ret_string = smprintf("Error: KMS Storage url is not mentioned for KMS-Native\n");
		goto bail;
	    }
	}else {
	    /*KMS External */
	    snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/encrypt/kms_external", streamname);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_external, mobile_node, NULL);
	    if(t_external && (ts_length(t_external) > 0 )) {
		fprintf(fp, "KMS_Type=\"%s\" ", ts_str(t_external));
	    }else {
		ret_string = smprintf("Error: KMS External type  is not mentioned for KMS-External\n");
		goto bail;
	    }
	    snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/encrypt/kms_srvr", streamname);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_key_url, mobile_node, NULL);
	    if(t_key_url && (ts_length(t_key_url) > 0 )) {
		fprintf(fp, "KMS_ServerAddress=\"%s\" ", ts_str(t_key_url));
	    }else {
		ret_string = smprintf("Error: KMS Server details is not mentioned for KMS-External\n");
		goto bail;
	    }
	    snprintf(mobile_node, sizeof(mobile_node), "/nkn/nvsd/mfp/config/%s/mobile/encrypt/kms_port", streamname);
	    err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		    &t_port, NULL, mobile_node, NULL);
	    if(t_port && (ts_length(t_port) > 0 )) {
		fprintf(fp, "KMS_ServerPort=\"%s\" ", ts_str(t_port));
	    }else {
		ret_string = smprintf("Error: KMS Server port details is not mentioned for KMS-External\n");
		goto bail;
	    }
	}
    }else {
	fprintf(fp, "Encryption=\"off\" ");
    }

    fprintf(fp, ">\r\n</MobileStreaming>\r\n");
    ts_free(&t_keyframe);
    ts_free(&t_start);
    ts_free(&t_rollover);
    ts_free(&t_precision);
    ts_free(&t_segplay);
    ts_free(&t_delivery);
    ts_free(&t_storage);

	/* BUG:6506 - Remove Flash streaming from web-page since it is not supported  now
	 * Commenting out the code for this portion of PMF generation
	 */

    /* flash streaming  output parameters*/
    if(ts_equal_str(t_media_type, "Live", false)) {
	snprintf(flash_node, sizeof(flash_node), "/nkn/nvsd/mfp/config/%s/flash/status", streamname);
	err = mdc_get_binding_tbool (mfp_mcc, NULL, NULL, NULL,
		&t_flash_status, flash_node, NULL);
	bail_error(err);
	if(t_flash_status) {
	    fprintf(fp, "<FlashStreaming status=\"on\" ");
	    output_param++;
	}else {
	    fprintf(fp, "<FlashStreaming status=\"off\" ");
	}
	snprintf(flash_node, sizeof(flash_node),"/nkn/nvsd/mfp/config/%s/flash/keyframe", streamname);
	err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		&t_keyframe, NULL,
		flash_node, NULL);
	if(t_keyframe && (ts_length(t_keyframe) > 0 )) {
	    fprintf(fp, "FragmentDuration=\"%s\" ", ts_str(t_keyframe));
	}else {
	    /*Error  keyframe not set*/
	    ret_string =  smprintf("Error:Fragment duration for Flash Streaming is not set\r\n");
	    goto bail;
	}
	snprintf(flash_node, sizeof(flash_node), "/nkn/nvsd/mfp/config/%s/flash/segment", streamname);
	err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		&t_segment, NULL,
		flash_node, NULL);
	if(t_segment && (ts_length(t_segment) > 0 )) {
	    fprintf(fp, "SegmentDuration=\"%s\" ", ts_str(t_segment));
	}else {
	    /*Error  keyframe not set*/
	    ret_string =  smprintf("Error:Segment duration for Flash streaming is not set\r\n");
	    goto bail;
	}
	snprintf(flash_node, sizeof(flash_node), "/nkn/nvsd/mfp/config/%s/flash/storage_url", streamname);
	err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_storage, flash_node, NULL);
	if(t_storage && (ts_length(t_storage) > 0 )) {
		/*TODO: check if the value is valid value*/
	    uint32 mount_error = 0;
	    uint8_t type = 0;
	    err = mfp_check_if_valid_mount(t_storage, &mount_error, &type);
	    /*error*/
	    if(mount_error) {
		ret_string = smprintf("Error: %s:No such Mount point exists\n", ts_str(t_storage));
		goto bail;
	    }
	    if (type) {
		/* this persistent -storage */
		node_name_t mount_path_node = {0};
		tstring *t_mount_path = NULL;

		snprintf(mount_path_node, sizeof(mount_path_node), 
			"/nkn/nfs_mount/config/%s/mountpath", ts_str(t_storage));
		err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
			&t_mount_path, mount_path_node, NULL);
		bail_error_null(err, t_mount_path);
		fprintf(fp, "StorageURL=\"%s/%s-flash\" ", ts_str(t_mount_path), streamname);
	    } else {
		fprintf(fp, "StorageURL=\"/nkn/nfs/%s/%s-flash\" ", ts_str(t_storage), streamname);
	    }
	}else {
	    /* error*/
	    if(t_flash_status) {
		ret_string =  smprintf("Error:Storage URL not set for Flash streaming\r\n");
		goto bail;
	    }else {
		fprintf(fp, "StorageURL=\"\" ");
	    }
	}
	snprintf(flash_node, sizeof(flash_node), "/nkn/nvsd/mfp/config/%s/flash/delivery_url", streamname);
	err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_delivery, flash_node, NULL);
	if(t_delivery && (ts_length(t_delivery) > 0 )) {
		/*TODO: check if the value is valid value*/
		fprintf(fp, "DeliveryURL=\"%s\" ", ts_str(t_delivery));
	}else {
	    /* eror*/
	    if(t_flash_status) {
		ret_string =  smprintf("Error:Delivery URL not set for Flash streaming\r\n");
		goto bail;
	    }else {
		fprintf(fp, "DeliveryURL=\"\" ");
	    }
	}
	/* Release 11 A  feature addion*/
	snprintf(flash_node, sizeof(flash_node), "/nkn/nvsd/mfp/config/%s/flash/stream_type", streamname);
	err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_stream, flash_node, NULL);
	if(t_stream && (ts_length(t_stream) > 0 )) {
		fprintf(fp, "StreamType=\"%s\" ", ts_str(t_stream));
	}else {
		ret_string =  smprintf("Error:Stream type  not set for Flash streaming\r\n");
		goto bail;
	}
	if(ts_equal_str(t_stream, "dvr", false)) {
	    snprintf(flash_node, sizeof(flash_node), "/nkn/nvsd/mfp/config/%s/flash/dvrsec", streamname);
	    err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		    &t_dvrsec, NULL,
		    flash_node, NULL);
	    if(t_dvrsec && (ts_length(t_dvrsec) > 0 )) {
		fprintf(fp, "DVRSeconds=\"%s\" ", ts_str(t_dvrsec));
	    }else {
		ret_string = smprintf(" DVR seconds value is not set for stream type DVR\n");
		goto bail;
	    }
	}

	snprintf(flash_node, sizeof(flash_node), "/nkn/nvsd/mfp/config/%s/flash/encrypt/enable", streamname);
	err = mdc_get_binding_tbool (mfp_mcc, NULL, NULL, NULL,
		&t_encrypt, flash_node, NULL);
	bail_error(err);
	if(t_encrypt) {
	    fprintf(fp, "Encryption=\"on\" ");
	    snprintf(flash_node, sizeof(flash_node), "/nkn/nvsd/mfp/config/%s/flash/encrypt/common_key", streamname);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_common_key, flash_node, NULL);
	    if(t_common_key && (ts_length(t_common_key) > 0 )) {
		fprintf(fp, "CommonKey=\"%s\" ", ts_str(t_common_key));
	    }else {
		ret_string =  smprintf("Error:Common key for encryption is not set for flash streaming\r\n");
		goto bail;
	    }
	    snprintf(flash_node, sizeof(flash_node), "/nkn/nvsd/mfp/config/%s/flash/encrypt/lic_srvr_url", streamname);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_lic_url, flash_node, NULL);
	    if(t_lic_url && (ts_length(t_lic_url) > 0 )) {
		fprintf(fp, "LicenseServerUrl=\"%s\" ", ts_str(t_lic_url));
	    }else {
		ret_string =  smprintf("Error:License server url for encryption is not set for Flash streaming\r\n");
		goto bail;
	    }
	    snprintf(flash_node, sizeof(flash_node), "/nkn/nvsd/mfp/config/%s/flash/encrypt/lic_srvr_cert", streamname);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_lic_cert, flash_node, NULL);
	    if(t_lic_cert && (ts_length(t_lic_cert) > 0 )) {
		fprintf(fp, "LicenseServerCert=\"%s\" ", ts_str(t_lic_cert));
	    }else {
		ret_string =  smprintf("Error:License server certificate for encryption is not set for Flash streaming\r\n");
		goto bail;
	    }
	    snprintf(flash_node, sizeof(flash_node), "/nkn/nvsd/mfp/config/%s/flash/encrypt/trans_cert", streamname);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_trans_cert, flash_node, NULL);
	    if(t_trans_cert && (ts_length(t_trans_cert) > 0 )) {
		fprintf(fp, "TransportCert=\"%s\" ", ts_str(t_trans_cert));
	    }else {
		ret_string =  smprintf("Error:Transport certificate for encryption is not set for Flash streaming\r\n");
		goto bail;
	    }
	    snprintf(flash_node, sizeof(flash_node), "/nkn/nvsd/mfp/config/%s/flash/encrypt/pkg_cred", streamname);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_pkg_cred, flash_node, NULL);
	    if(t_pkg_cred && (ts_length(t_pkg_cred) > 0 )) {
		fprintf(fp, "PackagerCredential=\"%s\" ", ts_str(t_pkg_cred));
	    }else {
		ret_string =  smprintf("Error:Packager credentials for encryption is not set for Flash streaming\r\n");
		goto bail;
	    }
	    snprintf(flash_node, sizeof(flash_node), "/nkn/nvsd/mfp/config/%s/flash/encrypt/cred_pswd", streamname);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_cred_pswd, flash_node, NULL);
	    if(t_cred_pswd && (ts_length(t_cred_pswd) > 0 )) {
		fprintf(fp, "CredentialPwd=\"%s\" ", ts_str(t_cred_pswd));
	    }else {
		ret_string =  smprintf("Error:Credential password for encryption is not set for Flash streaming\r\n");
		goto bail;
	    }
	    snprintf(flash_node, sizeof(flash_node), "/nkn/nvsd/mfp/config/%s/flash/encrypt/policy_file", streamname);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_policy, flash_node, NULL);
	    if(t_policy && (ts_length(t_policy) > 0 )) {
		fprintf(fp, "PolicyFile=\"%s\" ", ts_str(t_policy));
	    }else {
		ret_string =  smprintf("Error:Policy file for encryption is not set for Flash streaming\r\n");
		goto bail;
	    }
	    snprintf(flash_node, sizeof(flash_node), "/nkn/nvsd/mfp/config/%s/flash/encrypt/contentid", streamname);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_content, flash_node, NULL);
	    if(t_content && (ts_length(t_content) > 0 )) {
		fprintf(fp, "ContentId=\"%s\" ", ts_str(t_content));
	    }else {
		ret_string =  smprintf("Error:Content Id for encryption is not set for Flash streaming\r\n");
		goto bail;
	    }
	    snprintf(flash_node, sizeof(flash_node), "/nkn/nvsd/mfp/config/%s/flash/http_origin_reqd", streamname);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_http_reqd, flash_node, NULL);
	    if(t_http_reqd && (ts_length(t_http_reqd) > 0 )) {
		fprintf(fp, "OrigModReq=\"%s\" ", ts_str(t_http_reqd));
	    }
	}else {
	    fprintf(fp, "Encryption=\"off\" ");
	}
	
	fprintf(fp, ">\r\n</FlashStreaming>\r\n");
	ts_free(&t_keyframe);
	ts_free(&t_delivery);
	ts_free(&t_storage);
    }

    /* smooth streaming  output parameters*/

    smooth_node = smprintf("/nkn/nvsd/mfp/config/%s/smooth/status", streamname);
    bail_null(smooth_node);
    err = mdc_get_binding_tbool (mfp_mcc, NULL, NULL, NULL,
	    &t_smooth_status, smooth_node, NULL);
    bail_error(err);
    safe_free(smooth_node);
    /*FIX:
     * For file spts remove the smooth streaming tag in pubscheme
     */
    if(t_smooth_status) {
	fprintf(fp, "<SmoothStreaming status=\"on\" ");
	output_param++;
    }else {
	fprintf(fp, "<SmoothStreaming status=\"off\" ");
    }
	smooth_node = smprintf("/nkn/nvsd/mfp/config/%s/smooth/keyframe", streamname);
	err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		&t_keyframe, NULL,
		smooth_node, NULL);
	safe_free(smooth_node);
	/*
	 * BUG: 6505 : Remove Segment duration for FILE relate smooth-streaming
	 * segment duration in smooth streaming is valid for LIVE  alone.
	 * */
	if(ts_equal_str(t_media_type, "Live", false)) {
	    if(t_keyframe && (ts_length(t_keyframe) > 0 )) {
		fprintf(fp, "KeyFrameInterval=\"%s\" ", ts_str(t_keyframe));
	    }else {
		/*Error  keyframe not set*/
		ret_string =  smprintf("Error:Smoothstream Keyframe interval not set\r\n");
		goto bail;
	    }
	}
	smooth_node = smprintf("/nkn/nvsd/mfp/config/%s/smooth/dvr", streamname);
	err = mdc_get_binding_tbool (mfp_mcc, NULL, NULL, NULL,
		&t_dvr, smooth_node, NULL);
	bail_error(err);
	safe_free(smooth_node);
	if(t_dvr) {
	    fprintf(fp, "DVR=\"on\" ");
	}else {
	    if(ts_equal_str(t_media_type, "Live", false)) {
		    fprintf(fp, "DVR=\"off\" ");
	    }
	}
	err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		&t_dvrlength, NULL,
		"/nkn/nvsd/mfp/config/%s/smooth/dvrlength",
				streamname);
       if(ts_equal_str(t_media_type, "Live", false)) {
	   if(t_dvrlength && ts_length(t_dvrlength) > 0) {
	       fprintf(fp, "DVRWindowLength=\"%s\" ", ts_str(t_dvrlength));
	    }else {
		ret_string = smprintf("Error:Smoothstream DVR Window length is not set\r\n");
		goto bail;
	    }
	}
	smooth_node = smprintf("/nkn/nvsd/mfp/config/%s/smooth/storage_url", streamname);
	err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_storage, smooth_node, NULL);
	safe_free(smooth_node);
	if(t_storage && (ts_length(t_storage) > 0 )) {
		/*TODO: check if the value is valid value*/
	    uint32 mount_error = 0;
	    uint8_t type = 0;
	    err = mfp_check_if_valid_mount(t_storage, &mount_error, &type);
	    /*error*/
	    if(mount_error) {
		ret_string = smprintf("Error: %s:No such Mount point exists\n", ts_str(t_storage));
		goto bail;
	    }

	    if (type) {
		node_name_t mount_path_node = {0};
		tstring *t_mount_path = NULL;

		snprintf(mount_path_node, sizeof(mount_path_node), 
			"/nkn/nfs_mount/config/%s/mountpath", ts_str(t_storage));
		err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
			&t_mount_path, mount_path_node, NULL);
		bail_error_null(err, t_mount_path);
		fprintf(fp, "StorageURL=\"%s/%s-smooth\">\r\n ", ts_str(t_mount_path), streamname);
	    } else {
		fprintf(fp, "StorageURL=\"/nkn/nfs/%s/%s-smooth\">\r\n ", ts_str(t_storage), streamname);
	    }
	}else {
	    /* error*/
	    if(t_smooth_status) {
		ret_string =  smprintf("Error:Smoothstream Storage URL not set\r\n");
		goto bail;
	    }else {
		fprintf(fp, "StorageURL=\"\">\r\n");
	    }
	}
	smooth_node = smprintf("/nkn/nvsd/mfp/config/%s/smooth/delivery_url", streamname);
	err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_delivery, smooth_node, NULL);
	safe_free(smooth_node);
	if(t_delivery && (ts_length(t_delivery) > 0 )) {
		/*TODO: check if the value is valid value*/
#if 0
	    	err = mfp_escape_sequence_space_in_address(t_delivery);
	        bail_error(err);
#endif
		fprintf(fp, "DeliveryURL=\"%s\">\r\n", ts_str(t_delivery));
	}
	fprintf(fp, "</SmoothStreaming>\r\n");
	ts_free(&t_keyframe);
	ts_free(&t_delivery);
	ts_free(&t_storage);
	ts_free(&t_dvrlength);

	//if( (t_smooth_status == true) || (t_flash_status == true) || (t_mobile_status == true)) {
	if( (t_flash_status == true) || (t_smooth_status == true) || (t_mobile_status == true)) {
	    /*atleast one output pub scheme is set*/
	}else {
	    /* No output param set- please set atleast one*/
	    ret_string = smprintf("Error: No  publishing scheme enabled.\r\n"
		    "Enable atleast one publishing scheme.\r\n");
	}
	fprintf(fp, "</PubSchemes>\r\n");
bail:
    if(ret_string) {
	err = ts_new_str_takeover(ret_output, &ret_string, -1, -1);
	err = 1;
    }
    ts_free(&t_key_url);
    ts_free(&t_key_prefix);
    ts_free(&t_port);
    ts_free(&t_interval);
    ts_free(&t_external);
    ts_free(&t_http_reqd);
    ts_free(&t_content);
    ts_free(&t_policy);
    ts_free(&t_cred_pswd);
    ts_free(&t_pkg_cred);
    ts_free(&t_trans_cert);
    ts_free(&t_lic_cert);
    ts_free(&t_lic_url);
    ts_free(&t_common_key);
    ts_free(&t_stream);
    ts_free(&t_segment);
    ts_free(&t_dvrsec);
    return err;
}

int
mfp_action_remove( const char *streamname,
	tstring **ret_output)
{
    int err = 0;
    char *node_name = NULL;
    char filename[256] ;
    char dest_filename[256] ;
    tstring *t_media_encap_type = NULL;
    tstring *t_session_value = NULL;
    FILE  *fp = NULL;
    char  date_str[256];
    char *ret_string = NULL;

    if(!streamname || ! ret_output) {
	ret_string = smprintf("error: unable to find the stream-name\r\n");
     	goto bail;
    }

    time_t cur_time = time(NULL);
    struct tm *loctime = localtime(&cur_time);
    strftime(date_str, 256, "%Y%m%d_%H:%M:%S", loctime);
    node_name = smprintf("/nkn/nvsd/mfp/config/%s/name", streamname);
    bail_null(node_name);

    err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
 	    			&t_session_value, node_name, NULL);

    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap", streamname);
    bail_null(node_name);

    err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
 	    			&t_media_encap_type, node_name, NULL);

    if( t_media_encap_type && ts_length(t_media_encap_type) > 0) {
	/*If the prefix is file then it is File Media Source*/
	if( (ts_cmp_prefix_str( t_media_encap_type, "file_", false)) == 0) {
	    /* File media source*/
	    /* Create a file media source PMF*/
	    snprintf( filename, sizeof(filename), "/var/log/action_remove_file_%s_%s.xml", streamname, date_str);
	    snprintf( dest_filename, sizeof(dest_filename), "/nkn/vpe/file/action_remove_file_%s_%s.xml", streamname, date_str);
	} else {
	    /* Live media source*/
	    /* Create a file media source PMF*/
	    snprintf( filename, sizeof(filename), "/var/log/action_remove_live_%s_%s.xml", streamname, date_str);
	    snprintf( dest_filename, sizeof(dest_filename), "/nkn/vpe/live/action_remove_live_%s_%s.xml", streamname, date_str);
	}
	fp = fopen( filename, "w");
	if(!fp) {
	    lc_log_basic(LOG_NOTICE, "Failed to open file:%s", filename);
	    goto bail;
	}
	fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<PubManifest version=\"1.0\" name=\"%s\">\r\n<ControlMediaSource>\r\n<Action>REMOVE</Action>\r\n</ControlMediaSource>\r\n</PubManifest>", ts_str(t_session_value));

 	fclose(fp);
	    fp = NULL;
	err = lf_copy_file( filename, dest_filename, fco_ensure_dest_dir);
	bail_error(err);
	err = lf_remove_file(filename);
	bail_error(err);
	ret_string = smprintf("REMOVE Action Stream PMF created for session:%s\r\n", streamname); 
	//ret_string = smprintf("Action Stream PMF created:%s\r\n",dest_filename);
#if 1	
	node_name = smprintf("/nkn/nvsd/mfp/config/%s", streamname);
	bail_null(node_name);
	err = mdc_delete_binding(mfp_mcc, NULL, NULL, node_name);
#endif

    } else {
	/* unknown streamname message*/
	ret_string = smprintf("Stream:%s doesnt exist or mediasource type isnt set\r\n", streamname);
	err = 1;
    }


bail:
    if(err) {
	if(fp) {
	    fclose(fp);
	    fp = NULL;
	    err = lf_remove_file(filename);
 	    err = 1;
	}
    }
    if(ret_string) {
	err = ts_new_str_takeover(ret_output, &ret_string, -1, -1);
    }
    safe_free(node_name);
    ts_free(&t_media_encap_type);
    ts_free(&t_session_value);
    return err;
}

int
mfp_action_status( const char *streamname,
	tstring **ret_output)
{
    int err = 0;
    char *node_name = NULL;
    char filename[256] ;
    char dest_filename[256] ;
    tstring *t_media_encap_type = NULL;
    FILE  *fp = NULL;
    char  date_str[256];
    char *ret_string = NULL;
    
    if(!streamname || ! ret_output) {
	ret_string = smprintf("error: unable to find the stream-name\r\n");
     	goto bail;
    }

    time_t cur_time = time(NULL);
    struct tm *loctime = localtime(&cur_time);
    strftime(date_str, 256, "%Y%m%d_%H:%M:%S", loctime);


    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap", streamname);
    bail_null(node_name);

    err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
	    &t_media_encap_type, node_name, NULL);

    if( t_media_encap_type && ts_length(t_media_encap_type) > 0) {
	/*If the prefix is file then it is File Media Source*/
	if( (ts_cmp_prefix_str( t_media_encap_type, "file_", false)) == 0) {
	    /* File media source*/
 	    /* Create a file media source PMF*/
 	    snprintf( filename, sizeof(filename), "/var/log/action_status_file_%s_%s.xml", streamname, date_str);
 	    snprintf( dest_filename, sizeof(dest_filename), "/nkn/vpe/file/action_status_file_%s_%s.xml", streamname, date_str);
         } else {
             /* Live media source*/
             /* Create a file media source PMF*/
             snprintf( filename, sizeof(filename), "/var/log/action_status_live_%s_%s.xml", streamname, date_str);
             snprintf( dest_filename, sizeof(dest_filename), "/nkn/vpe/live/action_status_live_%s_%s.xml", streamname, date_str);
         }
 	    fp = fopen( filename, "w");
 	    if(!fp) {
 		lc_log_basic(LOG_NOTICE, "Failed to open file:%s", filename);
 		goto bail;
 	    }
 	    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<PubManifest version=\"1.0\" name=\"%s\" >\r\n<ControlMediaSource>\r\n<Action>STATUS</Action>\r\n</ControlMediaSource>\r\n</PubManifest>", streamname);
 	    fclose(fp);
	    fp = NULL;
 	    err = lf_copy_file( filename, dest_filename, fco_ensure_dest_dir);
 	    bail_error(err);
 	    err = lf_remove_file(filename);
 	    bail_error(err);
	    ret_string = smprintf("STATUS Action Stream PMF created for session:%s\r\n", streamname); 
 	    //ret_string = smprintf("Action Stream PMF created:%s\r\n",dest_filename);
    } else {
	/* unknown streamname message*/
	ret_string = smprintf("Stream:%s doesnt exist or mediasource type isnt set\r\n", streamname);
	err = 1;
    }
bail:
    if(err) {
	if(fp) {
	    fclose(fp);
	    fp = NULL;
	    err = lf_remove_file(filename);
	    err = 1;
	}
    }
    if(ret_string) {
	err = ts_new_str_takeover(ret_output, &ret_string, -1, -1);
    }
    safe_free(node_name);
    ts_free(&t_media_encap_type);
    return err;
}

int
mfp_action_stop( const char *streamname,
	tstring **ret_output)
{
    int err = 0;
    char *node_name = NULL;
    char filename[256] ;
    char dest_filename[256] ;
    tstring *t_media_encap_type = NULL;
    tstring *t_session_value = NULL;
    FILE  *fp = NULL;
    char  date_str[256];
    char *ret_string = NULL;

    if(!streamname || ! ret_output) {
	ret_string = smprintf("error: unable to find the stream-name\r\n");
     	goto bail;
    }

    time_t cur_time = time(NULL);
    struct tm *loctime = localtime(&cur_time);
    strftime(date_str, 256, "%Y%m%d_%H:%M:%S", loctime);
    node_name = smprintf("/nkn/nvsd/mfp/config/%s/media-encap", streamname);
    bail_null(node_name);

    err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
 	    			&t_media_encap_type, node_name, NULL);
    bail_error(err);

    /*Get the current session name*/
    node_name = smprintf("/nkn/nvsd/mfp/config/%s/name", streamname);
    bail_null(node_name);
    err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
 	    			&t_session_value, node_name, NULL);
    bail_error(err);

    if( t_media_encap_type && ts_length(t_media_encap_type) > 0) {
	/*If the prefix is file then it is File Media Source*/
	if( (ts_cmp_prefix_str( t_media_encap_type, "file_", false)) == 0) {
	    /* File media source*/
	    /* Create a file media source PMF*/
	    snprintf( filename, sizeof(filename), "/var/log/action_stop_file_%s_%s.xml", streamname, date_str);
	    snprintf( dest_filename, sizeof(dest_filename), "/nkn/vpe/file/action_stop_file_%s_%s.xml", streamname, date_str);
	} else {
	    /* Live media source*/
	    /* Create a file media source PMF*/
	    snprintf( filename, sizeof(filename), "/var/log/action_stop_live_%s_%s.xml", streamname, date_str);
	    snprintf( dest_filename, sizeof(dest_filename), "/nkn/vpe/live/action_stop_live_%s_%s.xml", streamname, date_str);
	}
	fp = fopen( filename, "w");
	if(!fp) {
	    lc_log_basic(LOG_NOTICE, "Failed to open file:%s", filename);
	    goto bail;
	}
	fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<PubManifest version=\"1.0\" name=\"%s\" >\r\n<ControlMediaSource>\r\n<Action>STOP</Action>\r\n</ControlMediaSource>\r\n</PubManifest>", ts_str(t_session_value));

 	fclose(fp);
	    fp = NULL;
	err = lf_copy_file( filename, dest_filename, fco_ensure_dest_dir);
	bail_error(err);
	err = lf_remove_file(filename);
	bail_error(err);
	ret_string = smprintf(" STOP Action Stream PMF created for session:%s\r\n", streamname); 
	//ret_string = smprintf("Action Stream PMF created:%s\r\n",dest_filename); 
    } else {
	/* unknown streamname message*/
	ret_string = smprintf("Stream:%s doesnt exist or mediasource type isnt set\r\n", streamname);
	err = 1;
    }


bail:
    if(err) {
	if(fp) {
	    fclose(fp);
	    fp = NULL;
	    err = lf_remove_file(filename);
 	    err = 1;
	}
    }
    if(ret_string) {
	err = ts_new_str_takeover(ret_output, &ret_string, -1, -1);
    }
    safe_free(node_name);
    ts_free(&t_media_encap_type);
    ts_free(&t_session_value);
    return err;
}
int
mfp_check_if_valid_mount( tstring * t_storage, uint32 *mount_error, uint8_t *mount_type)
{
    int err = 0;
    const char *mount_name = NULL;
    tstr_array *mount_names = NULL;
    int32 code = 0;
    node_name_t node_name = {0};
    tstring *type = NULL;
    /* set this to nfs */
    *mount_type = 0;

    if( !t_storage || !mount_error) {
	goto bail;
    }
    err = ts_tokenize(t_storage, '/', 0, 0, 
	    ttf_ignore_leading_separator |
	    ttf_ignore_trailing_separator |
	    ttf_single_empty_token_null, &mount_names);
    bail_null(mount_names);

    mount_name = tstr_array_get_str_quick(mount_names, 0);
    bail_null(mount_name);

    snprintf(node_name, sizeof(node_name), "/nkn/nfs_mount/config/%s/type", mount_name);
    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
	    &type, node_name, NULL);
    if (type && (0 == strcmp(ts_str(type), "disk"))) {
	code = 0;
	*mount_type = 1;
    } else {
	err = lc_launch_quick_status(&code, NULL, false, 2,
		"/opt/nkn/bin/mgmt_nfs_mountstat.sh",
		mount_name);
    }
    if(code) {
	*mount_error = code;
    }
	

bail: 
    if(code){
	err =1;
    }
    ts_free(&type);
    tstr_array_free(&mount_names);
    return err;
}
/*
 * BUG FIX: 6676
 *  Escape sequence the space in the address - storage url, delivery url,
 *  server manifest-url and storage path for all the MFP encap types.
 *  Update: Mar-01-2011- Commenting out this call as XML parser itselfd is escape 
 *  sequencing the space.
 */
int
mfp_escape_sequence_space_in_address( tstring *t_address)
{
    int err = 0;
    int32 initial_offset = 0;
    
    while( initial_offset < ts_length(t_address)) {
	int32 ret_offset = 0;
	err = ts_find_char(t_address, initial_offset, ' ' , &ret_offset);
	bail_error(err);
	if(ret_offset > 0) {
	    err = ts_insert_char(t_address, ret_offset, 0x05c );
	    initial_offset = ret_offset + 2;
	}else {
	    break;
	}
    }

bail:
    return err;
}
int
mfp_action_restart( const char *streamname,
	tstring **ret_output)
{
    int err = 0;
    char *ret_string = NULL;

    if(!streamname || ! ret_output) {
	ret_string = smprintf("error: unable to find the stream-name\r\n");
     	goto bail;
    }

    /* Create a new name for MFP */
    err = mfp_generate_stream_pmf_file(streamname, ret_output);



bail:
    if(ret_string) {
	 ts_new_str_takeover(ret_output, &ret_string, -1, -1);
    }
    return err;
}

int
mfp_get_status( int32 *file_published)
{
    int err = 0;
    char filename_file[256] ;
    char dest_file[256] ;
    char filename_live[256] ;
    char dest_live[256] ;
    FILE  *fp_file = NULL;
    FILE  *fp_live = NULL;
    char  date_str[256];
    tstr_array *session_list = NULL;
    tbool isfile = false;
    uint32 num_sessions = 0;
    char binding_name[256];
    tstring *t_media_type =NULL;
    tstring *t_current_name = NULL;
    tstring *t_current_status = NULL;
    uint32 idx;
    
    time_t cur_time = time(NULL);
    struct tm *loctime = localtime(&cur_time);
    strftime(date_str, 256, "%Y%m%d_%H:%M:%S", loctime);
    
    err = mfp_get_list_of_sessions(&session_list, isfile);
    bail_null(session_list);
    num_sessions = tstr_array_length_quick(session_list);
    if(num_sessions > 0) {
	/*If success then  PMF with status request was published,*/
	*file_published = 1;
	/* Create a control PMF for file session status */
	snprintf( filename_file, sizeof(filename_file), "/var/log/action_status_file_all_%s.xml", date_str);
	snprintf( dest_file, sizeof(dest_file), "/nkn/vpe/file/action_status_file_all_%s.xml", date_str);
	snprintf( filename_live, sizeof(filename_live), "/var/log/action_status_live_all_%s.xml", date_str);
	snprintf( dest_live, sizeof(dest_live), "/nkn/vpe/live/action_status_live_all_%s.xml", date_str);
	fp_file = fopen( filename_file, "w");
	if(!fp_file) {
	    lc_log_basic(LOG_NOTICE, "Failed to open file:%s", filename_file);
	    goto bail;
	}
	fp_live = fopen( filename_live, "w");
	if(!fp_live) {
	    lc_log_basic(LOG_NOTICE, "Failed to open file:%s", filename_live);
	    goto bail;
	}
	fprintf(fp_file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<PubManifest version=\"1.0\" name=\"\">\r\n<ControlMediaSource>\r\n<Action>STATUS_LIST</Action>\r\n");
	fprintf(fp_file, "<SessionList>\r\n");
	fprintf(fp_live, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<PubManifest version=\"1.0\" name=\"\">\r\n<ControlMediaSource>\r\n<Action>STATUS_LIST</Action>\r\n");
	fprintf(fp_live, "<SessionList>\r\n");
	for(idx = 0; idx < num_sessions; ++idx) {
	    const char *session_name = tstr_array_get_str_quick(session_list, idx);
	    snprintf(binding_name, sizeof(binding_name), "/nkn/nvsd/mfp/config/%s/media-type", session_name);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL, &t_media_type, binding_name, NULL);
	    bail_error(err);
	    snprintf(binding_name, sizeof(binding_name), "/nkn/nvsd/mfp/config/%s/name", session_name);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL, &t_current_name, binding_name, NULL);
	    bail_error(err);
	    snprintf(binding_name, sizeof(binding_name), "/nkn/nvsd/mfp/config/%s/status", session_name);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL, &t_current_status, binding_name, NULL);
	    bail_error(err);
	    /*Don't ask for the stream status that hasnt been published*/
	    if(ts_equal_str( t_current_status, "NEW", false)){
		continue;
	    }
	    if (ts_equal_str(t_media_type, "File", false))
	    {
		if(!t_current_name){
		    continue;
		}
		fprintf(fp_file, "<name>%s</name>\r\n", ts_str(t_current_name));
	    }else if (ts_equal_str(t_media_type, "Live", false))
	    {
		if(!t_current_name){
		    continue;
		}
		fprintf(fp_live, "<name>%s</name>\r\n", ts_str(t_current_name));
	    }
	}
	fprintf(fp_file, "</SessionList>\r\n</ControlMediaSource>\r\n</PubManifest>\r\n");
	fclose(fp_file);
	    fp_file = NULL;
	fprintf(fp_live, "</SessionList>\r\n</ControlMediaSource>\r\n</PubManifest>\r\n");
	fclose(fp_live);
	    fp_live = NULL;
	err = lf_copy_file( filename_file, dest_file, fco_ensure_dest_dir);
	bail_error(err);
	err = lf_copy_file( filename_live, dest_live, fco_ensure_dest_dir);
	bail_error(err);
	err = lf_remove_file(filename_file);
	bail_error(err);
	err = lf_remove_file(filename_live);
	bail_error(err);
    }


bail:
    if(err) {
	if(fp_file) {
	    fclose(fp_file);
	    fp_file = NULL;
	    err = lf_remove_file(filename_file);
	    err = 1;
	}
	if(fp_live) {
	    fclose(fp_live);
	    fp_live = NULL;
	    err = lf_remove_file(filename_live);
	    err = 1;
	}
	*file_published = 0;
    }
    tstr_array_free(&session_list);
    ts_free(&t_media_type);
    ts_free(&t_current_name);
    ts_free(&t_current_status);
    return err;
}


static int mfp_status_timer_context_allocate(
	lt_dur_ms  poll_frequency,
	mfp_status_timer_data_t **ret_context)
{
    int err = 0;
    int i = 0;
    mfp_status_timer_data_t *context = NULL;
    if (ret_context)
	*ret_context = NULL;

    context = &g_mfp_get_status;
    bail_null(context);
    context->callback = mfp_poll_timeout_handler;
    context->poll_frequency = poll_frequency;
    lc_log_basic(LOG_NOTICE, _("Timer created for getting status with poll period of %d ms."),
	    (unsigned int) poll_frequency);
    err = mfp_set_poll_timer(context);
    bail_error(err);
    if (ret_context)
	*ret_context = context;
bail:
    return err;
}

int mfp_set_poll_timer(mfp_status_timer_data_t *context)
{
    int err = 0;
    err = lew_event_reg_timer(mfp_lwc,
	    &(context->poll_event),
	    (context->callback),
	    (void *) (context),
	    (context->poll_frequency));
    bail_error(err);
bail:
    return err;
}

int mfp_poll_timeout_handler(int fd,
	short event_type,
	void *data,
	lew_context *ctxt)
{
    int err = 0;
    int32 ret_code = 0;
    tstring *msg = NULL;

    mfp_status_timer_data_t *context = (mfp_status_timer_data_t *) (data);
    bail_null(context);
    
    lc_log_basic(LOG_INFO, _("Getting status for MFP session.\n"));
    /* Send control PMF to get all session status */
    err = mfp_get_status(&ret_code);
    /*Start directory watch*/
    if(ret_code) {
	err = mfp_watch_dir_context_init();
    }
    
    if ( context->poll_frequency > 0 ) {
	err = mfp_set_poll_timer(context);
	bail_error(err);
    }
bail:
    ts_free(&msg);
    return err;

}
int mfp_status_delete_poll_timer(mfp_status_timer_data_t *context)
{
    int err = 0;
    err = lew_event_delete(mfp_lwc, &(context->poll_event));
    complain_error(err);
bail:
    return err;
}
int mfp_status_context_init(void)
{
    int err = 0;
    tstring *t_timer = NULL;
    int timer_value = 1;
    mfp_status_timer_data_t *mfp_stat = NULL;
    bn_binding_array *bindings = NULL;
    tbool rechecked_licenses = false;

    memset(&(g_mfp_get_status), 0, sizeof(mfp_status_timer_data_t));
    g_mfp_get_status.index = 0;
    g_mfp_get_status.context_data = &(g_mfp_get_status);
    
    err = mdc_get_binding_children(mfp_mcc,
	    NULL, NULL, true, &bindings, true, true,
	    mfp_status_node_binding);
    bail_error_null(err, bindings);
    err = bn_binding_array_foreach(bindings, mfp_config_handle_change,
                               &rechecked_licenses);
    bail_error(err);

#if 0
    err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
	    &t_timer, NULL,
	    "/nkn/nvsd/mfp/status-time");
    bail_error(err);
    if(t_timer && ts_length(t_timer) > 0) {
	timer_value = atoi(ts_str(t_timer));
        err = mfp_status_timer_context_allocate(timer_value*60000, &mfp_stat);
    }
        err = mfp_status_timer_context_allocate(timer_value*60000, &mfp_stat);
#endif

bail:
    bn_binding_array_free(&bindings);
    return err;
}

int mfp_directory_watch(int fd,	short event_type,
	void *data, lew_context *ctxt)
{
    int err = 0;
    /*This filename have to be seperate for file and live*/
    const char *file_sess_stat = "/nkn/vpe/stat/file_session_status_list.xml";
    const char *live_sess_stat = "/nkn/vpe/stat/live_session_status_list.xml";
    mfp_watch_dir_t *watch_data = (mfp_watch_dir_t *)data;
    lc_log_basic(LOG_DEBUG, _("Entering directory watch\n"));
    /*Check for events*/
    /*Calling parser for file status*/
    err = mfp_parse_status_file(file_sess_stat);
    if(err) {
	/*Ignore error since live session status is needed*/
	lc_log_basic(LOG_INFO, _("Failed for File status parsing:%s\n"), file_sess_stat);
    }
    /*Calling parser for live status*/
    err = mfp_parse_status_file(live_sess_stat);
    if(err) {
	lc_log_basic(LOG_INFO, _("Failed for Live status parsing:%s\n"), live_sess_stat);
    }

    err = lew_event_delete(mfp_lwc, &(watch_data->poll_event));
bail:
    return err;
}

static int
mfp_get_list_of_sessions(
	tstr_array **sess_names, tbool isfile)
{
    int err = 0;
    tstr_array *names = NULL;
    tstr_array_options opts;
    tstr_array *names_config = NULL;
    
    bail_null(sess_names);
    *sess_names = NULL;
    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);
    opts.tao_dup_policy = adp_delete_old;
    err = tstr_array_new(&names, &opts);
    bail_error(err);
    err = mdc_get_binding_children_tstr_array(mfp_mcc,
	    NULL, NULL, &names_config,
	    "/nkn/nvsd/mfp/config", NULL);
    bail_error_null(err, names_config);

    err = tstr_array_concat(names, &names_config);
    bail_error(err);
    *sess_names = names;
    names = NULL;
bail:
    tstr_array_free(&names);
    tstr_array_free(&names_config);
    return err;
}

/* Parser function*/

int mfp_parse_status_file(const char *status_file)
{
    int err = 0;
    FILE *fp =NULL;
    xmlDoc *doc = NULL;
    xmlNode *root = NULL;
    xmlXPathContext *xpath = NULL;

    xmlNode *node;
    size_t pmf_size;
    uint8_t *p_pmf_data = NULL;
    xmlXPathObject *xpath_obj = NULL;
    xmlXPathObject *xpath_stat = NULL;
    int idx;
    uint32 j;
    char *sessName = NULL;
    char *sessStat = NULL;
    tstr_array *session_list = NULL;
    tbool isfile = false;
    uint32 num_sessions = 0;
    char binding_name[256];
    tstring *t_current_name = NULL;



    /* open the pmf file */
    fp = fopen(status_file, "rb");
    if (!fp) {
	goto bail;
    }
    /* find the size of the pmf file */
    fseek(fp, 0, SEEK_END);
    pmf_size = ftell(fp);
    rewind(fp);
    /* allocate memory for mfp data */
    p_pmf_data =  (uint8_t *)malloc(pmf_size);
    if (!p_pmf_data) {
	goto bail;
    }
    /* read the pmf file */
    fread(p_pmf_data, 1, pmf_size, fp);
    
    /* create an xml parser context for the mfp data */
    doc = xmlReadMemory((char*)p_pmf_data, pmf_size, "noname.xml",
	    NULL, XML_PARSE_NOBLANKS);
    if(!doc) {
	goto bail;
    }
    root = xmlDocGetRootElement(doc);
    xpath = xmlXPathNewContext(doc);

    xpath_obj = xmlXPathEvalExpression((xmlChar *)"//ControlResponse/SessionList/sess/@name", xpath);
    if(!xpath_obj || !xpath_obj->nodesetval) {
	goto bail;
    }
    if (!xpath_obj->nodesetval->nodeNr ) {
	xmlXPathFreeObject(xpath_obj);
	goto bail;
    }
    xpath_stat = xmlXPathEvalExpression((xmlChar *)"//ControlResponse/SessionList/sess/@status", xpath);
    if(!xpath_stat|| !xpath_stat->nodesetval) {
	goto bail;
    }
    if (!xpath_stat->nodesetval->nodeNr ) {
	xmlXPathFreeObject(xpath_stat);
	goto bail;
    }
    err = mfp_get_list_of_sessions(&session_list, isfile);
    bail_null(session_list);
    num_sessions = tstr_array_length_quick(session_list);

    for(idx = 0; idx < xpath_obj->nodesetval->nodeNr; idx ++) {
	sessName = (char *)(xpath_obj->nodesetval->nodeTab[idx]->children->content);
	sessStat = (char *)(xpath_stat->nodesetval->nodeTab[idx]->children->content);
	for( j = 0; j < num_sessions; j++) {
	    const char *session_name = tstr_array_get_str_quick(session_list, j);
	    snprintf(binding_name, sizeof(binding_name), "/nkn/nvsd/mfp/config/%s/name", session_name);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL, &t_current_name, binding_name, NULL);
	    bail_error(err);
	    snprintf(binding_name, sizeof(binding_name), "/nkn/nvsd/mfp/config/%s/status", session_name);
	    bail_error(err);
	    if (ts_equal_str(t_current_name, sessName, false))
	    {
		err = mdc_set_binding(mfp_mcc, NULL, NULL,
			bsso_modify,bt_string, sessStat,
			binding_name);
		bail_error(err);
		break;
	    }
	}
    }
bail:
    ts_free(&t_current_name);
    /*Not sure if we have free these */
#if 0
    if(xpath) {
    xmlXPathFreeContext(xpath);
    }
    if(xpath_obj) {
	xmlXPathFreeObject(xpath_obj);
    }
    if(xpath_stat) {
	xmlXPathFreeObject(xpath_stat);
    }
#endif
    if(doc) {
	xmlFreeDoc(doc);
    }
    if(fp){
	fclose(fp);
	fp = NULL;
    }
    if(p_pmf_data) {
	safe_free(p_pmf_data);
    }
    return err;
}

int
mfp_config_handle_change(const bn_binding_array *arr, uint32 idx,
	                        bn_binding *binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tstring *retmsg = NULL;
    tbool *rechecked_licenses_p = data;
    mfp_status_timer_data_t *context = NULL;
    
    err = bn_binding_get_name(binding, &name);
    bail_error(err);
    if (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/mfp/status-time")) {
	uint32 t_time = 0;
	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_time);
	if(t_time > 0) {
	    err = mfp_status_delete_poll_timer(&g_mfp_get_status);
	    bail_error(err);
	    err = mfp_status_timer_context_allocate(t_time*60*1000, &context);
	    bail_error(err);
	}

    }
bail:
    ts_free(&retmsg);
    return err;
}
int mfp_watch_dir_context_init(void)
{
    int err = 0;
    mfp_watch_dir_t *watch_context = NULL;
    memset(&(g_watch_dir), 0, sizeof(mfp_watch_dir_t));
    g_watch_dir.index = 0;
    g_watch_dir.context_data = &(g_watch_dir);
    //g_watch_dir.fail_count = 0;
    //g_watch_dir.inotify_watch_fd = -1;
    //g_watch_dir.inotify_watch_wd = -1;

    watch_context = &g_watch_dir;
    bail_null(watch_context);
    watch_context->callback = mfp_directory_watch;
    watch_context->poll_frequency = 2000; // 2sec timer. Random number
    //watch_context->inotify_watch_fd = inotify_init();
    //watch_context->inotify_watch_wd = inotify_add_watch
//	(watch_context->inotify_watch_fd, "/nkn/vpe/stat", IN_CLOSE_WRITE);

    err = mfp_set_directory_watch_timer(watch_context);
bail:
    return err;
}

int mfp_set_directory_watch_timer(mfp_watch_dir_t *context)
{
    int err = 0;
    err = lew_event_reg_timer(mfp_lwc,
	    &(context->poll_event),
	    (context->callback),
	    (void *) (context),
	    (context->poll_frequency));
    bail_error(err);
bail:
    return err;
}

int
mfp_generate_encap_file_mp4( FILE *fp, const char *streamname,
				tstring **ret_output)
{
    int err  = 0;
    node_name_t node_name = {0};
    tstr_array *profile_config = NULL;
    uint32 i = 0;
    const char *profile_id = NULL;
    tstring *t_src = NULL;
    tstring *t_bitrate = NULL;
    tstring *t_protocol = NULL;
    char *ret_string = NULL;

    if(!fp || !streamname) {
	/*Error*/
	ret_string = smprintf("Filename or streamname not present\r\n");
	goto bail;
    }
    snprintf(node_name, sizeof(node_name), "/nkn/nvsd/mfp/config/%s/media-encap/file_mp4", streamname);
    err = mdc_get_binding_children_tstr_array(mfp_mcc,
            NULL, NULL, &profile_config,
	    node_name, NULL);
    bail_error_null(err, profile_config);

    /*If no profile is added, throw an error*/
    if(tstr_array_length_quick(profile_config)== 0){
	ret_string = smprintf("Error: No input profile configured\n");
	goto bail;
    }
    fprintf(fp, "<MP4_Encapsulation>\r\n");
    for(;i < tstr_array_length_quick(profile_config); i++) {
	profile_id = tstr_array_get_str_quick(profile_config, i);
	snprintf(node_name, sizeof(node_name), "/nkn/nvsd/mfp/config/%s/media-encap/file_mp4/%s/source", streamname, profile_id);
	/*get source*/
	err = mdc_get_binding_tstr (mfp_mcc, NULL, NULL, NULL,
		&t_src, node_name, NULL);
	if(t_src && (ts_length(t_src) > 0 )) {
	    snprintf(node_name, sizeof(node_name), "/nkn/nvsd/mfp/config/%s/media-encap/file_mp4/%s/protocol", streamname, profile_id);
	    err = mdc_get_binding_tstr(mfp_mcc, NULL, NULL, NULL,
		    &t_protocol, node_name, NULL);
	    if(t_protocol && (ts_equal_str(t_protocol, "nfs", false))) {
		fprintf(fp, "<MP4_Media>\r\n<ProfileId>%s</ProfileId>\r\n<Src>/nkn/nfs/%s</Src>\r\n",
			profile_id, ts_str(t_src));
	    }else if(t_protocol && (ts_equal_str(t_protocol, "http", false))) {
		fprintf(fp, "<MP4_Media>\r\n<ProfileId>%s</ProfileId>\r\n<Src>http://%s</Src>\r\n",
			profile_id, ts_str(t_src));
	    }else {
		fprintf(fp, "<MP4_Media>\r\n<ProfileId>%s</ProfileId>\r\n<Src>%s</Src>\r\n",
			profile_id, ts_str(t_src));
	    }
	    snprintf(node_name, sizeof(node_name), "/nkn/nvsd/mfp/config/%s/media-encap/file_mp4/%s/bitrate", streamname, profile_id);
	    err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		    &t_bitrate, NULL,
		    node_name, NULL);
	    if(t_bitrate && (ts_length(t_bitrate) > 0 )) {
		if( !(ts_equal_str(t_bitrate, "0", false)) ){
		    fprintf(fp, "<BitRate>%s</BitRate>\r\n", ts_str(t_bitrate));
		}
	    }
	    fprintf(fp, "</MP4_Media>\r\n");
	} else {
	    /* Profile source not set*/
	    ret_string = smprintf("Error:Profile input file path not set\r\n");
	    goto bail;
	}
	ts_free(&t_src);
	ts_free(&t_bitrate);
	ts_free(&t_protocol);
    }
    fprintf(fp, "</MP4_Encapsulation>\r\n");

bail:
    if(ret_string){
	err = ts_new_str_takeover(ret_output, &ret_string, -1, -1);
	err = 1;
    }
    return err;
}
