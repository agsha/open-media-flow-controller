/*
 *
 * sn_mod_reg.h
 *
 *
 *
 */

#ifndef __SN_MOD_REG_H_
#define __SN_MOD_REG_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "bnode.h"
#include "bnode_proto.h"
#include "node_enums.h"
#include "hash_table.h"
#include "md_client.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

/** \defgroup snmp SNMP module API */

/**
 * \file src/include/sn_mod_reg.h SNMP module registration API
 * \ingroup snmp
 */

/* ************************************************************************* */
/* ************************************************************************* */
/* *** Module interface to SNMP module registration                      *** */
/* ************************************************************************* */
/* ************************************************************************* */

/* ------------------------------------------------------------------------- */
typedef struct sn_mib    sn_mib;       /* Contains scalars, tables, notifs   */
typedef struct sn_scalar sn_scalar;    /* Standalone (non-wildcard) node     */
typedef struct sn_table  sn_table;     /* Wildcard: contains sn_fields       */
typedef struct sn_field  sn_field;     /* Field in a table                   */
typedef struct sn_notif  sn_notif;     /* Notification (trap or inform)      */

/**
 * Opaque context structure for an SNMP SET request.  Custom SET
 * handlers will be passed one of these, and it must be passed back to
 * various SNMP infrastructure APIs.
 */
typedef struct sn_set_request_state sn_set_request_state;

/**
 * Default amount of time to cache data.  (Currently only applies to
 * table data)
 */
static const lt_dur_ms sn_cache_lifetime_default_ms = 10000;
static const lt_dur_ms sn_cache_lifetime_long_default_ms = 20000;


/* ------------------------------------------------------------------------- */
/** Flags that can be applied to an sn_scalar or sn_field structure.
 *
 * NOTE: sn_var_flags_map (in sn_registration.c) must be kept in sync.
 */
typedef enum {
    svf_none = 0,

    /**
     * Applies only to variables (scalars or table fields) bound to
     * mgmtd nodes of type bt_bool.  By default, booleans are rendered
     * as INTEGER with 1 for true, and 2 for false, for compatibility
     * with the TruthValue textual convention.  Specifying this flag
     * renders them instead as UNSIGNED with 1 for true, and 0 for
     * false.  (This was the default behavior before the Dogwood
     * release of Samara, and so is still offered through this flag
     * for backward compatibility.)
     *
     * If this flag is used, the variable should be defined in the MIB
     * as "SYNTAX Unsigned32".  If not, use "SYNTAX TruthValue".
     */
    svf_bool_zero_false = 1 << 0,

    /**
     * Indicates that we should not register this variable with
     * NET-SNMP, effectively making it unavailable for query or set.
     * You might do this with a variable which you only registered
     * in order to provide instances of it in traps, but don't want
     * the user to be able to query the underlying nodes directly.
     */
    svf_hidden = 1 << 1,

    /**
     * Applies only to index table fields of variable length, mainly
     * anything that is rendered as an ASN_OCTET_STR or ASN_OBJECT_ID.
     * Normally these types are transferred into an OID prefixed with
     * a subid reflecting the number of subid components which will be
     * used for this index value (like a Pascal string).  This flag
     * requests that this preceding length value be omitted.
     *
     * This flag should only be used on indices declared with the
     * IMPLIED keyword in the MIB, since this tells clients not to
     * expect the length to be explicitly represented in the OID.
     * 
     * This may only be used on the last index of a table, since the
     * end of the index value can be detected by reaching the end of
     * the OID.  Otherwise, there would be no way to tell when this 
     * index ends and another begins.  Its usage is generally not
     * advisable even for the last index, and should only be used
     * if you have a special reason for needing it.
     *
     * If the field to which this flag is applied has an instance
     * which is the empty string, this will be an error, because there
     * will be no way to represent this index value in the OID.  Only
     * use this flag on a field whose value will never be the empty
     * string.
     */
    svf_implied = 1 << 2,

    /**
     * Assert that the values the node underlying this field will take
     * on will always fit in the range of a uint32.  This applies to
     * integral data types whose natural range extends below 0 and/or
     * beyond 2^31 - 1: int8, int16, int32, int64, uint64, date, 
     * time_*, datetime_*, duration_*.
     *
     * This flag causes these types to be rendered as ASN_UNSIGNED
     * (MIB SYNTAX type Unsigned32)
     *
     * This can be useful in two kinds of situations:
     *
     *   (a) For table indices where you don't want the value to be 
     *       rendered as ASCII, or using a synthetic table index.
     *       If the value is to be used directly, it must fit into
     *       a uint32 to guarantee we can put it into an OID subid.
     *
     *   (b) For non-index values where you simply want to be able
     *       to use Unsigned32 in the MIB.
     *
     * Note that by using this flag, you are promising that you will
     * make sure that the value will never go outside the range of 
     * a uint32.  If it does anyway, warnings will be logged.
     * If the field is a table index, the entire row will be skipped;
     * if it is a non-index, only that since field will be skipped.
     *
     * This flag is mutually exclusive with ::svf_force_string.
     */
    svf_range_uint32 = 1 << 3,

    /**
     * Request that the field be rendered as an ASCII string (SNMP
     * type ASN_OCTET_STR, MIB SYNTAX type OCTET STRING) even though
     * it is not a string data type.
     *
     * This is mostly only interesting for table indices.  If you have
     * a node with one of the fifteen integer data types mentioned
     * above under ::svf_range_uint32, and you want to use it as a table
     * index, but you cannot guarantee it will remain inside the range of
     * a uint32, your only alternative is to render it as a string.
     * (This is unfortunate, since there are certainly other conceivable
     * ways of representing these values in OIDs; but section 7.7 of 
     * RFC 2578 lists the ways of rendering different data types into
     * OIDs, and no other ways apply to these types.)
     *
     * This flag is mutually exclusive with ::svf_range_uint32.
     */
    svf_force_string = 1 << 4,

    /**
     * Request that the field be rendered into an ASN_OCTET_STR in a
     * binary format suitable for use with the DateAndTime textual
     * convention.  This is only suitable for use with fields whose
     * underlying mgmtd data types is bt_date, bt_time_*, or
     * bt_datetime_*.  In the latter case, it is not necessary because
     * this is the default.  It is mainly to be used with the former
     * two cases, which by default are rendered into an ASN_OCTET_STR
     * with an ASCII representation of the date or time of day.
     *
     * NOTE: this flag cannot be used on index nodes, since DateAndTime
     * is not a suitable data type for an index.  If one of these data
     * types is to be used as an index, you must instead use either
     * ::svf_range_uint32 or ::svf_force_string.
     *
     * NOTE: this flag is mutually exclusive with ::svf_range_uint32
     * and ::svf_force_string, since those request different rendering
     * formats.
     */
    svf_date_and_time = 1 << 5,

    /**
     * Request that the field be rendered as an ASN_COUNTER64.  This
     * can only be used with a field whose underlying mgmtd node type
     * is bt_uint64, and cannot be used with index fields.  It should
     * only be used if the node actually represents a counter,
     * i.e. monotonically increasing (except in the case of a wrap).
     * If this value ever declines in value, other than wrapping
     * around back to zero, it may confuse SNMP clients.
     */
    svf_counter64 = 1 << 6,

    /**
     * Applicable only to variables with underlying mgmtd data types
     * of bt_duration_*.  By default, these types are rendered as 
     * TimeTicks.  If ::svf_force_string is used, they are rendered
     * to an ASCII representation of the raw number (of seconds,
     * milliseconds, or microseconds).  This flag requests that 
     * they be rendered as a "fancy" string, using
     * lt_dur_ms_to_counter_str_ex2().
     *
     * Any applicable flags in the ss_time_render_flags (for scalars)
     * or the sf_time_render_flags (for table fields) are passed to
     * this function.
     *
     * Note that due to the nature of lt_dur_ms_to_counter_str_ex2(),
     * sub-millisecond precision (from bt_duration_us) is dropped.
     *
     * This flag does not require ::svf_force_string, and supercedes
     * it if both are specified.
     *
     * Note that if this is used on a read-write variable, then we
     * accept any string which could have been produced by
     * lt_dur_ms_to_counter_str_ex2() with any combination of option
     * flags.
     */
    svf_render_duration = 1 << 7,

    /**
     * This flag applies to nodes of any of the following types:
     *   - ::bt_ipv4addr
     *   - ::bt_ipv4prefix
     *   - ::bt_ipv6addr
     *   - ::bt_ipv6addrz
     *   - ::bt_ipv6prefix
     *   - ::bt_inetaddr
     *   - ::bt_inetaddrz
     *   - ::bt_inetprefix
     *
     * Nodes of these types each need at least two SNMP variables to
     * represent the value of a single node, as described in
     * INET-ADDRESS-MIB.txt: (a) an InetAddressType, (b) an
     * InetAddress, and (c) only for the 'prefix' types, an
     * InetAddressPrefixLength.
     *
     * If no flag is specified, (b) is served.  This flag requests (a).
     * The ::svf_inet_address_prefix_length flag requests (c).
     *
     * Note that this flag may ALSO be used on nodes of any other data
     * type whose string representation may be a valid instance of
     * those types listed above.  For example, there are some cases of
     * nodes of type bt_hostname whose validation logic requires that
     * they only contain IP addresses.  This flag may be used to
     * render such nodes, and as long as the string can be converted
     * back to an address or prefix of some sort, the rendering to an
     * SNMP variable will work.
     *
     * Note that for the bt_ipv4* and bt_ipv6* data types, this flag 
     * will cause the variable to always return the same value, since
     * the answer is inherent in the node data type.  However, for the
     * bt_inet* types, the value will depend on the contents of the node.
     *
     * If you have an SNMP table using a set of these as indices
     * (mapping to nodes with any of those address types as a wildcard),
     * the indices must come in the order they are listed here:
     * address type, then address, then address prefix length if applicable.
     */
    svf_inet_address_type = 1 << 8,

    /**
     * Please see the comment for ::svf_inet_address_type.  This flag
     * applies to the same set of data types, and requests that item 
     * (b) (the InetAddress portion of the node) be returned.
     */
    svf_inet_address = 1 << 9,

    /**
     * This flag applies to nodes of any of the following types:
     *   - ::bt_ipv4prefix
     *   - ::bt_ipv6prefix
     *   - ::bt_inetprefix
     *
     * Nodes of these types each need three SNMP variables to
     * represent the value of a single node, as described in
     * INET-ADDRESS-MIB.txt: (a) an InetAddressType, (b) an
     * InetAddress, and (c) an InetAddressPrefixLength.
     *
     * If no flag is specified, (b) is served.  This flag requests (c).
     * The ::svf_inet_address_type flag requests (a), and 
     * the ::svf_inet_address flag requests (b).
     *
     * Note that as described under ::svf_inet_address_type, any other
     * data type whose string representation can be converted back to
     * one of the approved types may be used.
     */
    svf_inet_address_prefix_length = 1 << 10,

    /**
     * This flag applies to nodes of the following types:
     *   - ::bt_inetaddr
     *   - ::bt_inetaddrz
     *
     * Nodes of this type are rendered as an ASCII OCTET STRING by
     * default.  This flag overrides and requests that they be rendered
     * as an IpAddress, but only if the address is an IPv4 address
     * (family is AF_INET).  Note that this does NOT apply to IPv6
     * addresses which happen to be IPv4-mapped.  If this flag is used
     * alone, any addresses with family AF_INET6 will not be rendered
     * at all.  See also ::svf_force_ipv4_if_v4mapped flag.
     *
     * In the case of a ::bt_inetaddrz binding, the zoneid (if any) 
     * is ignored.
     */
    svf_only_if_ipv4 = 1 << 11,

    /**
     * This flag applies to nodes of the following types:
     *   - ::bt_inetaddr
     *   - ::bt_inetaddrz
     *   - ::bt_ipv6addr
     *   - ::bt_ipv6addrz
     *
     * Nodes of these types are rendered as an ASCII OCTET STRING by
     * default.  This flag overrides and requests that they be
     * rendered as an IpAddress instead, but only if the addresses is
     * an IPv4-mapped address.
     *
     * In the case of ::bt_inetaddr and ::bt_inetaddrz, this flag 
     * MUST be used in conjunction with ::svf_only_if_ipv4, with the
     * result that any address that can be rendered as an IPv4 address
     * (because that's what it is natively, or because it was mapped)
     * will be.
     *
     * In the case of ::bt_ipv6addr and ::bt_ipv6addrz, this flag may
     * be used alone.  The ::svf_only_if_ipv4 flag is not relevant to
     * those types.
     *
     * In the case of a ::bt_inetaddrz or ::bt_ipv6addrz binding, any
     * zoneid is ignored.
     */
    svf_force_ipv4_if_v4mapped = 1 << 12,

    /**
     * Declare that this index is an "expansion index", meaning one of 
     * the indices in an "expansion table" which is borrowed from another
     * table.
     *
     * An index like this is declared normally (and without using this
     * flag) in the base table.  In the expansion table, it uses this 
     * flag, and omits st_rel_oid and st_rel_oid_len, since it is not
     * actually a field of this table, and has no OID relative to the
     * root of the table.  All other fields have to be filled in as
     * usual, though, including sf_index, sf_source, sf_wildcard_index,
     * and sf_mgmt_type.
     */
    svf_expansion_index = svf_hidden,

} sn_var_flags;

/** Bit field of ::sn_var_flags ORed together */
typedef uint32 sn_var_flags_bf;

extern const lc_enum_string_map sn_var_flags_map[];


/* ========================================================================= */
/** @name Set request handling
 *
 * SNMP SET handling is off by default for every registered variable.
 * To enable it:
 *
 *   - For a scalar variable, call sn_scalar_enable_sets() and then fill out
 *     the fields in the ss_set_params member of the registration structure.
 *
 *   - For a columnar variable, call sn_field_enable_sets() and then fill out
 *     the fields in the sf_set_params member of the registration structure.
 */
/*@{*/

/* ------------------------------------------------------------------------- */
/** Specify how a Set request for a scalar or columnar variable is to
 * be treated.
 */
typedef enum {
    sst_none = 0,
    
    /**
     * Set of this variable is translated into a mgmtd set request in
     * a data-driven way, as specified by other registration fields.
     *
     * NOTE: on columnar variables (fields in a table), if the table
     * has multiple indices, this value may not be used on any index
     * variables except for the last one.  That is, if there is going
     * to be set handling at all on those variables, it must be 
     * custom (::sst_custom).
     */
    sst_mgmt_set,
    
    /**
     * (Only applicable to scalar variables)  Set of this variable is
     * translated into a mgmtd action request in a data-driven way, as
     * specified by other registration fields.
     *
     * Note: the data type registered for this field in the MIB and
     * the module, as well as the value provided in the SET request at
     * runtime are usually unimportant.  Any SET of this variable will
     * trigger the registered action handling, regardless of the value.
     * The value will be completely ignored unless any of the parameters
     * is registered with ::ssbpt_self, in which case the value will be
     * used as the source for one of the binding parameters.
     */
    sst_mgmt_action,
    
    /**
     * (Only applicable to scalar variables)  Set of this variable is
     * translated into a mgmtd event request in a data-driven way, as
     * specified by other registration fields.
     *
     * Note: the data type registered for this field in the MIB and
     * the module, as well as the value provided in the SET request at
     * runtime are usually unimportant.  Any SET of this variable will
     * trigger the registered event handling, regardless of the value.
     * The value will be completely ignored unless any of the parameters
     * is registered with ::ssbpt_self, in which case the value will be
     * used as the source for one of the binding parameters.
     */
    sst_mgmt_event,
    
    /**
     * (Only applicable to scalar variables)  Set of this variable
     * stores the provided value into a temporary internal state
     * variable in the SNMP agent's memory.  This can be referenced by
     * Sets of other SNMP variables in the same Set request; the
     * temporary internal variables are cleared out between each
     * request.
     */
    sst_var_temp_set,
    
    /**
     * (Only applicable to scalar variables)  Set of this variable
     * stores the provided value into a sticky internal state variable
     * in the SNMP agent's memory.  This can be referenced by Sets of
     * other SNMP variables in the same or subsequent Set requests.
     * Sticky internal variables are not automatically cleared out
     * between requests, though neither are they stored persistently,
     * so they only live as long as the SNMP agent process.
     *
     * Note that all sticky variables are accessible from all
     * requests, regardless of the capability level with which the
     * accessing request, or the request which originally set it,
     * ran at.  So these should only be used for public data.
     * Temporary variables are available for data which should not
     * be accessible to other accounts.
     */
    sst_var_sticky_set,

    /**
     * (Only applicable to scalar variables)  Set of this variable
     * clears a sticky internal state variable from the SNMP agent's
     * memory.  The type and value being set are unimportant.
     */
    sst_var_sticky_clear,
    
    /**
     * Set of this variable calls a callback routine, of type
     * ::sn_custom_set_callback, provided by registration, each time
     * we are called by NET-SNMP.  It is up to your callback to handle
     * the Set (and each phase for which you are called) in a manner
     * consistent with how the infrastructure does.
     *
     * NOTE: your callback will usually be called more than once 
     * per SET request.  See the documentation on the
     * ::sn_custom_set_callback typedef for details.
     *
     * NOTE: if a variable has custom GET handling, it must have
     * custom SET handling, and vice-versa.
     *
     * NOTE: this is the only way for an SNMP SET handler to be 
     * asynchronous.  Your custom SET callback would call
     * sn_send_mgmt_msg_async_takeover() to go async.
     */
    sst_custom,
    
} sn_set_type;


typedef struct sn_custom_set_response {

    /**
     * This field defaults to 0 (success).  It may ONLY be set by a
     * ::sn_custom_set_callback or a ::sn_set_custom_param_callback.
     * It MAY NOT be set by a ::sn_async_msg_callback.  Setting it to
     * a nonzero value will halt the SNMP SET request with failure.
     * 
     * This takes an SNMP-specific return code (SNMP_ERR_*) from
     * src/bin/snmp/net-snmp/include/net-snmp/library/snmp.h.
     * DO NOT use Samara-specific return codes from errors.h.
     *
     * The most common ones to use are:
     *
     *   - SNMP_ERR_BADVALUE: one of your parameters did not validate.
     *
     *   - SNMP_ERR_INCONSISTENTVALUE: one of your parameters is 
     *     inconsistent with one of your other parameters.
     *
     *   - SNMP_ERR_NOACCESS: the user does not have permissions to perform
     *     this operation.
     *
     *   - SNMP_ERR_GENERR or SNMP_ERR_COMMITFAILED: generic failure.
     *
     * Note that SNMP_ERR_TOOBIG is probably NOT appropriate -- it
     * means the response is too big to send, not that the value in
     * the request was longer than some length limit.
     */
    int scsr_snmp_err;

    /**
     * GCL response message received from the completion of this
     * callback.
     *
     * NOTE: if you are a custom response handler (sn_async_msg_callback),
     * the response structure passed to you already has this field
     * pre-populated with the GCL response received.  Normally, you
     * should leave it here.  If you wish to stifle this response,
     * and deal with it yourself, you must explicitly remove it from
     * this field (and free it when you're done).
     *
     * NOTE: even if you send multiple requests, you may only post a
     * single response here.  If you return a response here, and have
     * already returned one in a previous call during the same SET,
     * this one will replace the previous one.
     */
    bn_response *scsr_gcl_resp;

    /**
     * Response code to report for this request.  This is mutually
     * exclusive with scsr_gcl_resp: either set it, or set this
     * (and optionally also scsr_result_msg).  This is provided as a 
     * simplified response reporting mechanism for cases where you
     * do not have a GCL response message, and simply want to report
     * a code and/or message of your own.
     */
    uint32 scsr_result_code;

    /**
     * Response message to report for this request.  This goes together
     * with scsr_result_code; both of these fields are mutually exclusive
     * with scsr_gcl_resp.
     */
    tstring *scsr_result_msg;

} sn_custom_set_response;


/* ------------------------------------------------------------------------- */
/** Custom callback to be called to handle an SNMP Set request, when
 * its ssp_set_type is ::sst_custom.  This can be used for either a
 * scalar or a table field.  Exactly one of the first or second
 * parameters will be set accordingly, and the other will be NULL.
 *
 * NOTE: this callback will usually be called more than once per set
 * request, with a different value for the 'snmp_set_phase' parameter,
 * described below.
 *
 * This callback may:
 *   - Use the md_client API to send synchronous requests.  Use sn_get_mcc()
 *     to send Set, Action, and Event requests.  Either that or 
 *     sn_get_admin_mcc() may be used for Query requests -- however, note
 *     that query enforcement may not be done by mgmtd.  Under both 
 *     Capabilities and ACLs, query enforcement must be explicitly enabled
 *     by the developer.
 *   - Call sn_intvar_set() and/or sn_intvar_get() to set or query 
 *     internal state variables.
 *   - Call sn_send_mgmt_msg_async_takeover() to send an asynchronous
 *     request.  NOTE: after you do this, you may no longer use the
 *     md_client API to send synchronous requests from this call.
 *
 * \param scalar If we were called by way of a scalar variable, the 
 * registration structure for that variable; otherwise NULL;
 * 
 * \param field If we were called by way of a columnar (table field)
 * variable, the registration structure for that variable; otherwise
 * NULL.
 *
 * \param snmp_set_phase The "phase" this SNMP SET request is in.
 * (What NET-SNMP calls the "action") See the Samara Technical Guide
 * section on SNMP SETs for details on what each phase means, and what
 * your function is expected to do.  Generally, you should do everything
 * in ACTION and ignore the other phases -- stick to this unless you know
 * exactly what you're doing!  NOTE: your req_state and ret_resp
 * parameters (see below) will be NULL for the phases COMMIT, FREE,
 * and UNDO.  This means that during those phases, you will not be
 * able to call any APIs which expect an 'sn_set_request_state *'
 * as a parameter, and you will not be able to report any results.
 * 
 * \param set_oid The OID in the Set request which was mapped to the
 * variable which registered this callback.
 *
 * \param set_oid_len The length of the OID in set_oid, in subids.
 *
 * \param set_val A buffer containing the SNMP native representation
 * of the value provided in the Set request.  The format of this
 * buffer is dependent on set_val_type.
 *
 * \param set_val_type The SNMP data type of the data in set_val,
 * such as ASN_UNSIGNED or ASN_OCTET_STR.
 *
 * \param set_val_len The size of the buffer pointed to be set_val, 
 * in bytes.
 *
 * \param data The value from the ssp_custom_data field in the
 * sn_set_params struct which pointed at this callback.
 *
 * \param req_state Opaque structure used by the infrastructure to
 * hold context information about this request.  Certain APIs need one
 * of these passed to them.  Beyond using it for those, you may ignore it.
 * NOTE: this will be NULL in the COMMIT, FREE, and UNDO phases.
 *
 * \retval ret_resp Structure to whose fields you can assign
 * responses.  See structure definition for details.
 * NOTE: this will be NULL in the COMMIT, FREE, and UNDO phases, and
 * no response reporting is possible in those phases.
 */
typedef int (*sn_custom_set_callback)(const sn_scalar *scalar,
                                      const sn_field *field,
                                      int snmp_set_phase,
                                      const oid *set_oid,
                                      size_t set_oid_len,
                                      const uint8 *set_val,
                                      int set_val_type,
                                      size_t set_val_len,
                                      void *data, 
                                      sn_set_request_state *req_state,
                                      sn_custom_set_response *ret_resp);


/* ------------------------------------------------------------------------- */
/** Manner in which to construct one or more binding parameters to be
 * included in an action or event triggered by an SNMP set of a scalar
 * variable.
 * 
 */
typedef enum {
    ssbpt_none = 0,

    /**
     * Add a single binding, with a value from a temporary internal
     * state variable, which was set (earlier in the same Set request)
     * via an SNMP variable registered with ::sst_var_temp_set.
     */
    ssbpt_var_temp,

    /**
     * Add a single binding, with a value from a sticky internal state
     * variable, which was set (in the same or an earlier SNMP Set
     * request) via an SNMP variable registered with ::sst_var_sticky_set.
     */
    ssbpt_var_sticky,

    /**
     * Add a single binding, containing the result of querying a 
     * mgmtd node.
     */
    ssbpt_query_node,

    /**
     * Add a single binding, with a constant type and value which are
     * specified in the registration structure.
     */
    ssbpt_constant,

    /**
     * Add a single binding, with a value derived from the value set
     * into the variable which invoked the action.
     *
     * For an action or event invoked by an SNMP SET of a scalar
     * variable, there may be no more than one binding parameter using
     * this type.  Most commonly there will be zero, and so the value
     * set onto the scalar invoking the action or event is ignored;
     * and any parameters to that action/event (bindings to be
     * included) come from other sources.  This option offers a more
     * compact notation, so that an action with only one parameter can
     * be invoked with a SET of only a single variable, rather than
     * with two.
     */
    ssbpt_self,

    /**
     * Call a custom callback, of type ::sn_set_custom_param_callback,
     * which may add zero or more bindings of its own creation.  The
     * callback is free to query mgmtd nodes, look up values of SNMP
     * temporary or sticky internal state variables, etc., as it
     * chooses.  The callback is set into the ssbp_custom_func 
     * registration field.
     */
    ssbpt_custom,
    
} sn_set_binding_param_type;


/* ------------------------------------------------------------------------- */
/** Custom callback to add parameters to an action or event message
 * which is to be sent as a result of an SNMP SET request against a
 * scalar variable.  This is called when the action contains an
 * sn_set_binding_param whose ssbp_type field is set to ::ssbpt_custom.
 *
 * This callback may:
 *   - Use the md_client API to send synchronous requests.  Use sn_get_mcc()
 *     to send Set, Action, and Event requests.  Either that or 
 *     sn_get_admin_mcc() may be used for Query requests -- however, note
 *     that query enforcement may not be done by mgmtd.  Under both 
 *     Capabilities and ACLs, query enforcement must be explicitly enabled
 *     by the developer.
 *   - Call sn_intvar_set() and/or sn_intvar_get() to set or query 
 *     internal state variables.
 *
 * This callback MUST NOT call sn_send_mgmt_msg_async_takeover().
 *
 * \param scalar The scalar variable registration structure which
 * pointed at this callback.
 *
 * \param set_oid The OID in the Set request which was mapped to the 
 * scalar whose registration pointed to this callback.
 *
 * \param set_oid_len The length of the OID in set_oid, in subids.
 *
 * \param data The value from the ssbp_custom_data field in the
 * sn_set_binding_param struct which pointed at this callback.
 *
 * \param req_state Opaque structure used by the infrastructure to
 * hold context information about this request.  Certain APIs need one
 * of these passed to them.  Beyond using it for those, you may ignore it.
 *
 * \retval ret_bindings A binding array, pre-allocated by the caller,
 * to which this function should append bindings it wants to include
 * in the action or event request.  It may use
 * bn_binding_array_append_takeover(), or other related APIs for this.
 *
 * \retval ret_resp Structure to whose fields you can assign
 * responses.  See structure definition for details.
 *
 * \return Zero for success, nonzero for failure.  Note that a nonzero
 * value here means an internal implementation error, which should
 * never happen.  If you want to report an error processing the request,
 * which is due to a problem with the request itself or the general
 * circumstances, return via ret_resp.
 */
typedef int (*sn_set_custom_param_callback)(const sn_scalar *scalar,
                                            const oid *set_oid,
                                            size_t set_oid_len,
                                            void *data, 
                                            sn_set_request_state *req_state,
                                            bn_binding_array *ret_bindings,
                                            sn_custom_set_response *ret_resp);


/* ------------------------------------------------------------------------- */
/** Details on how to construct a single binding parameter to be
 * included in an action or event triggered by an SNMP set of a scalar
 * variable.
 */
typedef struct sn_set_binding_param {

    /**
     * (Relevant for all parameters)  General manner in which to
     * construct the binding.  What is selected here will determine
     * which other fields of this struct are relevant for this
     * binding.
     */
    sn_set_binding_param_type ssbp_type;

    /**
     * (Relevant for all parameters)  Is this parameter mandatory?
     * If so, we'll return an general error if we cannot find it
     * (i.e. if the variable referenced is not defined, or if the 
     * node name referenced cannot be queried).  If not, we'll 
     * silently omit it from the request.  Hopefully in the latter
     * case, it's actually optional in the underlying action or 
     * event, since otherwise it will be an internal error.
     *
     * By default, parameters are required, and must be marked 'false'
     * if they are not.
     *
     * Note that this has no meaning for ::ssbpt_custom or
     * ::ssbpt_constant, which cannot not be found.
     */
    tbool ssbp_required;

    /**
     * (Relevant for all parameters, but optional)  Friendly description
     * for this parameter.  If it is required, but cannot be queried, this
     * is how we'll describe it in the log message.  Otherwise, we'll just
     * use whatever we have, which may be jargony (the binding name of 
     * the variable or node we tried to query).
     *
     * In the case of an internal variable, it may be most helpful if
     * this matched the name of the SNMP variable which is used to set
     * the parameter (i.e. the ss_snmp_var_name of the scalar
     * registration which used sst_var_temp_set or sst_var_sticky_set).
     */
    const char *ssbp_friendly_name;

    /**
     * (Relevant for parameters with ssbp_type set to
     * ::ssbpt_var_temp, ::ssbpt_var_sticky, ::ssbpt_query_node,
     * ::ssbpt_self, or ::ssbpt_constant)  Binding name to give to this
     * parameter.  Except for ::ssbpt_self and ::ssbpt_constant (for which
     * it is mandatory), leave as NULL to keep the name it had before (i.e.
     * the name from ssbp_var_name or ssbp_query_node_bname by which it was 
     * originally fetched).
     */
    const char *ssbp_param_bname;

    /**
     * (Relevant for parameters with ssbp_type set to ::ssbpt_var_temp
     * or ::ssbpt_var_sticky)  Name of internal state variable from
     * which the value of this parameter should be derived.  Note that
     * the name of the binding parameter in the action request will be
     * taken from ssbp_param_bname.
     */
    const char *ssbp_var_name;

    /**
     * (Relevant for parameters with ssbp_type set to
     * ::ssbpt_query_node)  Name of mgmtd binding to be queried to
     * obtain value of this parameter.  Note that the name of the
     * binding parameter in the action request will be taken from
     * ssbp_param_bname, so it need not be the same as this one.
     */
    const char *ssbp_query_node_bname;

    /**
     * (Relevant for parameters with ssbp_type set to ::ssbpt_constant
     * or ::ssbpt_self).  Data type of the binding to be created for
     * this parameter.
     */
    bn_type ssbp_btype;

    /**
     * (Relevant for parameters with ssbp_type set to
     * ::ssbpt_constant).  String representation of the value to
     * be used for this parameter.
     */
    const char *ssbp_constant_str;

    /**
     * (Relevant for parameters with ssbp_type set to ::ssbpt_custom)
     * Callback to be called to construct any number of bindings
     * (zero or more) to be included in the event.
     */
    sn_set_custom_param_callback ssbp_custom_func;

    /**
     * (Relevant for parameters with ssbp_type set to ::ssbpt_custom)
     * Data to be passed to ssbp_custom_func when it is called.
     */
    void *ssbp_custom_data;

} sn_set_binding_param;


/* ------------------------------------------------------------------------- */
/** Resistration structure to specify how a Set request for an SNMP
 * variable is to be handled.  This structure can be used on an
 * sn_scalar or sn_field registration.  Note that callers should not
 * allocate this structure themselves, but rather call
 * sn_scalar_enable_sets() or sn_field_enable_sets() to allocate one
 * for them.
 */
typedef struct sn_set_params {

    /**
     * General type of handling to be done for Set requests.  The value
     * in this field deteremines which other fields below are relevant.
     */
    sn_set_type ssp_set_type;

    /**
     * If ssp_set_type is ::sst_var_temp_set, ::sst_var_sticky_set, or
     * ::sst_var_sticky_clear, then this field holds the name of the
     * internal state variable which will be set or cleared when this
     * SNMP variable is set.
     *
     * Internal state variables are held in bn_binding data
     * structures, so this name is a binding name.  It may be absolute
     * (beginning with '/') or relative.  Whether the internal state
     * variable is temporary or sticky, it is held only in the SNMP
     * agent's memory, and does not share the namespace with the mgmtd
     * node hierarchy.
     */
    const char *ssp_var_name;

    /**
     * If ssp_set_type is ::sst_var_temp_set or ::sst_var_sticky_set,
     * this field tells which bnode data type to convert the data to
     * for storage in the internal state variable.  Note that this
     * field is not relevant to ::sst_var_sticky_clear, because the
     * type and value of the variable are unimportant in the case
     * of a clear.
     */
    bn_type ssp_var_btype;

    /**
     * If ssp_set_type is ::sst_mgmt_action or ::sst_mgmt_event, this
     * contains the name of the action or event to be invoked when
     * this variable is Set.  The parameters to the action or event
     * will be derived from ::ssp_binding_params.
     */
    const char *ssp_action_event_name;

    /**
     * If ssp_set_type is ::sst_mgmt_action or ::sst_mgmt_event, this
     * contains an array of pointers to structures, each of which
     * specifies one binding parameter to add to the action or event
     * before it is sent.
     *
     * The call to sn_scalar_enable_sets() allocates this array, and
     * the structuers it points to, so all yuo have to do is fill out
     * each of the structures.  However if num_binding_params is zero,
     * or if you called sn_field_enable_sets() for a table field
     * (which cannot take parameters like this), this will be NULL.
     */
    sn_set_binding_param **ssp_binding_params;

    /**
     * The number of array slots in the array pointed to by
     * ssp_binding_params, if any.  If that pointer is NULL, this
     * number will be zero.
     */
    int ssp_num_binding_params;

    /**
     * If ssp_set_type is ::sst_custom, this function will be called
     * to handle the Set request.  Note that it will potentially be
     * called multiple times per Set request, according to NET-SNMP's
     * API; see function type's comments for more detail.
     */
    sn_custom_set_callback ssp_custom_func;

    /**
     * If ssp_set_type is ::sst_custom, the contents of this field will
     * be passed to ssp_custom_func whenever it is called.
     */
    void *ssp_custom_data;

} sn_set_params;


/**
 * The name of the temporary variable to be set with the Owner
 * to be used for tracking the status and outcome of the SET request.
 */
#define SN_SETREQ_VAR_OWNER "/tms/snmp/request/owner"
extern const char sn_setreq_var_owner[];

/**
 * The name of the temporary variable to be set with the Request ID
 * to be used for tracking the status and outcome of the SET request.
 */
#define SN_SETREQ_VAR_REQID "/tms/snmp/request/reqid"
extern const char sn_setreq_var_reqid[];


/* ------------------------------------------------------------------------- */
/** Enable SNMP Set handling on a scalar variable, and allocate
 * registration structure to specify Set handling parameters.
 * The new sn_set_params structure is allocated, its ssp_set_type field
 * is set, and the structure is assigned to the ss_set_params field in 
 * the sn_scalar structure provided.  The caller should then fill out 
 * the remaining fields of this new structure as desired.
 *
 * \param scalar The scalar variable registration for which to enable
 * Set handling.  Note that this function should be called before the 
 * scalar is registered with sn_reg_scalar_register().
 *
 * \param set_type The general type of handling to be done for a Set
 * request on this variable.  Further details on how Sets are to be
 * handled are specified in the ss_set_params structure created within
 * the scalar.
 *
 * \param num_binding_params If set_type is ::sst_mgmt_action or
 * ::sst_mgmt_event, this number tells how many slots to allocate for
 * sn_set_binding_param registration structures in the
 * ss_set_params->ssp_binding_params array.  The caller may then fill
 * in these slots with parameter registrations.  If set_type is not
 * either of these two options, this parameter must be zero.
 */
int sn_scalar_enable_sets(sn_scalar *scalar, sn_set_type set_type,
                         int num_binding_params);

/* ------------------------------------------------------------------------- */
/** Enable SNMP Set handling on a columnar variable (represented by an
 * sn_field registration structure).  The new sn_set_params structure
 * is allocated and assigned to the sf_set_params field in the sn_field
 * structure provided; the caller should then fill out the fields of
 * this new structure as desired.
 *
 * NOTE: in a table with multiple indices, indices other than the last
 * one may NOT be enabled for SET handling unless custom handling is
 * specified (set_type is sst_custom).
 *
 * \param field The table field (columnar variable) for which to enable
 * Set handling.  Note that this function should be called before the
 * field is registered with sn_reg_field_register().
 *
 * \param set_type The general type of handling to be done for a Set
 * request on this variable.  Note that only certain types are applicable
 * to field variables: sst_mgmt_set and sst_custom.
 */
int sn_field_enable_sets(sn_field *field, sn_set_type set_type);


/* ------------------------------------------------------------------------- */
/** Set or clear a temporary or sticky internal variable during the 
 * handling of an SNMP SET request.
 *
 * The following temporary variables are reserved for special purposes:
 *   - "/tms/snmp/request/owner" (string): the Owner for this request.
 *   - "/tms/snmp/request/reqid" (string): the ReqID for this request.
 * While the request is running, and for some amount of time after it
 * is completed, the the SNMP infrastructure uses the contents of these
 * two variables to uniquely identify this request, for purposes of 
 * reporting the status and results of the request.  See the
 * "Response Reporting" section of snmp-sets.txt for details on how
 * these are used.
 *
 * IMPORTANT NOTE: SNMP SET requests which have gone asynchronous by
 * calling sn_send_mgmt_msg_async_takeover() MUST NOT query or set
 * sticky internal variables after they return from their initial
 * custom request handler, i.e. from their response handler callbacks.
 * Doing so would make them vulnerable to interference from subsequent
 * SNMP requests.  If you need access to sticky variables later on, it
 * is recommended that you copy their values into temporary variables.
 * The temporary variable space is unique to each request, whereas
 * the sticly variable space is shared by all requests.
 *
 * \param req_state The opaque request state structure passed to your
 * SNMP SET handler.
 *
 * \param sticky Is the variable to set temporary (false) or sticky
 * (true)?  Temporary and sticky internal state variables have 
 * separate namespaces.  
 *
 * \param name The name of the variable to set, within the namespace
 * determined by the 'sticky' parameter.
 *
 * \param attrib An attribute representing the value of the parameter
 * to be set, NULL to clear the variable.  If the variable is cleared,
 * it is removed from the namespace, as if it had never been added.
 * If an attribute is provided here, the infrastructure will make its
 * own copy of this attribute, without disturbing the caller's copy.
 */
int sn_intvar_set(sn_set_request_state *req_state, tbool sticky,
                  const char *name, const bn_attrib *attrib);


/* ------------------------------------------------------------------------- */
/** Look up a temporary or sticky internal state variable.
 *
 * \param sticky Is the variable to query temporary (false) or sticky
 * (true)?  Note that temporary and sticky internal state variables
 * have separate namespaces.
 *
 * \param req_state If querying a temporary variable, this must be the
 * the opaque request state structure passed to your SNMP SET handler,
 * as it only makes sense to query temporary variables during a SET.
 * If querying a sticky variable, this parameter is not needed, and
 * NULL may be passed (though passing req_state will not cause any
 * harm).
 *
 * \param name Name of the variable to look for.
 *
 * \retval ret_attrib A copy of the attribute representing the internal
 * state variable, if found.  Note that the caller assumes memory ownership
 * of this copy, and should free it when finished.  Changing this attribute
 * will NOT change the underlying variable; use sn_intvar_set() to do so.
 * If the variable is not found, NULL is returned with no error.
 */
int sn_intvar_get(sn_set_request_state *req_state, tbool sticky, 
                  const char *name, bn_attrib **ret_attrib);


/* ------------------------------------------------------------------------- */
/** Convenience wrapper around sn_intvar_get() to retrieve an internal
 * variable as a string representation.  This is basically just a call
 * to sn_intvar_get() and bn_attrib_get_str().
 */
int sn_intvar_get_str(sn_set_request_state *req_state, tbool sticky,
                      const char *name, tstring **ret_var_ts);


/* ------------------------------------------------------------------------- */
/** Convenience wrapper to copy the attribute in an internal variable 
 * into an action request message.
 */
int sn_intvar_copy_to_action(sn_set_request_state *req_state, tbool sticky,
                             const char *intvar_name, bn_request *req,
                             const char *bname);


/* ------------------------------------------------------------------------- */
/** Get the md_client_context pointer to use for synchronous Set,
 * Action, and Event requests to mgmtd.  Can also be used for Query
 * requests, though note that query capabilities/ACLs are NOT enforced,
 * so this is essentially like doing the query as Admin.  (Note that if
 * you are doing a query from a GET handler, you'll need to use 
 * sn_get_admin_mcc(), since you have no sn_set_request_state.)
 *
 * NOTE: if you want to send a request asynchronously, use
 * sn_send_mgmt_msg_async_takeover() instead.  The md_client API
 * is inherently synchronous.
 *
 * NOTE: a custom SET handler MUST NOT send any synchronous mgmtd
 * requests (and thus has no reason to call this function) after it has
 * sent an asynchronous message using sn_send_mgmt_msg_async_takeover().
 *
 * Any custom SNMP module code which wants to send a request to mgmtd
 * synchronously should do it using the pointer returned by this function.
 * Note that snmpd maintains multiple different GCL sessions (so requests
 * can run at the appropriate capability level, given how the request was 
 * authenticated), and this call chooses the right one.
 */
md_client_context *sn_get_mcc(sn_set_request_state *req_state);


/* ------------------------------------------------------------------------- */
/** Get the md_client_context pointer to use for Query requests.  This can
 * be used from either SNMP GET or SET handlers.
 *
 * NOTE: a custom SET handler MUST NOT send any synchronous mgmtd
 * requests (and thus has no reason to call this function) after it has
 * sent an asynchronous message using sn_send_mgmt_msg_async_takeover().
 */
md_client_context *sn_get_admin_mcc(void);


/* ------------------------------------------------------------------------- */
/** Render an SNMP SET phase/action number into a string, mainly for
 * logging purposes.
 */
const char *sn_set_phase_to_str(int phase);


/* ------------------------------------------------------------------------- */
/** Callback to be called when an asynchronous mgmtd transaction 
 * (initiated using sn_send_mgmt_msg_async_takeover()) has completed, 
 * whether successfully or not.
 *
 * Asynchronous mgmtd messages can be chained, so if you are not done
 * processing the request, you may call sn_send_mgmt_msg_async_takeover()
 * again.  The contents of the ::sn_custom_set_response structure passed
 * to you in set_resp will not be heeded yet, but they will be preserved
 * until you are ready to call the request complete.  If you are done, 
 * simply finish filling out and return without sending another async 
 * message.  At this point, the request will be considered fully handled,
 * and your response will be considered final.
 *
 * NOTE: although it may appear to succeed, your response handler MUST NOT
 * do either of the following:
 *
 *   - Send any synchronous mgmtd requests using the md_client API.
 *     Once you have gone async, all of your remaining interactions with
 *     mgmtd must also be asynchronous.
 *
 *   - Query or set any sticky internal variables.  Once you have gone
 *     async, it is no longer safe to access sticky variables, since 
 *     this variable space is shared between all requests, and their
 *     values may be altered by other requests that happened in the
 *     mean time.  Accessing temporary variables remains safe, so if
 *     there is a value in a sticky variable you need, you may wish to
 *     transfer it to a temporary variable before returning from the
 *     custom handler from which you are sending the first async request.
 *
 * \param result A response code from errors.h.  This will be 0 on success,
 * or something else on error.  (If we had timeouts, it could be 
 * lc_err_io_timeout, but we do not yet, see bug 14261.)
 *
 * \param set_resp This is an "in/out" parameter here.  Its scsr_gcl_resp
 * field is prepopulated with the GCL response message which was received
 * (and which triggered you to be called).  It also provides a means for
 * you to return result information to the infrastructure.  Usually, the
 * result is fully expressed by this GCL response, so you can just leave it.
 * However, you may remove it (make sure to free it if you do), and use
 * the other fields like scsr_result_code ad scsr_result_msg instead.
 * NOTE: if 'result' is nonzero, this could be NULL.
 *
 * \param code For your convenience, this (redundantly) contains the error
 * response code extracted from set_resp->scsr_gcl_resp.  It will be 0
 * for success, and nonzero for failure.  Often you will want to bail out
 * on error, and proceed with any other processing on success.
 *
 * \param req_state Opaque structure used by the infrastructure to
 * hold context information about this request.  Certain APIs need one
 * of these passed to them.  Beyond using it for those, you may ignore it.
 *
 * \param data The contents of the 'data' parameter provided to
 * sn_send_mgmt_msg_async_takeover() when the message was sent.
 */
typedef int (*sn_async_msg_callback)(int result,
                                     sn_custom_set_response *set_resp,
                                     uint32 code,
                                     sn_set_request_state *req_state,
                                     void *data);


typedef enum {
    samf_none = 0,

    /**
     * Track progress for this request.
     */
    samf_track_progress = 1 << 0,

} sn_async_msg_flags;

/** Bit field of ::sn_async_msg_flags ORed together */
typedef uint32 sn_async_msg_flags_bf;


/* ------------------------------------------------------------------------- */
/** Send a mgmtd message asynchronously from a custom SET request
 * handler on a scalar variable.  This API will return as soon as the
 * message is passed off to the GCL.  This does not necessarily mean
 * the message has even been sent, but it is "in the works", and will
 * be sent as soon as possible if it hasn't already.  The provided
 * callback will be called, with the contents of 'data' as one of its
 * arguments, when the response is received.
 *
 * NOTE: this may only be called from a custom SET handler 
 * (::sn_custom_set_callback, assigned to ssp_custom_func) on a scalar 
 * variable, OR from an async response handler (::sn_async_msg_callback,
 * passed to a prior invocation of this function).  It may NOT be called
 * from any other context.  Among other cases, this excludes calling it
 * from (a) a custom SET handler on a columnar variable, or 
 * (b) a custom parameter callback (::sn_set_custom_param_callback).
 *
 * NOTE: you MUST NOT send any synchronous mgmtd requests using the 
 * md_client API after calling this function (either from the function
 * who called this one, or from your async response handler).  Once you 
 * have "gone async", all of your remaining interactions with mgmtd must 
 * also be asynchronous.  The same is true of querying and setting sticky
 * internal variables.
 *
 * \param req_state The opaque_request state structure pointer
 * originally passed to your custom set handler.  This is used by the
 * infrastructure to hold context information about this request.
 * e.g. it knows whether the handler has "gone async" (i.e. whether or
 * not the request is considered complete when it returns).
 *
 * \param inout_req Request to be sent.  This function will take over 
 * ownership of the request, and free it when finished.  The caller's
 * pointer will be set to NULL on success, but may be left alone on error.
 *
 * \param flags Option flags.
 *
 * \param progress_oper_id Relevant only if 'flags' parameter has
 * ::samf_track_progress set.  Specifies progress operation ID to use
 * for tracking.  Typically only used if the caller specified the
 * operation ID in the request.  More commonly, the caller leaves this
 * up to mgmtd to choose automatically based on the peer ID and
 * request ID; in this case, pass NULL, and we compute the operation ID
 * in the same manner, using mdc_progress_infer_oper_id().
 *
 * \param progress_weight_pct Relevant only if 'flags' parameter has
 * ::samf_track_progress set.  Specifies the percentage of the entire
 * SNMP SET operation that this one request comnprises.  For example,
 * say you have an SNMP variable, which when SET, does an "image fetch and 
 * install" operation.  This is two separate actions.  First you send the
 * fetch request, and when that completes, your response handler sends
 * the image install action.  You estimate that the fetch will take 25%
 * of the execution time, and the install will take 75% of the time.
 * So you'd pass 25 for the first invocation, and 75 for the second.
 * The numbers should sum to 100, but the infrastructure will not 
 * enforce this, as it does not know which is the last request.
 *
 * \param timeout_override Not yet implemented (see bug 14261), and
 * currently ignored.  When we do have timeouts, 0 will mean to accept
 * the default timeout, and -1 will mean to have no timeout at all.
 *
 * \param callback_str A string containing the name of the callback
 * function being provided, for debugging purposes.  This should be
 * identical to the following parameter, except with double quotes
 * around it.  It is mandatory, except if the following parameter is
 * NULL it may also be NULL.
 *
 * \param callback Function to be called when we complete the message
 * exchange.  This could be either (a) receiving a response, (b) timeout
 * (not yet implemented), or (c) some other error.  This may be omitted
 * if you will have no further processing to do when the request is
 * completed, assuming that you want the response to the request to be
 * used as the final outcome of the SNMP SET.
 *
 * \param data Optional data to be passed back to the callback when it
 * is called.
 */
int sn_send_mgmt_msg_async_takeover(sn_set_request_state *req_state,
                                    bn_request **inout_req,
                                    sn_async_msg_flags_bf flags,
                                    const char *progress_oper_id,
                                    uint32 progress_weight_pct,
                                    lt_dur_ms timeout_override,
                                    const char *callback_str,
                                    sn_async_msg_callback callback,
                                    void *data);

/*@}*/

/* End of Set request handling section
 * ============================================================================
 */


/* ------------------------------------------------------------------------- */
/** Callback to fetch a custom value to be served for a scalar variable.
 *
 * \param scalar The registration structure for the scalar requested.
 *
 * \param data The ss_custom_query_data field out of the scalar
 * registration structure.
 *
 * \retval ret_binding A binding containing the value to be served for
 * this scalar.  The name of this binding is ignored; though for
 * debugging purposes, it would be best if it followed these guidelines:
 * (a) if the binding reflects the current value of any config or state
 * node in the mgmtd hierarchy at the time the request is served, it should
 * have the name of that node; (b) otherwise, if it does not, then it should
 * not have the name of any node in the mgmtd hierarchy.  One way to assure
 * this is to give it a relative name, or a name in a special subtree which
 * you do not use for anything else.
 */
typedef int (*sn_custom_scalar_query_callback)(const sn_scalar *scalar,
                                               void *data,
                                               bn_binding **ret_binding);


/* ------------------------------------------------------------------------- */
/** Registration to be filled out when registering an SNMP scalar variable.
 */
struct sn_scalar {
    /**
     * Name of the SNMP scalar variable, as declared in the MIB.
     */
    const char *     ss_snmp_var_name;

    /**
     * SNMP variable data type, like ASN_UNSIGNED, ASN_OCTET_STR, etc.
     */
    u_char           ss_snmp_var_type;

    /**
     * OID of SNMP scalar variable, relative to MIB's base OID
     */
    oid *            ss_rel_oid;

    /**
     * Length of the OID in ::ss_rel_oid, measured in subids
     * (numbers), NOT in bytes.
     */
    uint8            ss_rel_oid_len;

    /**
     * Name of the mgmtd node (either configuration or monitoring) to
     * which this variable is bound.  This applies to both queries
     * (for SNMP GET/GETNEXT requests) and sets (for SNMP SET requests,
     * if enabled).
     */
    const char *     ss_mgmt_node_name;

    /**
     * Mgmtd data type of the node specified in ::ss_mgmt_node_name.
     */
    bn_type          ss_mgmt_node_type;

    /**
     * Option flags specifying how this variable is to be handled.
     * Leave at 0 to accept all defaults.
     */
    sn_var_flags_bf  ss_flags;

    /**
     * For rendering time-related data types as ASCII strings.
     * This is used when a bt_duration_* data type is registered 
     * using ::svf_render_duration.
     *
     * Note that this only affects GETs of this variable; SETs try to
     * interpret time-based types in a variety of different formats.
     */
    lt_render_flags_bf ss_time_render_flags;

    /**
     * For rendering floating point data types as ASCII strings.
     * This is an optional format string which can be used to change
     * how a floating point variable is rendered when it is queried.
     * It is interpreted as a format string to pass to printf(), 
     * without the leading '%' character.  So if you wanted the
     * printf format string to be "%09.3f", set this to "09.3f".
     *
     * The default rendering is ".9f" for float32, and ".18g" for
     * float64.
     * 
     * Note that this only affects GETs of this variable; SETs try to
     * read floating point values using strtod().
     */
    const char *      ss_float_query_format;

    /**
     * A callback to be called to handle a GET of this variable.
     * This overrides ss_mgmt_node_name, and allows the callback
     * to compute its own value.
     *
     * NOTE: if a variable has custom GET handling, it must have
     * custom SET handling (if it handles SETs at all), and
     * vice-versa.
     */
    sn_custom_scalar_query_callback ss_custom_query_func;

    /**
     * Data to be passed to ss_custom_query_func whenever it is called.
     */
    void *ss_custom_query_data;

    /**
     * How is a Set request to this variable to be treated?
     *
     * NOTE: this field is NULL in a freshly-allocated sn_scalar
     * structure, and can be left that way of Set requests are not to
     * be supported.  To support Set requests, the module must first
     * call sn_scalar_enable_sets() to allocate this structure before
     * filling it out.
     *
     * NOTE: these options will only be heeded when
     * PROD_FEATURE_SNMP_SETS are enabled.
     */
    sn_set_params *ss_set_params;

    /* ..................................................................... */
    /** @name For infrastructure use only
     */
    /*@{*/
    sn_mib *      ss_mib;
    /*@}*/
};


typedef struct sn_table_cache_entry sn_table_cache_entry;


/* ------------------------------------------------------------------------- */
/** Provide a new cache entry for an SNMP table being populated.  This 
 * is meant to be called from an ::sn_custom_table_query_callback.
 * Each cache entry is a single columnar variable (a given column entry
 * in a given row).  The field determines which column we're talking
 * about; the relative OID (which is basically just all of the indices)
 * determines which row we're talking about.
 *
 * \param opaque The caller (assumed to be a ::sn_custom_table_query_callback)
 * should pass the value it was passed for its third parameter.
 *
 * \param rel_oid The OID of this cache entry, relative to the field
 * specified.  Note that the absolute OID of the entry will be the 
 * OID of the MIB, plus the OID of the table, plus the OID of the field, 
 * finally plus the OID given here.
 *
 * \param rel_oid_len The length of the OID in rel_oid.  This is
 * number of subids, NOT in bytes.
 *
 * \param inout_binding The binding which is to be the value for this
 * entry.  (XXX/EMT: does the name matter?)
 *
 * \param field A pointer to the table field registration structure
 * representing the column to which this entry belongs.
 */
int sn_table_cache_add_entry_takeover(void *opaque, oid rel_oid[],
                                      size_t rel_oid_len,
                                      bn_binding **inout_binding,
                                      const sn_field *field);


/* ------------------------------------------------------------------------- */
/** Callback to populate the cache for an SNMP table in a custom manner.
 * This function is expected to call sn_table_cache_add_entry_takeover()
 * repeatedly, to populate the cache.
 *
 * \param table The registration structure for the table requested.
 *
 * \param data The st_custom_query_data field out of the table registration
 * structure.
 *
 * \param opaque An opaque value to be passed back to
 * sn_table_cache_add_entry_takeover() while populating the cache.
 *
 * \retval ret_num_rows Return the number of rows with which the table
 * was populated.
 */
typedef int (*sn_custom_table_query_callback)(const sn_table *table,
                                              void *data,
                                              void *opaque,
                                              int32 *ret_num_rows);

/* ------------------------------------------------------------------------- */
/** Registration to be filled out when registering an SNMP table.
 */
struct sn_table {
    const char *  st_snmp_table_name;

    /** OID of table, relative to MIB's base OID */
    oid *         st_rel_oid;

    uint8         st_rel_oid_len;

    /**
     * A management node name pattern specifying the form of the names
     * of the root nodes for each of the table rows.  Each component
     * of the name may be either a string literal, or a single
     * asterisk indicating a wildcard.  e.g. for a table of configured
     * interfaces you would specify "/net/interface/config/ *"
     * (without the space before the '*').
     *
     * This pattern must have exactly one '*' part in it for each
     * index field the table will have.
     */
    const char *  st_mgmt_root_pattern;

    /**
     * If the entry definition for this table uses the AUGMENTS
     * keyword to use the full set of indices from another table
     * implemented using the Samara SNMP infrastructure, rather than
     * defining its own indices, set this to point to that table
     * registration.  When doing this, also:
     *   - Do not set st_mgmt_root_pattern for this table
     *   - Do not set sf_index to 'true' for any fields of this table
     *   - When registering the dependent table (the one which uses
     *     st_augments), the referenced table must be completely
     *     registered, including all of its fields having been defined.
     *
     * Note that AUGMENTS is useful only for cases where your table
     * has exactly the same set of indices as another table.  For
     * "expansion tables", where your table has a proper superset of
     * the indices of another table, AUGMENTS is not appropriate.
     * See ::svf_expansion_index flag for further explanation.
     */
    sn_table *    st_augments;

    /**
     * Custom callback to populate this table.  If this field is set, 
     * st_mgmt_root_pattern may be omitted from this table registration.
     *
     * NOTE: if a table has custom GET handling, it must have custom
     * SET handling (if it handles SETs at all), and vice-versa.
     */
    sn_custom_table_query_callback st_custom_query_func;

    /**
     * Data to be passed to st_custom_query_func whenever it is called.
     */
    void *st_custom_query_data;

    /**
     * Override the global cache lifetime for this table.  Of the three
     * places where cache lifetime can be defined, this is the order
     * of precedence for any given table:
     *   1. This field, in an individual table registration
     *   2. SNMP_CACHE_LIFETIME_MS from customer.h
     *   3. sn_cache_lifetime_default_ms from the SNMP infrastructure
     */
    lt_dur_ms     st_cache_lifetime_ms;

    /**
     * Override the long cache lifetime, used when a query we get is
     * detected as a continuation of a walk begun earlier.  Of the
     * three places where cache lifetime can be defined, this is the
     * order of precedence for any given table:
     *   1. This field, in an individual table registration
     *   2. SNMP_CACHE_LIFETIME_LONG_MS from customer.h
     *   3. sn_cache_lifetime_long_default_ms from the SNMP infrastructure
     */
    lt_dur_ms     st_cache_lifetime_long_ms;

    /**
     * Defaults to 'true', meaning that the cache of an SNMP table
     * will be flushed after an SNMP SET into that table is processed.
     * This will trigger a repopulation of the cache next time any
     * request (GET, GETNEXT, or SET) is made against the table.
     *
     * Set this to 'false' to skip flushing the cache.  Note that 
     * subsequent queries to the table may not show the new value
     * until the cache naturally expired.  (See also bug 14172)
     *
     * Note that the same is true of any other way that the values can
     * be changed, regardless of how this is set.  e.g. if a config
     * node is set through the CLI or Web UI, the table is never
     * automatically flushed either.  However, it may be worse in the
     * SNMP case because handling a SET against a table always entails
     * a query before the set is processed, which may trigger a
     * premature (re-)population of the table cache, prolonging the
     * lifetime of the stale data.
     */
    tbool         st_cache_flush_on_set;

    /* ..................................................................... */
    /** @name For infrastructure use only
     *
     * Fields may be registered in any order, but indices are always
     * put at the beginning, sorted by which wildcard they refer to,
     * so the nth wildcard is always indexed by the (n-1)th entry in
     * this array.
     */
    /*@{*/

    /*
     * All the fields registered directly with this table.
     * For an independent table, this will have all the indices first,
     * then all the non-index fields.  For a dependent table (which uses
     * AUGMENTS to reference the row object of another table), this will
     * have only non-index fields.
     *
     * Entries are of type (sn_field *)
     */
    array *       st_fields;

    /*
     * Reference to array of fields which begins with our own indices.
     * For an independent table, this will be equal to st_fields of this
     * table.  For a dependent table, this will be equal to the st_fields
     * of the table referenced by st_augments.  Code wishing to retrieve
     * index fields for this table should look here instead of st_fields.
     */
    const array * st_index_fields;

    /*
     * Number of indices for this table.  This is the number of index
     * entries in st_index_fields.  Note that it is correct also for
     * dependent tables.
     */
    uint8         st_num_indices;

    /* Last accessed OID, for cache lifetime determination */
    oid           st_last_oid[MAX_OID_LEN];
    uint8         st_last_oid_len;

    sn_mib *      st_mib;

    /*@}*/
};


/* ------------------------------------------------------------------------- */
/** An enum used to classify the source of data for the value of a field
 * in a table.
 */
typedef enum {
    sfs_none = 0,

    /**
     * Field value comes from the value of a child of the row root.
     *
     * NOTE: this cannot be used with index fields, except if (a) it is
     * only to be included as a varbind in a trap (not for query), AND
     * (b) it is the last index in the table.
     */
    sfs_child_value,

    /** 
     * Field value comes from the value of a wildcard in the row root name.
     */
    sfs_wildcard_value,

    /** 
     * Field value is a synthetic number which does not come from
     * anywhere in the mgmtd nodes.  The number uniquely identifies this
     * wildcard instance among all of its peers, and will remain stable
     * as long as this instance of snmpd is running, i.e. (a) this 
     * wildcard instance will always be represented with the same 
     * synthetic index number, and (b) no other instance will be 
     * represented with this number (even if this instance is deleted).
     *
     * Because of this behavior, please note that these numbers may
     * not always be packed, and they may not map to wildcard
     * instances in any kind of sorted order.
     *
     * NOTE: this cannot be used with non-index fields.  And although
     * it may be used on any index field, it is recommended that:
     *
     *   (a) In multi-index tables, it should only be used on the last
     *       index.  This is because you will typically have another
     *       table field which contains the value of the wildcard which
     *       the index maps to, and you cannot do this unless it's the 
     *       last wildcard.
     *
     *   (b) It should generally only be used for non-integer wildcards,
     *       since if the wildcard itself is an integer, it would usually
     *       be better to use it directly.
     */
    sfs_wildcard_synthetic,

    /*
     * Alias for sfs_wildcard_synthetic, for backward compatibility.
     * This value is deprecated; new code should not use it, and existing
     * code is encouraged to migrate to sfs_wildcard_synthetic.
     * The name 'sfs_wildcard_sequence' is misleading because the integer
     * index value generated does not necesarily reflect any sort of 
     * sequential ordering of the index values.  It may do so the first 
     * time the table is queried, but if values are deleted or added; or
     * if traps with these values are generated before the table is queried,
     * they could easily fall out of sorted order.  This is an inherent
     * side effect of having stable synthetic indices.
     */
    sfs_wildcard_sequence = sfs_wildcard_synthetic,

    /**
     * Indicates that this variable is the RowStatus column for the
     * table.  It is not tied to any mgmtd node, and thus does not 
     * set sf_mgmt_type, sf_mgmt_node_child_name, etc.  The variable
     * in the MIB should be declared using the RowStatus textual
     * convention (from SNMPv2-TC), and the sf_snmp_field_type field
     * should have ASN_INTEGER.
     *
     * When read, it will always return the value for 'active' 
     * (which happens to be 1), since Samara does not support the
     * other states offered for a a table row by the textual
     * convention ('notInService' and 'notReady').
     *
     * When written, it will accept either the value 'createAndGo'
     * (which creates the table row if it didn't already exist),
     * or 'destroy' (which deletes the table row).  Any other value
     * set into this field is an error.
     *
     * Notes on table row creation and deletion:
     *
     *   - Row creation is implicit, and does not require the 
     *     RowStatus to be set to 'createAndGo'.  Setting the RowStatus
     *     to this value does create the row, but if you are setting
     *     other fields in the table, it is unnecessary.
     *
     *   - If 'destroy' is specified, the same SET request should not 
     *     include any other columns in the same row, since that could
     *     just re-create the row just deleted (depending on the order
     *     in which the variables are honored).
     *
     * NOTE: this cannot be used with index fields, or with fields that 
     * may be included as varbinds in SNMP traps (notifications).
     */
    sfs_row_status,
    
    /** 
     * Field value comes from querying a mgmtd node whose name is
     * determined by a custom callback.
     *
     * NOTE: this cannot be used with index fields, or with fields that 
     * may be included as varbinds in SNMP traps (notifications).
     */
    sfs_custom,

} sn_field_source;


/* ------------------------------------------------------------------------- */
/** Callback to be called when we want the value of a table field
 * registered with sfs_custom.  The callback is told what instance we 
 * want the value for, and it returns a binding name (presumably not a
 * child of the table's base wildcard, as then a custom function would
 * not generally be needed).
 *
 * \param field The field registration structure we are being called for.
 * \param root_node_name The full name of the wildcard instance node 
 * representing this row in the table.  It is for this object that this
 * field is being requested.
 * \param data The data that was registered (in the 
 * sf_custom_query_callback_data reg field) to be passed to this function.
 * \retval ret_node_name The name of the binding to be fetched from mgmtd,
 * and then rendered as the value for this field.
 */
typedef int (*sn_custom_field_query_callback)(const sn_field *field,
                                              const char *root_node_name,
                                              void *data,
                                              tstring **ret_node_name);


typedef struct sn_synth_index_cache sn_synth_index_cache;


/* ------------------------------------------------------------------------- */
/** Registration structure to be filled out when registering an
 * SNMP columnar variable (table field).
 *
 * There are seven possible cases for each field.  Note that
 * sf_wildcard_index is filled out only for indices; and
 * sf_mgmt_node_child_name is filled out only if the source is the
 * contents of a child node.  The two sf_custom_... fields are only
 * filled out if the source is sfs_custom.  All of the other fields
 * are mandatory except where otherwise noted.
 *
 * 1. An index which is derived directly from a wildcard value.
 *
 *    sf_index: true
 *    sf_source: sfs_wildcard_value
 *    sf_wildcard_index: (index number of wildcard in table's
 *                        st_mgmt_root_pattern, starting from one, 
 *                        not counting the string literal components)
 *    sf_mgmt_node_child_name: (not filled out)
 *
 * 2. An index which is derived from the value of a child node.
 *
 *    (NOTE: this is only supported if your variable is only to be
 *    used in SNMP traps only; i.e. if you have called 
 *    sn_reg_mib_set_hidden() on your module.  If your variable is to
 *    be available for query through SNMP, this is not supported.)
 *
 *    sf_index: true
 *    sf_source: sfs_child_value
 *    sf_wildcard_index: (not filled out)
 *    sf_mgmt_node_child_name: (name of node relative to table entry root
 *                              node name matching st_mgmt_root_pattern)
 *   
 * 3. An index which is a synthetic number, uniquely identifying the value
 *    among its peers, starting from 1.
 *
 *    sf_index: true
 *    sf_source: sfs_wildcard_synthetic
 *    sf_wildcard_index: (same as in case 1 above)
 *    sf_mgmt_node_child_name: (not filled out)
 *
 * 4. A regular table field derived from the value of one of the wildcards
 *    in the table entry root's name
 *
 *    sf_index: false
 *    sf_source: sfs_wildcard_value
 *    sf_wildcard_index: (same as in case 1 above)
 *    sf_mgmt_node_child_name: (not filled out)
 *
 * 5. A regular table field derived from the value of a child node.
 * 
 *    sf_index: false
 *    sf_source: sfs_child_value
 *    sf_wildcard_index: (not filled out)
 *    sf_mgmt_node_child_name: (same as in case 2 above)
 *
 * 6. A regular table field which is a special "RowStatus" field.
 *    It is not derived from any mgmtd node, but can be used to
 *    create or delete rows of the table.
 *    sf_index: false
 *    sf_source: sfs_row_status
 *    sf_wildcard_index: (not filled out)
 *    sf_mgmt_type: (not filled out)
 *    sf_mgmt_node_child_name: (not filled out)
 *
 * 7. A regular table field derived from calling a custom callback.
 *
 *    sf_index: false
 *    sf_source: sfs_custom
 *    sf_wildcard_index: (not filled out)
 *    sf_mgmt_node_child_name: (not filled out)
 *    sf_custom_query_callback: function to determine value of field
 *                        (and sf_custom_query_callback_data may optionally
 *                        contain data to be passed to this callback)
 *
 *    NOTE: fields with this option selected cannot be used in
 *    notifications.
 *
 * Note: with five possible values for sf_source, there are ten
 * possible combinations, but the other three are not planned to be 
 * supported in any future release:
 * 8. sf_index == false, sf_source == sfs_wildcard_synthetic.
 * 9. sf_index == true,  sf_source == sfs_custom.
 * 10. sf_index == true, sf_source == sfs_row_status.
 *
 * Set the sf_mgmt_type field to the bn_type data type of the
 * underlying mgmd node.  If sf_source is sfs_wildcard_synthetic, 
 * you may omit setting this, since the value is not coming directly
 * from a mgmtd node.  The mgmtd data type of the synthetic index
 * generated will be uint32, so the infrastructure will automatically
 * set this to bt_uint32 if you do not.
 *
 * General note for table indices: SNMP OIDs have a maximum length of
 * 128 sub-IDs.  If you have variable-length indices (string, OID, or
 * binary data types), it is possible to exceed this limit by
 * accident, particularly if the values used for indices can be set by
 * the user.  If the limit is exceeded, a warning will be logged, and
 * the offending wildcard instance will be skipped.  (We do this rather
 * than truncating it because sometimes this can confuse the client,
 * e.g. if two wildcards are identical after truncation, the client can
 * give an "Error: OID not increasing" error and halt the walk.)
 * It is your responsibility to avoid this, probably by registering
 * maximum length limits on the wildcards involved.  Note that the 128
 * figure is the maximum total length of the OID, which will have other
 * components (including perhaps other indices), so the length limit 
 * would have to be somewhat lower.
 *
 * Note that table indices must be registered in order.  i.e. the first
 * one registered in a given table must have sf_wildcard_index of 1,
 * the second must have 2, and so forth.
 */
struct sn_field {
    const char *      sf_snmp_field_name;
    u_char            sf_snmp_field_type;

    /** OID of this field, relative to the table's base OID */
    oid *             sf_rel_oid;
    uint8             sf_rel_oid_len;

    tbool             sf_index;
    sn_field_source   sf_source;
    uint8             sf_wildcard_index;
    const char *      sf_mgmt_node_child_name;
    sn_custom_field_query_callback sf_custom_query_callback;
    void *            sf_custom_query_callback_data;
    bn_type           sf_mgmt_type;
    sn_var_flags_bf   sf_flags;

    /**
     * For rendering time-related data types as ASCII strings.
     * This is used when a bt_duration_* data type is registered 
     * using ::svf_render_duration.
     *
     * Note that this only affects GETs of this variable; SETs try to
     * interpret time-based types in a variety of different formats.
     */
    lt_render_flags_bf sf_time_render_flags;

    /**
     * For rendering floating point data types as ASCII strings.
     * This is an optional format string which can be used to change
     * how a floating point variable is rendered when it is queried.
     *
     * It acts just like ss_float_query_format; see comments above
     * that for details.
     */
    const char *      sf_float_query_format;

    /**
     * How is a Set request to this variable to be treated?
     *
     * NOTE: this field is NULL in a freshly-allocated sn_field
     * structure, and can be left that way of Set requests are not to
     * be supported.  To support Set requests, the module must first
     * call sn_field_enable_sets() to allocate this structure before
     * filling it out.
     *
     * NOTE: these options will only be heeded when
     * PROD_FEATURE_SNMP_SETS are enabled.
     */
    sn_set_params *sf_set_params;

    /* ..................................................................... */
    /** @name For infrastructure use only
     */
    /*@{*/

    sn_table *        sf_table;

    /*
     * If this field is an index, this and sf_wildcard_name_index hold
     * the name of the parent of the wildcard the index is associated
     * with, and the index number of the associated wildcard among the
     * parts of the name in sf_table->st_mgmt_root_pattern, starting
     * from 0.
     *
     * The first of these fields is dynamically allocated and must be
     * freed when the sn_field structure is freed.
     */
    char *            sf_wildcard_parent;
    uint16            sf_wildcard_name_index;

    /*
     * If this field is a synthetic index, this is used by the new
     * table implementation as a cache to remember which index we have
     * previously used for each value of this field under a given
     * parent.
     */
    sn_synth_index_cache *sf_index_cache;

    /*@}*/
};


/* ------------------------------------------------------------------------- */
/** Types of variables that can be included in notifications.
 *
 * Currently all notification variables must be ones which are exposed
 * as queryable variables from the same MIB.  Some redundant
 * information is eliminated by providing the registration structure
 * from the variable to be returned.  In the future we could allow
 * variables to be constructed from scratch, but since SNMP wants these
 * to be instances of real queryable variables, this is low priority.
 */
typedef enum {
    snvt_none = 0,
    snvt_scalar_reg,
    snvt_field_reg
} sn_notif_var_type;


/* ------------------------------------------------------------------------- */
/** Registration to be filled out when registering a variable which is to
 * be included in an SNMP notification (trap or inform).
 */
typedef struct sn_notif_var {
    sn_notif_var_type snv_type;

    /**
     * If snv_type is snvt_scalar_reg
     */
    sn_scalar *       snv_scalar;

    /**
     * If snv_type is snvt_field_reg
     */
    sn_field *        snv_field;

    /**
     * If set to 'true', we will not complain if we can't find the
     * binding needed to populate this in the event.  Default is 'false'.
     */ 
    tbool             snv_optional;

} sn_notif_var;


/* ------------------------------------------------------------------------- */
/** This type describes how to build a notification message for a
 * particular sn_notif registration: as an SNMP v1 or v2c trap.
 * This should correspond to how it is declared in the MIB:
 * snt_trap_v1 should be used if the notification is declared with the
 * TRAP-TYPE construct; snt_trap_v2 should be used if the notification
 * is declared with the NOTIFICATION-TYPE construct.
 *
 * Note that no distinction is made here between traps and informs.
 * An inform is essentially a v2 trap which expects an
 * acknowledgement.  Whether to send an inform or a trap is a matter
 * of configuration, not inherent to the notification declaration.
 *
 * Note that such a notification may not actually be sent as the SNMP
 * trap version it is declared as.  A notification is constructed with
 * the specified version, but it may be converted to the other if a
 * trap sink wants the other.
 *
 * XXX/EMT: explain v1-->v2 and v2-->v1 conversion.
 */
typedef enum {
    snt_none = 0, 
    snt_trap_v1, 
    snt_trap_v2c,
    snt_last = snt_trap_v2c
} sn_notif_type;


/* ------------------------------------------------------------------------- */
/** Response structure for ::sn_custom_notif_callback.
 */
typedef struct sn_custom_notif_response {
    /**
     * Defaults to 'true'.  Set to 'false' to skip this event entirely.
     * If 'false', all other fields of the response will be ignored.
     */
    tbool scnr_send_notif;

    /**
     * Defaults to NULL.  To return additional bindings (or to change
     * bindings that came with the event), the callback must allocate
     * an array here, and append bindings to it.  When constructing
     * SNMP variables to include in the trap, these bindings will be
     * treated as if they had arrived in the original event message.
     *
     * NOTE: if any bindings in this array have the same name as
     * bindings in the original event request, they will take
     * preceence over the original request.
     *
     * NOTE: your callback may not make mgmtd requests!  These bindings
     * must be solely the result of your own calculations, generally
     * based on the contents of other bindings found within the event.
     */
    bn_binding_array *scnr_extra_bindings;

    /**
     * Defaults to NULL, meaning that when the trap is sent, the OID
     * specified in sno_trap_oid and sno_trap_oid_len should be used.
     *
     * To override the OID of the v2c trap which is sent for this
     * event, allocate enough space for the OID and copy it into this
     * space.  You MUST NOT assign this to point to a constant, since
     * the SNMP infrastructure will assume this is dynamically
     * allocated, and will try to free it when finished.
     *
     * If you set this variable, you must also set ::scnr_trap_oid_len
     * to the number of subids in this OID.
     */
    oid *scnr_trap_oid;

    /**
     * Defaults to 0.  Must be set if and only if scnr_trap_oid is set.
     * Must contain the number of subids (OID components, NOT bytes)
     * in scnr_trap_oid.
     */
    uint8 scnr_trap_oid_len;

} sn_custom_notif_response;


/* ------------------------------------------------------------------------- */
/** Type of function to be called to do custom processing of an event
 * which is potentially to be converted into an SNMP notification
 * (trap).  A function of this type may be assigned to the
 * sno_custom_func field of an sn_notif struct.  If no such function
 * is present, the default handling will be done.  This function may
 * alter various aspects of the event's handling, by filling out
 * fields in its response structure.
 *
 * NOTE: this callback is NOT permitted to make mgmtd requests!
 * Doing so might appear to work most of the time, but it would leave
 * snmpd vulnerable to timing-sensitive bugs.  So even if it appears to
 * work, don't do it!
 *
 * \param notif The notification registration structure originally provided
 * by the module.
 *
 * \param event_req The specific event request which triggered this
 * call to this function.
 *
 * \param bindings The bindings extracted from the event request.
 * This is redundant with event_req, from which the bindings could be
 * extracted again, but is provided here for convenience and efficiency.
 *
 * \param data Contents of the sno_custom_data field of the sn_notif which
 * registered us (also accessible via notif->sno_custom_data).
 *
 * \retval ret_resp Response structure.  This structure is already
 * allocated and empty.  If your function returns without
 * changing/setting any fields in this structure, the event will be
 * processed as if this function had never been called.  Changing fields
 * in the struct may change how the event is processed; see documentation
 * for ::sn_custom_notif_response for details.
 */
typedef int (*sn_custom_notif_callback)(const sn_notif *notif,
                                        const bn_request *event_req,
                                        const bn_binding_array *bindings,
                                        void *data,
                                        sn_custom_notif_response *ret_resp);


/* ------------------------------------------------------------------------- */
/** Registration to be filled out when registering an SNMP notification
 * (trap or inform).
 */
struct sn_notif {
    /** Name of event to trigger this notification */
    const char *sno_event_name;

    /**
     * Name of notification in SNMP MIB.  We don't really need this, but
     * it's nice to have in case we need to log something about it; it's
     * better to use the user-visible notification name, than the mgmtd
     * event name.  If this is not specified, we use the event name.
     */
    const char *sno_notif_name;

    /** How was it declared in the MIB? */
    sn_notif_type sno_notif_type;

    /** @name v1 trap fields.
     * (Only heeded if sno_notif_type is snt_trap_v1)
     */
    /*@{*/
    int sno_generic_trap;
    /** If sno_generic_trap is SNMP_TRAP_ENTERPRISESPECIFIC */
    int sno_specific_trap;
    /*@}*/                              

    /** @name v2c trap fields.
     * (Only heeded if sno_notif_type is snt_trap_v2c)
     */
    /*@{*/
    /** OID of trap, relative to MIB's OID */
    oid *sno_trap_oid;
    /** Number of OID components, not bytes */
    uint32 sno_trap_oid_len;
    /*@}*/

    /**
     * Dynamically-allocated array of sn_notif_var structures.
     * The infrastructure allocates this array when the sn_notif
     * structure is created; the caller should then fill in each of 
     * the sn_notif_var structures.
     */
    sn_notif_var *sno_vars; 

    /**
     * Custom event processing function (optional).  If provided, this
     * function will be called whenever the specified mgmtd event is
     * received, and may alter the handling of the event.
     */
    sn_custom_notif_callback sno_custom_func;

    /**
     * Data to be passed to sno_custom_func whenever it is called.
     */
    void *sno_custom_data;

    /* ..................................................................... */
    /** @name For infrastructure use only
     */
    /*@{*/
    uint32 sno_num_vars;
    sn_mib *sno_mib;
    /*@}*/
};


/* ------------------------------------------------------------------------- */
/** Register a new MIB with the infrastructure.  Returns a pointer to
 * a MIB structure which must be passed back with every registration
 * of a variable or notification that goes in the MIB.  The
 * infrastructure owns the memory, and the client is not responsible
 * for freeing it later.
 */
int sn_reg_mib_new(sn_mib **ret_mib, const char *mib_name,
                   oid mib_base_oid[], uint8 mib_base_oid_len);


/* ------------------------------------------------------------------------- */
/** Create a new variable registration structure.  The caller should
 * fill out the structure and then register it with the infrastructure
 * using sn_reg_scalar_register().
 *
 * Note: the ss_rel_oid pointer is initialized to NULL; it does not
 * point at any allocated memory to hold the OID.  It is assumed that
 * clients will usually assign this to point to a const variable
 * holding the OID.
 */
int sn_reg_scalar_new(sn_scalar **ret_scalar);


/* ------------------------------------------------------------------------- */
/** Register an SNMP variable with the infrastructure and associate it
 * with the specified MIB.
 *
 * Copies are not made of the members of the variable structure; it is
 * assumed that they are constants.  The infrastructure does assume
 * ownership of the variable structure itself, and is responsible for
 * freeing it.
 */
int sn_reg_scalar_register(sn_mib *mib, sn_scalar *var);


/* ------------------------------------------------------------------------- */
/** Create a new table registration structure.  The caller should fill
 * out the structure and then register it with sn_reg_table_register().
 * Note that fields cannot be added to the table until it has been
 * registered with a MIB.
 */
int sn_reg_table_new(sn_table **ret_table);


/* ------------------------------------------------------------------------- */
/** Register an SNMP table with the infrastructure and associate it with
 * the specified MIB.  The infrastructure assumes ownership of the table
 * structure.
 *
 * XXX/EMT: if we had them do this after, rather than before, adding
 * fields to it, we could verify that it has an index, and possibly
 * other sanity checking that can only be done when all fields are
 * present.
 */
int sn_reg_table_register(sn_mib *mib, sn_table *table);


/* ------------------------------------------------------------------------- */
/** Create a new table field registration structure.  The caller
 * should fill out the structure and then add it to a registered table
 * using sn_reg_field_register().
 */
int sn_reg_field_new(sn_field **ret_field);


/* ------------------------------------------------------------------------- */
/** Register an SNMP table field (one variable in a table entry) with
 * the infrastructure, associating it with the specified SNMP table.
 * The infrastructure assumes ownership of the field structure.
 */
int sn_reg_field_register(sn_table *table, sn_field *field);


/* ------------------------------------------------------------------------- */
/** Create a new notification registration structure.  The caller
 * should fill out the structure and then register it using
 * sn_reg_notif_register().
 *
 * \param num_vars the number of variables to be passed along with the
 * trap or inform generated from this notification.  This number of
 * sn_notif_var structures will be allocated as part of the sn_notif
 * structure, to be filled in by the caller.
 * 
 * \retval ret_notif The notification registration structure created.
 */
int sn_reg_notif_new(sn_notif **ret_notif, uint16 num_vars);


/* ------------------------------------------------------------------------- */
/** Register an SNMP notification with the infrastructure, associating
 * it with the specified MIB.  The infrastructure assumes ownership of
 * the notification structure.
 */
int sn_reg_notif_register(sn_mib *mib, sn_notif *notif);


/* ------------------------------------------------------------------------- */
/** Tell the infrastructure whether or not the MIB being registered
 * here should have its variables registered with NET-SNMP.  If
 * 'hidden' is true, it will not be registered; otherwise, it will be
 * registered, which is the default if this API is not called.
 *
 * Hiding a MIB will prevent its variables from being queried through
 * the agent.  This is mainly interesting when the variables are only
 * being declared so they can be included in traps which your module
 * plans to send.
 *
 * Call this before registering any variables, as the hidden status 
 * may affect how registrations are processed.
 */
int sn_reg_mib_set_hidden(sn_mib *mib, tbool hidden);


/* ------------------------------------------------------------------------- */
/* Copy one or more OIDs while concatenating them together.
 * They are copied into the pre-allocated buffer provided.
 */
int sn_oid_concat(oid *ret_full_oid, uint8 full_oid_max_len,
                  uint8 *ret_full_oid_len, uint8 num_oids,
                  const oid *oid1, uint8 oid1_len, ...);


/* ------------------------------------------------------------------------- */
/** A helper function, meant to be called from a custom query callback,
 * to determine the status of an async operation, looked up by the OID
 * of the variable that initiated it.
 *
 * NOTE: this is an imprecise method of looking up the status of an
 * SNMP request.  Because it is done by OID and not Owner/ReqID, you
 * may end up looking at the status and results of another request
 * that was similar to yours, and invoked around the same time.
 * See the "Progress and Result Reporting" subsection of the Samara
 * Technical Guide (under the "SNMP" section) for details on an
 * alternative to this.
 *
 * \param req_oid The OID of the variable that initiated the request.
 * This is a key with which to look up which request(s) we care about
 * If there are multiple matches, we will look only at the one that
 * started most recently.
 *
 * \param req_oid_len The length of req_oid in number of subids.
 * If you had the OID in a single array, use the OID_LENGTH macro to
 * compute the length of it.  If you had multiple OIDs, use
 * sn_oid_concat() to concatenate them, and it will give you the total
 * length.
 *
 * \retval ret_status If no matching records were found, this will
 * get ::nssrs_none.  Otherwise, it reflects the status of the matching
 * operation which started most recently.
 *
 * \retval ret_pct_complete If the operation is still running
 * (::nssrs_running), what percentage of it is completed?  This will get 0
 * if the operation is not running.
 *
 * \retval ret_code If the operation completed (::nssrs_complete),
 * what was the result code?  This will get 0 if the operation is 
 * not complete.
 *
 * \retval ret_msg If the operation completed (::nssrs_complete),
 * what was the result message?  This will get NULL if the operation is 
 * not complete.  If the operation is complete but there was no message,
 * this will get the empty string.
 *
 * \retval ret_bindings If the operation completed (::nssrs_complete),
 * what bindings were returned in the response?  This will get NULL if
 * the operation is not complete.
 *
 * \retval ret_bname_base The name of the root node of the monitoring
 * subtree that tells about this operation.  This will match the name
 * pattern "/snmp/state/request/owner/ * /reqid/ *".
 */
int sn_lookup_request_status(oid *req_oid, uint32 req_oid_len,
                             nen_snmp_state_request_status *ret_status,
                             float64 *ret_pct_complete,
                             uint32 *ret_code, tstring **ret_msg,
                             bn_binding_array **ret_bindings,
                             tstring **ret_bname_base);


#ifdef __cplusplus
}
#endif

#endif /* __SN_MOD_REG_H_ */
