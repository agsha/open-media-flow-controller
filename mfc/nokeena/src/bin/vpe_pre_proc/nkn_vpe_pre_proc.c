/*
 *
 * Filename:  nkn_vpe_pre_proc.c
 * Date:      2009/02/06
 * Module:    pre - processing for Smooth Flow
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#define __USE_BSD
#include <sys/time.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <getopt.h>
#include <math.h>
#include <stdlib.h>
#include <malloc.h>

//nkn includes
#include "nkn_vpe_file_io.h"
#include "nkn_vpe_flv.h"
#include "nkn_vpe_bitstream.h"
#include "nkn_vpe_backend_io_plugin.h"
#include "nkn_vpe_pre_proc.h"
#include "nkn_vpe_metadata.h"
#include "nkn_vpe_codec_handlers.h"
#include "nkn_vpe_error_no.h"
#include "nkn_vpe_flv_parser.h"
#include "nkn_vpe_types.h"

#define BUFFERED_MEMORY_IO	0
#define FILE_IO 		1
#define MAX_PATH                256
#define PROFILE_GUARD_BAND      0.15

#define AFR_TBL_TYPE_MOVE       1
#define AFR_TBL_TYPE_NKN        2

#define AFR_QSTART_RATE_THRESHOLD 150000

//Prototypes
int qsort_compare(const void *a, const void *b);
static int afr_tbl_type = AFR_TBL_TYPE_NKN;
static float afr_qstart_threshold = AFR_QSTART_RATE_THRESHOLD;

/**Print Usage
 */
void print_usage(void){

    printf("nkn_flv_pre_proc [options] -add <input_file> .... -new <output_file>\n");
    printf("long opts\tshort opts\t\tdescription\n");
    printf("-----------------------------------------------------\n");
    printf("-np\t\t-n\t\tno of encoded bitrates\n");
    printf("-add\t\t-a\t\tinput video name, the number of file should be same as np\n");
    printf("-io_mode\t-i\t\tI/O backend choice 0-Buffered Memory 1-File I/O\n");
    printf("-bitrate\t-b\t\tbitrates in kbps, there should be 'np' bitrates\n");
    printf("-new\t\t-o\t\toutput video name; this will be suffixed with _pX_ABC where\n\
          \t\t\tX is profile number and ABC is the chunk number\n");
    printf("-fps\t\t-f\t\tframes per second\n");
    printf("-hint\t\t-t\t\tgenerate trick play hint track\n");
    printf("-scratch\t-s\t\ttemporary storage area for the pre-processed videos before\n\
            \t\t\tingestion\n");
    printf("\nTRICK PLAY SPECIFIC PARAMS\n");
    printf("----------------------------\n");
    printf("-speed\t\t-x\t\tplayback speed scaling factor\n");
    printf("-dirn\t\t-r\t\tdirection of playback '1' for forward '2' for reverse\n");
    printf("\nADVANCED USAGE\n");
    printf("--------------\n");
    printf("-afr\t\t-y\t\ttype of AFR table to write, '1' for MOVE like table and '2' for NKN like table See Explanation Below for information on AFR Tables\n");
    printf("-qt\t\t-q\t\tSee Explanation Below for information on AFR Tales and Thresholds\n");
  

    printf("\nAFR TABLES and Thresholds\n");
    printf("------------------------\n");
    printf("AFR Table Type 1 '-afr 1' - All values below a threshold have their AFR's pointing to the next available profile bitrate. AFR for profiles above the threshold is set to the 60%% mark between the succesive profiles. The threshold '-qt' should be in bytes/second \n");
    printf("AFR Table Type 2 '-afr 2' - AFR is set to 15%% more than the current profile bitrate, threshold '-qt' is ignored for this type\n");
    printf("AFR Table Type 3 '-afr 3' - AFR is multiplied by a fixed multiplier specified by '-qt'\n");

}

int MAIN(int argc, char *argv[]){

    FILE **f_in;
    uint8_t n_profiles;
    io_funcs iof;
    unsigned int i, j;
    struct timeval start, end;
    struct timezone tzp;
    int st_arg;
    int rv;
    char ch;
    char io_mode;
    int option_index;
    char *output_fname, *scratch_path, meta_fname[MAX_PATH];
    int chunk_duration;
    int st, error_no;
    double elapsed;
    codec_handler cd_hdl;
    meta_data md, **p_md_copy;
    uint32_t fps;
    char tplay_hint;
    int tplay_speed, tplay_direction;
    void *meta_desc;
    profile_order *po;
    struct stat stat_buf;
    int log_mask, log_opened;
    size_t tot_size;
    int32_t container_type;
    void *private_data;
    char *uri_prefix;

#ifdef UNIT_TEST
    void *tmp_hint, *tmp_out;
#endif

    const struct option long_options[] = {
	{ "np",    1, NULL, 'n' },
	{ "add",   1, NULL, 'a'},
	{ "io_mode", 1, NULL, 'i'},
	{ "new", 1, NULL, 'o'},
	{ "duration", 1, NULL, 'd'},
	{ "help", 0, NULL, 'h'},
	{ "scratch", 1, NULL, 's'},
	{ "bitrate", 1, NULL, 'b'},
	{ "fps", 1, NULL, 'f' },
	{ "hint", 0, NULL, 't'},
	{ "speed", 1, NULL, 'x'},
	{ "dirn", 1, NULL, 'r'},
	{ "log", 1, NULL, 'l'},
	{ "afr", 1, NULL, 'y'},
	{ "qt", 1, NULL, 'q'},
	{ "uri", 1, NULL, 'u'},
	{ NULL,       0, NULL, 0 }   /* Required at end of array.  */
    };

    //Initialization
    io_mode = FILE_IO;
    rv = 0;
    st_arg = 2;
    i = 0;
    j = 0;
    option_index = 0;
    f_in = NULL;
    chunk_duration = 0;
    output_fname = NULL;
    st =0;
    error_no =0;
    elapsed = 0.0;
    fps = 30;
    tplay_hint = 0;
    p_md_copy = NULL;
    scratch_path = NULL;
    tplay_speed = 2;
    tplay_direction = 1;
    po = NULL;
    log_mask = 3;
    n_profiles = 0;   
    tot_size = 0;
    container_type = 0;
    log_opened = 0;
    private_data = NULL;
    uri_prefix = NULL;

    memset(&stat_buf, 0, sizeof(struct stat));
    memset(&start, 0, sizeof(start));
    memset(&end, 0, sizeof(end));
    memset(&tzp, 0, sizeof(tzp));
    memset(&iof, 0, sizeof(iof));
    memset(&md, 0, sizeof(meta_data));


    st = clock();
    gettimeofday(&start, &tzp);

    //setup SYSLOG logging
    setlogmask(LOG_UPTO (log_mask) );
    openlog ("vpe_pre_proc", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    log_opened = 1;

    //default I/O mode is File I/O
    register_backend_io_funcs(nkn_open_file, NULL, NULL, nkn_read_file, nkn_write, nkn_seek, nkn_tell, nkn_get_size, 
			      nkn_close, NULL, nkn_fprintf, &iof);	

    //Initialize Codec Handlers
    VPE_PP_init_codec_handler(&cd_hdl, N_CODEC_HANDLERS);

    if(argc < 8){
	printf("Wrong number of arguments\n");
	print_usage();
	error_no = -1;
	goto cleanup;
    }


    while(1){
	ch = getopt_long_only(argc, argv, "n:a:i:d:o:b:f:ts:x:r:l:u:y:q", long_options, &option_index);

	if(ch == -1)
	    break;
    
	switch(ch){
	    case 'a':
		if(n_profiles <= 0){
		    printf("n_profile not initialized or initialized to zero!\n");
		    syslog (LOG_ERR,"%d: No of Profiles cannot be zero or absent & has to be the first input parameter!", NKN_VPE_WRONG_NO_OF_PROFILES);
		    error_no = NKN_VPE_WRONG_NO_OF_PROFILES;
		    goto cleanup;
		}
 
		container_type = get_container_type(optarg);
		f_in[i] = iof.nkn_create_descriptor_from_file(optarg, "rb");     
		p_md_copy[i]=NULL;
		if(f_in[i]==NULL){
		    printf("Invalid filename or path \n");
		    syslog (LOG_ERR,"%d: I/O error, unable to open file", NKN_VPE_INPUT_E_ACCESS);
		    error_no = NKN_VPE_INPUT_E_ACCESS;
		    goto cleanup;
		}
		i++;
		
		//init the copies to NULL in case something goes wrong even before the copy happens
	      	break;
	    case 'b':
		if(n_profiles <= 0){
		    printf("n_profile not initialized or initialized to zero!\n");
		    syslog (LOG_ERR,"%d: No of Profiles cannot be zero or absent & has to be the first input parameter!", NKN_VPE_WRONG_NO_OF_PROFILES);
		    error_no = NKN_VPE_WRONG_NO_OF_PROFILES;
		    goto cleanup;
		}	
		po[j].bitrate = atoi(optarg);
		po[j].profile_id = j;
		j++;
		break;
	    case 'f':
		md.fps = fps = atoi(optarg);
		break;
	    case 'n':
		n_profiles = atoi(optarg);
		if(n_profiles <= 0){
		    printf("n_profile not initialized or initialized to zero!\n");
		    syslog (LOG_ERR,"%d: No of Profiles cannot be zero or absent & has to be the first input parameter!", NKN_VPE_WRONG_NO_OF_PROFILES);
		    error_no = NKN_VPE_WRONG_NO_OF_PROFILES;
		    goto cleanup;
		}	
		f_in = (FILE**)calloc(n_profiles, sizeof(FILE*));
	       	p_md_copy = (meta_data**)calloc(n_profiles,  sizeof(meta_data*));
		po = (profile_order*)calloc(n_profiles,  sizeof(profile_order));
		if(!f_in || !p_md_copy || !po){
		    printf("Not enough free memory\n");
		    syslog (LOG_ERR,"%d: not enough free memory", NKN_VPE_E_MEM);
		    error_no = NKN_VPE_E_MEM;
		    goto cleanup;
		}

		break;
	    case 'i':
		io_mode = atoi(optarg);
            
		switch(io_mode){
		    case BUFFERED_MEMORY_IO:
			register_backend_io_funcs((fnp_create_descriptor_from_file)(open_bitstream), 
						  (fnp_create_raw_descriptor)(create_bitstream), 
						  NULL,
						  (fnp_read_data)(read_bitstream),
						  (fnp_write_data)(write_bitstream),
						  (fnp_seek)(seek_bitstream), 
						  (fnp_tell)(get_bitstream_offset),
						  (fnp_get_size)(get_bitstream_size),
						  (fnp_close_descriptor)(close_bitstream),
						  (fnp_write_raw_descriptor_to_file)(write_bitstream_to_file),
						  (fnp_formatted_write)(NULL),  &iof);
			break;
		    case FILE_IO:
			register_backend_io_funcs(nkn_open_file, NULL, NULL, nkn_read_file, nkn_write, nkn_seek, nkn_tell, nkn_get_size, 
						  nkn_close, NULL, nkn_fprintf, &iof);	
			break;
		    default:
			printf("Unsupported I/O mode\n");
			syslog (LOG_ERR,"%d: Unsupported IO mode", NKN_VPE_E_UNKNOWN_IO_HANDLER);			
			error_no = NKN_VPE_E_UNKNOWN_IO_HANDLER;
			goto cleanup;
		}
		break;
	    case 't':
		tplay_hint = 1;
		break;
	    case 'd':
		md.smooth_flow_duration = chunk_duration = atoi(optarg);
		break;
	    case 'o':
		md.video_name = output_fname = optarg;
		break;
	    case 's':
		scratch_path = optarg;
		break;
	    case 'x':
		tplay_speed = atoi(optarg);
		break;
	    case 'r':
		tplay_direction = ((atoi(optarg) == 1) * 1) + ((atoi(optarg) == 2) * -1);
		break;
	    case 'l':
		log_mask = atoi(optarg);
		break;
	    case 'y':
		afr_tbl_type = atoi(optarg);
		break;
	    case 'q':
		afr_qstart_threshold = atof(optarg);
		break;
	    case 'u':
		uri_prefix = optarg;
		uri_prefix = remove_trailing_slash(uri_prefix);
		break;
	    case 'h':
	    case '?':
		print_usage();
		break;
	}
    }

    //remove trailing slashes in the scratch area path
    scratch_path = remove_trailing_slash(scratch_path);

    //stat the scratch path for permissions
    stat(scratch_path, &stat_buf);

    /** Sanity Checking 
	a. check if scratch area has write permissions
	b. check if all the path names are valid strings and all paths have been entred
	c. chunk duration cannot be zero or negative
	d. no of input files and input bitrates dont match
	e. no of profiles and bitrate values dont match
	f. check if the AFR type is either 1 or 2 and not 0
    */
    if(!(access(scratch_path, W_OK)) && (!S_ISDIR(stat_buf.st_mode))){//used 'access' API to ascertain write permissions. Bug 1325
	syslog (LOG_ERR, "%d: Scratch Area does not have write permission or Scratch area is not a directory" , NKN_VPE_E_PERMISSION);
	printf("no write permission for the scratch area\n");
	scratch_path = NULL;
	error_no = NKN_VPE_E_PERMISSION;
	goto cleanup;
    }

    if(output_fname == NULL || scratch_path == NULL){
	syslog (LOG_ERR,"%d: input argument error output file name or scratch path missing, i cannot write the output to a file", NKN_VPE_E_ARG);
	printf("the URI, output name, scratch path  and path  must be entered\n");
	error_no = NKN_VPE_E_ARG;
	goto quit;
    }

    
    if(chunk_duration == 0){
	syslog (LOG_ERR,"%d: input argument error smooth flow duration not specified", NKN_VPE_E_ARG);
	printf("Smooth Flow duration must be specified\n");
	error_no = NKN_VPE_E_ARG;
	goto cleanup;
    }

    if(i != n_profiles){
	syslog (LOG_ERR,"%d: number of bitrates and number of input files dont match", NKN_VPE_E_OTHER);
	printf("not enought input bitrates\n");
	error_no = NKN_VPE_E_OTHER;
	goto cleanup;
    }

    if(i != j){
        syslog (LOG_ERR,"%d: not all input files have been assigned bitrate values use -b option for every input file", NKN_VPE_E_OTHER);
	error_no = NKN_VPE_E_OTHER;
	goto cleanup;
    }

    if(afr_tbl_type == 0 || afr_tbl_type > 3){
	syslog(LOG_ERR, "%d: AFR value must either be '1' or '2'", NKN_VPE_E_ARG);
	error_no = NKN_VPE_E_ARG;
	goto cleanup;

    }
    //end of sanity
    
    //sort the profiles in the ascending order
    qsort(po, n_profiles, sizeof(profile_order), qsort_compare);
    for(i = 0;i < n_profiles;i++){
	
	j = po[i].profile_id;

	//create copies of this object in case we need to scale these on threads
	//not a big overhead a few bytes only, eliminates need to expensive synchornization
	p_md_copy[i] = (meta_data*)calloc(1, sizeof(meta_data));

	if(!p_md_copy[i]){
	    syslog (LOG_ERR,"%d: unable to allocate memory", NKN_VPE_E_MEM);
	    error_no = NKN_VPE_E_MEM;
	    goto cleanup;
	}

	memcpy(p_md_copy[i], &md, sizeof(md));

	tot_size = tot_size + iof.nkn_get_size(f_in[j]);
	p_md_copy[i]->bitrate = po[i].bitrate;
	switch(container_type){
	    case 0:
		error_no = VPE_PP_chunk_flv_data((void*)f_in[j], io_mode, i, p_md_copy[i], scratch_path, &iof, &cd_hdl,tplay_hint, &private_data);
		p_md_copy[i]->avcc = (avcc_config*)(private_data);
		break;
	    case 1:
		error_no = VPE_PP_chunk_MP4_data((void*)f_in[j], io_mode, i, p_md_copy[i], scratch_path, &iof, &cd_hdl,tplay_hint, &private_data);
		break;
	    case 2:
		error_no = VPE_PP_chunk_TS_data((void*)f_in[j], io_mode, i, p_md_copy[i], scratch_path, &iof, &cd_hdl,tplay_hint, &private_data);
		break;
	}
	if(error_no < 0){
	    goto cleanup;
	}

#ifdef TPLAY_ENABLE	
	if(i == 0 && tplay_hint){
	    char hint_fname[MAX_PATH];
	    void *tmp_hint, *tmp_out;
	    snprintf(hint_fname, strlen(scratch_path)+strlen(p_md_copy[i]->video_name)+11, "%s/%s_p%02d.hint", scratch_path, p_md_copy[i]->video_name, i);
	    tmp_hint = iof.nkn_create_descriptor_from_file(hint_fname, "rb");
	    if(!tmp_hint){
		error_no = -1;
		goto cleanup;
	    }
	    snprintf(hint_fname, strlen(scratch_path)+12, "%s/txplay.flv", scratch_path);
	    tmp_out = iof.nkn_create_descriptor_from_file(hint_fname, "wb");
	    if(!tmp_out){
		error_no = -1;
		goto cleanup;
	    }
	    iof.nkn_seek(f_in[j], 0, SEEK_SET);
	    create_tplay_data_from_index(tmp_hint, f_in[j], tmp_out, &iof, 
					 tplay_speed, tplay_direction, 0);				
	    iof.nkn_close(tmp_out);
	} 
	if(f_in[j]){
	    iof.nkn_close(f_in[j]);
	    f_in[j] = NULL;
	}
#endif
    }

    //write meta file
    meta_desc = NULL;
    if(container_type < 2) { 
	snprintf(meta_fname, strlen(scratch_path)+strlen(p_md_copy[0]->video_name)+6, "%s/%s.dat", scratch_path, p_md_copy[0]->video_name);  
	meta_desc = (void*)fopen(meta_fname, "wb");
	
	if(!meta_desc){
	    syslog (LOG_ERR,"%d: I/O error, unable to open file", NKN_VPE_OUTPUT_E_ACCESS);
	    error_no = NKN_VPE_OUTPUT_E_ACCESS;
	    goto cleanup;
	}
	if(write_meta_data(meta_desc, p_md_copy, n_profiles)){
	    syslog (LOG_ERR,"%d: Unable to server client meta data", NKN_VPE_OUTPUT_E_ACCESS);
	    error_no = NKN_VPE_OUTPUT_E_ACCESS;
	    goto cleanup;
	}

    } else {
	write_m3u8_file(p_md_copy, n_profiles, output_fname, scratch_path, uri_prefix);
	
    }
	
	
    if(meta_desc) {
	fclose(meta_desc);
    }

    //write client meta data file
    if(container_type < 2) {
	snprintf(meta_fname, strlen(scratch_path)+strlen(p_md_copy[0]->video_name)+6, "%s/%s.xml", scratch_path, p_md_copy[0]->video_name);  
	meta_desc = (void*)fopen(meta_fname, "wb");
	if(!meta_desc){
	    syslog (LOG_ERR,"%d: I/O error, unable to open file", NKN_VPE_OUTPUT_E_ACCESS);
	    error_no = NKN_VPE_OUTPUT_E_ACCESS;
	    goto cleanup;
	}

	if(write_client_meta_data(meta_desc, p_md_copy, n_profiles)){
	    syslog (LOG_ERR,"%d: Unable to write client meta data", NKN_VPE_OUTPUT_E_ACCESS);
	    error_no = NKN_VPE_OUTPUT_E_ACCESS;
	    goto cleanup;
	}
	fclose(meta_desc);
	meta_desc = NULL;
    }

    if(log_opened)
	closelog();
    gettimeofday(&end, &tzp);
    elapsed = end.tv_sec - start.tv_sec + (end.tv_usec - start.tv_usec) / 1.e6;
    printf("Elapsed time %.9f seconds\n", elapsed);

    if(scratch_path && md.video_name){
	snprintf(meta_fname,strlen(scratch_path)+strlen(md.video_name)+6, "%s/%s.log", scratch_path, md.video_name);  
	meta_desc = (void*)fopen(meta_fname, "wb");
	if(!meta_desc){
	    syslog (LOG_ERR,"%d: I/O error, unable to open file", NKN_VPE_OUTPUT_E_ACCESS);
	    error_no = NKN_VPE_OUTPUT_E_ACCESS;
	    goto cleanup;
	}
	write_job_log(meta_desc, n_profiles, elapsed, scratch_path,  error_no, tot_size, p_md_copy[0]->n_chunks);
    }

    if(meta_desc) {
	fclose(meta_desc);
	meta_desc = NULL;
    }

    if(f_in){
	free(f_in);
	f_in = NULL;
    }

    for(i=0;i<n_profiles;i++){
	if(p_md_copy[i]){
	    if(p_md_copy[i]->avcc && p_md_copy[i]->avcc->p_avcc_data) {
		free(p_md_copy[i]->avcc->p_avcc_data);
	    }
	    free(p_md_copy[i]);
	    p_md_copy[i] = NULL;
	}
    }
    
    if(p_md_copy){
	free(p_md_copy);
	p_md_copy = NULL;
    }

    if(po){
	free(po);
	po = NULL;
    }
   
    VPE_PP_destroy_codec_handler(&cd_hdl);


    return error_no;

 cleanup:

    if(log_opened)
	closelog();

    gettimeofday(&end, &tzp);
    elapsed = end.tv_sec - start.tv_sec + (end.tv_usec - start.tv_usec) / 1.e6;
    printf("Elapsed time %.9f seconds\n", elapsed);

    if(scratch_path  && md.video_name){
	snprintf(meta_fname, strlen(scratch_path)+strlen(md.video_name)+6, "%s/%s.log", scratch_path, md.video_name);  
	meta_desc = (void*)fopen(meta_fname, "wb");
	if(!meta_desc){
	    syslog (LOG_ERR,"%d: I/O error, unable to open file", NKN_VPE_OUTPUT_E_ACCESS);
	    error_no = NKN_VPE_OUTPUT_E_ACCESS;
	    goto quit;
	}
	write_job_log(meta_desc, n_profiles, elapsed, scratch_path,  error_no, tot_size, md.n_chunks);
    }
  

 quit:
    if(f_in && error_no !=  0){;
	for(i=0;i<n_profiles;i++){
	    if(f_in[i]){
		iof.nkn_close(f_in[i]);
		f_in[i]=NULL;
	    }
	    if(p_md_copy[i]){
		if(p_md_copy[i]->avcc) {
		    if(p_md_copy[i]->avcc->p_avcc_data) {
			free(p_md_copy[i]->avcc->p_avcc_data);
			p_md_copy[i]->avcc->p_avcc_data = NULL;
		    }
		    free(p_md_copy[i]->avcc);
		    p_md_copy[i]->avcc = NULL;
		}
		free(p_md_copy[i]);
		p_md_copy[i]=NULL;
	    }
	}
    }

    if(f_in && error_no > 0){
	free(f_in);
	f_in = NULL;
    }

    if(p_md_copy){
	free(p_md_copy);
	p_md_copy = NULL;
    }

    VPE_PP_destroy_codec_handler(&cd_hdl);

    if(po){
	free(po);
	po=NULL;
    }

    return error_no;

}

int 
VPE_PP_chunk_MP4_data(void *in_de, char io_mode, uint32_t profile, meta_data *md, char *out_path, io_funcs *iof, codec_handler *cd, char thint, void **private_data)
{
    void *in_desc;
    unsigned char codec_id;
    unsigned char codec_found;
    int i, error_no;

    UNUSED_ARGUMENT(thint);

    in_desc = NULL;
    codec_id = 0;
    i = 0;
    codec_found = 0;
    error_no = 0;

    if(in_de==NULL)
	return -1;

    in_desc = in_de;
    codec_id = 0x24;

    md->container_id = CONTAINER_ID_MP4;
    md->codec_id = ACODEC_ID_AAC | VCODEC_ID_AVC;

    VPE_PP_add_codec_handler(cd,VPE_PP_mp4_frag_chunking, 0x24);

    for(i = 0; i < N_CODEC_HANDLERS;i++){
	if(codec_id == cd->codec_id[i] && !codec_found){
	    error_no=(cd->hdl[i])(in_de, io_mode, profile, md, out_path, iof, private_data);
	    if(error_no<0)
		return error_no;
	    codec_found = 1;
	}
    }
    

    if(!codec_found){
	printf("none of the registered codec handlers are matching for this Codec ID\n");	error_no = -1;
	return error_no;
    }

    return error_no;
}

int 
VPE_PP_chunk_flv_data(void *in_de, char io_mode, uint32_t profile, meta_data *md, char *out_path, io_funcs *iof, codec_handler *cd,char thint, void **private_data)
{
    void *in_desc;
    unsigned char codec_id;
    unsigned char codec_found;
    int i, error_no;

    in_desc = NULL;
    codec_id = 0;
    i = 0;
    codec_found = 0;
    error_no = 0;

    if(in_de==NULL)
	return -1;

    md->container_id = CONTAINER_ID_FLV;

    in_desc = in_de;
    codec_id = get_codec_info(in_desc, iof);
    if(codec_id == 0){
	printf("\nUnknown FLV format or version\n");
	return -1;
    }

    switch(codec_id){
	case 0x22:
	    md->codec_id = ACODEC_ID_MP3 | VCODEC_ID_VP6;
	    break;
	case 0xa2:
            md->codec_id = ACODEC_ID_AAC | VCODEC_ID_VP6;
            break;
	case 0x27:
            md->codec_id = ACODEC_ID_MP3 | VCODEC_ID_AVC;
            break;
	case 0xa7:
            md->codec_id = ACODEC_ID_AAC | VCODEC_ID_AVC;
            break;
	case 0x07:
	     md->codec_id = ACODEC_ID_AAC | VCODEC_ID_AVC;
	     codec_id = 0xa7;
	     break;

    }

    if(thint){
	if(codec_id == 0x22 || codec_id==0x24||codec_id == 0xa2 || codec_id==0xa4){
	    VPE_PP_add_codec_handler(cd,VPE_PP_generic_chunking_with_hint, 0x24);
	    VPE_PP_add_codec_handler(cd,VPE_PP_generic_chunking_with_hint, 0x22);
	    VPE_PP_add_codec_handler(cd,VPE_PP_generic_chunking_with_hint, 0xa4);
	    VPE_PP_add_codec_handler(cd,VPE_PP_generic_chunking_with_hint, 0xa2);
	} else{
	    VPE_PP_add_codec_handler(cd, VPE_PP_h264_chunking_with_hint, 0xa7);
	    VPE_PP_add_codec_handler(cd,VPE_PP_h264_chunking_with_hint, 0x27);
    
	}
    }else{
	if(codec_id == 0x22 || codec_id==0x24){
	    VPE_PP_add_codec_handler(cd,VPE_PP_generic_chunking, 0x24);
	    VPE_PP_add_codec_handler(cd,VPE_PP_generic_chunking, 0x22);
	}else{
	    VPE_PP_add_codec_handler(cd, VPE_PP_h264_chunking, 0xa7);
	    VPE_PP_add_codec_handler(cd,VPE_PP_h264_chunking, 0x27);
	}
    }

    for(i = 0; i < N_CODEC_HANDLERS;i++){
	if(codec_id == cd->codec_id[i] && !codec_found){
	    error_no=(cd->hdl[i])(in_de, io_mode, profile, md, out_path, iof, private_data);
	    if(error_no<0)
		return error_no;
	    codec_found = 1;
	}
    }

    if(!codec_found){
	printf("none of the registered codec handlers are matching for this Codec ID\n");
	error_no = -1;
	return error_no;
    }

    return error_no;  
}

int 
VPE_PP_chunk_TS_data(void *in_de, char io_mode, uint32_t profile, meta_data *md, char *out_path, io_funcs *iof, codec_handler *cd, char thint, void **private_data)
{
    void *in_desc;
    unsigned char codec_id;
    unsigned char codec_found;
    int i, error_no;

    UNUSED_ARGUMENT(thint);

    in_desc = NULL;
    codec_id = 0;
    i = 0;
    codec_found = 0;
    error_no = 0;

    if(in_de==NULL)
	return -1;

    in_desc = in_de;
    codec_id = 0x24;

    md->container_id = CONTAINER_ID_TS;
    md->codec_id = ACODEC_ID_AAC | VCODEC_ID_AVC;

    VPE_PP_add_codec_handler(cd, VPE_PP_MP2TS_chunking, 0x24);

    for(i = 0; i < N_CODEC_HANDLERS;i++){
	if(codec_id == cd->codec_id[i] && !codec_found){
	    error_no=(cd->hdl[i])(in_de, io_mode, profile, md, out_path, iof, private_data);
	    if(error_no<0)
		return error_no;
	    codec_found = 1;
	}
    }
    

    if(!codec_found){
	printf("none of the registered codec handlers are matching for this Codec ID\n");	error_no = -1;
	return error_no;
    }

    return error_no;
}

/****************************************************************************/
//                          FLV UTILITIES 
//
/***************************************************************************/

/**Writes FLV header
 *@param void *out_desc - the descriptor for the output device
 *@param io_funcs *iof -
 */ 
static inline int
write_flv_hdr(void *out_desc, io_funcs *iof, nkn_vpe_flv_header_t *fh, uint32_t last_tag_size)
{
    int err_no;

    err_no = 0;

    iof->nkn_write(fh, 1, sizeof(nkn_vpe_flv_header_t), out_desc);
    iof->nkn_write(&last_tag_size, 1, sizeof(last_tag_size), out_desc);
    
    return err_no;
}

static inline int
read_nkn_vpe_flv_tag_t(void *in_desc, io_funcs *iof, nkn_vpe_flv_tag_t *ft)
{

    int err_no;

    err_no = 0;
    iof->nkn_read(ft, 1, sizeof(nkn_vpe_flv_tag_t), in_desc);

    return err_no;

}

static inline int
read_flv_packet_data(void *in_desc, io_funcs *iof, 
		     char *p_nkn_vpe_flv_tag_t_data, int *last_tag_size,
		     uint32_t body_length)
{
    int err_no = 0;

    err_no = 0;
    iof->nkn_read(p_nkn_vpe_flv_tag_t_data, 1,body_length,in_desc); 
    iof->nkn_read(last_tag_size, 1,sizeof(int),in_desc);
    

    return err_no;

}

static inline int
write_flv_packet(void *out_desc, io_funcs *iof, 
		 nkn_vpe_flv_tag_t *ft, char *p_nkn_vpe_flv_tag_t_data, 
		 int last_tag_size, uint32_t body_length)
{
    int err_no = 0;

    err_no = 0;
    iof->nkn_write(ft, 1, sizeof(nkn_vpe_flv_tag_t), out_desc);
    iof->nkn_write(p_nkn_vpe_flv_tag_t_data, 1, body_length, out_desc);	
    iof->nkn_write(&last_tag_size, sizeof(int), 1, out_desc);

    return err_no;
}

static inline void
flv_tag_set_timestamp(nkn_vpe_flv_tag_t * tag, uint32_t timestamp)
{
    tag->timestamp = nkn_vpe_convert_uint32_to_uint24(timestamp);
    tag->timestamp_extended = (uint8_t)((timestamp & 0xFF000000) >> 24);
}

/****************************************************************************/
//
//                          UTILITY FUNCTIONS
//
//
/***************************************************************************/
int 
create_tplay_data_from_index(void *hint_desc, void *in_desc,void *out_desc, 
			     io_funcs *iof, int speed, int direction, double st_time)
{
    packet_info pkt;
    int pos, size, skip_bytes;
    int error_no, tag_size, n_frames, i;
    char* pkt_data;
    uint32_t ts, fps, duration;
    char flv_hdr[] = {0x46, 0x4c, 0x56, //FLV magic word
		      0x01,//FLV
		      0x05,//Video only 0x05 for audio and video
		      0x00,0x00,0x00,0x09};//size of FLV header 9 bytes
    int tplay_st_pos;

    UNUSED_ARGUMENT(st_time);
    UNUSED_ARGUMENT(speed);

    pos = 0;
    size = 0;
    tag_size = 0;
    ts = 0;
    pkt_data = NULL;
    n_frames = 0;
    fps = 24;
    i = 0;
    duration = 0;
    skip_bytes = 0;
    error_no = 0;
    tplay_st_pos = 0;

    tplay_st_pos = floor(st_time);
    size = iof->nkn_get_size(hint_desc);
    pos = iof->nkn_tell(hint_desc);

    if(pos>=size){
	printf("already at end of file\n");
	error_no = 1;
	goto cleanup;
    }
    
    //write flv header & tag size
    iof->nkn_write(flv_hdr, 1, sizeof(flv_hdr), out_desc);
    iof->nkn_write(&tag_size, 1, sizeof(int), out_desc);

    //read no of entries and duration
    iof->nkn_read(&n_frames, 1, sizeof(int), hint_desc);
    iof->nkn_read(&duration, 1, sizeof(int), hint_desc);

    //read first hint packet SPS/PPS
    iof->nkn_read(&pkt, 1, sizeof(packet_info), hint_desc);

    if(direction == -1){
	skip_bytes = sizeof(packet_info);skip_bytes = skip_bytes * -1;
	iof->nkn_seek(hint_desc, skip_bytes, SEEK_END);
	skip_bytes = sizeof(packet_info);skip_bytes = skip_bytes * -2;
    }
    else{
	skip_bytes = 0;
    }

    while(i < n_frames){
	iof->nkn_seek(in_desc, pkt.byte_offset, SEEK_SET);
	pkt_data = (char*)malloc(pkt.size);
	iof->nkn_read(pkt_data, 1, pkt.size, in_desc);
	flv_tag_set_timestamp((nkn_vpe_flv_tag_t*)(pkt_data), ts);
	iof->nkn_write(pkt_data, 1, pkt.size, out_desc);
	ts += (duration/speed)/(n_frames);

	//printf("i-frame no %d at %ld bytes and %ld ms \n", n_iframes, pkt.byte_offset, pkt.time_offset);
		
	free(pkt_data);
	pkt_data = NULL;
	
	iof->nkn_seek(hint_desc, sizeof(pkt)*(speed-1), SEEK_CUR);
	iof->nkn_read(&pkt, 1, sizeof(pkt), hint_desc);
	iof->nkn_seek(hint_desc, skip_bytes, SEEK_CUR);
	i++;

	pos = iof->nkn_tell(hint_desc);
    }

    return error_no;

 cleanup:
    if(pkt_data){
	free(pkt_data);
    }
    return error_no;
}

int
write_hint_track(void *desc, GList *list, int n_iframes, int duration)
{
    int n_frames;
    GList *p_list;

    n_frames = 0;
    p_list = list;

    p_list = g_list_last(p_list);

    fwrite(&n_iframes, 1, sizeof(int), desc);
    fwrite(&duration, 1, sizeof(int), desc);
    packet_info *pkt;
    while(p_list){
	pkt = p_list->data;
	fwrite(pkt, 1, sizeof(packet_info), desc);
	n_frames++;
	//fprintf(desc, "%ld\t%ld\t%ld\n", pkt->byte_offset, pkt->time_offset, pkt->size);
	p_list = g_list_previous(p_list);
    }
    return 0;
}

void 
free_tplay_list(GList *list)
{
    GList *ptr = list;
    while(list){
	if(list)
	    free(list->data);
	list = g_list_next(list);
    }
    g_list_free(ptr);
}

int
qsort_compare(const void *a1, const void *b1)
{
    profile_order *a = (profile_order*)(a1);
    profile_order *b = (profile_order*)(b1);

    
    return ((a->bitrate<b->bitrate)*(-1)) + ((a->bitrate==b->bitrate)*0) + ((a->bitrate>b->bitrate)*1);
}

int 
write_meta_data(void *desc, meta_data **p_md, int profiles)
{
    int i, feature_tbl_size, p;
    int pos, n_features;
    meta_data *md;
    sf_meta_data sf;
    tplay_meta_data tp;
    nkn_meta_hdr hdr;
    nkn_feature_tag tag;
    nkn_feature_table *feature_tbl;
    avcc_config *avcc;

    md= NULL;
    avcc = NULL;
    i = 0;
    feature_tbl_size = 0;
    feature_tbl = NULL;
    pos = 0;
    n_features = 3;
    sf.pseudo_content_length = 0;
    md = p_md[0];

    //copy parameters into smooth flow meta data structure & calculate the pseudo content length
    memset(&sf, 0, sizeof(sf_meta_data));
    sf.n_profiles = profiles;
    sf.keyframe_period = md->iframe_interval;
    sf.sequence_duration = md->sequence_duration;
    sf.chunk_duration = md->smooth_flow_duration*1000;//convert to milliseconds 
    sf.num_chunks = md->n_chunks;
    sf.codec_id = md->codec_id;
    sf.container_id = md->container_id;
    memset(sf.profile_rate, 0, sizeof(int)*MAX_NKN_SMOOTHFLOW_PROFILES);

    //populate the bitrates and calculate the pseudo content length
    for(p = 0;p < profiles;p++){
	sf.profile_rate[p] = p_md[p]->bitrate*(1000/8);
	sf.pseudo_content_length = (p_md[p]->content_length >= sf.pseudo_content_length) * p_md[p]->content_length + (p_md[p]->content_length < sf.pseudo_content_length) * sf.pseudo_content_length;
    }

    //do some sanity check over the afr threshold (min_bitrate <= afr_qstart_threshold <= max_bitrate), if this is found to be true, then we set
    //the afr threshold to mid point between the max and min bitrates
    if(afr_qstart_threshold < sf.profile_rate[0] || \
       afr_qstart_threshold > sf.profile_rate[profiles - 1]) {
	afr_qstart_threshold = (sf.profile_rate[profiles - 1] - sf.profile_rate[0]) / 2;
    }

    //write the AFR tabel based on the type chosen through command line; defaults to NKN type
    // for MOVE type table we need to have afr(n) = afr(n+1);
    // for NKN type we need to have afr = afr(n) + 15%(afr(n));
    for(p = 0;p < profiles-1;p++){
	if(afr_tbl_type == AFR_TBL_TYPE_MOVE){
	    if(sf.profile_rate[p] < afr_qstart_threshold){
		sf.afr_tbl[p] = sf.profile_rate[p+1];
	    }else{
		sf.afr_tbl[p] = sf.profile_rate[p] + (0.6 * (sf.profile_rate[p+1] - sf.profile_rate[p]));
	    }
	}else if(afr_tbl_type == AFR_TBL_TYPE_NKN){
	    sf.afr_tbl[p] = sf.profile_rate[p] + (PROFILE_GUARD_BAND * sf.profile_rate[p]);
	}else{
	    sf.afr_tbl[p] = sf.profile_rate[p] * afr_qstart_threshold;
	}

    }

    //popultate the last value in the table
    if(afr_tbl_type == AFR_TBL_TYPE_MOVE){
	sf.afr_tbl[profiles-1] = sf.profile_rate[profiles-1] + (PROFILE_GUARD_BAND * sf.profile_rate[profiles-1]);
    }else if (afr_tbl_type == AFR_TBL_TYPE_NKN){
	sf.afr_tbl[profiles-1] = sf.profile_rate[profiles-1] + (PROFILE_GUARD_BAND * sf.profile_rate[profiles-1]);
    }else{
	sf.afr_tbl[p] = sf.profile_rate[profiles-1] + (PROFILE_GUARD_BAND * sf.profile_rate[profiles-1]);
    }


    //Initialize NKN Meta Data Header
    hdr.meta_signature = NKN_META_DATA_SIGNATURE;
    hdr.n_features = n_features;
    fwrite(&hdr, 1, META_HDR_SIZE, desc);

    //store feature table position to update later
    pos = ftell(desc);

    //Initialize Feature Table
    feature_tbl_size = n_features * FEATURE_TABLE_SIZE;
    feature_tbl = (nkn_feature_table*)malloc(n_features * FEATURE_TABLE_SIZE);
    memset(feature_tbl, 0, feature_tbl_size);
    fwrite(feature_tbl, 1, feature_tbl_size, desc);

    feature_tbl[i].feature_id = FEATURE_SMOOTHFLOW;
    feature_tbl[i].feature_tag_offset = ftell(desc);
   
    //Initialize SmoothFlow feature tag
    tag.feature_id = FEATURE_SMOOTHFLOW;
    tag.tag_body_size = sizeof(sf_meta_data) + (profiles * sizeof(uint32_t));
    fwrite(&tag, 1, sizeof(nkn_feature_tag), desc);
    fwrite(&sf, 1, sizeof(sf_meta_data), desc);

    i++;
    feature_tbl[i].feature_id = FEATURE_TRICKPLAY;
    feature_tbl[i].feature_tag_offset = ftell(desc);
    tag.feature_id = FEATURE_TRICKPLAY;
    tag.tag_body_size = sizeof(tplay_meta_data);
    tp.keyframe_period = md->smooth_flow_duration;
    tp.n_iframes = md->n_iframes;
    fwrite(&tag, 1, sizeof(nkn_feature_tag), desc);
    fwrite(&tp, 1, sizeof(tplay_meta_data), desc);
    
    i++;
    avcc = p_md[0]->avcc;
    feature_tbl[i].feature_id = FEATURE_AVCC_PACKET;
    feature_tbl[i].feature_tag_offset = ftell(desc);
    tag.feature_id = FEATURE_AVCC_PACKET;
    tag.tag_body_size = p_md[0]->avcc->avcc_data_size + sizeof(p_md[0]->avcc->avcc_data_size);
    fwrite(&tag, 1, sizeof(nkn_feature_tag), desc);
    fwrite(&p_md[0]->avcc->avcc_data_size, 1, sizeof(p_md[0]->avcc->avcc_data_size), desc);
    fwrite(p_md[0]->avcc->p_avcc_data, 1, p_md[0]->avcc->avcc_data_size, desc);

    fseek(desc, pos, SEEK_SET);
    fwrite(feature_tbl, 1, feature_tbl_size, desc);
 
    free(feature_tbl);
    return 0;
}

int32_t
write_m3u8_file(meta_data **p_md, int32_t n_profiles, char *video_name, char *out_path, char *uri_prefix)
{
    char m3u8_name[MAX_PATH];
    char parent_m3u8_name[MAX_PATH];
    char uri[4096];
    int32_t i,j;
    FILE *f_parent_m3u8_file, *f_m3u8_file;

    f_parent_m3u8_file = NULL;
    f_m3u8_file = NULL;

    snprintf(parent_m3u8_name, strlen(out_path) + strlen(video_name) + 7, "%s/%s.m3u8", out_path, video_name);
    f_parent_m3u8_file = fopen(parent_m3u8_name, "wb");
    fprintf(f_parent_m3u8_file, "#EXTM3U\n");

    for(i = 1; i <= n_profiles; i++) { 
	snprintf(m3u8_name, strlen(out_path) + strlen(video_name) + 11, "%s/%s_p%02d.m3u8", out_path, video_name, i);
	f_m3u8_file = fopen(m3u8_name, "wb");
	fprintf(f_m3u8_file, "#EXTM3U\n#EXT-X-TARGETDURATION:%d\n",p_md[0]->smooth_flow_duration);
       	fprintf(f_parent_m3u8_file, "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=%d\n", p_md[i-1]->bitrate*8);
	fprintf(f_parent_m3u8_file, "%s/%s\n", uri_prefix, (m3u8_name+strlen(out_path)+1) );
	for(j = 1; j <= (int32_t)(p_md[0]->n_chunks); j++) { 
	    snprintf(uri, strlen(uri_prefix) + strlen(video_name) + 14, "%s/%s_p%02d_%04d.ts", uri_prefix, video_name, i, j);
	    fprintf(f_m3u8_file, "#EXTINF:%d,\n%s\n", (p_md[0]->smooth_flow_duration), uri);
	}
	fprintf(f_m3u8_file, "#EXT-X-ENDLIST");
    }

   
    fclose(f_parent_m3u8_file);
    fclose(f_m3u8_file);

    return 0;

}
uint8_t
get_codec_info(void *in_desc, io_funcs *iof)
{
 
    size_t st_pos, pos;
    size_t input_size;
    int error_no;
    char got_av_packet;
    char ad, vd, codec_info;
    nkn_vpe_flv_header_t fh;
    nkn_vpe_flv_tag_t ft;
    int body_length;
    int lasttagsize=0;
    char avtype;
    char  meta_flag=0;
    pos = 0;
    st_pos = 0;
    error_no = -1;
    got_av_packet = 0;
    body_length = 0;
    ad = vd = 0;
    codec_info = 0;
    memset(&fh, 0, sizeof(fh));

    if(in_desc == NULL)
	return 0;
   
    input_size = iof->nkn_get_size(in_desc);

    st_pos = iof->nkn_tell(in_desc);

    iof->nkn_read(&fh, 1, sizeof(fh), in_desc);

    iof->nkn_read(&lasttagsize, 1, sizeof(uint32_t), in_desc);
   
    if(lasttagsize!=0)
	return 0;
    // iof->nkn_seek(in_desc, sizeof(uint32_t_be), SEEK_CUR);

    if(!check_flv_header(&fh)){
	return codec_info;
    }

    //store the current position of the stream, before parsing the FLV header!
    pos = iof->nkn_tell(in_desc);
 
    //parse tags sequentially, check if they are Audio/Video/Meta and if the 
    //Video tag timestamp crosses the chunk duration cut till that point. Obviously 
    //this is done only at I-Frame boundaries
    while(pos < input_size && got_av_packet < 4){
      
	pos = iof->nkn_tell(in_desc);
      
	//read FLV Tag
	iof->nkn_read(&ft, 1, sizeof(ft), in_desc);
      
	//read FLV Body Length (Big Endian :-( )
	body_length = nkn_vpe_convert_uint24_to_uint32(ft.body_length);
      
	//wait for 1st video packet, and return the codec id
	switch(ft.type){
	    case NKN_VPE_FLV_TAG_TYPE_VIDEO:
		iof->nkn_read(&vd, 1, sizeof(char), in_desc);//(flv_video_data*)(bs->_data);
		iof->nkn_seek(in_desc, (int)(-1*sizeof(char)), SEEK_CUR);
		codec_info |= vd & 0x0F;
		avtype=vd & 0xF;
		if(avtype==0x5) {}
		else if( avtype ==0x4){}
		else if(avtype ==0x7){}
		else {
		    codec_info=0;     
		    printf("Unknown codec Format");   
		    return codec_info;}
		got_av_packet++;
		break;
	    case NKN_VPE_FLV_TAG_TYPE_AUDIO:
		iof->nkn_read(&ad, 1, sizeof(char), in_desc);
		iof->nkn_seek(in_desc, (int)(-1*sizeof(char)), SEEK_CUR);
		codec_info |= ad & 0xF0;
		avtype=(ad & 0xF0)>>4;
		//if(avtype!=0x2 || avtype !=0xa || avtype !=0xe)
		//  codec_info=-1;
		//avtype=codec_info>>4;
		if(avtype==0x2) {}
		else if( avtype ==0xa){}
		else if(avtype ==0xe){}
		else{ 
		    codec_info=0;  
		    printf("Unknown codec Format");
		    return codec_info;}
		got_av_packet++;
		break;
	    case NKN_VPE_FLV_TAG_TYPE_META:
		if(!meta_flag){ 
		    meta_flag=1;
		    break;}
		else{
		    printf("Too many meta data information");
		    return 0;
		}
	    default:
		codec_info=0;	
		return 0;
	}
	//go to next packet
	iof->nkn_seek(in_desc, body_length+sizeof(uint32_t), SEEK_CUR);
    }

    iof->nkn_seek(in_desc, st_pos, SEEK_SET);
    return codec_info;
}

char*
remove_trailing_slash(char *str)
{
    int len,i;
    
    i=0;
    len = 0;

    if(str==NULL){
	printf("string cannot be NULL\n");
	return NULL;
    }

    i = len = strlen(str);

    if(len == 0){
	printf("string cannot be a blank\n");
	return NULL;
    }

    while( i && str[--i]=='/'){
    
    }

    str[i+1]='\0';

    return str;

}

char 
check_flv_header(nkn_vpe_flv_header_t *fh){

    char *magic;

    magic = NULL;
    magic  = (char*)(fh);

    //check magic word
    if(magic[0] != 'F' || magic[1] != 'L' || magic[2] != 'V'){
	printf("error invalid FLV magic word");
	return 0;
    }

    //check FLV version
    if(fh->version != 1){
	printf("Unsupported FLV version");
	return 0;
    }
  
    if(fh->flags==0){
	printf("No audio/video data present or header corrupted");
	return 0;
    }
    if(fh->offset != 150994944){//150994944 is 00 00 00 09 in BE format
	printf("Illegal FLV structure");
	return 0;
    }

    return 1;
  
}

char 
check_free_space(const char *path, uint64_t required_space){

    uint64_t free_space;
    struct statvfs f_stat;

    free_space = 0;

    if(statvfs(path, &f_stat)==-1){
	printf("unabled to get file system stats\n");
	return 0;
    }

    if((f_stat.f_bavail * f_stat.f_bsize) <= required_space){
	return 0;
    }

    return 1;

}

int
write_client_meta_data(void *fp, meta_data **md, int n_profiles)
{

    int error_no, i;
    int threshold;

    threshold = afr_qstart_threshold;
    error_no = 0;
    i = 0;

    fprintf(fp, "<xml>\n");
    fprintf(fp, "\t<sf_version>1.0</sf_version>\n");
    fprintf(fp, "\t<n_profiles>%d</n_profiles>\n", n_profiles);
    fprintf(fp, "\t<profile_info>\n");
    for(i = 0; i < n_profiles; i++){
	fprintf(fp, "\t<profile_map>\n");
	fprintf(fp, "\t\t<profile_name>p%02d</profile_name>\n", i+1);
	fprintf(fp, "\t\t<br>%d</br>\n", md[i]->bitrate);
	fprintf(fp, "\t</profile_map>\n");
    }
    fprintf(fp, "\t</profile_info>\n");
    fprintf(fp, "\t<afr_th>%d</afr_th>\n", threshold);
    fprintf(fp, "</xml>");

    return error_no;
}


int
write_job_log(void *fp, int n_profiles, float time_elapsed, char *scratch_path, int error_no, size_t tot_size, uint32_t n_chunks)
{
    
    fprintf(fp, "No of profiles: %d\n", n_profiles);
    fprintf(fp, "Output written to: %s\n", scratch_path);
    fprintf(fp, "No of Output Chunks: %d\n", n_chunks);
    fprintf(fp, "job completed in: %.4f seconds\n", time_elapsed);
    fprintf(fp, "total data processed: %ld bytes\n", tot_size);
    fprintf(fp, "data rate: %.3f MBytes/second\n", ceil(time_elapsed)>0?(tot_size/time_elapsed)/(1024*1024):0);
    fprintf(fp, "error code: %d\n", error_no);
    return 0;
}

int32_t
get_container_type(char *fname)
{
    int32_t len = 0; 
    int32_t error_code = -1;
    char *extn = NULL;

    len = strlen(fname);

    if(!len){
	error_code = -1;
	return error_code;
    }

    extn = strrchr(fname,'.');//fname+len-4;

    if(!strcmp(extn, ".flv")){
	return 0;
    }else if (!strcmp(extn, ".mp4")){
	return 1;
    }else if (!strcmp(extn, ".ts")){
	return 2;
    }
    
    return error_code;
}

