/*
 *
 * src/bin/snmp/tms/sn_utils.h
 *
 *
 *
 */

#ifndef __SN_UTILS_H_
#define __SN_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "sn_mod_reg.h"

extern const lc_enum_string_map sn_type_map[];

typedef enum {
    srs_none = 0,
    srs_active = 1,
    srs_not_in_service = 2,
    srs_not_ready = 3,
    srs_create_and_go = 4,
    srs_create_and_wait = 5,
    srs_destroy = 6,
} sn_row_status;

extern const lc_enum_string_map sn_row_status_map[];


/* ------------------------------------------------------------------------- */
/* Dump the contents of an OID to the logs.
 */
int sn_oid_dump(int log_level, const oid *oid_data, size_t oid_len,
                tbool is_abs, const char *prefix_fmt, ...)
    __attribute__ ((format (printf, 5, 6)));


/* ------------------------------------------------------------------------- */
/* Render an OID to a string.
 */
int sn_oid_to_str(const oid *oid_data, size_t oid_num_subids,
                  tbool is_abs, tstring **ret_oid_str);


/* ------------------------------------------------------------------------- */
/* Given an existing OID, append a single subid to it, and update the
 * variable pointing to its length.
 *
 * NOTE: it is assumed that the OID buffer provided has room for
 * MAX_OID_LEN subids.  We will not append beyond that point.  If this
 * prevents us from appending what we were asked, lc_err_no_buffer_space
 * is (silently) returned.
 */
int sn_oid_append(oid *inout_oid, size_t *inout_oid_len, oid subid);


/* ------------------------------------------------------------------------- */
/* Get the value of a management node specified by name, in the form
 * of an SNMP return value.  The result is copied into the buffer
 * provided.  This is a thin wrapper around mdc_get_binding() and
 * sn_binding_to_snmp_result().
 *
 * Pass -1 for expected_snmp_type if you don't want to specify.
 */
int sn_get_snmp_value_by_name(const char *node_name, bn_type expected_bn_type,
                              int32 expected_snmp_type,
                              sn_var_flags_bf flags,
                              lt_render_flags_bf dur_flags,
                              const char *float_query_format,
                              void *ret_result, size_t result_max_size, 
                              size_t *ret_result_size, u_char *ret_snmp_type);


/* ------------------------------------------------------------------------- */
/* Free a dynamically-allocated linked list of netsnmp_variable_list 
 * structures.
 */
int sn_free_var_list(netsnmp_variable_list **inout_vars);


/*
 * This matches the number on line 231 of net-snmp/agent/snmp_vars.c,
 * which is not currently exported from any of its header files.
 */
static const uint32 sn_return_buf_size = 256;


int sn_synth_index_cache_free(sn_synth_index_cache **inout_cache);

/*
 * Return a synthetic index representing a given binding name.  This
 * index will uniquely identify the last binding name part, in a
 * namespace defined by the preceding n-1 binding name parts.
 * Previously used indices are stored in the sf_index_cache field of 
 * the sn_field provided.
 * 
 * This function takes over ownership of the name parts array provided,
 * and frees it before returning.
 */
int sn_utils_synth_index_get(sn_field *field, tstr_array **inout_sub_parts,
                             uint32 *ret_index);

/*
 * Essentially the reverse of sn_utils_synth_index_get(): given a name
 * prefix (the binding name up to but not including the value which
 * maps to the synthetic index, which determines the namespace for our
 * lookup) and a synthetic index, look up that index and return the
 * value of the last binding name part, if any.
 *
 * If the index is not found, returns NULL with no error.
 */
int sn_utils_synth_index_lookup(const sn_field *field, const char *prefix,
                                uint32 idx, tstring **ret_mapped_name);

int sn_synth_index_cache_free_some(const char *table_name);

/*
 * Create a new binding array with a duplicate policy of
 * adp_delete_old.  So whenever we add a new binding, it will make
 * sure any previous binding by the same name is removed first.
 */
int sn_binding_unique_array_new(bn_binding_array **ret_bindings);

#ifdef __cplusplus
}
#endif

#endif /* __SN_UTILS_H_ */
