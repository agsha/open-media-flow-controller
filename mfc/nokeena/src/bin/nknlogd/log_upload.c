#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#define __USE_GNU
#include <fcntl.h>		/* only the defines on windows */
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <md_client.h>
#include <license.h>
#include <alloca.h>

#include <curl/curl.h>
#define HASH_UNITTEST
#include "nkn_hash.h"
#include "log_accesslog_pri.h"
#include "nknlogd.h"

#include "log_channel.h"


int log_upload_file(struct channel *chan,
	const char *local_file,
	const char *rmt_file);


int log_upload_file(struct channel *chan,
	const char *local_file,
	const char *rmt_file)
{
    CURL *curl;
    CURLcode res;
    FILE *hd_src;
    struct stat file_info;
    curl_off_t fsize;

    struct curl_slist *headerlist = NULL;

    if (stat(local_file, &file_info)) {
	complain_error_errno(1, "stat failed");
	return -1;
    }

    fsize = (curl_off_t) file_info.st_size;

    hd_src = fopen(local_file, "rb");
    if(hd_src == NULL)
    {
	complain_error_errno(1, "Opening %s failed", local_file);
    }

    curl_global_init(CURL_GLOBAL_ALL);

    /* get a curl handle */
    curl = curl_easy_init();
    if (curl) {
	/* build a list of commands to pass to libcurl */
	curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
		(curl_off_t) fsize);

	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

	//curl_easy_setopt(curl, CURLOPT_URL, chan->config.accesslog.upload_url);

	curl_easy_setopt(curl, CURLOPT_READDATA, hd_src);

	res = curl_easy_perform(curl);

	curl_easy_cleanup(curl);
    }

    fclose(hd_src);
    curl_global_cleanup();

    return res;
}
