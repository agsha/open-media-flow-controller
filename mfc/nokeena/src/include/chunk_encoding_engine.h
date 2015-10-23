/*
 * chunk_encoding_engine.h -- To/From Chunk Encoding transformation support.
 */
#ifndef CHUNK_ENCODING_ENGINE_H
#define CHUNK_ENCODING_ENGINE_H

/*
 *******************************************************************************
 *		P U B L I C  D E F I N I T I O N S
 *******************************************************************************
 */
typedef struct chunk_encoding_input_args {
    off_t in_logical_data_offset;
    off_t in_data_offset; // skip input bytes
    const nkn_iovec_t *in_iovec;
    int in_iovec_len;

    /* Output data */
    nkn_iovec_t *out_iovec; 
    int out_iovec_len; // insure (out_iovec_len == out_iovec_token_len)

    /* Output data buffer tokens */
    void **out_iovec_token;
    int out_iovec_token_len;

    /* Output parameters */
    int *used_out_iovecs;
    off_t *out_logical_data_offset;
    off_t *out_data_length;
    mime_header_t **trailer_hdrs;
} chunk_encoding_input_args_t;

/*
 *******************************************************************************
 *		P R I V A T E  D E F I N I T I O N S
 *******************************************************************************
 */
typedef enum ce_engine_state { 
    CE_ST_UNDEFINED,
    CE_ST_INIT_C2D=1, // Chunk to Data 
    CE_ST_INIT_D2C,   // Data to Chunk
    CE_ST_PARSE_HDR,
    CE_ST_GET_DATA,
    CE_ST_PARSE_TRAILER
} ce_engine_state_t;

typedef enum parse_chunk_header_state {
    PC_SCAN_UNDEFINED,
    PC_SCAN_CHUNK_LEN=1,
    PC_SCAN_CHUNK_LEN_NEXT,
    PC_SCAN_CHUNK_EXTENSION,
    PC_SCAN_CR,
    PC_SCAN_LF,
} parse_chunk_header_state_t;

typedef enum copy_data_state {
    CD_SCAN_UNDEFINED,
    CD_SCAN_MOVE_DATA,
    CD_SCAN_CR,
    CD_SCAN_LF
} copy_data_state_t;

typedef enum parse_chunk_trailer_state {
    PCT_SCAN_CR,
    PCT_SCAN_LF,
    PCT_COPY_DATA,
    PCT_COPY_DATA_LF
} parse_chunk_trailer_state_t;

typedef struct chunk_encoding_ctx {
    ce_engine_state_t 	state;
    uint64_t		flags;
    void *(*alloc_buf)(off_t *);
    char *(*buf2data)(void *);
    int (*free_buf)(void *);

    /*
     *********************
     * Global state data * 
     *********************
     */
    off_t st_content_bytes;
    off_t st_bytes_to_caller;
    off_t st_in_data_offset;

    parse_chunk_header_state_t parse_chunk_header_state;
    int64_t chunksize;
    int64_t chunk_bytes_xfered;

    copy_data_state_t copy_data_state;
    parse_chunk_trailer_state_t parse_chunk_trailer_state;
    char *trailer_data;
    int trailer_data_size;
    int trailer_data_used;

    void *st_outbuf_token;
    nkn_iovec_t st_outbuf_base_iovec;
    nkn_iovec_t st_outbuf_cur_iovec;

    /*
     *****************************
     * Per invocation state data * 
     *****************************
     */
    nkn_iovec_t st_in_cur_iovec;
    int st_in_cur_iovec_ix;
    nkn_iovec_t getchar_in_iovec;

} chunk_encoding_ctx_t;

/* chunk_encoding_ctx_t.flags definitions */
#define CE_FL_C2D	0x0000000000000001
#define CE_FL_D2C	0x0000000000000002
#define CE_FL_PARSE_C2D 0x0000000000000004

/*
 *******************************************************************************
 *		P U B L I C  F U N C T I O N S
 *******************************************************************************
 */
chunk_encoding_ctx_t *ce_alloc_chunk_encoding_ctx_t(int chunk2data,
	    				void *(*alloc_buf)(off_t *), 
					char *(*buf2data)(void *),
					int (*free_buf)(void *));

void ce_free_chunk_encoding_ctx_t(chunk_encoding_ctx_t *CTX);

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
int ce_chunk2data(chunk_encoding_ctx_t *CTX, chunk_encoding_input_args_t *IN);

int ce_data2chunk(chunk_encoding_ctx_t *CTX, chunk_encoding_input_args_t *IN);

#endif /* CHUNK_ENCODING_ENGINE_H */

/*
 * End of chunk_encoding_engine.h
 */
