/*
 * Filename:    nkn_cntr_api.h
 * Date:        2008-11-20
 * Author:      Dhruva N
 * 
 * (C) Copyright 2008, Nokeena Networks, Inc.
 * All rights reserved.
 */

#ifndef __NKN_CNTR_API_H__
#define __NKN_CNTR_API_H__


/*
 * Library initializaton. 
 *  - Grabs a shmid and attaches to a shared memory 
 *  identified by the SHMKEY.
 *  - WARNING : Call once and only once in your module!
 */
int libnkncnt_init(void);

/*
 * Library un-init
 * - Detaches from the shared memory segment.
 */
int libnkncnt_close(void);

/*----------------------------------------------------------------------------
 * HELPER FUNCTIONS TO ACCESS COUNTER VARIABLES
 *  - name : name of the global variable
 *--------------------------------------------------------------------------*/

int16_t nkncnt_get_int16(const char *name);
uint16_t nkncnt_get_uint16(const char *name);
int32_t nkncnt_get_int32(const char *name);
uint32_t nkncnt_get_uint32(const char *name);
int64_t nkncnt_get_int64(const char *name);
uint64_t nkncnt_get_uint64(const char *name);

int nkncnt_cache_refresh(void);
int nkncnt_get_counter_index(const char *name, int dynamic, int *index);
int nkncnt_get_counter_byindex(const char *name, int index, uint64_t *value);
uint64_t cntr_cache_get_value(const char *name, int dynamic, int type);

#endif
