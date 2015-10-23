#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define __USE_GNU
#include <fcntl.h> /* only the defines on windows */
#include <errno.h>
#include <sys/time.h>
#include <syslog.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <libgen.h>
#include "log_accesslog_pri.h"
#include "nknlogd.h"
#include "lgd_worker.h"
#include "file_utils.h"

extern char * log_folderpath;
static int has_opensyslog = 0;
static const char * logpath = "/log/varlog/nkn";
static const char * savefilename = "/var/log/nkn/.nknlogd_log.fileid";
static const char * crawl_vfilename = "/var/log/nkn/.crawl.version";
static const char * logexportpath = LOG_EXPORT_HOME;
extern pthread_mutex_t acc_lock;
extern pthread_mutex_t upload_mgmt_log_lock;
int streamlogid=0, accesslogid=0, tracelogid=0, cachelogid=0, crawllogid=0;
int fuselogid=0, errorlogid=0, mfplogid=0;
int cblogid=0;
void syslog_init(logd_file *plf);

extern void setbuffer(FILE *stream, char *buf, size_t size);

extern int
log_upload(
    logd_file *plf,
    const char *this, 
    const char *that);

void log_read_fileid(void)
{
        FILE * fp;

        fp = fopen(savefilename, "r");
        if(!fp) return;
        fscanf(fp, "fileid=%d %d %d %d %d %d %d %d %d\n", 
		&accesslogid, &tracelogid, &cachelogid, &streamlogid,
		&fuselogid, &errorlogid, &mfplogid, &crawllogid, &cblogid);
        fclose(fp);
}

void crawl_read_version(logd_file *plf)
{
        FILE * fp;

        fp = fopen(crawl_vfilename, "r");
        if(!fp) return;
        fscanf(fp, "version_number=%d\n",
                &plf->version_number);
        fscanf(fp, "end_time=%lu\n", &plf->version_etime);
        fclose(fp);
}

void crawl_write_version(logd_file *plf)
{
        FILE * fp;

        fp = fopen(crawl_vfilename, "w");
        if(!fp) return;
        fprintf(fp, "version_number=%d\n",
                plf->version_number);
        fprintf(fp, "end_time=%lu\n", plf->version_etime);
        fclose(fp);
}



void log_save_fileid(void)
{
        FILE * fp;
	logd_file * plf;

        for (plf = g_lf_head.lh_first; plf != NULL; plf = plf->entries.le_next) {
		if(plf->type == TYPE_ACCESSLOG) { accesslogid = plf->cur_fileid; }
		else if(plf->type == TYPE_TRACELOG) { tracelogid = plf->cur_fileid; }
		else if(plf->type == TYPE_CACHELOG) { cachelogid = plf->cur_fileid; }
		else if(plf->type == TYPE_FUSELOG) { fuselogid = plf->cur_fileid; }
		else if(plf->type == TYPE_STREAMLOG) { streamlogid = plf->cur_fileid; }
		else if(plf->type == TYPE_ERRORLOG) { errorlogid = plf->cur_fileid; }
		else if(plf->type == TYPE_MFPLOG) { mfplogid = plf->cur_fileid; }
		else if(plf->type == TYPE_CRAWLLOG) { crawllogid = plf->cur_fileid; }
		else if(plf->type == TYPE_CBLOG) { cblogid = plf->cur_fileid; }
        }
        fp = fopen(savefilename, "w");
        if(!fp) 
	{
	    complain_error_errno(1, "Failed in opening  file %s", savefilename);
	    return;
	}
        fprintf(fp, "fileid=%d %d %d %d %d %d %d %d %d\n", 
		accesslogid, tracelogid, cachelogid, streamlogid,
		fuselogid, errorlogid, mfplogid, crawllogid, cblogid);
        fclose(fp);
}


void syslog_init ( logd_file *plf)
{
	if(plf->replicate_syslog) {
		if(has_opensyslog == 0) {
			openlog("nknlogd", LOG_NDELAY,  LOG_SYSLOG);
			has_opensyslog = 1;
		}
	}
}

void log_file_init(logd_file * plf)
{
	char fullfilename[256];
	char mylink[256];
	const char *symfilename;
	char date_str[256];
	char gmt_str[256];
	char cmd_str[256];
	int ret;
	int round_off = 0;
	syslog_init ( plf );
	aln_interval *pal = NULL;
        time_t et;
        struct tm etime;
	pal =(aln_interval *)malloc(sizeof(aln_interval));
	if(pal == NULL){
	    complain_error_msg(1, "Memory allocation failed!");
	    exit(EXIT_FAILURE);
	}
	memset(pal ,0,sizeof(aln_interval));
	plf->pai = pal;
	/* Get current time */
	time_t cur_time = time(NULL);
	struct tm loctime ;
	struct tm gtime;

	localtime_r(&cur_time, &loctime);
	gmtime_r(&cur_time, &gtime);
	plf->st_time = (uint64_t)cur_time;
	strftime(plf->hour, 3, "%H", &loctime);
	// modify the date_str to include the file_start_min
	if(pal->use_file_time == 1){
	    loctime.tm_min = pal->file_start_min;
	    pal->use_file_time = 0;
	}
        if((loctime.tm_min )&&(plf->rt_interval)){
                round_off  = loctime.tm_min%plf->rt_interval;
                if(round_off){
                        loctime.tm_min = abs(loctime.tm_min - round_off);
                        loctime.tm_sec = 0;
                }
        }
	strftime(plf->file_start_time,SIZE_OF_TIMEBUFFER,
		"%G%m%d%H%M", &loctime);
        if(plf->rt_interval){
            et = mktime(&loctime);
            et = et + (60 * plf->rt_interval);
            localtime_r(&et,&etime);
            strftime(plf->file_end_time,
                SIZE_OF_TIMEBUFFER, "%G%m%d%H%M", &etime);
        } else {
            memset(plf->file_end_time,'\0',sizeof(plf->file_end_time));
        }
	/* adjust the fileid */
	plf->cur_fileid ++;
	if(plf->log_rotation){//if log-rotation is enabled, then we need to increament the file-id
	    if(plf->cur_fileid >= plf->max_fileid) plf->cur_fileid = 0;
	    log_save_fileid();
	}

	if (plf->type != TYPE_SYSLOG) {//no log file is created for syslog
	    if(plf->log_rotation){
		snprintf(cmd_str, sizeof(cmd_str), "%s %s/%s.%d.%s", "rm -f",
			log_folderpath, plf->filename, plf->cur_fileid, "*");
		//printf("----cmd_str= %s\n", cmd_str);
		system(cmd_str);
	    }

	    strftime(date_str, 256, "%Y%m%d_%H:%M:%S", &loctime);
	    snprintf(fullfilename, sizeof(fullfilename), "%s/%s.%d.%s",
		    log_folderpath, plf->filename, plf->cur_fileid, date_str);
	    if(plf->fullfilename) { free(plf->fullfilename); }
		plf->fullfilename = strdup(fullfilename);

		plf->fp = fopen(fullfilename, "w");
		if(!plf->fp) {
		    complain_error_errno(1, "Opening %s", fullfilename);
		}

		if (plf->type == TYPE_ACCESSLOG) {
			plf->io_buf = (char *) calloc(IOBUF_SIZE, 1);
			setbuffer(plf->fp, plf->io_buf, IOBUF_SIZE);
		}

		plf->cur_filesize = 0;
	}
    symfilename = plf->filename; // default: "access.log"
if((plf->fp) && (plf->type != TYPE_SYSLOG)){
    ret = fprintf(plf->fp, "#Version: 1.0\n");
    plf->cur_filesize += ret;

    ret = fprintf(plf->fp,
            "#Software: Media Flow Controller v(%s)\n",
            NKN_BUILD_PROD_RELEASE);
    plf->cur_filesize += ret;

    strftime(gmt_str, 256, "%Y-%m-%d %T", &gtime);
    ret = fprintf(plf->fp, "#Date: %s\n", gmt_str);
    plf->cur_filesize += ret;

    ret = fprintf(plf->fp, "#Remarks: -\n");
    plf->cur_filesize += ret;
}

	switch(plf->type) {
	case TYPE_ACCESSLOG:
		symfilename = plf->filename; // default: "access.log"
		if(plf->show_format){
			ret = fprintf(plf->fp, "#Version: 1.0\n");
			plf->cur_filesize += ret;
			strftime(date_str, 256, "%m-%d-%Y %H:%M:%S", &loctime);
			ret = fprintf(plf->fp, "#Date: %s\n", date_str);
			plf->cur_filesize += ret;
			ret = fprintf(plf->fp, "#Fields: %s\n", plf->format);
			plf->cur_filesize += ret;
                	fflush(plf->fp);
		}
		break;
	case TYPE_TRACELOG: symfilename = plf->filename; break; //default: trace.log
	case TYPE_CACHELOG: symfilename = plf->filename; break; // default : "cache.log"
	case TYPE_FUSELOG: symfilename = plf->filename; break; // default : "fuse.log"
	case TYPE_STREAMLOG:
		symfilename = plf->filename; // default : "stream.log"
		if(plf->show_format){
/*			ret = fprintf(plf->fp, "#Version: 1.0\n");
			plf->cur_filesize += ret;
			strftime(date_str, 256, "%m-%d-%Y %H:%M:%S", loctime);
			ret = fprintf(plf->fp, "#Date: %s\n", date_str);
			plf->cur_filesize += ret;*/
			ret = fprintf(plf->fp, "#Fields: %s\n", plf->format);
			plf->cur_filesize += ret;
                	fflush(plf->fp);
		}
		break;
	case TYPE_SYSLOG: break; //We don't create a file for syslog
	case TYPE_ERRORLOG: symfilename = plf->filename; break; // default :  "error.log"
	case TYPE_MFPLOG: symfilename = plf->filename; break; // default :  "mfp.log"
	case TYPE_CRAWLLOG: symfilename = plf->filename; 
                            crawl_write_version(plf);
                            break; // default :  "crawl.log"
	case TYPE_CBLOG: symfilename = plf->filename; break; // default :  "cb.log"
	default: symfilename = "other.log"; break;
	}
	/* Create a symbol link for web GUI */
	if (plf->type != TYPE_SYSLOG) {
		snprintf(mylink, sizeof(mylink), "ln -fs %s %s/%s",
			fullfilename, log_folderpath, symfilename);
		system(mylink);
	}
	return ;
}

void log_file_exit(logd_file * plf)
{
	pid_t pid;
	int ret_len = 0;
	int round_off = 0;
	/* Get current time and assign to end time*/
	time_t cur_time = time(NULL);
	struct tm ltime;
	aln_interval *pal = NULL;
        char tmp_filename[1024] = { 0 };
        char cmd[1024] = { 0 };
        struct stat file_status;
        FILE *pipefd = NULL;
        char path[1024];
	char *token = NULL;
	int field = 0;
	char trailer_record[SIZE_OF_TIMEBUFFER] = {0};
	struct tm mtime;
	struct tm etime;
	time_t et;
	unsigned long tmp_et;
	uploadInfo *p_uploadInfo = NULL;
	char gz_filename[MAX_FILE_SZ] = { 0 };
	int err = 0;
	char *bname = NULL, *b = NULL;
	char *bname_upldinfo = NULL, *b_upldinfo = NULL;
        char *dname_upldinfo = NULL, *d_upldinfo = NULL;

	memset(&ltime, 0, sizeof(struct tm) );
	memset(&etime, 0, sizeof(struct tm) );

	if(!plf->fp) {
	    snprintf(cmd,1024,"/bin/ls %s/%s.%d*",
		    "/log/varlog/nkn", plf->filename, plf->cur_fileid);
	    pipefd = popen(cmd, "r");
	    if(pipefd == NULL) {
		return;
            }
            fscanf(pipefd,"%s\n",path);
            plf->fullfilename = strdup(path);
            pclose(pipefd);
        }
        strlcpy(tmp_filename,plf->fullfilename,sizeof(tmp_filename));
        //Parse the crawl.log file name when started for
        // the first time parsse from :"crawl.log.0.20120517_09:45:00"

        if(strcmp(plf->file_start_time,"") == 0){
            token = strtok(tmp_filename,"._:");
            while(token != NULL) {
              /* The 3rd field till 5th field indicates the start timestamp*/
		if((field >= 3)&&(field <= 5)) {
		    strlcat(plf->file_start_time ,token,
			    sizeof(plf->file_start_time));
		}
		token = strtok(NULL, "._:");
		field++;
	    }
	}
	//Fix for PR782738
	if(strcmp(plf->file_end_time,"") == 0){
	    if(strcmp(plf->file_start_time,"") != 0) {
		if(plf->rt_interval){
		    strptime(plf->file_start_time, "%Y%m%d%H%M", &ltime );
		    et =  mktime(&ltime);
		    et = et + (60 * plf->rt_interval);
		    localtime_r(&et, &etime);
		}
		else {
		    time_t now = time(NULL);
		    localtime_r(&now ,&etime);
		}

		strftime(plf->file_end_time,
			SIZE_OF_TIMEBUFFER, "%G%m%d%H%M", &etime);
	    }
	}
        sscanf(plf->file_end_time,"%lu",&tmp_et);
        if(plf->version_etime == 0){
            plf->version_etime = tmp_et;
            plf->version_number ++;
        }else{
            if(tmp_et > plf->version_etime){
                plf->version_number = 0;
            }else {
                plf->version_number ++;
            }
            plf->version_etime = tmp_et;
        }

	if(stat(plf->fullfilename, &file_status) == 0) {
	    //Got the file status
            if(plf->fp == NULL){
                plf->fp = fopen(plf->fullfilename, "a+");
            }
	    if(plf->fp != NULL)
	    {
		localtime_r(&file_status.st_mtime, &mtime);
		strftime (trailer_record, SIZE_OF_TIMEBUFFER, "%G%m%d%H%M%S",
			&mtime);
		fprintf(plf->fp,"\n#Last Modified : %s",trailer_record);
	    }
            else {
                lc_log_basic(LOG_ERR, "Failed to update trailer in previous file %s", plf->fullfilename);
            }
        } else {
            lc_log_basic(LOG_ERR, "Failed to get stat of previous file %s",  plf->fullfilename);
        }

	if(plf->fp){
	    int ret = 0;
	    pthread_mutex_lock(&plf->fp_lock);
	    ret = fflush(plf->fp);
	    log_write_report_err(ret, plf);

	    fclose(plf->fp);
	    plf->fp=NULL;
	    if(plf->pai){
		free(plf->pai);
	    }
	    pthread_mutex_unlock(&plf->fp_lock);
	}

	p_uploadInfo = malloc(sizeof(struct uploadInfo));
	if (p_uploadInfo == NULL){
	    complain_error_msg(1, "Memory allocation failed!,may not zip log file");
	    bail_error(1);
	}
	memset(p_uploadInfo, 0, sizeof(struct uploadInfo));
	b = basename(bname = strdup(plf->fullfilename));
	if((plf->fullfilename)&&(plf->filename)){
	    if(plf->type == TYPE_CRAWLLOG){
		snprintf(gz_filename, MAX_FILE_SZ, "%s/%s_%d_%s_%s", logpath,
			plf->filename, plf->version_number,
			plf->file_start_time, plf->file_end_time);
	    }
	    else{
		snprintf(gz_filename, MAX_FILE_SZ, "%s", plf->fullfilename);

	    }
	    p_uploadInfo->fullfilename = strdup(gz_filename);
	    p_uploadInfo->filename =strdup(b);
	    p_uploadInfo->logtype = plf->type;
	}
	if((plf->remote_url)||(plf->remote_pass)){
	    p_uploadInfo->remote_url = strdup(plf->remote_url);
	    p_uploadInfo->password = strdup(plf->remote_pass);
	}

	b_upldinfo = basename(bname_upldinfo = strdup(p_uploadInfo->fullfilename));
	d_upldinfo = dirname(dname_upldinfo = strdup(p_uploadInfo->fullfilename));

	if(plf->type == TYPE_CRAWLLOG){
	    memset(gz_filename, 0, MAX_FILE_SZ);
	    // Append "." to the file name so we dont end up in loosing the file before we zip as in log_file_init the files may be deleted by unlink
	    snprintf(gz_filename, MAX_FILE_SZ, "%s/.%s", d_upldinfo,
		    b_upldinfo);
	    lc_log_basic(LOG_INFO, "log_file.c ...file name......%s",
			 gz_filename);
	    err = lf_rename_file(plf->fullfilename, gz_filename);
	    bail_error(err);
	}
	err = post_msg_to_gzip(p_uploadInfo);
	bail_error(err);

bail:
	safe_free(bname);
	safe_free(dname_upldinfo);
	safe_free(bname_upldinfo);
	return;
}

void log_write(logd_file * plf, char * p, int len)
{
	int ret;
	int cur_hr, pre_hr;
        time_t cur_time;
        struct tm loctime;
	char temp_syslog_buf[8096];

        if(plf->replicate_syslog) {
                strncpy(temp_syslog_buf, p, len);
                temp_syslog_buf[len] = '\0';
                syslog(LOG_NOTICE, "%s", temp_syslog_buf);
        }

	//in case of TYPE_SYSLOG, we don't do any thing to our log files
	if(plf->type == TYPE_SYSLOG)
		return;
	// adjust the fillin_accesslog_size,
	// So we will write into accesslog next time.
	while(len) {
	    ret = fwrite(p, 1, len, plf->fp);
	    if (ret <= 0) {
		    log_write_report_err(ret, plf);
		    return;

	    }
		p += ret;
		len -= ret;
		plf->cur_filesize += ret;
	}
	//fflush(plf->fp);

	cur_time = time(NULL);
	localtime_r(&cur_time, &loctime);
	cur_hr = loctime.tm_hour;
	pre_hr = atoi((char*)plf->hour);
    /* Check whether time-interval is hit, if so, close & re-init
     * else, chck if size is hit.
     * If neither, return.
     */
	if ((plf->max_filesize)
		&& (plf->cur_filesize > plf->max_filesize)) {
	    goto reinit;
	}else if(plf->on_the_hour){//on the hour is set
	    if (cur_hr != pre_hr){
		goto reinit;
	    }
	}

	return;

reinit:
	    log_file_exit(plf);
	    log_file_init(plf);
	}


