/*
*******************************************************************************
* ssp_queryparams.h -- SSP Query Params
*******************************************************************************
*/

#ifndef _SSP_QUERYPARAMS_H
#define _SSP_QUERYPARAMS_H

#include <stdio.h>
#include "server.h"

#define MP4_INIT_BYTE_RANGE_LEN (32768) //32KB of data for moov atom size

/* Public API for Query Parameters */
int query_full_download(con_t *con, const char *queryName, const char *matchStr);
int header_full_download(con_t *con, const char *headerName, const char *matchStr);
int setDownloadRate(con_t *con, const ssp_config_t *ssp_cfg);

int query_fast_start_size(con_t *con, const char *queryName);
int query_fast_start_time(con_t *con, const char *queryName, uint32_t afr);

int query_flv_seek(con_t *con, const char *queryName, const char *querySeekLenName);
#if 0
int query_throttle_to_afr(con_t *con, const char *queryName);
#else
int query_throttle_to_rc(con_t *con, const char *queryName);
#endif
int setAFRate(con_t *con, const ssp_config_t *ssp_cfg, int64_t steadyRate);

int generate_om_seek_uri(con_t *con, const char *queryName);

int query_mp4_seek_for_any_moov(con_t *con, const char *queryName, off_t contentLen, const char *dataBuf, int bufLen, int timeUnits);
int query_mp4_seek(con_t *con, const char *queryName, off_t contentLen, const char *dataBuf, int bufLen, int timeUnits);

void writeMDAT_Header(char *p_src, uint64_t mdat_size);

int query_flv_timeseek(con_t *con, const char *queryName, off_t contentLen, const char *dataBuf, int bufLen, int timeUnits);

#endif /* _SSP_QUERYPARAMS_H */

/*
 * End of ssp_queryparams.h
 */
