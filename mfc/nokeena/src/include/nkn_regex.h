/*
 *******************************************************************************
 * nkn_regex.h -- Regex interface
 *******************************************************************************
 */

#ifndef _NKN_REGEX_H
#define _NKN_REGEX_H

struct nkn_regex_ctx {
    int opaque;
};
typedef struct nkn_regex_ctx nkn_regex_ctx_t;

typedef struct nkn_regmatch /* Alias for regmatch_t in /usr/include/regex.h */
{
    int rm_so;
    int rm_eo;
} nkn_regmatch_t;

/*
 * Allocate/Free nkn_regex_ctx_t
 */
nkn_regex_ctx_t *nkn_regex_ctx_alloc(void);
void nkn_regex_ctx_free(nkn_regex_ctx_t *ctx);


// array of regex allocation
nkn_regex_ctx_t *nkn_regex_ctx_arr_alloc(uint32_t cnt);

// regex_t array manipulation
nkn_regex_ctx_t *nkn_regaindex(nkn_regex_ctx_t *ctx,
			       uint32_t num);

/*
 * nkn_regcomp() -- Compile regex
 *   Input:
 *     ctx -- Allocated via nkn_regex_ctx_alloc()
 *     regex -- Null terminated regex expression string
 *     errorbuf -- Optional
 *     errorbuf_size -- sizeof(errorbuf) if used
 *
 *   Returns: 0=Success
 */
int nkn_regcomp(nkn_regex_ctx_t *ctx, const char *regex, 
		char *errorbuf, int errorbuf_size);

/*
 * nkn_regcomp_match() -- Compile regex with the match option
 *   Input:
 *     ctx -- Allocated via nkn_regex_ctx_alloc()
 *     regex -- Null terminated regex expression string
 *     errorbuf -- Optional
 *     errorbuf_size -- sizeof(errorbuf) if used
 *
 *   Returns: 0=Success
 */
int nkn_regcomp_match(nkn_regex_ctx_t *ctx, const char *regex, 
		      char *errorbuf, int errorbuf_size);

/*
 * nkn_valid_regex() -- Check regex syntax
 *   Input:
 *     regex -- Null terminated regex expression string
 *     errorbuf -- Optional
 *     errorbuf_size -- sizeof(errorbuf) if used
 *
 *   Returns: 1=Valid syntax
 */
int nkn_valid_regex(const char *regex, char *errorbuf, int errorbuf_size);

/*
 * nkn_regexec() -- Execute regex on given string
 *   Input:
 *     ctx -- Initialized by nkn_regcomp()
 *     string -- Null terminated data string to apply regex on
 *     errorbuf -- Optional
 *     errorbuf_size -- sizeof(errorbuf) if used
 *
 *   Returns: 0=Success
 */
int nkn_regexec(const nkn_regex_ctx_t *ctx, char *string,
		char *errorbuf, int errorbuf_size);

/*
 * nkn_regexec_match() -- Execute regex on given string
 *   Input:
 *     ctx -- Initialized by nkn_regcomp()
 *     string -- Null terminated data string to apply regex on
 *     match_size -- Size of match[] array
 *     match[] -- Match data (match_data[].rm_so != -1) => valid data
 *     errorbuf -- Optional
 *     errorbuf_size -- sizeof(errorbuf) if used
 *
 *   Returns: 0=Success
 */
int nkn_regexec_match(const nkn_regex_ctx_t *ctx, char *string,
		      size_t match_size, nkn_regmatch_t match[],
		      char *errorbuf, int errorbuf_size);

#endif  /* _NKN_REGEX_H */

/*
 * End of nkn_regex.h
 */
