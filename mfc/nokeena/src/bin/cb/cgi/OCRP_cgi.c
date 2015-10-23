/*
 * OCRP_cgi.c -- Origin Content Routing Protocol cgi handler
 */
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "cgic.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ftw.h>
#include <libgen.h>
#include <alloca.h>
#include "curl/curl.h"

#include "cb/xml/OCRP_XMLops.h"
#include "cb/OCRP_cgi_params.h"
#include "cb/OCRP_cgi_errorcodes.h"

/*
 * Global definitions
 */
FILE *DBG_fd = 0;

/*
 * Macro definitions
 */
#define UNUSED_ARGUMENT(x) (void)x

#define DBGMSG(fmt, ...) { \
    if (DBG_fd) { \
    	fprintf(DBG_fd, "[%s:%d] "fmt"\n", \
	    	__FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } \
}

#define ERRMSG(_errcode, fmt, ...) { \
    snprintf(errbuf, sizeof(errbuf), \
	     "Internal Server Error, [Suberror:%d] %s:%d", \
	     (_errcode), __FUNCTION__, __LINE__); \
    cgiHeaderStatus(500, errbuf); \
    fprintf(cgiOut, "<html>\r\n<body>\r\n"fmt"\r\n</body>\r\n</html>\r\n", \
	    ##__VA_ARGS__); \
    if (DBG_fd) { \
    	fprintf(DBG_fd, "[%s:%d] "fmt"\n", \
	    	__FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } \
}

static
int create_post_tmpfile(char *filename, int max_filename, 
			char *debug_filename, int max_debug_filename)
{
    int fd_post;
    int fd_post_debug;
    int rv = 0;

    while (1) {
    	strncpy(filename, POST_TEMPLATE_DATAFILE, max_filename);
	filename[max_filename-1] = '\0';

    	fd_post = mkstemp(filename);
	if (fd_post < 0) {
	    rv = 1;
	    break;
	}
	close(fd_post);
	chmod(filename, 0666);

	if (!debug_filename) {
	    break;
	}
    	strncpy(debug_filename, filename, max_debug_filename);
	debug_filename[max_debug_filename-1] = '\0';
    	strncat(debug_filename, ".dbg", 
		max_debug_filename-strlen(debug_filename)-1);
	fd_post_debug = open(debug_filename, O_CREAT|O_EXCL, 0666);
	if (fd_post_debug >= 0) {
	    close(fd_post_debug);
	} else {
	   unlink(filename);
	   rv = 2;
	}
	break;
    }
    return rv;
}

static 
int create_get_debug_file()
{
    int retries;
    int fd;
    int rv = 0;
    int ret;
    long filenum;
    size_t wrtcnt;
    ssize_t xfercnt;
    char numbuf[32];
    char filename[1024];

    // Open file sequence number file

    retries = 2;
    while (retries--) {
    	fd = open(GET_DBG_SEQNUM_FILE, O_RDWR);
    	if (fd >= 0) {
	    break;
	} else {
	    fd = open(GET_DBG_SEQNUM_FILE, O_RDWR|O_CREAT|O_EXCL, 0666);
	    if (fd >= 0) {
	    	break;
	    }
	}
    }

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    if (fd < 0) {
    	rv = 1;
	break;
    }

    ret = flock(fd, LOCK_EX);
    if (ret < 0) {
	rv = 2;
	break;
    }

    // Get file sequence number value
    memset(numbuf, 0, sizeof(numbuf));
    ret = read(fd, numbuf, sizeof(numbuf));
    if (ret > 0) {
    	filenum = atol(numbuf);
    } else if (ret == 0) {
    	filenum = 1;
    } else {
	rv = 3;
    	flock(fd, LOCK_UN);
	break;
    }

    // Increment file sequence number and write it back
    wrtcnt = snprintf(numbuf, sizeof(numbuf), "%ld\n", filenum + 1);
    lseek(fd, 0, SEEK_SET);
    xfercnt = write(fd, numbuf, wrtcnt);
    if (xfercnt != (ssize_t)wrtcnt) {
	rv = 4;
	break;
    }

    ret = snprintf(filename, sizeof(filename), "%s%ld", 
		   GET_BASE_DEBUG_DATAFILE, filenum);
    if (ret >= sizeof(filename)) {
    	filename[sizeof(filename)-1] = '\0';
    }
    DBG_fd = fopen(filename, "w+");

    break;
    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    return rv;
}

static long storage_in_bytes;

static
int ftw_callback(const char *path, const struct stat *sb, int typeflag)
{
    UNUSED_ARGUMENT(path);

    if (typeflag == FTW_F) {
    	storage_in_bytes += sb->st_size;
    }
    return 0;
}

static
long dir_storage_used(void)
{
    int ret;

    storage_in_bytes = 0;
    ret = ftw(OCRP_BASE_DIR, ftw_callback, 8);

    return storage_in_bytes;
}

static
int dump_cgi_env(const char *filename)
{
    DBGMSG("POST file:[%s]", filename);
    DBGMSG("cgiServerSoftware=%s", cgiServerSoftware);
    DBGMSG("cgiServerName=%s", cgiServerName);
    DBGMSG("cgiGatewayInterface=%s", cgiGatewayInterface);
    DBGMSG("cgiServerProtocol=%s", cgiServerProtocol);
    DBGMSG("cgiServerPort=%s", cgiServerPort);
    DBGMSG("cgiRequestMethod=%s", cgiRequestMethod);
    DBGMSG("cgiPathInfo=%s", cgiPathInfo);
    DBGMSG("cgiPathTranslated=%s", cgiPathTranslated);
    DBGMSG("cgiScriptName=%s", cgiScriptName);
    DBGMSG("cgiQueryString=%s", cgiQueryString);
    DBGMSG("cgiRemoteHost=%s", cgiRemoteHost);
    DBGMSG("cgiRemoteAddr=%s", cgiRemoteAddr);
    DBGMSG("cgiAuthType=%s", cgiAuthType);
    DBGMSG("cgiRemoteUser=%s", cgiRemoteUser);
    DBGMSG("cgiRemoteIdent=%s", cgiRemoteIdent);
    DBGMSG("cgiContentType=%s",  cgiContentType);
    DBGMSG("cgiCookie=%s", cgiCookie);
    DBGMSG("cgiContentLength=%d", cgiContentLength);
    DBGMSG("cgiAccept=%s", cgiAccept);
    DBGMSG("cgiUserAgent=%s", cgiUserAgent);
    DBGMSG("cgiReferrer=%s", cgiReferrer);

    return 0;
}

static
int validate_POST(const char *filename, const char *script_basename)
{
    const char *DTD;
    XMLDocCTX_t *ctx = 0;
    OCRP_record_t ocrp;
    char errbuf[1024 * 1024];
    char xml_errbuf[1024];
    int ret = 0;
    int rv;
    int num_elements;
    
    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    if (strcasecmp(script_basename, POST_SCRIPT_NAME_LOAD) == 0) {
    	DTD = OCRP_DTD;
    } else if (strcasecmp(script_basename, POST_SCRIPT_NAME_UPDATE) == 0) {
    	DTD = OCRPupdate_DTD;
    } else {
	ERRMSG(ER_BAD_POST_SCRIPTNAME, "[Unknown scriptname=[%s]]", 
	       script_basename);
        ret = 1; // Unknown script
	break;
    }

    ctx = OCRP_newXMLDocCTX();
    if (!ctx) {
	ERRMSG(ER_XML_LIB_ERR, "[OCRP_newXMLDocCTX() failed]");
    	ret = 2;
	break;
    }

    // Validate XML structure
    rv = OCRP_validateXML(filename, DTD, ctx, xml_errbuf, sizeof(xml_errbuf));
    if (rv) {
	ERRMSG(ER_XML_VALIDATE_ERROR,
	       "[OCRP_validateXML() failed, rv=%d err=[%s]]", rv, xml_errbuf);
	ret = 3;
	break;
    }

    // Validate XML elements
    num_elements = 0;
    while (1) {
    	rv = OCRP_XML2Struct(ctx, &ocrp,  xml_errbuf, sizeof(xml_errbuf));
	if (!rv) {
	    if (++num_elements <= POST_MAX_ELEMENTS) {
	    	continue;
	    } else {
	    	ERRMSG(ER_MAX_POST_ELEMENTS,
		       "Maximum POST elements of %d exceeded", 
		       POST_MAX_ELEMENTS);
	    	ret = 4;
	    	break;
	    }
	} else if (rv > 0) {
	    ERRMSG(ER_XML_ELEMENT_ERROR, 
		   "[OCRP_XML2Struct() failed, rv=%d err=[%s]]",
		   rv, xml_errbuf);
	    ret = 5;
	    break;
	} else if (rv < 0) { // EOF
	    break;
	}
    }
    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    if (ctx) {
    	OCRP_deleteXMLDocCTX(ctx);
    }

    return ret;
}

static
int move_post_file(const char *filename, const char *debug_filename, 
		   const char *script_basename)
{
    char errbuf[1024 * 1024];
    char new_filename[1024];
    char numbuf[32];
    int retries;
    int fd;
    int rv = 0;
    int ret;
    long filenum;
    size_t wrtcnt;
    ssize_t xfercnt;

    // Open file sequence number file

    retries = 2;
    while (retries--) {
    	fd = open(POST_SEQNUM_FILE, O_RDWR);
    	if (fd >= 0) {
	    break;
	} else {
	    fd = open(POST_SEQNUM_FILE, O_RDWR|O_CREAT|O_EXCL, 0666);
	    if (fd >= 0) {
	    	break;
	    }
	}
    }

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    if (fd < 0) {
	ERRMSG(ER_SFILE_OPEN_FAILED, "[open(%s) failed, errno=%d]", 
	       POST_SEQNUM_FILE, errno);
	rv = 1;
	break;
    }

    ret = flock(fd, LOCK_EX);
    if (ret < 0) {
	ERRMSG(ER_SFILE_FLOCK_FAILED, "[flock(%s) failed, errno=%d]", 
	       POST_SEQNUM_FILE, errno);
	rv = 2;
	break;
    }

    // Get file sequence number value
    memset(numbuf, 0, sizeof(numbuf));
    ret = read(fd, numbuf, sizeof(numbuf));
    if (ret > 0) {
    	filenum = atol(numbuf);
    } else if (ret == 0) {
    	filenum = 1;
    } else {
	ERRMSG(ER_SFILE_READ_FAILED, "[read(%s) failed, ret=%d errno=%d]", 
	       POST_SEQNUM_FILE, ret, errno);
	rv = 3;
    	flock(fd, LOCK_UN);
	break;
    }

    // Increment file sequence number and write it back
    wrtcnt = snprintf(numbuf, sizeof(numbuf), "%ld\n", filenum + 1);
    lseek(fd, 0, SEEK_SET);
    xfercnt = write(fd, numbuf, wrtcnt);
    if (xfercnt != (ssize_t)wrtcnt) {
	ERRMSG(ER_SFILE_WRITE_FAILED, 
	       "[write(%s) failed, cnt=%ld expected=%ld errno=%d]",
	       POST_SEQNUM_FILE, xfercnt, wrtcnt, errno);
	rv = 4;
	break;
    }

    // Rename temp POST file to sequenced POST file
    snprintf(new_filename, sizeof(new_filename), "%s%s.%ld", 
	     POST_BASE_DATAFILE, script_basename, filenum);
    ret = rename(filename, new_filename);
    if (ret < 0) {
	ERRMSG(ER_PTFILE_RENAME_FAILED, "[rename(%s, %s) failed, errno=%d]", 
	       filename, new_filename, errno);
	rv = 5;
	break;
    }

    if (debug_filename) {
    	// Rename debug temp POST file to sequenced debug POST file
	snprintf(new_filename, sizeof(new_filename), "%s%s.%ld", 
		 POST_BASE_DEBUG_DATAFILE, script_basename, filenum);
	ret = rename(debug_filename, new_filename);
	if (ret < 0) {
	    ERRMSG(ER_PTFILE_DBG_RENAME_FAILED, 
	    	   "[rename(%s, %s) failed, errno=%d]", 
		   debug_filename, new_filename, errno);
	    rv = 6;
	    break;
	}
    }

    ret = flock(fd, LOCK_UN);
    close(fd);
    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    return rv;
}

static
int process_POST(const char *filename, const char *debug_filename)
{
    FILE *post_fd;

    char errbuf[1024 * 1024];
    char buf[1024 * 1024];
    size_t bytes_read;
    size_t bytes_write;
    size_t total_bytes_write = 0;
    long storage_bytes;
    char *script_basename;

    int ret;
    int rv = 0;

    ////////////////////////////////////////////////////////////////////////////
    while (1) {  // Begin while
    ////////////////////////////////////////////////////////////////////////////

    post_fd = fopen(filename, "w+");
    if (!post_fd) {
	ERRMSG(ER_PTFILE_FOPEN_FAILED, "[fopen(%s) failed]", filename);
	rv = 1;
	break;
    }
    if (debug_filename) {
    	DBG_fd = fopen(debug_filename, "w+");
	dump_cgi_env(filename);
    }

    // Enforce OCRP_BASE_DIR storage quota
    storage_bytes = dir_storage_used();
    if ((storage_bytes + cgiContentLength) > OCRP_DIR_MAX_STORAGE_BYTES) {
	ERRMSG(ER_STORE_QUOTA_EXCEEDED, "['%s' storage quota exceeded "
		"request=%d + current=%ld=%ld limit=%d]",
	       OCRP_BASE_DIR, cgiContentLength, storage_bytes, 
	       cgiContentLength + storage_bytes, OCRP_DIR_MAX_STORAGE_BYTES);
	rv = 2;
	break;
    }

    while ((bytes_read = fread(buf, 1, sizeof(buf), cgiIn)) > 0) {
	bytes_write = fwrite(buf, 1, bytes_read, post_fd);
	if (bytes_write != bytes_read) {
	    ERRMSG(ER_PTFILE_FWRITE_FAILED, 
		   "[fwrite err, actual=%ld expected=%ld]", 
		   bytes_write, bytes_read);
	    rv = 3;
	    break;
	}
	total_bytes_write += bytes_write;
    }

    if (!rv) {
    	if (feof(cgiIn)) {
	    fclose(post_fd);
	    post_fd = 0;
	    if (total_bytes_write == (size_t)cgiContentLength) {
	    	// All data successfully read

		script_basename = basename(cgiScriptName);
	    	ret = validate_POST(filename, script_basename);
		if (ret) {
		    rv = 4;
		    break;
		}

		ret = move_post_file(filename, debug_filename, script_basename);
		if (!ret) {
		    cgiHeaderStatus(200, (char *)"OK");
		} else {
		    rv = 5;
		}
	    } else {
	    	ERRMSG(ER_PTFILE_SHORT_WRITE,
		       "[short write, actual=%ld expected=%d]", 
		       total_bytes_write, cgiContentLength);
	    	rv = 6;
	    }
    	} else {
	    ERRMSG(ER_CGIIN_BADEOF, 
	    	   "[Error EOF, cgiIn, actual=%ld expected=%d]",
		   total_bytes_write, cgiContentLength);
	    rv = 7;
    	}
    }
    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    if (rv) {
    	if (post_fd) {
	    fclose(post_fd);
	}
	unlink(filename);
    }
    if (DBG_fd) {
	fclose(DBG_fd);
    }
    return rv;
}

static
int process_get_status(void)
{
    char errbuf[1024 * 1024];
    char buf[1024 * 1024];
    int fd;
    int rv = 0;
    int ret;

    fd = open(GET_STATUS_CONTENT_FILE, O_RDONLY);

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    if (fd < 0) {
	ERRMSG(ER_GETFILE_OPEN_FAILED, 
	       "[open(%s) failed, errno=%d]", GET_STATUS_CONTENT_FILE, errno);
	rv = 1;
	break;
    }

    ret = flock(fd, LOCK_EX);
    if (ret < 0) {
	ERRMSG(ER_GETFILE_FLOCK_FAILED,
	       "[flock(%s) failed, errno=%d]", GET_STATUS_CONTENT_FILE, errno);
	rv = 2;
	break;
    }

    cgiHeaderContentType((char *)"application/octet-stream");

    while ((ret = read(fd, buf, sizeof(buf))) > 0) {
    	fwrite(buf, 1, ret, cgiOut);
    }
    ret = flock(fd, LOCK_UN);
    close(fd);

    break;
    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    return rv;
}

static size_t 
curl_writefunc(void* buffer, size_t size, size_t nmemb, void *userp)
{
    UNUSED_ARGUMENT(userp);
    size_t bytes;

    bytes = fwrite(buffer, size, nmemb, cgiOut);

    DBGMSG("fwrite(bytes=%ld buf=%p, size=%ld nmemb=%ld userp=%p", 
		   bytes, buffer, size, nmemb, userp);
    return nmemb;
}

static int
curl_debugfunc(CURL *ech, curl_infotype type, char *buf, size_t bufsize, 
	       void *handle)
{
    char *info = (char*)alloca(bufsize+1);
    memcpy(info, buf, bufsize);
    info[bufsize] = '\0';

    switch (type) {
    case CURLINFO_TEXT:
    	DBGMSG("CURLINFO_TEXT [%s] buf=%p bufsize=%ld handle=%p\n", 
	       info, buf, bufsize, handle);
    	break;
    case CURLINFO_HEADER_IN:
    	DBGMSG("CURLINFO_HEADER_IN [%s] buf=%p bufsize=%ld handle=%p\n", 
	       info, buf, bufsize, handle);
    	break;
    case CURLINFO_HEADER_OUT:
    	DBGMSG("CURLINFO_HEADER_OUT [%s] buf=%p bufsize=%ld handle=%p\n", 
	       info, buf, bufsize, handle);
    	break;
    case CURLINFO_DATA_IN:
    	DBGMSG("CURLINFO_DATA_IN [%s] buf=%p bufsize=%ld handle=%p\n", 
	       info, buf, bufsize, handle);
    	break;
    case CURLINFO_DATA_OUT:
    	DBGMSG("CURLINFO_DATA_OUT [%s] buf=%p bufsize=%ld handle=%p\n", 
	       info, buf, bufsize, handle);
    	break;
    default:
    	DBGMSG("Invalid type, [%s] buf=%p bufsize=%ld handle=%p\n", 
	       info, buf, bufsize, handle);
    	break;
    }
    return 0;
}

static
int process_get_asset_map(void)
{
    char errbuf[1024 * 1024];
    char URL[4 * 1024];
    const char *key;
    const char *format;
    CURL *ech = 0;
    CURLcode eret = CURLE_OK;
    long arglval;

    int rv;
    int ret = 0;

    if (cgiQueryString && *cgiQueryString) {
    	// Get a specific entry
	key = strstr(cgiQueryString, GET_ASSET_MAP_QS_KEY);
	if (key) {
	    key += GET_ASSET_MAP_QS_KEY_STRLEN;
	}

	if (!key || !(*key)) {
	    ERRMSG(ER_BAD_QUERYSTRING, "[Invalid querystring=[%s]]", 
	    	   cgiQueryString);
	    return 1;
	}
    } else {
    	// Get all entries
	key = "/";
    }

    // Setup HTTP CURL client
    if (*key == '/') {
    	format = "http://%s:%d%s";
    } else {
    	format = "http://%s:%d/%s";
    }
    rv = snprintf(URL, sizeof(URL), format, 
		  HTTP_GET_ASSET_SERVER_IP, HTTP_GET_ASSET_SERVER_PORT, key);
    if (rv >= sizeof(URL)) {
    	URL[sizeof(URL)-1] = '\0';
    }


    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    ech = curl_easy_init();
    if (!ech) {
    	ret = 10;
	break;
    }

    eret = curl_easy_setopt(ech, CURLOPT_URL, URL);
    if (eret != CURLE_OK) {
    	ret = 11;
	break;
    }

#if 0
    arglval = 1;
    eret = curl_easy_setopt(ech, CURLOPT_HEADER, arglval);
    if (eret != CURLE_OK) {
    	ret = 12;
	break;
    }
#endif

    eret = curl_easy_setopt(ech, CURLOPT_WRITEFUNCTION, curl_writefunc);
    if (eret != CURLE_OK) {
    	ret = 13;
	break;
    }

    eret = curl_easy_setopt(ech, CURLOPT_DEBUGFUNCTION, curl_debugfunc);
    if (eret != CURLE_OK) {
    	ret = 14;
	break;
    }

    arglval = 1;
    eret = curl_easy_setopt(ech, CURLOPT_VERBOSE, arglval);
    if (eret != CURLE_OK) {
    	ret = 15;
	break;
    }

    arglval = 1;
    eret = curl_easy_setopt(ech, CURLOPT_NOSIGNAL, arglval);
    if (eret != CURLE_OK) {
    	ret = 16;
	break;
    }

    arglval = 100; // 100 msecs
    eret = curl_easy_setopt(ech, CURLOPT_CONNECTTIMEOUT_MS, arglval);
    if (eret != CURLE_OK) {
    	ret = 17;
	break;
    }

    cgiHeaderContentType((char *)"application/octet-stream");

    eret = curl_easy_perform(ech);
    if (eret != CURLE_OK) {
    	ret = 18;
	break;
    }

    break;
    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    if (ret) {
	ERRMSG(ER_GET_PROCESSING, "ret=%d eret=%d", ret, eret);
    }

    if (ech) {
    	curl_easy_cleanup(ech);
    }

    return ret;
}

static
int process_GET(void)
{
    char errbuf[1024 * 1024];
    char *script_basename;
    int rv;
    int ret = 0;

    script_basename = basename(cgiScriptName);

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    if (strcasecmp(script_basename, GET_SCRIPT_NAME_STATUS) == 0) {
    	rv = process_get_status();
	if (rv) {
	    ret = 100 + rv;
	    break;
	}
    } else if (strcasecmp(script_basename, GET_SCRIPT_NAME_ASSET_MAP) == 0) {
    	rv = process_get_asset_map();
	if (rv) {
	    ret = 200 + rv;
	    break;
	}
    } else {
	ERRMSG(ER_BAD_GET_SCRIPTNAME, "[Unknown scriptname=[%s]]", 
	       script_basename);
	ret = 1;
	break;
    }

    break;
    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    if (DBG_fd) {
	fclose(DBG_fd);
    }
    return ret;
}

int cgiMain(void) 
{
    char errbuf[1024 * 1024];
    char filename[1024];
    char debug_filename[1024];
    int enable_debug;

    int ret;
    int rv = 0;

    if (!strcasecmp(cgiRequestMethod, "POST")) {
    	if (!strcasecmp(cgiContentType, POST_CONTENT_TYPE)) {
	    enable_debug = 0;
	} else if (!strcasecmp(cgiContentType, POST_DEBUG_CONTENT_TYPE)) {
	    enable_debug = 1;
	} else {
	    ERRMSG(ER_BAD_POST_CT, "[Invalid POST Content-type: %s]", 
	    	   cgiContentType);
	    return -1;
	}

	ret = create_post_tmpfile(filename, sizeof(filename), 
			      enable_debug ? debug_filename : 0, 
			      enable_debug ? sizeof(debug_filename) : 0);
	if (ret) {
	    ERRMSG(ER_CREATE_TFILE_FAILED,
		   "[create_post_tmpfile() failed, ret=%d errno=%d]", 
		   ret, errno);
	    return -1;
	}

	ret = process_POST(filename, enable_debug ? debug_filename : 0);
	if (ret) {
	    return -1;
	}
    } else if (!strcasecmp(cgiRequestMethod, "GET")) {
    	if (!strcasecmp(cgiContentType, GET_DEBUG_CONTENT_TYPE)) {
	    enable_debug = 1;
	    ret = create_get_debug_file();
	} else {
	    enable_debug = 0;
	}
	ret = process_GET();
	if (ret) {
	    return -1;
	}
    }
    return rv;
}

/*
 * End of OCRP_cgi.c
 */
