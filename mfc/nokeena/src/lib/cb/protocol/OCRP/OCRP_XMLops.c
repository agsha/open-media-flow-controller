/*
 * OCRP_XMLops.c -- OCRP specific XML ops
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <limits.h>
#include <alloca.h>
#include <errno.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlreader.h>
#include "cb/xml/generic_XMLops.h"
#include "cb/xml/OCRP_XMLops.h"

/* 
 * Macro definitions
 */
#define UNUSED_ARGUMENT(x) (void)x

/*
 * Declarations
 */
static int int_OCRP_XML2Struct(XMLDocCTX_t *ctx, void *protoStruct, 
			   char *errbuf, int sizeof_errbuf);

/*
 * Define the global OCRP_XMLops
 */
const XMLops_t OCRP_XMLops = {newXMLDocCTX, resetXMLDocCTX, deleteXMLDocCTX, 
			      validateXML, int_OCRP_XML2Struct};

/*
 * Global functions
 */
int OCRP_parse_H(const uint8_t *buf, uint64_t *pseqno, uint64_t *pvers)
{
    /* 
     * H record format: <seqno>:<OCRP version>
     */
    uint64_t seqno;
    uint64_t vers;
    char *endptr;

    errno = 0;
    seqno = strtol((char *)buf, &endptr, 10);
    if ((errno == EINVAL) || (errno == ERANGE)) { 
    	return 1;
    }

    if (*endptr != ':') {
    	return 2;
    }
    errno = 0;
    vers = strtol((char *)(endptr+1), &endptr, 10);
    if ((errno == EINVAL) || (errno == ERANGE)) { 
    	return 3;
    }

    if (endptr != (char *)(buf+strlen((char *)buf))) {
    	return 4;
    }

    *pseqno = seqno;
    *pvers = vers;
    return 0;
}

int OCRP_parse_K(const uint8_t *buf, const char **pkey)
{
    /*
     * K record format: <key>
     */
    if (!*buf) {
    	return 1;
    }
    *pkey = (const char *)buf;
    return 0;
}

int OCRP_parse_A(const uint8_t *buf, const char **pattr)
{
    /*
     * A record format: <attr>
     */
    if (!*buf) {
    	return 1;
    }
    *pattr = (const char *)buf;
    return 0;
}

int OCRP_parse_L(const uint8_t *buf, 
		 int *pentries,
		 const char *FQDN_start[L_ELEMENT_MAX_TUPLES],
		 const char *FQDN_end[L_ELEMENT_MAX_TUPLES],
		 long (*port)[L_ELEMENT_MAX_TUPLES],
		 long (*weight)[L_ELEMENT_MAX_TUPLES])
{
    /*
     * L record format: <FQDN>:<Port>:<Weight>, ... ,<FQDN>:<Port>:<Weight> 
     */
    int rv = 0;
    int entries;
    const char *p_start;
    const char *p_end;
    const char *p;
    char *endptr;

    entries = 0, 
    p_start = (const char *)buf;
    p_end = strchr(p_start, ','); 
    if (!p_end) {
    	p_end = p_start + strlen(p_start);
    }
    
    while (p_end) {
    	if (entries >= L_ELEMENT_MAX_TUPLES) {
	    rv = 1; // Max entries exceeded
	    break;
	}

	FQDN_start[entries] = p_start;
	p = strchr(p_start, ':');
	if (!p || (p >= p_end)) {
	    rv = 2;
	    break;
	}
	FQDN_end[entries] = p-1;

	errno = 0;
	(*port)[entries] = strtol((char *)(p+1), &endptr, 10);
	if ((errno == EINVAL) || (errno == ERANGE)) {
	    rv = 3;
	    break;
	}

	if ((*endptr != ':') || (endptr >= p_end)) {
	    rv = 4;
	    break;
	}
	p = endptr;

	errno = 0;
	(*weight)[entries] = strtol((char *)(p+1), &endptr, 10);
	if ((errno == EINVAL) || (errno == ERANGE)) {
	    rv = 5;
	    break;
	}

	if (endptr != p_end) {
	    rv = 6;
	    break;
	}

        entries++;
	if (*p_end) {
	    p_start = p_end+1;
	    p_end = strchr(p_start, ','); 
	    if (!p_end) {
    		p_end = p_start + strlen(p_start);
	    }
	} else {
	    break;
	}
    }

    *pentries = entries;
    return rv;
}

/*
 * Private functions
 */
#define PRTERRBUF(fmt, ...) { \
    int _ret; \
    _ret = snprintf(errbuf, sizeof_errbuf, \
		    "[%s:%d] "fmt"\n", \
		    __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    if (_ret >= sizeof_errbuf) { \
    	errbuf[sizeof_errbuf-1] = '\0'; \
    } \
}

static int 
process_OCRP_element(XMLDocCTX_t *ctx, const char *element,
			 int (*validate)(XMLDocCTX_t *ctx,
			 		 const unsigned char *value, 
			  		 char *errbuf, int sizeof_errbuf),
			 const unsigned char **ppval, 
			 char *errbuf, int sizeof_errbuf)
{
    int rv;
    int ret = 0;
    xmlReaderTypes type;
    const unsigned char *value;
    xmlTextReaderPtr rdptr = ctx->XMLrdr;

    rv = xmlTextReaderRead(rdptr);
    type = xmlTextReaderNodeType(rdptr);

    if ((rv == 1) && (type == XML_READER_TYPE_TEXT)) {
	value = xmlTextReaderConstValue(rdptr);
	rv = (*validate)(ctx, value, errbuf, sizeof_errbuf);
	if (!rv) {
	    *ppval = value;
	} else {
	    ret = 1;
	}
    } else {
	PRTERRBUF("Record #%d, NULL element [<%s>] value",
		  ctx->current_recnum, element);
	ret = 2;
    }
    return ret;
}

static int 
validate_ELEMENT_H(XMLDocCTX_t *ctx, const unsigned char *value, 
		   char *errbuf, int sizeof_errbuf)
{
    int rv;
    uint64_t seqno;
    uint64_t vers;

    rv = OCRP_parse_H(value, &seqno, &vers);
    if (rv) {
    	PRTERRBUF("Record #%d, invalid H record [%s] rv=%d", 
		  ctx->current_recnum, value, rv);
	return 1;
    }
    if (vers != OCRP_VERS) {
    	PRTERRBUF("Record #%d, invalid H record [%s] OCRP "
		  "version (vers=%ld expected vers=%d) rv=%d", 
		  ctx->current_recnum, value, vers, OCRP_VERS, rv);
	return 2;
    }
    return 0;
}

static int 
validate_ELEMENT_K(XMLDocCTX_t *ctx, const unsigned char *value, 
		   char *errbuf, int sizeof_errbuf)
{
    int rv;
    const char *key;
    const char *p;

    rv = OCRP_parse_K(value, &key);
    if (rv) {
    	PRTERRBUF("Record #%d, invalid K record [%s] rv=%d", 
		  ctx->current_recnum, value, rv);
	return 1;
    }

    for (p = key; *p; p++) {
    	if (!isgraph(*p)) { // non-printable or space
	    PRTERRBUF("Record #%d, invalid character (0x%hx) in K record [%s]", 
		      ctx->current_recnum, *p, key);
	    return 2;
	}
    }
    return 0;
}

static int 
validate_ELEMENT_A(XMLDocCTX_t *ctx, const unsigned char *value, 
		   char *errbuf, int sizeof_errbuf)
{
    int rv;
    const char *attr;

    rv = OCRP_parse_A(value, &attr);
    if (rv) {
    	PRTERRBUF("Record #%d, invalid A record [%s] rv=%d", 
		  ctx->current_recnum, value, rv);
	return 1;
    }

    if (strcasecmp(OCRP_ATTR_PIN, attr)) {
    	PRTERRBUF("Record #%d, invalid A record attribute [%s]", 
		  ctx->current_recnum, attr);
	return 2;
    }
    return 0;
}

static int 
validate_ELEMENT_L(XMLDocCTX_t *ctx, const unsigned char *value, 
		   char *errbuf, int sizeof_errbuf)
{
    int rv;
    int entries;
    int ix;
    int slen;
    const char *p;


    const char *FQDN_start[L_ELEMENT_MAX_TUPLES];
    const char *FQDN_end[L_ELEMENT_MAX_TUPLES];
    long port[L_ELEMENT_MAX_TUPLES];
    long weight[L_ELEMENT_MAX_TUPLES];

    rv = OCRP_parse_L(value, &entries, FQDN_start, FQDN_end, &port, &weight);
    if (rv) {
    	PRTERRBUF("Record #%d, invalid L record [%s] rv=%d", 
		  ctx->current_recnum, value, rv);
    	return 1;
    }

    if (entries) {
	for (ix = 0; ix < entries; ix++) {
	    slen = (FQDN_end[ix] - FQDN_start[ix]) + 1;
	    if (slen > L_ELEMENT_FQDN_MAXLEN) {
    		PRTERRBUF("Record #%d, invalid L record FQDN strlen (%d > %d)",
			  ctx->current_recnum, slen, L_ELEMENT_FQDN_MAXLEN);
	    	return 2;
	    }
	    for (p = FQDN_start[ix]; p <= FQDN_end[ix]; p++) {
	        if (isalnum(*p) || (*p == '.') || (*p == '-') || (*p == '_')) {
		    continue;
		} else {
		    int FQDN_strlen = (FQDN_end[ix] - FQDN_start[ix]) + 1;
		    char *pFQDN = alloca(FQDN_strlen + 1);
		    memcpy(pFQDN, FQDN_start[ix], FQDN_strlen);
		    pFQDN[FQDN_strlen] = '\0';

		    PRTERRBUF("Record #%d, invalid L record "
			      "FQDN [%s] char (0x%hx)",
			      ctx->current_recnum, pFQDN, *p);
		    return 3;
		}
	    }

	    if (!port[ix]  || (port[ix] > USHRT_MAX)) { // invalid TCP port
    		PRTERRBUF("Record #%d, invalid L record TCP port value (%ld)",
			  ctx->current_recnum, port[ix]);
	    	return 4;
	    }
	    if ((weight[ix] < L_ELEMENT_MIN_WEIGHT)  || 
	        (weight[ix] > L_ELEMENT_MAX_WEIGHT)) {
    		PRTERRBUF("Record #%d, invalid L record weight "
			  "value (%ld), expected range %d-%d",
			  ctx->current_recnum, weight[ix], 
			  L_ELEMENT_MIN_WEIGHT, L_ELEMENT_MAX_WEIGHT);
	    	return 5;
	    }
	}
    }
    return 0;
}

static int 
process_OCRP_record(XMLDocCTX_t *ctx, const char *root_name, 
		    OCRP_record_t *pocrp, char *errbuf, int sizeof_errbuf)
{
    const char *name;
    const unsigned char **pval;
    xmlReaderTypes type;
    int rv;
    int ret = 0;
    xmlTextReaderPtr rdptr = ctx->XMLrdr;

    while (xmlTextReaderRead(rdptr) == 1) {
	name = (const char *)xmlTextReaderConstName(rdptr);
    	type = xmlTextReaderNodeType(rdptr);

	if (type == XML_READER_TYPE_ELEMENT) {
	    if (strcmp(name, ELEMENT_H) == 0) {
	    	if (pocrp->type == OCRP_Entry) {
		    pval = &pocrp->u.Entry.H;
		} else {
		    pval = &pocrp->u.DeleteEntry.H;
		}
	    	rv = process_OCRP_element(ctx, ELEMENT_H, 
					  validate_ELEMENT_H, pval, 
					  errbuf, sizeof_errbuf);
					  
		if (rv) {
		    ret = 10 + rv;
		    break;
		}
	    } else if (strcmp(name, ELEMENT_K) == 0) {
	    	if (pocrp->type == OCRP_Entry) {
		    pval = &pocrp->u.Entry.K;
		} else {
		    pval = &pocrp->u.DeleteEntry.K;
		}
	    	rv = process_OCRP_element(ctx, ELEMENT_K, 
					  validate_ELEMENT_K, pval,
					  errbuf, sizeof_errbuf);
		if (rv) {
		    ret = 20 + rv;
		    break;
		}
	    } else if (strcmp(name, ELEMENT_A) == 0) {
		if (pocrp->u.Entry.num_A < MAX_A_ELEMENTS) {
		    pval = &pocrp->u.Entry.A[pocrp->u.Entry.num_A++];
		} else {
		    PRTERRBUF("Record #%d, Max ELEMENT A exceeded (%d %d)",
		    	      ctx->current_recnum, pocrp->u.Entry.num_A,
			      MAX_A_ELEMENTS);
		    ret = 2;
		    break;
		}
	    	rv = process_OCRP_element(ctx, ELEMENT_A,
					  validate_ELEMENT_A, pval,
					  errbuf, sizeof_errbuf);
		if (rv) {
		    ret = 30 + rv;
		    break;
		}
	    } else if (strcmp(name, ELEMENT_L) == 0) {
		pval = &pocrp->u.Entry.L;
	    	rv = process_OCRP_element(ctx, ELEMENT_L, 
					  validate_ELEMENT_L, pval,
					  errbuf, sizeof_errbuf);
		if (rv) {
		    ret = 40 + rv;
		    break;
		}
	    }
	} else if (type == XML_READER_TYPE_END_ELEMENT) {
	    if (strcmp(name, root_name) == 0) {
	    	xmlTextReaderRead(rdptr); // Point to #text 
	    	break;
	    }
	}
    }
    return ret;
}

static int 
int_OCRP_XML2Struct(XMLDocCTX_t *ctx, void *protoStruct, 
		    char *errbuf, int sizeof_errbuf)
{
    const char *name;
    xmlReaderTypes type;
    int rv;
    int ret = -1; // EOF
    xmlTextReaderPtr rdptr = ctx->XMLrdr;
    OCRP_record_t *pocrp = (OCRP_record_t *)protoStruct;

    memset(pocrp, 0, sizeof(*pocrp));

    while (xmlTextReaderRead(rdptr) == 1) {
	name = (const char *)xmlTextReaderConstName(rdptr);
    	type = xmlTextReaderNodeType(rdptr);

	if (type == XML_READER_TYPE_ELEMENT) {
	    if ((strcmp(name, ELEMENT_ENTRY) == 0)) {
	    	pocrp->type = OCRP_Entry;
	    	ctx->current_recnum++;
		rv = process_OCRP_record(ctx, ELEMENT_ENTRY, pocrp,
					 errbuf, sizeof_errbuf);
		if (!rv) {
		    ret = 0;
		} else {
		    ret = rv ? 1 : 0;
		}
		break;
	    } else if ((strcmp(name, ELEMENT_DELETE_ENTRY) == 0)) {
	    	pocrp->type = OCRP_DeleteEntry;
	    	ctx->current_recnum++;
		rv = process_OCRP_record(ctx, ELEMENT_DELETE_ENTRY, pocrp, 
					 errbuf, sizeof_errbuf);
		if (!rv) {
		    ret = 0;
		} else {
		    ret = rv ? 2 : 0;
		}
		break;
	    }
	}
    }
    return ret;
}

/*
 * End of OCRP_XMLops.c
 */

