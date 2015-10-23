//
// (C) Copyright 2014, Juniper Networks, Inc.
// All rights reserved.
//

//
// URLfmtMap.cpp -- Translate URLfmt to binary format
//
// Input File format:
//   Type=URL Version=1.0\r\n
//   http://xyz.com/\r\n
//   http://xyz.com:8081/\r\n
//   http://abc.com/index.html\r\n
//   http://def.com/a/b/c/d/e\r\n
//   \r\n\r\n
//
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "mapurl.h"
#include "Utils.h"
#include "uf_fmt_url_bin.h"

#define INPUT_FILE_TRAILER "\r\n\r\n"
#define INPUT_FILE_TRAILER_LEN 4
#define INPUT_FILE_LINE_TERM "\r\n"
#define INPUT_FILE_LINE_TERM_LEN 2
#define INPUT_FILE_MAXLINESZ (64 * 1024)

#define INPUT_FILE_REC_HDR "http://"
#define INPUT_FILE_REC_HDR_LEN 7
 
 #define INPUT_TYPE "Type=URL"
 #define INPUT_VERSION "Version=1.0"

static
int valid_source(const char *addr, size_t src_fsize)
{
    int rv;
    const char *p;
    const char *p_end;
    char buf[INPUT_FILE_MAXLINESZ];
    int line_size;

    // Verify file length
    if ((long)(src_fsize - INPUT_FILE_TRAILER_LEN 
    		- INPUT_FILE_LINE_TERM_LEN) <= 0) {
    	PRT("Invalid input file length, filesize=%ld", src_fsize);
	return 6;
    }

    // Verify file trailer
    p = addr + src_fsize - INPUT_FILE_TRAILER_LEN;
    rv = bcmp(p, INPUT_FILE_TRAILER, INPUT_FILE_TRAILER_LEN);
    if (rv) { 
    	PRT("Invalid input file trailer, bcmp() rv=%d", rv);
	return 1;
    }

    // Verify file trailer contains no data
    p -= INPUT_FILE_LINE_TERM_LEN;
    rv = bcmp(p, INPUT_FILE_LINE_TERM, INPUT_FILE_LINE_TERM_LEN);
    if (rv) {
    	PRT("Invalid input file, trailer record contains data");
	return 2;
    }

    // Verify file header
    p = addr;
    p_end = strstr(addr, INPUT_FILE_LINE_TERM);
    line_size = p_end - addr;

    if (line_size >= INPUT_FILE_MAXLINESZ) {
    	PRT("Invalid file header (line 1), size >= %d", INPUT_FILE_MAXLINESZ);
	return 3;
    }
    memcpy(buf, addr, line_size);
    buf[line_size] = '\0';

    p = strstr(buf, INPUT_TYPE);
    if (!p) {
    	PRT("Invalid input file type, expect [%s]", INPUT_TYPE);
	return 4;
    }

    p = strstr(buf, INPUT_VERSION);
    if (!p) {
    	PRT("Invalid input file version, expect [%s]", INPUT_VERSION);
	return 5;
    }
    return 0;
}

static
int transform_record(char *pbuf, int pbuf_strlen)
{
    int ret = 0;
    char *p;
    int slen;
    char *outbuf;
    char *res_outbuf;
    int outbufused;

    // Map hostname to upper case
    for (p = pbuf; *p && (*p != '/'); p++) {
        *p = toupper(*p);
    }

    // Strip :80
    if ((p[-3] == ':') && (p[-2] == '8') && (p[-1] == '0')) {
    	memmove(&p[-3], p, pbuf_strlen - (&p[-3] - pbuf) - 3 + 1);
	p = &p[-3];
    }

    if (!validFQDNPort(pbuf, p - pbuf)) {
	PRT("Invalid character(s) in FQDN:port");
	ret = 1;
	goto exit;
    }

    if (*p == '/') {
    	// Unescape %XX encoding
	slen = strlen(p);
	outbuf = (char *)alloca(slen + 1);
	res_outbuf = UnescapeStr(p, slen, 0, outbuf, slen, &outbufused);

	if (res_outbuf) {
	    memcpy(p, outbuf, outbufused);
	    p[outbufused] = '\0';
	} else {
	    PRT("UnescapeStr() failed, slen=%d outbufused=%d", 
	    	slen, outbufused);
	    ret = 2;
	}
    } else {
    	// Domain only without terminating slash.  Add slash.
	*p = '/';
	*(p + 1) = '\0';
    }

exit:

    return ret;
}

static
int process_data(const char *addr, size_t src_fsize, int dest_fd)
{
    int ret = 0;
    int rv;
    bin_uf_fmt_url_hdr_t hdr;
    bin_uf_fmt_url_rec trailer_rec;

    ssize_t cnt;
    ssize_t write_cnt;
    const char *curp = addr;
    const char *curpend;
    const char *p;
    char *pbuf;
    int buf_strlen;
    int line;

    char buf[INPUT_FILE_MAXLINESZ];
    bin_uf_fmt_url_rec_t *prec;

    prec = (bin_uf_fmt_url_rec_t *)alloca(sizeof(bin_uf_fmt_url_rec_t) + 
    					  INPUT_FILE_MAXLINESZ + 1024 /*pad*/);
    memset((void *)&hdr, 0, sizeof(hdr));
    memset((void *)&trailer_rec, 0, sizeof(trailer_rec));

    // Write file header
    hdr.magicno = BIN_UF_FMT_URL_MAGIC;
    hdr.version_maj = BIN_UF_FMT_URL_VERS_MAJ;
    hdr.version_min = BIN_UF_FMT_URL_VERS_MIN;
    hdr.options = BIN_UF_FMT_URL_OPT_LE;

    ////////////////////////////////////////////////////////////////////////////
    while (1) {
    ////////////////////////////////////////////////////////////////////////////

    cnt = write(dest_fd, &hdr, sizeof(hdr));
    if (cnt != sizeof(hdr)) {
    	PRT("write() failed, cnt=%ld expected=%ld", cnt, sizeof(hdr));
	ret = 1;
	break;
    }

    curp = strstr(curp, INPUT_FILE_LINE_TERM);
    curp += INPUT_FILE_LINE_TERM_LEN;
    line = 2;

    while (1) {
       	curpend = strstr(curp, INPUT_FILE_LINE_TERM);

	if ((curpend - curp) >= INPUT_FILE_MAXLINESZ) {
	    PRT("line=%d invalid record length >= %d", 
	    	line, INPUT_FILE_MAXLINESZ);
	    ret = 2;
	    break;
	}

	// Copy line data to buf less space characters
	for (p = curp, pbuf = buf, buf_strlen = 0; p < curpend; p++) {
	    if (*p != ' ') {
	    	*(pbuf++) = *p;
		buf_strlen++;
	    }
	}
	*pbuf = '\0';

	pbuf = strstr(buf, INPUT_FILE_REC_HDR);
	if (!pbuf) {
	    PRT("line=%d missing record header (%s)", 
	    	line, INPUT_FILE_REC_HDR);
	    ret = 3;
	    break;
	}

	// Transform data to internal format
	pbuf += INPUT_FILE_REC_HDR_LEN;

	rv = transform_record(pbuf, buf_strlen - (pbuf - buf));
	if (rv) {
	    PRT("transform_record() failed, line=%d rv=%d", line, rv);
	    ret = 4;
	    break;
	}

	// Write record to output
	prec->flags = 0;
	prec->sizeof_data = strlen(pbuf) + 1;
	memcpy(prec->data, pbuf, prec->sizeof_data);

	write_cnt = sizeof(*prec) + prec->sizeof_data - 1;
    	cnt = write(dest_fd, prec, write_cnt);
    	if (cnt != write_cnt) {
	    PRT("record write() failed, cnt=%ld expected=%ld", cnt, write_cnt);
	    ret = 5;
	    break;
    	}

	curp = curpend + INPUT_FILE_LINE_TERM_LEN;
       	if (!bcmp(curp, INPUT_FILE_TRAILER, INPUT_FILE_TRAILER_LEN)) {
	    break; // EOF
       	}
	line++;
    }

    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End of while
    ////////////////////////////////////////////////////////////////////////////

    if (!ret) {
    	// Write trailer record
    	cnt = write(dest_fd, &trailer_rec, sizeof(trailer_rec));
    	if (cnt != sizeof(trailer_rec)) {
	    PRT("trailer write() failed, cnt=%ld expected=%ld", 
	    	cnt, sizeof(hdr));
	    ret = 2;
    	}
    }
    return ret;
}

int URLfmt_handler(const char *input, const char *output)
{
    int rv;
    int ret = 0;
    int src_fd = -1;
    char *src_addr;
    size_t src_fsize;
    int dest_fd = -1;

    ////////////////////////////////////////////////////////////////////////////
    while (1) {
    ////////////////////////////////////////////////////////////////////////////

    rv = MMapFile(input, &src_fd, &src_addr, &src_fsize);
    if (rv) {
    	PRT("MMapFile(%s) failed, rv=%d", input, rv);
    	ret = 100 + rv;
	break;
    }

    dest_fd = open(output, O_RDWR|O_CREAT|O_TRUNC, 0644);
    if (dest_fd < 0) {
    	PRT("open(%s) failed, errno=%d", output, errno);
    	ret = 1;
	break;
    }

    rv = valid_source(src_addr, src_fsize);
    if (rv) {
    	PRT("valid_source(%s) failed, rv=%d", input, rv);
    	ret = 2;
	break;
    }

    rv = process_data(src_addr, src_fsize, dest_fd);
    if (rv) {
    	PRT("process_data(%s) failed, rv=%d", input, rv);
	ret = 3;
	break;
    }

    break;

    ////////////////////////////////////////////////////////////////////////////
    } // end of while
    ////////////////////////////////////////////////////////////////////////////

    if (src_fd >= 0) {
    	munmap((void *)src_addr, src_fsize);
    	close(src_fd);
    }

    if (dest_fd >= 0) {
    	close(dest_fd);
    }
    return ret;
}

//
// End of URLfmtMap.cpp
//
