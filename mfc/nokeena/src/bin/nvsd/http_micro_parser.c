#include <stdlib.h>
#include <string.h>
#include <nkn_util.h>

/*
 * Micro Parser Call back
 * Defines Action code call back type.
 * Should be called if additional parsing/validation has to be done on a token
 */
typedef http_parse_status_t (*http_micro_parser_cb_t)(http_cb_t * phttp,
						       http_header_id_t id,
						       const char
						       *token_buf,
						       int token_len,
						       const char **end);

/* Maximum validation levels
 */
#define MAX_LEVEL (0x2)
static http_micro_parser_cb_t validator_table[MAX_LEVEL][MIME_HDR_MAX_DEFS];

#define SET_MICRO_PARSER_0(token, cb_function) \
{\
    assert ((token>=0) && (token < MIME_HDR_MAX_DEFS));\
    assert(validator_table[0][token] == NULL);\
    validator_table[0][token] = cb_function;\
}

#define MICRO_PARSER_0(token) (validator_table[0][token])

#define RESET_MICRO_PARSER(level) \
{\
    assert(level >= 0 && level < MAX_LEVEL);\
    int token;\
    for (token = 0; token < MIME_HDR_MAX_DEFS; token++) {\
	validator_table[level][token] = NULL;\
    }\
}

/**
 * @brief Micro parser code to parse Range header
 * FIXME - Validation of 'bytes' token not present
 *
 * @param phttp - Http control block
 * @param token - Header token enum
 * @param buf - Buffer
 * @param buf_len - Buffer Length
 * @param end - End pointer after parsing
 *
 * @return Parser Status
 */
static http_parse_status_t
http_mp_range(http_cb_t * phttp, http_header_id_t token,
	      const char *buf, int buf_len, const char **end)
{

    UNUSED_ARGUMENT(token);
    UNUSED_ARGUMENT(buf_len);
    // "Rang" Range: Bytes 21010-47021
    const char *p, *p_st, *p_end;
    const char *p2;
    p = nkn_skip_colon(buf);

    // skip "Bytes="
    p_st = p;
    p += 6;
    phttp->brstart = atol(p);
    p_end = p;
    p2 = strchr(p, '-');
    if (p2) {
	p_end = p2 + 1;
	phttp->brstop = atol(p_end);
	/* three cases here: 
	 * Range: bytes=1-  ==> valid return 206 whole file - first byte
	 * Range: bytes=1-0 ==> invalid return 416
	 * Range: bytes=0-0 ==> valid return 206 with 1 byte.
	 */
	if ((*p_end == '0') && (phttp->brstart > phttp->brstop)) {
	    phttp->brstop = -1;
	    // hack set to -1. Then http_check_request() will fail.
	}
    } else {
	phttp->brstop = 0;
    }
    SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
    //printf("byte range: %d -- %d\n", phttp->brstart, phttp->brstop);
    p_end = nkn_find_digit_end(p_end);
    p_end++;
    if (end) {
	*end = p_end;
    }
    return (HPS_OK);
}

/** 
 * @brief Micro parser for "Connection:" header
 * 
 * @param phttp - Http control block
 * @param token - Header token enum
 * @param buf - Buffer
 * @param buf_len - Buffer Length
 * @param end - End pointer after parsing
 * 
 * @return Parser Status
 */
static http_parse_status_t
http_mp_connection(http_cb_t * phttp, http_header_id_t token,
		   const char *buf, int buf_len, const char **end)
{
    // Connection: Keep-Alive
    // Connection: Close
    UNUSED_ARGUMENT(token);
    UNUSED_ARGUMENT(buf_len);
    const char *p, *p_end;
    p = nkn_skip_colon(buf);

    if (nkn_strcmp_incase(p, "close", 5) == 0) {
	SET_HTTP_FLAG(phttp, HRF_CONNECTION_CLOSE);
    } else if (nkn_strcmp_incase(p, "keep-alive", 10) == 0) {
	SET_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
    }

    p_end = nkn_find_hdrvalue_end(p);
    p_end++;
    if (end) {
	*end = p_end;
    }
    return (HPS_OK);
}

/** 
 * @brief Micro parser for "Content-Length:" header
 * 
 * @param phttp - Http control block
 * @param token - Header token enum
 * @param buf - Buffer
 * @param buf_len - Buffer Length
 * @param end - End pointer after parsing
 * 
 * @return Parser Status
 */
static http_parse_status_t
http_mp_content_length(http_cb_t * phttp, http_header_id_t token,
		       const char *buf, int buf_len, const char **end)
{
    UNUSED_ARGUMENT(token);
    UNUSED_ARGUMENT(buf_len);
    const char *p, *p_end;
    p = nkn_skip_colon(buf);
    SET_HTTP_FLAG(phttp, HRF_CONTENT_LENGTH);
    phttp->content_length = atol(p);
    //printf("byte range: %d -- %d\n", con->req.brstart, con->req.brstop);
    p_end = nkn_find_digit_end(p);
    p_end++;
    if (end) {
	*end = p_end;
    }
    return (HPS_OK);
}

void http_micro_parser_init(void);
void http_micro_parser_init(void)
{
    SET_MICRO_PARSER_0(MIME_HDR_RANGE, http_mp_range);
    SET_MICRO_PARSER_0(MIME_HDR_CONNECTION, http_mp_connection);
    SET_MICRO_PARSER_0(MIME_HDR_CONTENT_LENGTH, http_mp_content_length);
}

void http_micro_parser_exit(void);
void http_micro_parser_exit(void)
{
    RESET_MICRO_PARSER(0);
}
