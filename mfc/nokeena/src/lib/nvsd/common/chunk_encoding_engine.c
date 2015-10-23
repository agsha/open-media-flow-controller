/*
 * chunk_encoding_engine.c -- To/From Chunk Encoding transformation support.
 */
#include <sys/param.h>
#include "nkn_defs.h"
#include "nkn_debug.h"
#include "http_header.h"
#include "chunk_encoding_engine.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "nkn_assert.h"

/*
 *******************************************************************************
 * Definitions
 *******************************************************************************
 */
#define DBG(_fmt, ...) DBG_LOG(MSG, MOD_CE, _fmt, __VA_ARGS__)

#if 1
#define STATIC static
#else
#define STATIC
#endif

#define CR '\r'
#define LF '\n'
#define SPC ' '

/*
 *******************************************************************************
 * Declarations
 *******************************************************************************
 */
STATIC int init_chunk2data(chunk_encoding_ctx_t *CTX, 
			   chunk_encoding_input_args_t *IN, int *retcode);

STATIC int parse_chunk_header(chunk_encoding_ctx_t *CTX,
			      chunk_encoding_input_args_t *IN, int *retcode);

STATIC int copy_data(chunk_encoding_ctx_t *CTX,
		     chunk_encoding_input_args_t *IN, int *retcode);

STATIC int parse_chunk_trailer(chunk_encoding_ctx_t *CTX,
		     	       chunk_encoding_input_args_t *IN, int *retcode);

STATIC int chunk_complete(chunk_encoding_ctx_t *CTX, 
			  chunk_encoding_input_args_t *IN);

STATIC void init_per_invocation_data(chunk_encoding_ctx_t *CTX);

STATIC int getchar_input(chunk_encoding_ctx_t *CTX, 
		  	 chunk_encoding_input_args_t *IN, char *outchar);

STATIC char *get_input_data(chunk_encoding_ctx_t *CTX, 
			    chunk_encoding_input_args_t *IN, off_t *len);

STATIC void consume_input(chunk_encoding_ctx_t *CTX, off_t bytes);

STATIC char *get_output_data(chunk_encoding_ctx_t *CTX, 
		      	     chunk_encoding_input_args_t *IN, off_t *len);

STATIC int consume_output(chunk_encoding_ctx_t *CTX, 
			  chunk_encoding_input_args_t *IN, off_t bytes, 
			  int flushbuffer);
/*
 *******************************************************************************
 *		P R I V A T E  F U N C T I O N S
 *******************************************************************************
 */
STATIC
int init_chunk2data(chunk_encoding_ctx_t *CTX, 
		    chunk_encoding_input_args_t *IN, int *retcode)
{
    UNUSED_ARGUMENT(IN);
    UNUSED_ARGUMENT(retcode);

    CTX->state = CE_ST_PARSE_HDR;
    CTX->parse_chunk_header_state = PC_SCAN_CHUNK_LEN;
    CTX->chunksize = 0;
    CTX->chunk_bytes_xfered = 0;
    return 0;
}

STATIC
int parse_chunk_header(chunk_encoding_ctx_t *CTX, 
		       chunk_encoding_input_args_t *IN, int *retcode)
{
    char c;

    while (!getchar_input(CTX, IN, &c)) {
	c = tolower(c);
	switch(CTX->parse_chunk_header_state) {
	case PC_SCAN_CHUNK_LEN:
	    if ((c >= '0') && (c <= '9')) {
		CTX->chunksize *= 16;
		CTX->chunksize += c - '0';
	    } else if ((c >= 'a') && (c <= 'f')) {
		CTX->chunksize *= 16;
		CTX->chunksize += (c - 'a') + 10;
	    } else {
		DBG("Bad chunk length sequence, CTX=%p", CTX);
		*retcode = 200+1; // Invalid chunk data format
		return 1;
	    }
	    CTX->parse_chunk_header_state = PC_SCAN_CHUNK_LEN_NEXT;
	    break;
	case PC_SCAN_CHUNK_LEN_NEXT:
	    if ((c >= '0') && (c <= '9')) {
		CTX->chunksize *= 16;
		CTX->chunksize += c - '0';
	    } else if ((c >= 'a') && (c <= 'f')) {
		CTX->chunksize *= 16;
		CTX->chunksize += (c - 'a') + 10;
	    } else if (c == CR) {
		CTX->parse_chunk_header_state = PC_SCAN_LF;
	    } else if (c == SPC) {
		CTX->parse_chunk_header_state = PC_SCAN_CHUNK_EXTENSION;
	    } else if (c == ';') {
		CTX->parse_chunk_header_state = PC_SCAN_CHUNK_EXTENSION;
	    } else {
		DBG("Bad chunk length sequence, CTX=%p", CTX);
		*retcode = 200+1; // Invalid chunk data format
		return 1;
	    }
	    break;
	case PC_SCAN_CHUNK_EXTENSION:
	    if (c == CR) {
		CTX->parse_chunk_header_state = PC_SCAN_LF;
	    }
	    break;
	case PC_SCAN_CR:
	    if (c == CR) {
		CTX->parse_chunk_header_state = PC_SCAN_LF;
	    }
	    break;
	case PC_SCAN_LF:
	    if (c == LF) {
		if (CTX->chunksize == 0) {
		    CTX->parse_chunk_trailer_state = PCT_SCAN_CR;
		    CTX->state = CE_ST_PARSE_TRAILER;
		} else {
		    CTX->copy_data_state = CD_SCAN_MOVE_DATA;
		    CTX->state = CE_ST_GET_DATA;
		}
		return 0;
	    } else {
		DBG("Bad CRLF termination sequence, CTX=%p", CTX);
		*retcode = 200+1; // Invalid chunk data format
		return 1;
	    }
	default:
	    DBG("Invalid state, CTX=%p", CTX);
	    *retcode = 200+2; // Invalid state
	    return 1;
	}
    }
    return 1; // Terminate, no more input data
}

STATIC
int copy_data(chunk_encoding_ctx_t *CTX, chunk_encoding_input_args_t *IN, 
	      int *retcode)
{
    off_t in_len;
    char *p_in;
    off_t out_len;
    char *p_out;
    off_t bytes_to_xfer;
    int rv;
    char c;

    switch(CTX->copy_data_state) {
    case CD_SCAN_MOVE_DATA:
    	while (CTX->chunk_bytes_xfered < CTX->chunksize) {
    	    p_in = get_input_data(CTX, IN, &in_len);
	    if (!p_in) {
	   	 return 1; // Terminate, need more data
	    }

    	    p_out = get_output_data(CTX, IN, &out_len);
	    if (!p_out) {
	   	 /* Should never happen */
	    	DBG("No more output data, CTX=%p", CTX);
	    	*retcode = 300+1;
	    	return 1;
	    }
	    bytes_to_xfer = CTX->chunksize - CTX->chunk_bytes_xfered;
	    bytes_to_xfer = MIN(bytes_to_xfer, in_len);
	    bytes_to_xfer = MIN(bytes_to_xfer, out_len);
	    if (!(CTX->flags & CE_FL_PARSE_C2D)) {
	    	memcpy(p_out, p_in, bytes_to_xfer);
	    }

	    consume_input(CTX, bytes_to_xfer);
	    consume_output(CTX, IN, bytes_to_xfer, 0);
	    CTX->chunk_bytes_xfered += bytes_to_xfer;
    	}
	CTX->copy_data_state = CD_SCAN_CR;
	/* Fall through */

    case CD_SCAN_CR:
	rv = getchar_input(CTX, IN, &c);
	if (rv) {
	    return 1; // Terminate, no more input data
	}
	if (c == CR) {
	    CTX->copy_data_state = CD_SCAN_LF;
	} else {
	    DBG("No CRLF sequence after chunk, CTX=%p", CTX);
	    *retcode = 300+2;
	    return 1;
	}
	/* Fall through */
    	
    case CD_SCAN_LF:
	rv = getchar_input(CTX, IN, &c);
	if (rv) {
	    return 1; // Terminate, no more input data
	}
	if (c == LF) {
	    CTX->state = CE_ST_INIT_C2D;
	    return 0;
	} else {
	    DBG("No CRLF sequence after chunk, CTX=%p", CTX);
	    *retcode = 300+3;
	    return 1;
	}
    default:
        break;
    }

    DBG("Invalid state, CTX=%p state=%d", CTX, CTX->copy_data_state);
    *retcode = 300+4;
    return 1;
}

STATIC
int parse_chunk_trailer(chunk_encoding_ctx_t *CTX, 
			chunk_encoding_input_args_t *IN, int *retcode)
{
    char c;
    int rv;

    while (!getchar_input(CTX, IN, &c)) {
restart:
    	switch(CTX->parse_chunk_trailer_state) {
    	case PCT_SCAN_CR:
	    if (c == CR) {
	    	CTX->parse_chunk_trailer_state = PCT_SCAN_LF;
	    } else {
	    	CTX->parse_chunk_trailer_state = PCT_COPY_DATA;
	    	goto restart;
	    }
	    break;
    	case PCT_SCAN_LF:
	    if (c == LF) {
	    	rv = chunk_complete(CTX, IN);
		if (!rv) {
		    *retcode = -1; // EOF
		} else {
		    DBG("chunk_complete() failed, rv=%d CTX=%p", rv, CTX);
		    *retcode = 400+1;
		}
	    } else {
		DBG("Invalid CRLF sequence, CTX=%p", CTX);
		*retcode = 400+2;
	    }
	    return 1;

    	case PCT_COPY_DATA:
    	case PCT_COPY_DATA_LF:
	    if (!CTX->trailer_data) {
	    	CTX->trailer_data_size = 4096;
	    	CTX->trailer_data_used = 0;
		CTX->trailer_data = nkn_malloc_type(CTX->trailer_data_size+1, mod_chunk_trailer_data);
		if (!CTX->trailer_data) {
		    DBG("nkn_malloc_type() failed, CTX=%p", CTX);
		    *retcode = 400+3;
		    return 1;
		}
	    } 
	    if (CTX->trailer_data_used < CTX->trailer_data_size) {
		CTX->trailer_data[CTX->trailer_data_used++] = c;

		switch(CTX->parse_chunk_trailer_state) {
		case PCT_COPY_DATA:
		    if (c == CR) {
			CTX->parse_chunk_trailer_state = PCT_COPY_DATA_LF;
		    }
		    break;
		case PCT_COPY_DATA_LF:
		    if (c == LF) {
			CTX->parse_chunk_trailer_state = PCT_SCAN_CR;
		    }
		    break;
		default:
		    DBG("Invalid state, CTX=%p state=%d", CTX, 
			CTX->parse_chunk_trailer_state);
		    *retcode = 400+4;
		    return 1;
		}
	    } else {
		DBG("trailer buffer exceeded, CTX=%p", CTX);
		*retcode = 400+5;
		return 1;
	    }
	    break;
    	default:
	    DBG("Invalid state, CTX=%p state=%d", CTX, 
	    	CTX->parse_chunk_trailer_state);
	    *retcode = 400+6;
	    return 1;
    	}
    }
    return 1; // Terminate, no more input data
}

STATIC
int chunk_complete(chunk_encoding_ctx_t *CTX, chunk_encoding_input_args_t *IN)
{
    int rv;
    mime_header_t *phttphdr;

    char *p;
    char *phdr;
    int hdr_len;
    char *pval;
    int val_len;
    int hdr_enum;
    char content_length_str[64];

    phttphdr = (mime_header_t *)nkn_malloc_type(sizeof(mime_header_t), mod_chunk_http_header_t);
    if (!phttphdr) {
    	DBG("nkn_malloc_type() failed, size=%ld, CTX=%p", sizeof(mime_header_t), CTX);
	return 1;
    }
    rv = init_http_header(phttphdr, 0, 0);
    if (rv) {
    	DBG("init_http_header() failed, rv=%d, CTX=%p", rv, CTX);
	return 2;
    }

    if (CTX->trailer_data) {
        /* Trailer headers present */
	CTX->trailer_data[CTX->trailer_data_used] = '\0';
	p = CTX->trailer_data;

	while (p <= &CTX->trailer_data[CTX->trailer_data_used-1]) {
	    phdr = p;
	    p = strchr(phdr, ':');
	    if (p) {
	    	hdr_len = p-phdr;
	    } else {
	    	break;
	    }

	    pval = p+1;
	    p = strchr(p, CR);
	    if (p) {
	        while (*pval == SPC) {
		    pval++;
		}
	    	val_len = p-pval;
	    } else {
	    	break;
	    }
	    p = p + 2; // Skip LF

	    if (!string_to_known_header_enum(phdr, hdr_len, &hdr_enum)) {
	    	rv = add_known_header(phttphdr, hdr_enum, pval, val_len);
		if (rv) {
		    DBG("add_known_header() failed, rv=%d, CTX=%p", rv, CTX);
		}
	    } else {
	    	rv = add_unknown_header(phttphdr, phdr, hdr_len,
					pval, val_len);
		if (rv) {
		    DBG("add_unknown_header() failed, rv=%d, CTX=%p", rv, CTX);
		}
	    }
	}
    }

    /* Add Content-Length header */
    rv = snprintf(content_length_str, sizeof(content_length_str), "%ld",
    		  CTX->st_content_bytes);
    content_length_str[rv] = '\0';
    rv = add_known_header(phttphdr, MIME_HDR_CONTENT_LENGTH, content_length_str,
    			  strlen(content_length_str));
    /* Flush output buffer */
    consume_output(CTX, IN, 0, 1 /* Flush */);

    *IN->trailer_hdrs = phttphdr;
    return 0;
}

STATIC
void init_per_invocation_data(chunk_encoding_ctx_t *CTX)
{
    memset(&CTX->st_in_cur_iovec, 0, sizeof(CTX->st_in_cur_iovec));
    CTX->st_in_cur_iovec_ix = 0;

    memset(&CTX->getchar_in_iovec, 0, sizeof(CTX->getchar_in_iovec));
}

STATIC
int getchar_input(chunk_encoding_ctx_t *CTX, 
		  chunk_encoding_input_args_t *IN, char *outchar)
{
    if (!CTX->getchar_in_iovec.iov_len) {
        CTX->getchar_in_iovec.iov_base = 
		get_input_data(CTX, IN, &CTX->getchar_in_iovec.iov_len);
	if (!CTX->getchar_in_iovec.iov_len) {
	    return 1;
	}
    }

    *outchar = *CTX->getchar_in_iovec.iov_base;
    CTX->getchar_in_iovec.iov_base++;
    CTX->getchar_in_iovec.iov_len--;
    consume_input(CTX, 1);

    return 0;
}

STATIC
char *get_input_data(chunk_encoding_ctx_t *CTX, chunk_encoding_input_args_t *IN,
		     off_t *len)
{
    CTX->getchar_in_iovec.iov_len = 0; // Disable getchar_input() buffer
retry:
    if (!CTX->st_in_cur_iovec.iov_base) {
    	assert(IN->in_data_offset <= IN->in_iovec->iov_len);

	CTX->st_in_cur_iovec = *IN->in_iovec;
	CTX->st_in_cur_iovec.iov_base += IN->in_data_offset;
	CTX->st_in_cur_iovec.iov_len -= IN->in_data_offset;
	CTX->st_in_cur_iovec_ix++;

	if (!CTX->st_in_cur_iovec.iov_len && 
	    (CTX->st_in_cur_iovec_ix < IN->in_iovec_len)) {
	    goto retry;
	}

    } else if (!CTX->st_in_cur_iovec.iov_len) {
        if (CTX->st_in_cur_iovec_ix < IN->in_iovec_len) {
	    CTX->st_in_cur_iovec = IN->in_iovec[CTX->st_in_cur_iovec_ix];
	    CTX->st_in_cur_iovec_ix++;
	} else {
	    CTX->st_in_cur_iovec.iov_base = 0;
	}
    }

    *len = CTX->st_in_cur_iovec.iov_len;
    return CTX->st_in_cur_iovec.iov_base;
}

STATIC
void consume_input(chunk_encoding_ctx_t *CTX, off_t bytes)
{
    assert(bytes <= CTX->st_in_cur_iovec.iov_len);
    CTX->st_in_cur_iovec.iov_base += bytes;
    CTX->st_in_cur_iovec.iov_len -= bytes;
    CTX->st_in_data_offset += bytes;
}

STATIC
char *get_output_data(chunk_encoding_ctx_t *CTX, 
		      chunk_encoding_input_args_t *IN, off_t *len)
{
    UNUSED_ARGUMENT(IN);

    if (!CTX->st_outbuf_cur_iovec.iov_len) {
    	CTX->st_outbuf_token = (*CTX->alloc_buf)
					(&CTX->st_outbuf_cur_iovec.iov_len);
    	if (CTX->st_outbuf_token) {
    	    CTX->st_outbuf_cur_iovec.iov_base = 
	    	(*CTX->buf2data)(CTX->st_outbuf_token);
	    if (CTX->st_outbuf_cur_iovec.iov_base) {
	    	CTX->st_outbuf_base_iovec = CTX->st_outbuf_cur_iovec;
	    }
    	}
    }

    *len = CTX->st_outbuf_cur_iovec.iov_len;
    return CTX->st_outbuf_cur_iovec.iov_base;
}

STATIC
int consume_output(chunk_encoding_ctx_t *CTX, chunk_encoding_input_args_t *IN,
		   off_t bytes, int flushbuffer)
{
    if (flushbuffer) {
    	bytes = 0;
    }
    assert(bytes <= CTX->st_outbuf_cur_iovec.iov_len);
    CTX->st_outbuf_cur_iovec.iov_base += bytes;
    CTX->st_outbuf_cur_iovec.iov_len -= bytes;
    CTX->st_content_bytes += bytes;

    if (CTX->st_outbuf_token && 
    	(flushbuffer || !CTX->st_outbuf_cur_iovec.iov_len)) {
    	if (*IN->used_out_iovecs < IN->out_iovec_len) {
	    IN->out_iovec_token[*IN->used_out_iovecs] = CTX->st_outbuf_token;
	    IN->out_iovec[*IN->used_out_iovecs] = CTX->st_outbuf_base_iovec;
	    if (flushbuffer) {
	    	IN->out_iovec[*IN->used_out_iovecs].iov_len -= 
					CTX->st_outbuf_cur_iovec.iov_len;
	    }
	    CTX->st_bytes_to_caller += 
	    	IN->out_iovec[(*IN->used_out_iovecs)++].iov_len;

	    CTX->st_outbuf_token = 0;
	    CTX->st_outbuf_cur_iovec.iov_base = 0;
	} else if ((*IN->used_out_iovecs == IN->out_iovec_len) && flushbuffer) {
	    IN->out_iovec[*IN->used_out_iovecs].iov_len -=
					CTX->st_outbuf_cur_iovec.iov_len;
	    return 0;
	} else {
	    DBG_LOG(SEVERE, MOD_CE, "output iovec full");
	    NKN_ASSERT(0);
	    return 1; // output iovec full, should never happen
	}
    }
    return 0;
}

static void *nop_alloc_buf(off_t *bufsize)
{
    *bufsize = 32 * 1024;
    return (void *) 0xdeaddead;
}

static char *nop_buf2data(void *token)
{
    UNUSED_ARGUMENT(token);
    return (char *) 0xffffdead;
}

static int nop_free_buf(void *token)
{
    UNUSED_ARGUMENT(token);
    return 0;
}

/*
 *******************************************************************************
 *		P U B L I C  F U N C T I O N S
 *******************************************************************************
 */
chunk_encoding_ctx_t * 
ce_alloc_chunk_encoding_ctx_t(int chunk2data, 
		              void *(*alloc_buf)(off_t *),
			      char *(*buf2data)(void *),
		              int (*free_buf)(void *))
{
    chunk_encoding_ctx_t *CTX;

    CTX = (chunk_encoding_ctx_t *)
    	nkn_calloc_type(1, sizeof(chunk_encoding_ctx_t), mod_chunk_encoding_ctx_t);
    if (CTX) {
    	CTX->state = chunk2data ? CE_ST_INIT_C2D : CE_ST_INIT_D2C;
    	CTX->flags = chunk2data ? CE_FL_C2D : CE_FL_D2C;
	if (chunk2data < 0) {
    	    CTX->flags |= CE_FL_PARSE_C2D;
    	    CTX->alloc_buf = nop_alloc_buf;
    	    CTX->buf2data = nop_buf2data;
    	    CTX->free_buf = nop_free_buf;
	} else {
    	    CTX->alloc_buf = alloc_buf;
    	    CTX->buf2data = buf2data;
    	    CTX->free_buf = free_buf;
	}
    }
    return CTX;
}

void ce_free_chunk_encoding_ctx_t(chunk_encoding_ctx_t *CTX)
{
    int rv;

    if (CTX->st_outbuf_token) {
    	rv = (*CTX->free_buf)(CTX->st_outbuf_token);
	if (rv) {
	    DBG("(*CTX->free_buf)(CTX->st_outbuf_token) failed, rv=%d", rv);
	}
    	CTX->st_outbuf_token = 0;
    }
    if (CTX->trailer_data) {
    	free(CTX->trailer_data);
	CTX->trailer_data = 0;
    }
    free(CTX);
}

/*
 * Return:
 *   > 0 => Error
 *   = 0 => Continue, more data required, transformed data noted in
 *          used_out_iovecs 
 *   < 0 => EOF. Trailer_hdrs contains all trailer headers, 
 *          object length is denoted by known header "Content-Length"
 *
 *   Note:
 *     1) Caller must deallocate the trailer_hdrs as follows:
 *            shutdown_http_header(trailer_hdrs); 
 *            free(trailer_hdrs);
 */
int ce_chunk2data(chunk_encoding_ctx_t *CTX, chunk_encoding_input_args_t *IN)
{
    int terminate = 0;
    int retcode = 0;

    if (!CTX || !IN ||
    	!IN->in_iovec || !IN->in_iovec_len ||
        !IN->out_iovec || !IN->out_iovec_len || 
        !IN->out_iovec_token || !IN->out_iovec_token_len || 
        !IN->used_out_iovecs || !IN->out_logical_data_offset || 
	!IN->out_data_length || !IN->trailer_hdrs) {

	DBG("Invalid input: CTX=%p IN=%p in_iovec=%p in_iovec_len=%d "
	    "out_iovec=%p out_iovec_len=%d "
	    "out_iovec_token=%p out_iovec_token_len=%d "
	    "used_out_iovecs=%p "
	    "out_logical_data_offset=%p out_data_length=%p trailer_hdrs=%p",
	    CTX, IN, IN->in_iovec, IN->in_iovec_len, 
	    IN->out_iovec, IN->out_iovec_len, 
	    IN->out_iovec_token, IN->out_iovec_token_len, 
	    IN->used_out_iovecs, 
	    IN->out_logical_data_offset, IN->out_data_length, IN->trailer_hdrs);

	if (IN->used_out_iovecs) {
    	    *IN->used_out_iovecs = 0;
	}
        retcode = 1;
	goto exit;
    }
    *IN->used_out_iovecs = 0;
    *IN->out_logical_data_offset = CTX->st_bytes_to_caller;
    *IN->out_data_length = 0;

    /* Sanity checks */
    assert(CTX->flags & CE_FL_C2D);
    if (IN->in_logical_data_offset != CTX->st_in_data_offset) {
    	DBG("in_logical_data_offset(0x%lx) != st_in_data_offset(0x%lx)",
	    IN->in_logical_data_offset, CTX->st_in_data_offset);
    	retcode = 2;
	goto exit;
    }

    if (IN->in_iovec_len > IN->out_iovec_len) {
    	DBG("CTX=%p, in_iovec_len(%d) > out_iovec_len(%d)",
	    CTX, IN->in_iovec_len, IN->out_iovec_len);
    	retcode = 3;
	goto exit;
    }

    if (IN->out_iovec_len != IN->out_iovec_token_len) {
    	DBG("CTX=%p, out_iovec_len(%d) != out_iovec_token_len(%d)",
	    CTX, IN->out_iovec_len, IN->out_iovec_token_len);
    	retcode = 4;
	goto exit;
    }

    init_per_invocation_data(CTX);
    CTX->st_in_data_offset += IN->in_data_offset; // Allow for skipped bytes

    while (1) {
    	switch(CTX->state) {
    	case CE_ST_INIT_C2D:
    	    terminate = init_chunk2data(CTX, IN, &retcode);
	    break;

    	case CE_ST_PARSE_HDR:
    	    terminate = parse_chunk_header(CTX, IN, &retcode);
	    break;

    	case CE_ST_GET_DATA:
    	    terminate = copy_data(CTX, IN, &retcode);
	    break;

    	case CE_ST_PARSE_TRAILER:
    	    terminate = parse_chunk_trailer(CTX, IN, &retcode);
	    break;

    	case CE_ST_INIT_D2C:
    	case CE_ST_UNDEFINED:
    	default:
    	    retcode = 5;
	    break;
        }

	if (terminate) {
    	    *IN->out_data_length = 
	    	CTX->st_bytes_to_caller - *IN->out_logical_data_offset;
	    break;
	}
    }

exit:

    DBG("CTX=%p retcode=%d out_offset=%ld out_length=%ld in_offset=%ld", 
    	CTX, retcode, *IN->out_logical_data_offset, *IN->out_data_length,
	IN->in_logical_data_offset);
    return retcode;
}

int ce_data2chunk(chunk_encoding_ctx_t *CTX, chunk_encoding_input_args_t *IN)
{
    UNUSED_ARGUMENT(IN);

    assert(CTX->flags & CE_FL_D2C);
    assert(!"ce_data2chunk() not implemented");
    return 0;
}

/*
 * End of chunk_encoding_engine.c
 */
