/*
 * OCRP_cgi_errorcodes.h -- CGI HTTP error return codes
 *   Embedded error codes in HTTP error response.
 *   Example:
 *
 *   HTTP/1.1 500 Internal Server Error, [Suberror:<integer>] <error text>
 */
#ifndef _OCRP_CGI_ERRORCODES_H
#define _OCRP_CGI_ERRORCODES_H

#define ER_PTFILE_OPEN_FAILED 100	// POST temp file open failed
#define ER_PTFILE_FSTAT_FAILED 101	// POST temp file stat failed
#define ER_PTFILE_RENAME_FAILED	102	// POST temp file rename failed
#define ER_PTFILE_DBG_RENAME_FAILED 103	// POST DBG temp file rename failed
#define ER_PTFILE_FOPEN_FAILED 104	// POST temp file fopen failed
#define ER_PTFILE_FWRITE_FAILED	105	// POST temp file fwrite failed
#define ER_PTFILE_SHORT_WRITE 106	// POST temp file short write

#define ER_SFILE_OPEN_FAILED 200	// Seqno file open fail
#define ER_SFILE_FLOCK_FAILED 201	// Seqno file flock failed
#define ER_SFILE_READ_FAILED 202	// Seqno file read failed
#define ER_SFILE_WRITE_FAILED 203	// Seqno file write failed


#define ER_STORE_QUOTA_EXCEEDED	300	// Input directory quota exceeded
#define ER_CGIIN_BADEOF	301		// Bad EOF on cgiIn

#define ER_GETFILE_OPEN_FAILED 400	// GET content file open failed
#define ER_GETFILE_FLOCK_FAILED	401	// GET conent file flock failed
#define ER_BAD_GET_SCRIPTNAME 402	// Invalid GET script name
#define ER_BAD_QUERYSTRING 403		// Invalid GET query string
#define ER_GET_PROCESSING 404		// GET processing error

#define ER_BAD_POST_CT 500		// Invalid POST "Content-length:"
#define ER_BAD_POST_SCRIPTNAME 501	// Invalid POST script name
#define ER_CREATE_TFILE_FAILED 502	// POST temp file name creation failed
#define ER_MAX_POST_ELEMENTS 503	// Max POST elements exceeded

#define ER_XML_LIB_ERR 600		// XML lib internal error
#define ER_XML_VALIDATE_ERROR 601	// XML validation against DTD failed
#define ER_XML_ELEMENT_ERROR 602	// XML element validation failed

#endif /* _OCRP_CGI_ERRORCODES_H */
/*
 * End of OCRP_cgi_errorcodes.h
 */
