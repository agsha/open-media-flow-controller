/* File : rtsp_response.c
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */

/** 
 * @file rtsp_response.c
 * @brief 
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-09-25
 */

#include <stdio.h>

#include "nkn_defs.h"
#include "nkn_stat.h"
#include "rtsp_server.h"
#include "rtsp_header.h"

/* 
 * Defining Global counter variables.
 * */
NKNCNT_EXTERN(rtsp_tot_status_resp_100, int);
NKNCNT_EXTERN(rtsp_tot_status_resp_200, int);
NKNCNT_EXTERN(rtsp_tot_status_resp_201, int);
NKNCNT_EXTERN(rtsp_tot_status_resp_250, int);
NKNCNT_EXTERN(rtsp_tot_status_resp_300, int);
NKNCNT_EXTERN(rtsp_tot_status_resp_301, int);
NKNCNT_EXTERN(rtsp_tot_status_resp_302, int);
NKNCNT_EXTERN(rtsp_tot_status_resp_303, int);
NKNCNT_EXTERN(rtsp_tot_status_resp_304, int);
NKNCNT_EXTERN(rtsp_tot_status_resp_305, int)
NKNCNT_EXTERN(rtsp_tot_status_resp_400, int);//, "Total 400 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_401, int);//, "Total 401 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_402, int);//, "Total 402 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_403, int);//, "Total 403 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_404, int);//, "Total 404 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_405, int);//, "Total 405 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_406, int);//, "Total 406 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_407, int);//, "Total 407 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_408, int);//, "Total 408 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_410, int);//, "Total 410 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_411, int);//, "Total 411 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_412, int);//, "Total 412 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_413, int);//, "Total 413 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_414, int);//, "Total 414 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_415, int);//, "Total 415 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_451, int);//, "Total 451 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_452, int);//, "Total 452 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_453, int);//, "Total 453 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_454, int);//, "Total 454 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_455, int);//, "Total 455 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_456, int);//, "Total 456 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_457, int);//, "Total 457 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_458, int);//, "Total 458 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_459, int);//, "Total 459 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_460, int);//, "Total 460 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_461, int);//, "Total 461 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_462, int);//, "Total 462 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_500, int);//, "Total 500 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_501, int);//, "Total 501 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_502, int);//, "Total 502 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_503, int);//, "Total 503 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_504, int);//, "Total 504 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_505, int);//, "Total 505 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_551, int);//, "Total 551 response");

#define UNUSED_FN_ARGS(a, b, c, d) \
{\
    UNUSED_ARGUMENT(a);\
    UNUSED_ARGUMENT(b);\
    UNUSED_ARGUMENT(c);\
    UNUSED_ARGUMENT(d);\
}

extern char rl_rtsp_server_str[];

/*
 * Functions registered to build headers specific to the status code 
 * Functions below should get information from the context prtsp sructure and
 * fill headers in 'buf'. 
 */
static rtsp_parse_status_t
rtsp_build_hdr_100 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_200 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_201 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_250 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_300 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_301 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_302 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_303 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_304 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_305 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_400 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_ARGUMENT(prtsp);
    int print_len = 0;
    print_len = snprintf(buf, buf_len, "Connection: Close\r\n");

    if (print_len < 0) {
	return (RPS_ERROR);
    }

    /* check buf overflow */
    if ((print_len == buf_len) && (buf[print_len] != '\0')) {
	return (RPS_NEED_MORE_SPACE);
    }

    /* Next pointer to continue */
    if (end) {
	*end = &buf[print_len];
    }

    return (RPS_OK);
}

static rtsp_parse_status_t
rtsp_build_hdr_401 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_402 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_403 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_404 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_405 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_406 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_407 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_408 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_410 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_411 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_412 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_413 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_414 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_415 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_451 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_452 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_453 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_454 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_455 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_456 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_457 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_458 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_459 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_460 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_461 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_462 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_500 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_501 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_502 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_503 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_504 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_505 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

static rtsp_parse_status_t
rtsp_build_hdr_551 (rtsp_cb_t *prtsp, char *buf, int buf_len, char **end)
{
    UNUSED_FN_ARGS(prtsp, buf, buf_len, end);
    return RPS_OK;
}

#define NKN_GLOB(name) glob_##name
/** 
 * @brief This function registers the counters and the header composing functions
 * to the rtsp status table.
 * 
 * @return RPS_OK
 *
 */
static rtsp_parse_status_t rtsp_status_register (void) {
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_100 ,NKN_GLOB(rtsp_tot_status_resp_100) , rtsp_build_hdr_100);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_200 ,NKN_GLOB(rtsp_tot_status_resp_200) , rtsp_build_hdr_200);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_201 ,NKN_GLOB(rtsp_tot_status_resp_201) , rtsp_build_hdr_201);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_250 ,NKN_GLOB(rtsp_tot_status_resp_250) , rtsp_build_hdr_250);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_300 ,NKN_GLOB(rtsp_tot_status_resp_300) , rtsp_build_hdr_300);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_301 ,NKN_GLOB(rtsp_tot_status_resp_301) , rtsp_build_hdr_301);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_302 ,NKN_GLOB(rtsp_tot_status_resp_302) , rtsp_build_hdr_302);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_303 ,NKN_GLOB(rtsp_tot_status_resp_303) , rtsp_build_hdr_303);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_304 ,NKN_GLOB(rtsp_tot_status_resp_304) , rtsp_build_hdr_304);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_305 ,NKN_GLOB(rtsp_tot_status_resp_305) , rtsp_build_hdr_305);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_400 ,NKN_GLOB(rtsp_tot_status_resp_400) , rtsp_build_hdr_400);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_401 ,NKN_GLOB(rtsp_tot_status_resp_401) , rtsp_build_hdr_401);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_402 ,NKN_GLOB(rtsp_tot_status_resp_402) , rtsp_build_hdr_402);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_403 ,NKN_GLOB(rtsp_tot_status_resp_403) , rtsp_build_hdr_403);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_404 ,NKN_GLOB(rtsp_tot_status_resp_404) , rtsp_build_hdr_404);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_405 ,NKN_GLOB(rtsp_tot_status_resp_405) , rtsp_build_hdr_405);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_406 ,NKN_GLOB(rtsp_tot_status_resp_406) , rtsp_build_hdr_406);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_407 ,NKN_GLOB(rtsp_tot_status_resp_407) , rtsp_build_hdr_407);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_408 ,NKN_GLOB(rtsp_tot_status_resp_408) , rtsp_build_hdr_408);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_410 ,NKN_GLOB(rtsp_tot_status_resp_410) , rtsp_build_hdr_410);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_411 ,NKN_GLOB(rtsp_tot_status_resp_411) , rtsp_build_hdr_411);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_412 ,NKN_GLOB(rtsp_tot_status_resp_412) , rtsp_build_hdr_412);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_413 ,NKN_GLOB(rtsp_tot_status_resp_413) , rtsp_build_hdr_413);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_414 ,NKN_GLOB(rtsp_tot_status_resp_414) , rtsp_build_hdr_414);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_415 ,NKN_GLOB(rtsp_tot_status_resp_415) , rtsp_build_hdr_415);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_451 ,NKN_GLOB(rtsp_tot_status_resp_451) , rtsp_build_hdr_451);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_452 ,NKN_GLOB(rtsp_tot_status_resp_452) , rtsp_build_hdr_452);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_453 ,NKN_GLOB(rtsp_tot_status_resp_453) , rtsp_build_hdr_453);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_454 ,NKN_GLOB(rtsp_tot_status_resp_454) , rtsp_build_hdr_454);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_455 ,NKN_GLOB(rtsp_tot_status_resp_455) , rtsp_build_hdr_455);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_456 ,NKN_GLOB(rtsp_tot_status_resp_456) , rtsp_build_hdr_456);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_457 ,NKN_GLOB(rtsp_tot_status_resp_457) , rtsp_build_hdr_457);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_458 ,NKN_GLOB(rtsp_tot_status_resp_458) , rtsp_build_hdr_458);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_459 ,NKN_GLOB(rtsp_tot_status_resp_459) , rtsp_build_hdr_459);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_460 ,NKN_GLOB(rtsp_tot_status_resp_460) , rtsp_build_hdr_460);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_461 ,NKN_GLOB(rtsp_tot_status_resp_461) , rtsp_build_hdr_461);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_462 ,NKN_GLOB(rtsp_tot_status_resp_462) , rtsp_build_hdr_462);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_500 ,NKN_GLOB(rtsp_tot_status_resp_500) , rtsp_build_hdr_500);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_501 ,NKN_GLOB(rtsp_tot_status_resp_501) , rtsp_build_hdr_501);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_502 ,NKN_GLOB(rtsp_tot_status_resp_502) , rtsp_build_hdr_502);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_503 ,NKN_GLOB(rtsp_tot_status_resp_503) , rtsp_build_hdr_503);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_504 ,NKN_GLOB(rtsp_tot_status_resp_504) , rtsp_build_hdr_504);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_505 ,NKN_GLOB(rtsp_tot_status_resp_505) , rtsp_build_hdr_505);
    RTSP_REG_CNT_AND_FN(RTSP_STATUS_551 ,NKN_GLOB(rtsp_tot_status_resp_551) , rtsp_build_hdr_551);
    return (RPS_OK);
}

int rtsp_status_map_init (void )
{
    if (RPS_OK == rtsp_status_register()) {
	return 0;
    }
    return -1;
}

int rtsp_status_map_exit (void)
{
    return 0;
}

static rtsp_parse_status_t
rtsp_compose_status_line(char *buffer, int buflen,
			 rtsp_status_t status, int sub_error_code,
			 char **end)
{
    int print_len = 0;
    if (!buffer || buflen <= 0 || !IS_RTSP_STATUS_VALID(status)) {
	return (RPS_ERROR);
    }

    /*
     * Status-Line =   RTSP-Version SP Status-Code SP Reason-Phrase <SUB Error Code> CRLF
     */
    if (sub_error_code) {
	print_len = snprintf(buffer, buflen, "RTSP/1.0 %s %s %d\r\n",
			     RTSP_STATUS_CODE_STR(status),
			     RTSP_STATUS_REASON_STR(status),
			     sub_error_code);
    } else {
	print_len = snprintf(buffer, buflen, "RTSP/1.0 %s %s \r\n",
			     RTSP_STATUS_CODE_STR(status),
			     RTSP_STATUS_REASON_STR(status));
    }

    if (print_len < 0) {
	return (RPS_ERROR);
    }

    /* check buf overflow */
    if ((print_len == buflen) && (buffer[print_len] != '\0')) {
	return (RPS_NEED_MORE_SPACE);
    }

    /* Next pointer to continue */
    if (end) {
	*end = &buffer[print_len];
    }

    return (RPS_OK);
}


static rtsp_parse_status_t
rtsp_compose_common_hdrs(rtsp_cb_t * prtsp, char *buf, int buf_len,
			 char **end)
{
	int print_len = 0;
	if(prtsp->cseq[0] == '\0'){
		print_len = snprintf(buf, buf_len, 
					"Server: %s\r\n"
					"Date: %s\r\n", 
					rl_rtsp_server_str,
					nkn_get_datestr(NULL));
	} else {
		print_len = snprintf(buf, buf_len, 
					"CSeq: %s\r\n"
					"Server: %s\r\n"
					"Date: %s\r\n", 
					prtsp->cseq,
					rl_rtsp_server_str,
					nkn_get_datestr(NULL));
	}

	if (print_len < 0) {
		return (RPS_ERROR);
	}

	/* check buf overflow */
	if ((print_len == buf_len) && (buf[print_len] != '\0')) {
		return (RPS_NEED_MORE_SPACE);
	}

	/* Next pointer to continue */
	if (end) {
		*end = &buf[print_len];
	}

	return (RPS_OK);
}

rtsp_parse_status_t
rtsp_build_response(rtsp_cb_t * prtsp, rtsp_status_t id, int sub_err_code)
{
	rtsp_parse_status_t ret_stat = RPS_ERROR;
	char *buf = prtsp->cb_respbuf;
	char *tmp_end = buf;
	int buf_len = RTSP_BUFFER_SIZE;

	/*
	 * Response buffer already filled in with data 
	 * */
	if (RTSP_STATUS_CODE_NUM(id) != -1)
		prtsp->status = RTSP_STATUS_CODE_NUM(id);
	if (prtsp->cb_resplen)
		return (RPS_OK);

	/*
	 * Build Status Line 
	 */
	ret_stat =
	rtsp_compose_status_line(prtsp->cb_respbuf, buf_len, id,
	sub_err_code, &tmp_end);

	if (ret_stat != RPS_OK)
		return ret_stat;
	buf = tmp_end;
	buf_len = RTSP_BUFFER_SIZE - (buf - prtsp->cb_respbuf);

	ret_stat = rtsp_compose_common_hdrs(prtsp, buf, buf_len, &tmp_end);

	if (ret_stat != RPS_OK)
		return ret_stat;
	buf = tmp_end;
	buf_len = RTSP_BUFFER_SIZE - (buf - prtsp->cb_respbuf);

	/* 
	 * Call call back registered functins for specific status code 
	 */
	rtsp_build_hdr_t build_hdr_cb = RTSP_STATUS_BUILD_HDR(id);
	if (build_hdr_cb) {
		ret_stat = (*build_hdr_cb) (prtsp, buf, buf_len, &tmp_end);
		if (ret_stat != RPS_OK)
			return ret_stat;
		buf = tmp_end;
		buf_len = RTSP_BUFFER_SIZE - (buf - prtsp->cb_respbuf);
	}

	if (buf_len <= 2) {
		return (RPS_NEED_MORE_SPACE);
	}

	*buf++ = '\r';
	*buf++ = '\n';
	*buf = '\0';
	buf_len -= 2;
	prtsp->cb_resplen = buf - prtsp->cb_respbuf;
	return (RPS_OK);
}
