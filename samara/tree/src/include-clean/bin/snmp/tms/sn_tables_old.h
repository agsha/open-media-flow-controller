/*
 *
 * src/bin/snmp/tms/sn_tables_old.h
 *
 *
 *
 */

#ifndef __SN_TABLES_OLD_H_
#define __SN_TABLES_OLD_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ............................................................................
 * NOTE: this file is part of the old implementation of SNMP tables.
 *       This has been replaced with a new implementation, and is 
 *       currently inert.
 * ............................................................................
 */


/* ------------------------------------------------------------------------- */
/* Main GET handler for columnar variables (table fields)
 *
 * Result is placed in pre-allocated buffer pointed to by ret_result, and
 * is truncated in size at result_buf_size.
 */
int sn_table_get_field(const sn_field *field,
                       const oid *base_oid, size_t base_oid_len,
                       const oid *req_oid, size_t req_oid_len, tbool exact,
                       oid *ret_oid, size_t *ret_oid_len,
                       void *ret_result, size_t result_buf_size,
                       size_t *ret_result_len);


#ifdef __cplusplus
}
#endif

#endif /* __SN_TABLES_OLD_H_ */
