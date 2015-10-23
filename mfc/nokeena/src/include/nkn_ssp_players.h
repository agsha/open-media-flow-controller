/*
 *******************************************************************************
 * nkn_ssp_players.h -- Server Side Player
 *******************************************************************************
 */

#ifndef _NKN_SSP_PLAYERS_H
#define _NKN_SSP_PLAYERS_H

#include <stdio.h>
#include "server.h"

int moveSSP(con_t *con, const char *uriData, int uriLen, const ssp_config_t *ssp_cfg);

int breakSSP(con_t *con, const char *uriData, int uriLen, const char *hostData, int hostLen, const ssp_config_t *ssp_cfg, off_t contentLen, const char *dataBuf, int bufLen);

int nknGenericSSP(con_t *con, const char *uriData, int uriLen, const char *hostData, int hostLen, const ssp_config_t *ssp_cfg, off_t contentLen, const char *dataBuf, int bufLen);

int yahooSSP(con_t *con, const char *uriData, int uriLen, const char *hostData, int hostLen, const ssp_config_t *ssp_cfg);

int youtubeSSP(con_t *con, const char *uriData, int uriLen, const char *hostData, int hostLen, const ssp_config_t *ssp_cfg, const namespace_config_t *cfg, off_t contentLen, const char *dataBuf, int bufLen); // BZ 8382

int smoothflowSSP(con_t *con, const char *uriData, int uriLen, const char *hostData, int hostLen,  const ssp_config_t *ssp_cfg, off_t contentLen, const char *dataBuf, int bufLen);

int smoothflowLoopbackRequest(con_t *con);

int smoothstreamingSSP(con_t *con, const char *uriData, int uriLen, const ssp_config_t *ssp_cfg, off_t contentLen, const char *dataBuf, int bufLen);

int ssp_zeri_streamingSSP(con_t *con, const char *uriData, int uriLen, \
			  const ssp_config_t *ssp_cfg, \
			  off_t contentLen, const char *dataBuf, int bufLen);

#endif /* _NKN_SSP_PLAYERS_H */

/*
 * End of nkn_ssp_players.h
 */
