/*
 * generic_XMLops.h -- Generic XML ops
 */
#ifndef _GENERIC_XMLOPS_H_
#define _GENERIC_XMLOPS_H_
#include "cb/xml/XMLops.h"

extern XMLDocCTX_t *newXMLDocCTX(void);
extern int resetXMLDocCTX(XMLDocCTX_t *ctx);
extern void deleteXMLDocCTX(XMLDocCTX_t *ctx);
extern int validateXML(const char *input_file, const char *DTDdata, 
                       XMLDocCTX_t *ctx, char *errbuf, int sizeof_errbuf);

#endif /* _GENERIC_XMLOPS_H_ */
/*
 * End of generic_XMLops.h
 */
