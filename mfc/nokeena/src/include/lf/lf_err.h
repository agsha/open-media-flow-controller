#ifndef _LF_ERR_H
#define _LF_ERR_H

#define E_LF_NO_MEM 1
#define E_LF_INVAL 2
#define E_LF_APP_METRICS_INTF_REG 3
#define E_LF_NO_FREE_TOKEN 4
#define E_LF_OPEN_CFG_FILE 5
#define E_LF_XML_GENERIC 6
#define E_LF_INVAL_LISTEN_PORT 7
#define E_LF_SAMP_RATE_WND_LEN_INCOMPAT 8
#define E_LF_INVAL_SAMP_RATE 9
#define E_LF_INVAL_WND_LEN 10

/* VM connector error codes */
#define E_LF_VM_HYPERV_CONN 101
#define E_LF_VM_DOMAIN_CONN 102
#define E_LF_VM_DEV_ENUM_FAIL 103

/* NA connector error codes */
#define E_LF_NA_CONN 201
#define E_LF_NA_NO_FREE_TOKEN 202
#define E_NA_HTTP_EXCEEDS_RP_CNT 203
#define E_LF_NA_HTTP_UNAVAIL 204
#define E_NA_HTTP_EXCEEDS_VIP_CNT 205

/* SYS connector error codes */
#define E_LF_SYS_CONN 301
#define E_LF_SYS_IF_STATS 302
#endif //_LF_ERR_H
