/*
 *
 * src/bin/snmp/tms/sn_tables_new.h
 *
 *
 *
 */

#ifndef __SN_TABLES_NEW_H_
#define __SN_TABLES_NEW_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif


int sn_table_init(void);

int sn_table_deinit(void);


/* ------------------------------------------------------------------------- */
/* Main GET handler for columnar variables (table fields).
 *
 * Result is placed in pre-allocated buffer pointed to by ret_result, and
 * is truncated in size at result_buf_size.
 *
 * Note that OID lengths are in number of subids, NOT in bytes!
 * Each subid is four bytes.
 */
int sn_table_get_field(const sn_field *field,
                       const oid *base_oid, size_t base_oid_len,
                       const oid *req_oid, size_t req_oid_len, tbool exact,
                       oid *ret_oid, size_t *ret_oid_len,
                       void *ret_result, size_t result_buf_size,
                       size_t *ret_result_len, tbool *ret_got_result);


/* ------------------------------------------------------------------------- */
/* We are passed a registration structure that is part of a trap we
 * are about to send.  This structure identifies a table field.
 * We search the provided binding array for a binding which could be
 * the source for an instance of this table field.  We return the name
 * of the binding (which our caller will fetch out of the event message)
 * and the OID by which the binding should be known in the trap.
 */
int sn_notif_get_event_var_field(sn_notif_var *var_reg,
                                 bn_binding_array *bindings,
                                 char **ret_binding_name,
                                 oid *ret_oid_name, size_t *ret_oid_name_len);


/* ------------------------------------------------------------------------- */
/* Clear table data caches.  Clear one if name specified, otherwise all.
 */
int sn_table_caches_free(const char *table_name);


extern lt_dur_ms sn_cache_lifetime_ms;
extern lt_dur_ms sn_cache_lifetime_long_ms;


#ifdef __cplusplus
}
#endif

#endif /* __SN_TABLES_NEW_H_ */
