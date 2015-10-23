/*
 *******************************************************************************
 * ssp.h -- Server Side Player
 *******************************************************************************
 */

#ifndef _SSP_H
#define _SSP_H

#include <stdio.h>
#include "server.h"

/*
 * SSP Return Codes
 */
#define SSP_OK 0
#define SSP_NOOK -1

#define SSP_FEATURE_NOT_SUPP 	-2
#define SSP_CLOSE_CONN		-1
#define SSP_SEND_DATA_OTW 	0
#define SSP_WAIT_FOR_DATA 	1
#define SSP_SEND_DATA_BACK 	2
#define SSP_DETECT_MEDIA_ATTR_FAIL    4
#define SSP_200_OK		3

#define SSP_SF_MULTIPART_LOOPBACK 	1
#define SSP_SF_MULTIPART_CLOSE_CONN     0

#define MAX_VIRTUAL_PLAYER_TYPES 20

#define SSP_MAX_EXTENSION_SZ (10)

// Public API Definitions

int SSP_init(void);

/*
 * startSSP
 *     Return values:
 *      3 return 200 OK
 *	2 return the data buffer back to SSP and do not send over the wire
 *      1 SSP is still waiting for all the pieces of .txt to arrive
 *	0 Fetch and send data over the wire
 *    < 0 Error
 *	-1 available bandwidth is insufficient for this conn (HPE to reject) 
 */
int startSSP(con_t *con, off_t contentLen, const char *dataBuf, int bufLen);

/*
 * updateSSP
 *     Return values:
 *      NONE
*/
void updateSSP(con_t *con);

int requestdoneSSP(con_t *con);

int close_conn_ssp(con_t *con);

int cleanSSP_force_tunnel(con_t *con);

// Private API Defintions

//int selectSSP(con_t *con, const char *uriData, int uriLen, off_t contentLen, const char *dataBuf, int bufLen);

//int nknPythonSSP(con_t *con, const char *uriData, int uriLen, const char *hostData, int hostLen);

#endif /* _SSP_H */

/*
 * End of ssp.h
 */
