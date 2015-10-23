/*
 * OCRP_XMLops.h -- OCRP specific XML operations
 */
#ifndef _OCRP_XMLOPS_H_
#define _OCRP_XMLOPS_H_

#include <stdint.h>
#include "cb/xml/XMLops.h"

extern const XMLops_t OCRP_XMLops;
extern const char *OCRP_DTD;
extern const char *OCRPupdate_DTD;

#define OCRP_VERS 10
#define MAX_A_ELEMENTS 8

#define L_ELEMENT_MAX_TUPLES 8
#define L_ELEMENT_FQDN_MAXLEN 79
#define L_ELEMENT_MIN_WEIGHT 1
#define L_ELEMENT_MAX_WEIGHT 255

#define ELEMENT_ENTRY "Entry"
#define ELEMENT_DELETE_ENTRY "DeleteEntry"
#define ELEMENT_H "H"
#define ELEMENT_K "K"
#define ELEMENT_A "A"
#define ELEMENT_L "L"

#define OCRP_ATTR_PIN "pin"

typedef enum {
    OCRP_Entry=1,
    OCRP_DeleteEntry,
} OCRP_record_type;

typedef struct OCRP_record {
  OCRP_record_type type;
  union {
    struct s_Entry {
    	const unsigned char *H;
    	const unsigned char *K;
	int num_A;
    	const unsigned char *A[MAX_A_ELEMENTS];
    	const unsigned char *L;
    } Entry;

    struct s_DeleteEntry {
    	const unsigned char *H;
    	const unsigned char *K;
    } DeleteEntry;
  } u;
} OCRP_record_t;

/*
 * XML op function wrappers
 */
#define OCRP_newXMLDocCTX() (*OCRP_XMLops.newXMLDocCTX)()

#define OCRP_resetXMLDocCTX(_ctx) (*OCRP_XMLops.resetXMLDocCTX)((_ctx))

#define OCRP_deleteXMLDocCTX(_ctx) (*OCRP_XMLops.deleteXMLDocCTX)((_ctx))

#define OCRP_validateXML(_input_file, _DTDdata, _ctx, _errbuf, _sizeof_errbuf) \
	(*OCRP_XMLops.validateXML)((_input_file), (_DTDdata), \
				   (_ctx), (_errbuf), (_sizeof_errbuf))

#define OCRP_XML2Struct(_ctx, _OCRP_record_t_ptr, _errbuf, _sizeof_errbuf) \
	(*OCRP_XMLops.XML2Struct)((_ctx), (void *)(_OCRP_record_t_ptr), \
				  (_errbuf), (_sizeof_errbuf))

/*
 *******************************************************************************
 * OCRP record parse functions
 *******************************************************************************
 */

 /*
  * Parse A record
  *     buf -- Input buffer (null terminated)
  *     pattr -- Output, ptr to attribute (null terminated)
  *
  *   Return:
  *     ==0, Success
  *     !=0, Error
  */
int OCRP_parse_A(const uint8_t *buf, const char **pattr);

 /*
  * Parse H record
  *     buf -- Input buffer (null terminated)
  *     pseqno -- Output, sequence number
  *     pvers -- Output, OCRP version number
  *
  *   Return:
  *     ==0, Success
  *     !=0, Error
  */
int OCRP_parse_H(const uint8_t *buf, uint64_t *pseqno, uint64_t *pvers);

 /*
  * Parse K record
  *     buf -- Input buffer (null terminated)
  *     pkey -- Output, ptr to key (null terminated)
  *
  *   Return:
  *     ==0, Success
  *     !=0, Error
  */
int OCRP_parse_K(const uint8_t *buf, const char **pkey);


 /*
  * Parse L record
  *     buf -- Input buffer (null terminated)
  *	pentries -- Output, valid entries
  *	FQDN_start[] -- Output, FQDN start ptr
  *	FQDN_end[] -- Output, FQDN end ptr
  *	port[] -- Output, TCP port
  *	weight[] -- Output, weight
  *
  *   Return:
  *     ==0, Success
  *     !=0, Error
  */
int OCRP_parse_L(const uint8_t *buf, int *pentries,
                 const char *FQDN_start[L_ELEMENT_MAX_TUPLES],
                 const char *FQDN_end[L_ELEMENT_MAX_TUPLES],
                 long (*port)[L_ELEMENT_MAX_TUPLES],
                 long (*weight)[L_ELEMENT_MAX_TUPLES]);

#endif /* _OCRP_XMLOPS_H_ */

/*
 * End of OCRP_XMLops.h
 */
