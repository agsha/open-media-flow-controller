/*
************************************************
* nkn_ssp_player_prop.h -- SSP Player Properties
************************************************/

#ifndef _NKN_SSP_PLAYER_PROP_H
#define _NKN_SSP_PLAYER_PROP_H

#include <stdio.h>
#include "server.h"

/* Public API for Query Parameters */
int
ssp_auth_md5hash_req(con_t *con, const ssp_config_t *ssp_cfg,
		 const char *uriData, int uriLen,
		 const char *hostData, int hostLen);

int ssp_set_transcode(const ssp_config_t *ssp_cfg, int* transType,
                      uint64_t* threshold, char transRatioData[][4],
                      int* transRatioLen);

int
ssp_set_faststart_size(con_t *con, const ssp_config_t *ssp_cfg, off_t contentLen);

#if 0
int
ssp_set_afr(con_t *con, const ssp_config_t *ssp_cfg);
#else
int
ssp_set_rc(con_t *con, const ssp_config_t *ssp_cfg);
#endif

int
ssp_full_download_req(con_t *con, const ssp_config_t *ssp_cfg);

int
ssp_detect_media_attr(con_t *con, const ssp_config_t *ssp_cfg,
		      off_t contentLen, const char *dataBuf, int bufLen);
int32_t
vpe_detect_media_type(uint8_t* data);

int
ssp_seek_req(con_t *con, const ssp_config_t *ssp_cfg,
             off_t contentLen, const char *dataBuf, int bufLen);
int
ssp_detect_media_type(con_t *con, const ssp_config_t *ssp_cfg,
		     off_t contentLen, const char *dataBuf, int bufLen);

#endif /* _NKN_SSP_PLAYER_PROP_H */

/*
 * End of nkn_ssp_player_prop.h
 */
