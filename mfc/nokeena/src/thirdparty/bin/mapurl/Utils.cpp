//
// (C) Copyright 2014, Juniper Networks, Inc.
// All rights reserved.
//

//
// Utils.cpp -- Miscellaneous utility routines
//
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

#include "Utils.h"
#include "uf_utils.h"
#include "mapurl.h"

int printmsg(const char *file, int line, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    printf("[%s:%d]:", file, line);
    vprintf(fmt, ap);
    printf("\n");

    return 0;
}

int MMapFile(const char *file, int *fd, char **addr, size_t *size) 
{
    int ret = 0;
    int rv;
    struct stat sb;

    *fd = -1;

    ////////////////////////////////////////////////////////////////////////////
    while (1) {
    ////////////////////////////////////////////////////////////////////////////

    *fd = open(file, O_RDONLY);
    if (*fd < 0) {
    	PRT("open(%s) failed, errno=%d", file, errno);
   	ret = 1;
	break;
    }
    rv = fstat(*fd, &sb);
    if (rv != 0) {
    	PRT("fstat(fd=%d) failed, errno=%d", *fd, errno);
   	ret = 2;
	break;
    }
    *size = sb.st_size;

    *addr = (char *)mmap64(0, *size, PROT_READ, MAP_PRIVATE, *fd, 0);
    if (*addr == MAP_FAILED) {
    	PRT("mmap64(fd=%d) failed, errno=%d", *fd, errno);
    	ret = 3;
	break;
    }

    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End of while
    ////////////////////////////////////////////////////////////////////////////
    
    if (ret) {
    	if (*fd >= 0) {
	    close(*fd);
	}
    }
    return ret;
}

static unsigned char get_hex_value(char c)
{
    if (c >= '0' && c <= '9') {
	return c - '0';
    } else if (c >= 'a' && c <= 'f') {
	return c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
	return c - 'A' + 10;
    } else {
	return c;
    }
}

static
char *int_UnescapeStr(const char *p, int len, int query_str, char *outbuf,
		      int outbufsz, int *outbytesused /* Not including NULL */ )
{
    int n;
    int outbuf_ix;
    unsigned char cval1 = 0;
    unsigned char cval2;

    enum { GETCHAR, HEXCHAR_1, HEXCHAR_2 };
    int state;
    int encoded_percent = 0;

    state = GETCHAR;

    if (!outbuf) {
	return 0;
    }
    outbuf_ix = 0;

    for (n = 0; n < len; n++) {
	if (outbuf_ix >= outbufsz) {
	    PRT("outbuf size exceeded, len=%d outbufsz=%d", len, outbufsz);
	    outbuf_ix--;	/* Add premature NULL and exit */
	    goto exit;
	}
	switch (state) {
	case GETCHAR:
	    if (query_str && (p[n] == '+')) {
		outbuf[outbuf_ix++] = p[n];
	    } else if (p[n] == '%') {
		state = HEXCHAR_1;
	    } else {
		outbuf[outbuf_ix++] = p[n];
	    }
	    break;

	case HEXCHAR_1:
	    cval1 = get_hex_value(p[n]);
	    if (p[n] == cval1) {	/* Not hex */
		outbuf[outbuf_ix++] = '%';
		n--;
		state = GETCHAR;
	    } else {
		state = HEXCHAR_2;
	    }
	    break;

	case HEXCHAR_2:
	    cval2 = get_hex_value(p[n]);
	    if (p[n] == cval2) {	/* Not hex */
		outbuf[outbuf_ix++] = '%';
		n -= 2;
	    } else {
		/* bug 1188 Removing %00 */
		if(cval1 | cval2){
		    outbuf[outbuf_ix++] = (cval1 << 4) | cval2;
		    if (outbuf[outbuf_ix-1] == '%') {
		    	encoded_percent++;
		    }
		}
	    }
	    state = GETCHAR;
	    break;

	default:
	    PRT("Invalid state state=%d", state);
	    return 0;
	    break;
	}
    }

exit:

    outbuf[outbuf_ix++] = '\0';
    if (outbytesused) {
	*outbytesused = outbuf_ix;
    }

    if (encoded_percent) {
    	char *tbuf;
    	tbuf = (char*)alloca(outbuf_ix);
	memcpy(tbuf, outbuf, outbuf_ix);
	return int_UnescapeStr(tbuf, outbuf_ix-1, query_str, outbuf, 
			       outbufsz, outbytesused);
    }
    return outbuf;
}

char *UnescapeStr(const char *p, int len, int query_str, char *outbuf,
	      	  int outbufsz, int *outbytesused /* Not including NULL */ )
{
    char *rp;
    int removed_slashes;
    char *before_buf;

    rp = int_UnescapeStr(p, len, query_str, outbuf, outbufsz, outbytesused);
    if (!rp) {
    	return rp;
    }

    // Remove redundant '/'(s) from URL absolute path

    if (!opt_verbose) {
    	removed_slashes = CompressURLSlashes(outbuf, outbytesused);
    } else {
    	before_buf = (char*)alloca(*outbytesused+1);
	memcpy((void*)before_buf, (void*)outbuf, *outbytesused);
	before_buf[*outbytesused] = '\0';

    	removed_slashes = CompressURLSlashes(outbuf, outbytesused);
	if (removed_slashes) {
	    PRT("removed_slashes=%d\n[Before]: \"%s\"\n[After]:  \"%s\"\n", 
	    	removed_slashes, before_buf, outbuf);
	}
    }
    return rp;
}

int validFQDNPort(const char *name, int name_len)
{
    int n;
    int scan_for_port = 0;

    for (n = 0; n < name_len; n++) {
    	if (!scan_for_port) {
	    if (!isalnum(name[n])) {
		switch(name[n]) {
		case '.':
		case '-':
		case '_':
		    break;
		case ':':
		    if ((n + 1) < name_len) {
		    	scan_for_port = 1;
		    	break;
		    } else {
		    	return 0; // host: without port number
		    }
		default:
		    return 0;
		}
	    }
	} else {
	    if (!isdigit(name[n])) {
	    	return 0;
	    }
	}
    }
    return 1;
}

//
// End of Utils.cpp
//
