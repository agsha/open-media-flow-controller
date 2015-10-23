/*
 *******************************************************************************
 * nkn_regex.c -- Regex interface
 *******************************************************************************
 */
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include "nkn_defs.h"
#include "nkn_regex.h"

nkn_regex_ctx_t *nkn_regex_ctx_alloc()
{
    regex_t *pregex;
    pregex = nkn_malloc_type(sizeof(regex_t), mod_reg_regex_t);
    return (nkn_regex_ctx_t *)pregex;
}

void nkn_regex_ctx_free(nkn_regex_ctx_t *ctx)
{
    free(ctx);
}

// functions to alloc/free array of regex ctx

nkn_regex_ctx_t *nkn_regex_ctx_arr_alloc(uint32_t cnt)
{
    regex_t *pregex;
    pregex = nkn_calloc_type(sizeof(regex_t),  cnt, mod_reg_regex_t);
    return (nkn_regex_ctx_t *)pregex;
}

nkn_regex_ctx_t *nkn_regaindex(nkn_regex_ctx_t *ctx, uint32_t num)
{
    regex_t *pregex = (regex_t *)ctx;
    return (nkn_regex_ctx_t *) (pregex + num);
    
}


int nkn_regcomp(nkn_regex_ctx_t *ctx, const char *regex, 
		char *errorbuf, int errorbuf_size)
{
    int rv;
    regex_t *pregex = (regex_t *)ctx;

    rv = regcomp(pregex, regex, REG_EXTENDED|REG_NOSUB|REG_NEWLINE);
    if (rv) {
        if (errorbuf && errorbuf_size) {
	    regerror(rv, pregex, errorbuf, errorbuf_size);
	}
    }
    return rv;
}

int nkn_regcomp_match(nkn_regex_ctx_t *ctx, const char *regex, 
		      char *errorbuf, int errorbuf_size)
{
    int rv;
    regex_t *pregex = (regex_t *)ctx;

    rv = regcomp(pregex, regex, REG_EXTENDED|REG_NEWLINE);
    if (rv) {
        if (errorbuf && errorbuf_size) {
	    regerror(rv, pregex, errorbuf, errorbuf_size);
	}
    }
    return rv;
}

int nkn_valid_regex(const char *regex, char *errorbuf, int errorbuf_size)
{
    int rv;
    nkn_regex_ctx_t *ctx;

    if (!regex || (regex && !(*regex))) {
    	return 0;
    }
    ctx = nkn_regex_ctx_alloc();
    rv = nkn_regcomp(ctx, regex, errorbuf, errorbuf_size);
    nkn_regex_ctx_free(ctx);

    return (rv == 0) ? 1 : 0;
}

int nkn_regexec(const nkn_regex_ctx_t *ctx, char *string,
		char *errorbuf, int errorbuf_size)
{
    int rv;

    rv = regexec((regex_t *)ctx, string, 0, 0, 0);
    if (rv) {
        if (errorbuf && errorbuf_size) {
	    regerror(rv, 0, errorbuf, errorbuf_size);
	}
    }
    return rv;
}

int nkn_regexec_match(const nkn_regex_ctx_t *ctx, char *string,
		      size_t match_size, nkn_regmatch_t match[],
		      char *errorbuf, int errorbuf_size)
{
    int rv;

    rv = regexec((regex_t *)ctx, string, match_size, (regmatch_t *)match, 0);
    if (rv) {
        if (errorbuf && errorbuf_size) {
	    regerror(rv, 0, errorbuf, errorbuf_size);
	}
    }
    return rv;
}

/*
 * End of nkn_regex.c
 */
