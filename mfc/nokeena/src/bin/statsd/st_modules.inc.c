/*
 * (C) Copyright 2015 Juniper Networks
 * All rights reserved.
 */

#ifndef __ST_MODULES_INC_C_
#define __ST_MODULES_INC_C_
static const char *rcsid_h__ST_MODULES_INC_C_ __attribute__((unused)) =
    "$Id: st_modules.inc.c,v 1.3 2006/03/29 23:36:13 gregs Exp $";
#endif /* __ST_MODULES_INC_C_ */


/* 
 * This file is include by src/bin/statsd/st_modules.c .  The different
 * blocks are chosen by the caller #define'ing ST_MOD_INC_GRAFT_POINT to the
 * numbers seen below.
*/

/* Graft point 1: declarations */
#if ST_MOD_INC_GRAFT_POINT == 1

int stm_nkn_alarms_init(const lc_dso_info *info, void *data);

#endif /* GRAFT_POINT 1 */


/* Graft point 2: init */
#if ST_MOD_INC_GRAFT_POINT == 2

    err = stm_nkn_alarms_init(NULL, NULL);
    complain_error(err);
   
    err = stm_nkn_stats_init(NULL,NULL,NULL);
    complain_error(err);


#endif /* GRAFT_POINT 2 */


/* Graft point 3: deinit */
#if ST_MOD_INC_GRAFT_POINT == 3

#endif /* GRAFT_POINT 3 */

