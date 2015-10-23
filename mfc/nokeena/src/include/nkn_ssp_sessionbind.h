/*
 *
 * Filename:  nkn_ssp_sessionbind.h
 * Date:      2009/03/02
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 */

#ifndef _NKN_SSP_SESSIONBIND_
#define _NKN_SSP_SESSIONBIND_

#include <stdio.h>

void nkn_ssp_sessbind_init(void);

int nkn_ssp_sess_add(nkn_ssp_session_t *sess_obj);

nkn_ssp_session_t* nkn_ssp_sess_get(char *session_id);

void nkn_ssp_sess_remove(nkn_ssp_session_t *sess_obj);

nkn_ssp_session_t* sess_obj_alloc(void);

void sess_obj_free(nkn_ssp_session_t *sess_obj);

#endif //_NKN_SSP_SESSIONBIND_
 
