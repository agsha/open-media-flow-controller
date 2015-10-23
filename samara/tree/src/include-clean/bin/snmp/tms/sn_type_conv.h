/*
 *
 * src/bin/snmp/tms/sn_type_conv.h
 *
 *
 *
 */

#ifndef __SN_TYPE_CONV_H_
#define __SN_TYPE_CONV_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/*
 * This file takes care of converting between different
 * representations of various data types.  We need to convert between
 * our own native bn_attribs and two others: SNMP OIDs, and NET-SNMP's
 * native representation.  So that leaves us with four conversions:
 *
 * 1. bn_attrib --> OID         sn_oid_append_index (takes string form)
 * 2. bn_attrib --> SNMP var    sn_binding_to_snmp_result (takes binding)
 * 3. OID       --> bn_attrib   sn_oid_get_index_attrib
 * 3. SNMP var  --> bn_attrib   sn_snmp_native_to_attrib
 *
 * Because special handling is needed for the types where multiple SNMP
 * variables map to a single node, we have separate routines for those:
 *
 * 3'. OID      --> bn_attrib   sn_oid_get_addr_index_attrib
 * 4'. SNMP var --> bn_attrib   sn_snmp_native_mult_to_attrib
 */

#include "common.h"
#include "bnode.h"
#include "sn_mod_reg.h"
#include "sn_sets.h"


/* ------------------------------------------------------------------------- */
/* Given an existing OID, append to it whatever subids are needed to
 * represent the provided value as an index.  The length pointed to by
 * 'inout_oid_len' is used to determine at what point we begin appending,
 * and is also updated to reflect the OID's new length after the append.
 *
 * NOTE: it is assumed that the OID buffer provided has room for
 * MAX_OID_LEN subids.  We will not append beyond that point.  If this
 * prevents us from appending what we were asked, lc_err_no_buffer_space
 * is (silently) returned.
 */
int sn_oid_append_index(oid *inout_oid, size_t *inout_oid_len,
                        const char *val, bn_type btype, sn_var_flags_bf flags,
                        lt_render_flags_bf dur_render_flags,
                        const char *float_query_format);


/* ------------------------------------------------------------------------- */
/* Given an OID containing the value of a table index, and some
 * information about what data type / encoding is expected, convert
 * it into a bn_attrib representing the wildcard instance this would
 * map to.
 *
 * NOTE: we only handle cases where a single SNMP variable maps to a
 * single attribute.  This precludes cases like inetaddrs, which are
 * handled by sn_oid_get_addr_index_attrib() instead.  Our caller
 * must distinguish.
 *
 * \param idx_oid The OID from which we should extract an attribute.
 * This is expected to start with the index we are extracting (we do
 * not add an offset), so it is usually going to be partway into the
 * full OID that the caller started with.
 *
 * \param idx_oid_len The number of subids in the OID we are passed.
 *
 * \param snmp_type The SNMP data type (ASN_UNSIGNED, etc.) of the
 * index field whose value we are extracting.
 *
 * \param btype The mgmtd data type (bt_uint32, etc.) of the wildcard
 * associated with the index field whose value we are extracting.
 *
 * \param flags Registration flags for the SNMP index field variable.
 * These may affect how the field is rendered into an OID, and therefore 
 * the way we interpret (reverse-render) it.
 *
 * \retval ret_idx_oid_offset The offset (in number of subids) of the
 * next subid in this OID after the attribute we extracted.  In other
 * words, the number of subids we consumed in order to create the
 * attribute we are returning.  Note that if this index was the last
 * part of the OID, this should equal idx_oid_len, and would point
 * beyond the end of the OID, meaning we're done.
 *
 * \retval ret_attrib  The attribute we extracted from the OID.
 */
int sn_oid_get_index_attrib(const oid *idx_oid, size_t idx_oid_len,
                            u_char snmp_type, bn_type btype, 
                            sn_var_flags_bf flags, size_t *ret_idx_oid_offset,
                            bn_attrib **ret_attrib);


/* ------------------------------------------------------------------------- */
/* Variant on sn_oid_get_index_attrib() which can read multiple index
 * variables and map them onto a single attribute.
 *
 * \param idx_oid Same as above.
 *
 * \param idx_oid_len Same as above.
 *
 * \param fields Array of type 'sn_field_*' which are the fields for the
 * table we're dealing with.
 *
 * \param field_idx Index of the field in 'fields' at which we should start.
 * It is expected that this field has the svf_inet_address_type flag set.
 * The next one should have svf_inet_address set.  And depending on the type,
 * there might be a third field expected with svf_inet_address_prefix_length
 * set.
 *
 * \retval ret_snmp_err If a user error is encountered (generally because
 * the size or contents of the variables is not consistent with the bnode
 * type the variables are bound to), this may come back with an error code
 * in it.  That is distinct from the function returning an error in its
 * return code, which means an internal error (either in the infrastructure,
 * or in the module).
 *
 * \retval ret_vars_consumed Number of field variables we have consumed 
 * in reading the attribute.  On success this should be either 2 or 3
 * depending on whether the type involved required a prefix length.
 *
 * \retval ret_idx_oid_offset Same as above.  Note that this will include
 * the number of subids for all variables read, not just the first.
 *
 * \retval ret_attrib Same as above.
 */
int sn_oid_get_addr_index_attrib(const oid *idx_oid, size_t idx_oid_len,
                                 const array *fields, uint8 field_idx,
                                 int *ret_snmp_err,
                                 uint8 *ret_vars_consumed,
                                 size_t *ret_idx_oid_offset,
                                 bn_attrib **ret_attrib);


/* ------------------------------------------------------------------------- */
/* Convert a binding to an SNMP data type in the form required to return
 * from NET-SNMP get handler functions.
 *
 * \param binding The binding whose data to convert.
 *
 * \param expected_bn_type If the caller thinks he knows what bn_type
 * the binding is, this can be specified as a sanity check.
 * Otherwise, pass bt_NONE.
 *
 * \param expected_snmp_type If the caller is expecting a particular 
 * SNMP data type in return, this can be specified.  Otherwise, pass -1.
 *
 * \param flags SNMP variable flags from the registration of the variable
 * being rendered.
 *
 * \param result_max_len The size of the buffer passed for ret_result.
 * No more bytes than this will be filled in.  If the result exceeds
 * this length, it will be truncated and a warning logged.
 *
 * \retval ret_result The result of the conversion is placed into this
 * pre-allocated buffer.
 *
 * \retval ret_result_len The length of the result in bytes.  This
 * will always be less than or equal to result_max_len.
 *
 * \retval ret_snmp_type The ASN.1 data type of the returned value.
 * This may be NULL if the caller doesn't care about the type.
 */
int sn_binding_to_snmp_result(const bn_binding *binding, 
                              bn_type expected_bn_type,
                              int32 expected_snmp_type,
                              sn_var_flags_bf flags,
                              lt_render_flags_bf dur_render_flags,
                              const char *float_query_format,
                              void *ret_result, size_t result_max_len, 
                              size_t *ret_result_len, u_char *ret_snmp_type);


/* ------------------------------------------------------------------------- */
/* Convert a native SNMP representation of a variable into a bn_attrib.
 * This is somewhat like the inverse of sn_binding_to_snmp_result(),
 * though we use attributes instead of bindings because the input can
 * only map to one attribute, and it's the caller's business where the
 * attribute goes after we're done.
 *
 * There are three "...flags" parameters, which are all bit fields 
 * accomodating different sets of flags:
 *
 *   - btype_flags (bn_type_flags_bf): bnode type flags, to be used in 
 *     construction of the attribute.
 *
 *   - reg_flags (sn_var_flags_bf): flags registered on the SNMP variable
 *     for which the conversion is being done.  Some of these flags affect
 *     how the value is rendered, and thus how we would have to interpret
 *     an attempt to set the value.
 *
 *   - flags (uint32): currently unused, and must be zero.  This is meant
 *     to leave room for future expansion of this API; the flags will be
 *     specific to this function.
 */
int sn_snmp_native_to_attrib(const u_char *snmp_val, u_char snmp_type,
                             size_t snmp_val_len, 
                             bn_type btype, bn_type_flags_bf btype_flags,
                             sn_var_flags_bf reg_flags,
                             uint32 flags, bn_attribute_id attrib_id,
                             bn_attrib **ret_attrib, int *ret_snmp_err);


/* ------------------------------------------------------------------------- */
/* Variant of sn_snmp_native_to_attrib(): convert a native SNMP
 * representation of a variable into a bn_attrib, in the case where the
 * variable is part of two or three variables mapping to a single node.
 *
 * The difference here is that the information we are passed is not 
 * necessarily sufficient to know the final answer of the attrib we
 * want to return.  There might be up to one or two other variables,
 * coming before or after us, which have to be combined to produce the
 * final attrib.  Some of the variables expected by the attrib type
 * might also be missing, and we'd have to fill in the blanks.
 *
 * The parameters are the same as sn_snmp_native_to_attrib(), except
 * that there are three new ones: req_state, set_type, and node_name.
 * These are used to find whatever work we have done so far on this
 * node, which we will then build on.
 *
 * Our approach is as follows:
 *
 *   1. Look up whatever work has been done so far on this attribute, if any.
 *      i.e. if we have already processed another variable bound to this same
 *      node.  We do NOT want to consider the value it might have had before
 *      this particular SET request.  The source of the lookup has three
 *      cases:
 *
 *      (a) If 'set_type' is sst_mgmt_set, we are constructing a GCL Set 
 *          request, so we look up the attribute in there by 'node_name'.
 *
 *      (b) If 'set_type' is sst_var_temp_set, we just fetch it out of the
 *          temporary variables we have so far.  By the nature of temporary
 *          variables, we know they are all from this request.
 *
 *      (c) If 'set_type' is sst_var_sticky_set, we could just fetch it out
 *          of the sticky variable pool, but first we need to know if it has
 *          been set yet during this request.  If so, we want that attribute.
 *          But if not, we do NOT want what's there, because each request is
 *          supposed to start from a clean slate (at least for consistency
 *          with how Sets of mgmtd nodes work).  So we consult an array
 *          (ssrs_multivars_sticky_set) which tracks the sticky variables
 *          set so far during this request.  If it's in there, we can fetch
 *          the sticky variable and use its value as a starting point.
 *
 *      In any of these cases, we might produce an attribute, partly
 *      filled in so far; or we might produce a NULL, meaning this is
 *      the first variable in this SET request which is bound to this
 *      target.
 *
 *   2. Apply the new piece of information passed to us to this attribute.
 *      (Or if we had a NULL from step 1, create a new attribute that has
 *      just this piece of information).  This will produce a new attribute,
 *      which may be complete or incomplete.
 *
 *   3. Return the attribute.  Our caller will then treat it just as any
 *      other fully complete attribute they would have gotten from a
 *      normal single variable.  There are two cases:
 *
 *      (a) For a GCL Set request, we'll just append this new attribute
 *          to the request.  Because it comes later, it will overwrite
 *          the results of the earlier entry under the same name, if any.
 *          It would be slightly more elegant to delete the old entry,
 *          but the GCL has no API to do that.
 *
 *      (b) For an internal variable (whether temporary or sticky), we'll 
 *          just write over the old value.  In the case of a sticky variable,
 *          we also make a note of its name in the array mentioned in 1c
 *          (ssrs_multivars_sticky_set).
 */
int sn_snmp_native_mult_to_attrib(const u_char *snmp_val, u_char snmp_type,
                                  size_t snmp_val_len, 
                                  sn_set_request_state *req_state,
                                  bn_binding_array *sticky_vars,
                                  sn_set_type set_type,
                                  const char *node_name,
                                  bn_type btype, bn_type_flags_bf btype_flags,
                                  sn_var_flags_bf reg_flags,
                                  uint32 flags, bn_attribute_id attrib_id,
                                  bn_attrib **ret_attrib, int *ret_snmp_err);


/* ------------------------------------------------------------------------- */
/* Tells if this type falls into the 'I' category in snmp-index-types.txt,
 * i.e. those which are integer-based types whose range does not fall
 * into the range of a uint32.
 */
tbool sn_type_int_range(bn_type btype);


/* ------------------------------------------------------------------------- */
/* Tells, based on SNMP registration flags, whether this variable is 
 * part of a set of multiple variables which map to a single node.
 */
tbool sn_type_is_multvar(sn_var_flags_bf flags);


#ifdef __cplusplus
}
#endif

#endif /* __SN_TYPE_CONV_H_ */
