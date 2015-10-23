/*
 * XMLops.c -- Generic XML ops 
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlreader.h>

#include "cb/xml/XMLops.h"
#include "cb/xml/generic_XMLops.h"

#define UNUSED_ARGUMENT(x) (void)x

XMLDocCTX_t *newXMLDocCTX(void)
{
    XMLDocCTX_t *ctx = calloc(1, sizeof(XMLDocCTX_t));
    return ctx;
}

int resetXMLDocCTX(XMLDocCTX_t *ctx)
{
    if (ctx->XMLrdr) {
	xmlFreeTextReader(ctx->XMLrdr);
	ctx->XMLrdr = 0;
    }
    if (ctx->XMLdoc) {
	xmlFreeDoc((xmlDocPtr)ctx->XMLdoc);
	ctx->XMLdoc = 0;
    }
    return 0;
}

void deleteXMLDocCTX(XMLDocCTX_t *ctx)
{
    if (ctx) {
    	resetXMLDocCTX(ctx);
	free(ctx);
    }
}

static void
libxmlErrorFunc(void *ctx, const char *msg, ...)
{
    UNUSED_ARGUMENT(ctx);
    UNUSED_ARGUMENT(msg);
}

static int
nop_fprintf(FILE *stream, const char *format, ...)
{
    UNUSED_ARGUMENT(stream);
    UNUSED_ARGUMENT(format);
    return 0;
}

int validateXML(const char *input_file, const char *DTDdata, 
	        XMLDocCTX_t *ctx, char *errbuf, int sizeof_errbuf)
{
#define PRTERRBUF(fmt, ...) { \
    int _ret; \
    xmlErrorPtr _xmlerrp; \
    _xmlerrp = xmlGetLastError(); \
    _ret = snprintf(errbuf, sizeof_errbuf, \
		    "[%s:%d] XMLerr:%d XMLmsg:[%s] "fmt"\n", \
		    __FUNCTION__, __LINE__, _xmlerrp->code, _xmlerrp->message, \
		    ##__VA_ARGS__); \
    if (ret >= sizeof_errbuf) { \
    	errbuf[sizeof_errbuf-1] = '\0'; \
    } \
}

    int ret = 0;
    xmlDocPtr doc = 0;
    xmlDtdPtr dtd = 0;
    xmlValidCtxtPtr cvp = 0;
    xmlParserInputBufferPtr bp;
    xmlGenericErrorFunc handler;

    handler = (xmlGenericErrorFunc)libxmlErrorFunc;
    initGenericErrorDefaultFunc(&handler);

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    doc = xmlParseFile(input_file);
    if (!doc) {
    	PRTERRBUF("xmlParseFile(\"%s\") failed\n", input_file);
	ret = 1;
	break;
    }

    bp = xmlParserInputBufferCreateMem(DTDdata, strlen(DTDdata), 
    				       XML_CHAR_ENCODING_NONE);
    dtd = xmlIOParseDTD(NULL, bp, XML_CHAR_ENCODING_NONE);
    if (!dtd) {
    	PRTERRBUF("xmlIOParseDTD() failed\n");
	ret = 2;
	break;
    }

    cvp = xmlNewValidCtxt();
    if (!cvp) {
    	PRTERRBUF("xmlNewValidCtxt() failed\n");
	ret = 3;
	break;
    }
    cvp->userData = (void*)0;
    cvp->error = (xmlValidityErrorFunc)nop_fprintf;
    cvp->warning = (xmlValidityWarningFunc)nop_fprintf; 

    if (!xmlValidateDtd(cvp, doc, dtd)) {
    	PRTERRBUF("xmlValidateDtd() failed\n");
	ret = 4;
	break;
    }

    if (ctx) {
    	ctx->XMLrdr = xmlReaderWalker(doc);
	if (!ctx->XMLrdr) {
	    PRTERRBUF("xmlReaderWalker() failed\n");
	    ret = 5;
	    break;
	}
    	ctx->XMLdoc = (void *)doc;
	doc = 0;
    }

    ret = 0;
    break;
    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    if (doc) {
    	xmlFreeDoc(doc);
    }
    if (dtd) {
    	xmlFreeDtd(dtd);
    }
    if (cvp) {
    	xmlFreeValidCtxt(cvp);
    }
    return ret;
#undef PRTERRBUF
}

/*
 * End of XMLops.c
 */
