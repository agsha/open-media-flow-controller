/*****************************************************************************
*
*	File : nkn_common_config.h
*
*	Created By : Ramanand Narayanan (ramanand@nokeena.com)
*
*	Created On : 1st July, 2008
*
*	Modified On : 
*
*	Copyright (c) Nokeena Inc., 2008
*
*****************************************************************************/
#ifndef NKN_COMMON_CONFIG_H
#define  NKN_COMMON_CONFIG_H

/* Set macros for the nokeena system files */
#define NKN_CONFIG_DIR	"/config/nkn/"
#define NKN_SBIN_DIR	"/opt/nkn/sbin/"
#define NKN_BIN_DIR	"/opt/nkn/bin/"
#define NKN_LOG_DIR	"/var/log/nkn"
#define NKN_CONFIG_OUTPUT_DIR	"/var/opt/nkn/output"

#define NKNSRV_CONF_FILE		NKN_CONFIG_DIR"nkn.conf.local"
#define NKNSRV_CONF_DEFAULT_FILE	NKN_CONFIG_DIR"nkn.conf.default"
#define NKNSRV_LOG_FILE                 NKN_LOG_DIR"nkn.log"
#define DISKCACHE_INFO_FILE		NKN_CONFIG_DIR"diskcache.info"
#define ORIGINMGR_PID_FILE		NKN_CONFIG_DIR".originmgr_pid"
#define OORIGINMGR_PID_FILE		NKN_CONFIG_DIR".ooriginmgr_pid"
#define OORIGINMGR_DATA_INFO_FILE	NKN_CONFIG_DIR".ooriginmgr_data_info"
#define VPEMGR_PID_FILE                 NKN_CONFIG_DIR".vpemgr_pid"
#define VPEMGR_DATA_INFO_FILE           NKN_CONFIG_DIR".vpemgr_data_info"
#define CMM_PID_FILE			NKN_CONFIG_DIR".cmm_pid"

#define NKN_SSP_PROFILE_FILE	NKN_CONFIG_DIR"nkn_profile_list.cfg"

#define NKN_MAX_VIPS			8
/* Other handy definitions */
typedef enum boolean
{
	NKN_TRUE = 1,
	NKN_FALSE = 0
} boolean ;


#define MAX_NS_COMPRESS_FILETYPES 256
#define MAX_UNSUCCESSFUL_RESPONSE_CODES 8
#endif /* NKN_COMMON_CONFIG_H */
/* End of nkn_common_config.h */
