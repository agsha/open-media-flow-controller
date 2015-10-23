/*
 *
 * src/bin/snmp/tms/sn_scalars.h
 *
 *
 *
 */

#ifndef __SN_SCALARS_H_
#define __SN_SCALARS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif


/* ------------------------------------------------------------------------- */
/* Main GET handler for scalar variables
 *
 * Result is placed in pre-allocated buffer pointed to by ret_result, and
 * is truncated in size at result_buf_size.
 */
int sn_scalar_get(const sn_scalar *scalar,
                  const oid *base_oid, size_t base_oid_len,
                  const oid *req_oid, size_t req_oid_len, tbool exact,
                  oid *ret_oid, size_t *ret_oid_len,
                  void *ret_result, size_t result_buf_size, 
                  size_t *ret_result_len);


#ifdef __cplusplus
}
#endif

#endif /* __SN_SCALARS_H_ */
