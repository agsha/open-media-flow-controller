#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> /* only the defines on windows */
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#include "accesslog_pub.h"
#include "accesslog_pri.h"


extern int use_syslog;
extern int max_filename_cnt;
extern char * al_filename;
extern int64_t al_max_filesize;
static FILE * alfd;
volatile int64_t al_current_filesize;
int al_fileid=0;
const char * filename="/var/log/nkn/.accesslog.fileid";
const char log_path[] = "/var/log/nkn";


extern int al_exit_req;
extern pthread_mutex_t acc_lock;



void fix_format_field(accesslog_item_t * item);
void http_accesslog_write(accesslog_item_t * item);


void nkn_read_fileid(void) 
{
	FILE * fp;
	fp = fopen(filename, "r");
	if(!fp) return;
	fscanf(fp, "fileid=%d\n", &al_fileid);
	fclose(fp);
}

void nkn_save_fileid(void) 
{
	FILE * fp;
	fp = fopen(filename, "w");
	if(!fp) return;
	al_fileid %= max_filename_cnt;
	fprintf(fp, "fileid=%d\n", al_fileid);
	fclose(fp);
}

void http_accesslog_init(void) 
{
	struct stat statbuf;
	char al_filename_cmd[256];
	int i;

	if(use_syslog) {
		openlog("nknaccesslog", LOG_NDELAY,  LOG_SYSLOG);
	}
	/*
	 * If accesslog filename or format is not configured
	 * We will return.
	 */
	if( !al_filename || ff_used==0) {
        al_log_basic("Bad filename or format specifier\n");
		return;
    }

	/*
	 * The accesslog filename format is: <accesslog_string>.<id>.tar
	 * each accesslog file size is no larget than al_max_filesize.
	 */
#if 0 
	for(i=0; i<max_filename_cnt; i++) {
		al_fileid++;
		al_fileid %= max_filename_cnt;
		// check out both files with and without .gz
		snprintf(al_filename_cmd, 256, "%s/%s.%d.gz", log_path, al_filename, al_fileid);
		if(stat(al_filename_cmd, &statbuf) == 0) continue;
		snprintf(al_filename_cmd, 256, "%s/%s.%d", log_path, al_filename, al_fileid);
		if(stat(al_filename_cmd, &statbuf) == 0) continue;

        nkn_save_fileid();

		/* we got a filename which does not exist */
		break;
	}
#else
	// Fix BUG 1345
	al_fileid++;
	al_fileid %= max_filename_cnt;
        nkn_save_fileid();
#endif // 0

	/* Create this file */
	snprintf(al_filename_cmd, 256, "%s/%s.%d.gz", log_path, al_filename, al_fileid);
	remove(al_filename_cmd);
    al_log_debug("Removed file %s\n", al_filename_cmd);

	snprintf(al_filename_cmd, 256, "%s/%s.%d", log_path, al_filename, al_fileid);
	alfd = fopen(al_filename_cmd, "w");

    al_log_debug("Opening new file : %s\n", al_filename_cmd);

	if(alfd == NULL) {

		al_log_basic("Failed to open accesslog file %s/%s.%d (%s)\n", 
			log_path, al_filename, al_fileid, strerror(errno));
		assert(0);
	}
	al_current_filesize = 0;
	al_log_basic("Accesslog %s.%d has been opened\n", al_filename, al_fileid);

	return ;
}

void http_accesslog_exit(void) 
{
    int ret = 0;
	char al_filename_cmd[256];
    char this_file[256], that_file[256];
    char hostname[128];

	if(use_syslog) {
		closelog();
	}
	if( alfd == NULL ) {
		return;
	}
	fclose(alfd);
	alfd=NULL;

	/* Do a compression to save the diskspace */
	sprintf(al_filename_cmd, "gzip %s/%s.%d", log_path, al_filename, al_fileid);
	system(al_filename_cmd);

    // Upload the file
    sprintf(this_file, "%s.%d.gz", al_filename, al_fileid);

    if ((ret=gethostname(hostname, sizeof(hostname))) < 0) {
        al_log_basic("Error reading hostname (%d)!", ret);
        return;
    }
    sprintf(that_file, "%s-%s.%d.gz", hostname, al_filename, al_fileid);

    if ( pthread_mutex_trylock(&acc_lock) == EBUSY) {
        return;
    }
    else {
        al_log_upload(this_file, that_file);
        pthread_mutex_unlock(&acc_lock);
    }
}

void fix_format_field(accesslog_item_t * item)
{
	int j;
	char * p;

	// Server will return the string in the same order that we sent out.
	// If it is none, server will fill in "-" for this field
	p=(char *)item + accesslog_item_s;
	for(j=0; j<ff_used; j++) {
		if(ff_ptr[j].string[0] == 0) continue;
		if(ff_ptr[j].field == FORMAT_STRING) continue;
		ff_ptr[j].pdata = p;
		p+=strlen(p)+1;
	}
}

void http_accesslog_write(accesslog_item_t * item) {
	int j, len;
	char * p;
	const char * value;
	char logbuf[4096];
	struct in_addr in;


    if (al_exit_req) {
        return;
    }

    if (ff_ptr == NULL)
        return;

    if (pthread_mutex_trylock(&acc_lock) == EBUSY) {
        al_log_basic("Failed to grab lock.. abort http_accesslog_write");
        return;
    }


	fix_format_field(item);

	p = &logbuf[0];

	for (j = 0; j < ff_used; j++) {
		value=NULL;

		switch(ff_ptr[j].field) {

		/*
		 * Whatever configured, copy as is.
		 */
		case FORMAT_STRING:
			strcpy(p, ff_ptr[j].string);
			p += strlen(p);
		break;

		/*
		 * Time stamp
		 */
		case FORMAT_TIMESTAMP:
		{
			/* cache the generated timestamp */
			//[02/Oct/2008:18:54:52 -0700] 
			struct tm tm;
			localtime_r(&(item->end_ts), &tm);
			strftime(p, 255, "[%d/%b/%Y:%H:%M:%S %z]", &tm);
			p += strlen(p);
		}
		break;

		/*
		 * Elapsted Time
		 */
		case FORMAT_TIME_USED_SEC:
			sprintf(p, "%ld", item->end_ts - item->start_ts);
			p += strlen(p);
		break;
		case FORMAT_TIME_USED_MS:
			sprintf(p, "%ld", (item->end_ts - item->start_ts)*1000);
			p += strlen(p);
		break;

		/*
		 * These fields have been sent over to server in the configuration handshake time
		 * so all we need to do here is pick up the data.
		 */
		case FORMAT_REMOTE_USER:
		case FORMAT_HEADER:
		case FORMAT_RESPONSE_HEADER:
		case FORMAT_QUERY_STRING:
		case FORMAT_URL:
		case FORMAT_HTTP_HOST:
		case FORMAT_COOKIE:
			if( ff_ptr[j].pdata ) {
				strcpy(p, ff_ptr[j].pdata);
				p += strlen(p);
			}
			else {
				*p='-';
				p++;
			}

		break;

		/*
		 * HTTP transaction response code.
		 */
		case FORMAT_PEER_STATUS:
		case FORMAT_STATUS:
			sprintf(p, "%d", item->status);
			p += strlen(p);
		break;

		/*
		 * HTTP transaction response subcode.
		 */
		case FORMAT_STATUS_SUBCODE:
			sprintf(p, "%d", item->subcode);
			p += strlen(p);
		break;

		/*
		 * Bytes counter information
		 */
		case FORMAT_BYTES_OUT:
			sprintf(p, "%ld", item->out_bytes);
			p += strlen(p);
		break;
		case FORMAT_BYTES_IN:
			sprintf(p, "%ld", item->in_bytes);
			p += strlen(p);
		break;
		case FORMAT_BYTES_OUT_NO_HEADER:
			sprintf(p, "%ld", item->out_bytes - item->resp_hdr_size);
			p += strlen(p);
		break;

		/*
		 * All fields are set in the flag
		 */
		case FORMAT_REQUEST_PROTOCOL:
			value = CHECK_ALITEM_FLAG(item, ALF_HTTP_10) ?
				"HTTP/1.0" : "HTTP/1.1";
		break;
		case FORMAT_REQUEST_METHOD:
			if( CHECK_ALITEM_FLAG(item, ALF_METHOD_GET) ) {
				strcpy(p, "GET");
				p += strlen(p);
			}
			else {
				*p='-';
				p++;
			}
		break;
		case FORMAT_CONNECTION_STATUS:
			value = CHECK_ALITEM_FLAG(item, ALF_CONNECTION_KEEP_ALIVE) ?
				"Keep-Alive" : "Close";
		break;
		case FORMAT_CONNECTION_TYPE:
			value = CHECK_ALITEM_FLAG(item, ALF_SECURED_CONNECTION) ?
				"https" : "http";
		break;
		case FORMAT_CACHE_HIT:
			//printf("FORMAT_CACHE_HIT flag=%x\n", item->flag);
			if( CHECK_ALITEM_FLAG(item, ALF_SENT_FROM_BUFFER_CACHE) ) {
				strcpy(p, "Buffer"); p+= 6;
			} else if( CHECK_ALITEM_FLAG(item, ALF_SENT_FROM_ORIGIN_MGR) ) {
				strcpy(p, "Origin"); p+= 6;
			} else if( CHECK_ALITEM_FLAG(item, ALF_SENT_FROM_SSD_DISK) ) {
				strcpy(p, "SSD"); p += 3;
			} else if( CHECK_ALITEM_FLAG(item, ALF_SENT_FROM_SAS2_DISK) ) {
				strcpy(p, "SAS"); p+= 3;
			} else if( CHECK_ALITEM_FLAG(item, ALF_SENT_FROM_SATA_DISK) ) {
				strcpy(p, "SATA"); p+= 4;
			} else if( CHECK_ALITEM_FLAG(item, ALF_SENT_FROM_NFS_MGR) ) {
				strcpy(p, "NFS"); p+= 3;
			} else if( CHECK_ALITEM_FLAG(item, ALF_SENT_FROM_TFM_MGR) ) {
				strcpy(p, "TFM"); p+= 3;
			} else {
				*p='-'; p++;
			}
		break;

		/*
		 * Server side information
		 */
		case FORMAT_SERVER_PORT:
			sprintf(p, "%d", item->dst_port);
			p += strlen(p);
		break;
		case FORMAT_REMOTE_ADDR:
		case FORMAT_REMOTE_HOST:
			in.s_addr=item->src_ip;
			sprintf(p, "%s", inet_ntoa(in));
			p += strlen(p);
		break;
		case FORMAT_PEER_HOST:
		case FORMAT_LOCAL_ADDR:
			in.s_addr=item->dst_ip;
			sprintf(p, "%s", inet_ntoa(in));
			p += strlen(p);
		break;

		/*
		 * Create Request Line.
		 */
		case FORMAT_REQUEST_LINE:
			assert(ff_ptr[j].pdata[0] != 0);
			sprintf(p, "%s %s %s", 
			        CHECK_ALITEM_FLAG(item, ALF_METHOD_GET) ? "GET" : 
                    (CHECK_ALITEM_FLAG(item, ALF_METHOD_HEAD) ? "HEAD" : "UNKNOWN"), 
				ff_ptr[j].pdata,
				CHECK_ALITEM_FLAG(item, ALF_HTTP_10) ?  "HTTP/1.0" : "HTTP/1.1");
			p += strlen(p);
		break;

		/*
		 * To be implemented fields
		 */
		case FORMAT_FILENAME:
		case FORMAT_RFC_931_AUTHENTICATION_SERVER:
		case FORMAT_REMOTE_IDENT:
		case FORMAT_SERVER_NAME:
		case FORMAT_ENV:
		default:
			*p = '-';
			p++;
		break;
		}

		if(value) {
			strcpy(p, value);
			p += strlen(p);
		}
	}

	if(use_syslog) {
		*p='\0';
		syslog(LOG_NOTICE, "%s", logbuf);
	}

	*p='\n';
	p++;

	// adjust the fillin_accesslog_size,
	// So we will write into accesslog next time.
	fwrite(logbuf, p - logbuf, 1, alfd);
	fflush(alfd);

    pthread_mutex_unlock(&acc_lock);

	/*
	 * Monitoring the accesslog file size to avoid the file size is
	 * larger than max filesize.
	 */
	al_current_filesize += (p - logbuf);
	if(al_current_filesize >= al_max_filesize) {
		http_accesslog_exit();
		/* open a new file */
		http_accesslog_init();
	}


}

