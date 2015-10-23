/*
*******************************************************************************
* ssp_authentication.h -- SSP Authentication Mechanisms (Hash, MD-5, ...)
*******************************************************************************
*/

#ifndef _SSP_AUTHENTICATION_H
#define _SSP_AUTHENTICATION_H

#include <stdio.h>
#include "server.h"

/* Public API for Authentication Requests */
int verifyMD5Hash(const char *hostData, const char *uriData, int hostLen, mime_header_t *hd, const hash_verify_t cfg_hash);

int verifyPythonHash(const char *hostData, const char *uriData, int hostLen, mime_header_t *hd);

int verifyYahooHash(const char *hostData, const char *uriData, int hostLen, mime_header_t *hd, const req_auth_t req_auth);
/* Private API */
int findStr(const char *pcMainStr, const char *pcSubStr, int length);

#endif /* _SSP_AUTHENTICATION_H */

/*
 * End of ssp_authentication.h
 */
