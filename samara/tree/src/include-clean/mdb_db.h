/*
 *
 * mdb_db.h
 *
 *
 *
 */

#ifndef __MDB_DB_H_
#define __MDB_DB_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "addr_utils.h"
#include "bnode.h"
#include "bnode_proto.h"
#include "texpr.h"

/** \defgroup mdb LibMdb: Management daemon database library */

/**
 * \file mdb_db.h Management daemon database API.
 * \ingroup mdb
 *
 * This is the interface that management daemon modules and other parts
 * of the management daemon have to the management db.
 * 
 * The db is accessed through these functions which allow for gets, iterates,
 * and sets on nodes.  This API is called the db frontend.
 *
 * The nodes themselves are owned by a db backend.  The backend is
 * responsible for answering all db frontend requests.  
 */

typedef struct md_commit md_commit;
typedef struct mdb_node_reg mdb_node_reg;

/**
 * Flags that can be specified on a query request.
 *
 * What is the difference between these flags and bn_query_node_flags?
 * Those are for use in GCL requests, while these are for querying 
 * the database directly from within mgmtd using libmdb.
 *
 * See comment at top of bn_query_node_flags about usage of these two
 * sets of flags.
 *
 * NOTE: mdb_db_query_flags_map (in mdb_db.c) must be kept in sync.
 */
typedef enum {
    /** Do not return any configuration nodes; monitoring only */
    mdqf_sel_class_no_config =    0x0100,

    /** Do not return any monitoring state nodes; configuration only */
    mdqf_sel_class_no_state =     0x0200,

    /** Let the query traverse into passthrough nodes */
    mdqf_sel_class_enter_passthrough =  0x0400,

    /**
     * Omit nodes which (a) are config literals, (b) have a ba_value
     * attribute equal to their registered initial value, and (c) are
     * registered with ::mrf_omit_defaults_eligible.
     *
     * This is generally most interesting with Iterate requests, but also
     * works with Get requests.
     *
     * Performance note: there is a slight cost to using this flag
     * (mainly the cost of bn_attrib_compare() on each value against its
     * initial value).  Test queries using this flag, on large numbers of
     * nodes where all nodes had non-default values (and so none were
     * filtered out), took about 2.5% longer than the same query without
     * this flag.  Actual cost will vary depending on the specific data
     * involved.  Of course, once some nodes do start getting filtered
     * out, this cost is quickly recouped.
     */
    mdqf_sel_class_omit_defaults =      0x0800,

    /** (Iterate only) Return all descendants, not just children */
    mdqf_sel_iterate_subtree =             0x01000,

    /** (Iterate only) Return the node specified, not just 
     * descendants/children */
    mdqf_sel_iterate_include_self =        0x02000,

    /** 
     * (Iterate only) Only return leaf nodes, no interior nodes.
     *
     * NOTE: this means nodes whose corresponding registration node is
     * a leaf in the registration tree.  So e.g. if you have an instance
     * of "/hosts/ipv4/ *" with no children, it would not show up, because
     * "/hosts/ipv4/ *" has children in the registration tree.
     */
    mdqf_sel_iterate_leaf_nodes_only =     0x04000,

    /** (Iterate only) Just return the count of the number of nodes that
     * would be returned, rather than the nodes themselves.  The count is
     * returned as an attribute on the node queried.  Note that this only
     * counts immediate children, even if mdqf_sel_iterate_subtree is 
     * specified.
     *
     * (Note: this does not take into account advanced query
     * filtering; see bug 11836)
     */
    mdqf_sel_iterate_count =               0x10000,

    /** Obfuscate the values of any nodes that have the
     * mrf_exposure_obfuscate registration flag set.  By default,
     * even nodes with that flag are returned in cleartext.
     *
     * NOTE that unlike all the other flags in this enum (so far),
     * this one is NOT identical to the corresponding one in
     * bn_query_node_flags, so it is actully important to use the 
     * correct set of flags. */
    mdqf_obfuscate =                       0x20000,

    /*
     * Do not use 0x40000 here; this is reserved for a "do not
     * obfuscate" flag in bn_query_node_flags.  See comment at top of
     * that enum definition for why we need to keep sync.
     */
    
    /**
     * Return bindings exactly as they would be written to persistent
     * storage.  Currently, this means only "if the mrf_exposure_encrypt
     * flag is set, return the binding encrypted", since that is the way 
     * in which saving a config db to persistent storage varies by default
     * from servicing queries.  When a binding is encrypted, the ba_value
     * attribute is removed, and replaced with a ba_encrypted_value
     * attribute of type bt_binary, which holds the old contents of the
     * ba_value in encrypted form.
     *
     * Note that this flag cannot be used with mdqf_obfuscate.  If the
     * combination is attempted, a warning will be logged, and the
     * mdqf_as_saved flag will be ignored.
     */
    mdqf_as_saved =                        0x80000,
} mdb_db_query_flags;

/* Bit field of ::mdb_db_query_flags ORed together */
typedef uint32 mdb_db_query_flags_bf;

extern const lc_enum_string_map mdb_db_query_flags_map[];

/**
 * Flags that can be specified on a set request.
 *
 */
typedef enum {
    mdsf_mod_has_cont_cookie =    0x0010, /* has a continuation cookie */
    
    mdsf_fail_node_exist =        0x0020, 
    mdsf_fail_node_no_exist =     0x0040, 

    mdsf_require_path_exists =    0x0080, /* do not auto create parent nodes */

    mdsf_link_no_follow =         0x0100, /* operate on link, no link target */

#if 0
    mdsf_attrib_replace =         0x0200, /* existing attribs are reset 1st */
#endif
    mdsf_reparent_by_copy =       0x0400, /* copy instead of move */

    /* xxx-later: create wci from template target flag */
    /* xxx-later: per node modtime/rev id/hash checks */
    /* xxx-later: fail on attribute exist / values */

} mdb_db_set_flags;

/* Bit field of ::mdb_db_set_flags ORed together */
typedef uint32 mdb_db_set_flags_bf;


/* Per-node flags for the the backends to use for in-memory nodes */
/* XXX need a way for the monitoring pseudo-backend to specify cacheability*/
typedef enum {
    mnf_none = 0,
    mnf_mem_loaded = 1,
    mnf_mem_cached,
} mdb_node_flags;

/* This is the state that every backend keeps per-node */
typedef struct mdb_node mdb_node;

/* This is the main db struct, that points at the backend for the db */
typedef struct mdb_db mdb_db;
typedef struct mdb_dbbe mdb_dbbe;
typedef struct mdb_reg_state mdb_reg_state;

TYPED_ARRAY_HEADER_NEW_NONE(mdb_node, mdb_node *);
int mdb_node_array_new(mdb_node_array **ret_arr);

typedef int (*mdb_iterate_binding_cb_func)(md_commit *commit, 
                                           const mdb_db *db, 
                                           const bn_binding *binding, 
                                           void *arg);
typedef int (*mdb_iterate_node_cb_func)(md_commit *commit, 
                                        const mdb_db *db,
                                        const mdb_node *node,
                                        void *arg);

typedef int (*mdb_external_get_func)(md_commit *commit,
                                     const mdb_db *db,
                                     const char *node_name,
                                     const mdb_node_reg *reg_node,
                                     mdb_db_query_flags_bf db_query_flags,
                                     mdb_node **ret_node);

typedef int (*mdb_external_iterate_func)(md_commit *commit,
                                         const mdb_db *db,
                                         const char *node_name,
                                         const mdb_node_reg *reg_node,
                                         mdb_db_query_flags_bf db_query_flags,
                                         tstr_array **ret_name_array);

/*
 * Encrypt or decrypt a binding in place, by modifying the binding
 * passed in.  The 'data' parameter passed to the function will be the
 * one provided when the mdb_crypt_func itself is registered.
 *
 * Encryption means serializing the contents of the ba_value
 * attribute, encrypting this data, setting the ba_encrypted_value
 * attribute with the results, and deleting the old ba_value
 * attribute.  This is generally done while saving a db, right before
 * writing a binding.
 *
 * Decryption means decrypting the data from the ba_encrypted_value
 * attribute, de-serializing the result of this into the ba_value
 * attribute, and deleting the old ba_encrypted_value attribute.
 * This is generally done while loading a db, right after reading
 * a binding.
 *
 * The function may return an error.  The most likely one would be 
 * lc_err_decryption_failure when decryption was requested, in case
 * we do not have the right key to decrypt the data.
 */
typedef int (*mdb_crypt_func)(void *data, bn_binding *binding,
                              tbool do_encrypt);

typedef enum {
    mqt_none =        -1,
    mqt_dbbe =        0, mqt_FIRST = mqt_dbbe,
    mqt_mon =         1,
    mqt_extmon =      2,   
    mqt_passthrough = 3,
    mqt_LAST
} mdb_query_target_type;


typedef struct mdb_query_state mdb_query_state;

int mdb_query_target_dbbe_get(mdb_query_target_type target_type,
                              int32 binding_num,
                              uint32 curr_node_id,
                              const char *node_name,
                              const tstr_array *name_parts, 
                              mdb_node_reg *reg_node,
                              mdb_query_state *inout_query_state);

int mdb_query_target_dbbe_iterate_full(const md_commit *commit, 
                                       const mdb_db *db, 
                                       uint32 node_id,
                                       const char *node_name,
                                       const tstr_array *name_parts, 
                                       mdb_db_query_flags_bf db_query_flags,
                                       mdb_query_state *inout_query_state);

int mdb_query_target_dbbe_iterate_names(mdb_query_target_type target_type,
                                        int32 binding_num,
                                        uint32 curr_node_id,
                                        const char *node_name,
                                        const tstr_array *name_parts, 
                                        mdb_node_reg *reg_node,
                                        mdb_query_state *inout_query_state,
                                        tstr_array **ret_child_self_names);

int mdb_node_reg_flags_to_target_type(uint32 reg_flags, 
                                      mdb_query_target_type *ret_target_type);

int
mdb_query_target_dbbe_handler(mdb_query_target_type target_type,
                        mdb_query_state *inout_query_state);


int mdb_query(md_commit *commit, 
              const mdb_db *db,
              const bn_binding_array *node_names,
              const uint32_array *node_name_ids,
              const array *node_settings, /* XXX bn_request_per_node_settings */
              tbool msg_wait_for_response,
              tbool msg_nonbarrier,
              tbool msg_force_nonbarrier,

              uint32 *ret_return_code,
              tstring **ret_return_message,
              bn_binding_array **ret_bindings,
              uint32_array **ret_binding_node_ids);


int mdb_get_node(md_commit *commit, const mdb_db *db, const char *node_name, 
                 mdb_db_query_flags_bf db_query_flags, mdb_node **ret_node);

int mdb_node_to_binding(const mdb_node *node,
                        uint32_array *attrib_ids,
                        bn_binding **ret_binding);

int mdb_node_get_binding(const mdb_node *node, 
                         const bn_binding **ret_binding);

const tstring *mdb_node_get_name_quick(const mdb_node *node);

const tstr_array *mdb_node_get_name_parts_quick(const mdb_node *node);

int mdb_node_get_attribs(const mdb_node *node, 
                         const bn_attrib_array **ret_attribs);

const bn_attrib_array *mdb_node_get_attribs_quick(const mdb_node *node);

/** @name Database Queries */

/*@{*/

/**
 * Query a single node and get the result as a binding.
 * \param commit Commit structure, serving as a context for this query.
 * \param db Database to query.
 * \param node_name Name of node to query.  This can be a configuration
 * node, in which case it will be serviced directly by the mgmtd
 * infrastructure; or a monitoring node, in which case it will be 
 * handled as a query would normally be handled, possibly calling a
 * module's monitoring get function or sending a request to an external
 * provider and waiting for the response.
 * \param db_query_flags A bit field of ::mdb_db_query_flags ORed together.
 * \retval ret_binding The binding retrieved from the query, if successful.
 */
int mdb_get_node_binding(md_commit *commit,
                         const mdb_db *db,
                         const char *node_name, 
                         mdb_db_query_flags_bf db_query_flags, 
                         bn_binding **ret_binding);

/**
 * Get the value of a single node in string form.
 * \param commit Commit structure, serving as a context for this query.
 * \param db Database to query.
 * \param node_name Name of node to query: can be config or monitoring.
 * \param db_query_flags A bit field of ::mdb_db_query_flags ORed together.
 * \retval ret_found Indicates whether or not the node was found.
 * \retval ret_value If the node was found, this returns the value of the 
 * specified node, in string form.  IMPORTANT NOTE: if the node is not 
 * found, this is NOT set to NULL.  If the pointer previously pointed
 * to a value, that will be left alone.
 */
int mdb_get_node_value_tstr(md_commit *commit, const mdb_db *db,
                            const char *node_name, 
                            mdb_db_query_flags_bf db_query_flags,
                            tbool *ret_found, tstring **ret_value);

/**
 * Get an array of strings out of the database.  The strings are taken
 * from the nodes named \<root_name\>/ * /\<child_name\>.
 *
 * \param commit The opaque commit structure provided by the
 * management daemon to hold per-request context.
 *
 * \param db The name of the database to query, or NULL for the active one.
 *
 * \param root_name The name of the parent node of the wildcard containing
 * the array.
 *
 * \param child_name A name relative to the wildcard that identifies
 * which nodes' values should be put into the array.  If child_name is
 * NULL, the values of the wildcards themselves (which are the same as
 * the last component of their names) are used.
 *
 * \param use_indices If \c true, it is assumed that the wildcard is
 * an unsigned integer type, and the numbers that are the wildcard
 * instance names will be used as the array indices of the elements
 * added.  If the wildcard is not an unsigned integer type, an error
 * will be returned.  If this parameter is \c false, the elements will
 * all be added with consecutive indices starting from 0, in order
 * sorted by their index number.
 *
 * \param db_query_flags A bit field of #mdb_db_query_flags ORed together.
 *
 * \retval ret_arr A tstr_array containing string representations of
 * the values of all of the matching nodes found.
 *
 */
int mdb_get_tstr_array(md_commit *commit, const mdb_db *db,
                       const char *root_name, 
                       const char *child_name, tbool use_indices,
                       mdb_db_query_flags_bf db_query_flags, 
                       tstr_array **ret_arr);

/**
 * Get an array of strings containing the full names of all bindings
 * matching a specified pattern.
 *
 * Note that it is not necessary to pass in the mdqf_sel_iterate_subtree
 * query flag.  This is added automatically by the function, as it is
 * required for this to function properly.
 *
 * \param commit The opaque commit structure provided by the
 * management daemon to hold per-request context.
 *
 * \param db The name of the database to query, or NULL for the active one.
 *
 * \param pattern_name 
 *
 * \param db_query_flags A bit field of #mdb_db_query_flags ORed together.
 *
 * \retval ret_arr An array of the names of all bindings matching the
 * specified pattern.
 */
int mdb_get_matching_tstr_array(md_commit *commit, const mdb_db *db,
                                const char *pattern_name, 
                                mdb_db_query_flags_bf db_query_flags, 
                                tstr_array **ret_arr);

/**
 * Get an array of strings containing string representations of the
 * values of a specified attribute (probably usually ba_value) of 
 * all bindings matching a specified pattern.
 *
 * Note that it is not necessary to pass in the mdqf_sel_iterate_subtree
 * query flag.  This is added automatically by the function, as it is
 * required for this to function properly.
 *
 * \param commit The opaque commit structure provided by the
 * management daemon to hold per-request context.
 *
 * \param db The name of the database to query, or NULL for the active one.
 *
 * \param pattern_name 
 *
 * \param db_query_flags A bit field of #mdb_db_query_flags ORed together.
 *
 * \param attrib_id The attribute ID of the attribute to fetch, or 0
 * (ba_NONE) to fetch the names of the bindings.  Passing 0 here is
 * the same as calling mdb_get_matching_tstr_array().
 *
 * \retval ret_arr Values of the specified attribute of all bindings
 * matching the pattern.
 */
int mdb_get_matching_tstr_array_attrib(md_commit *commit, const mdb_db *db,
                                       const char *pattern_name, 
                                       mdb_db_query_flags_bf db_query_flags, 
                                       bn_attribute_id attrib_id,
                                       tstr_array **ret_arr);

/**
 * Return an array of all children of a specified node, in the
 * form of ::bn_binding objects.
 *
 * \param commit The opaque commit structure provided by the
 * management daemon to hold per-request context.
 *
 * \param db The name of the database to query, or NULL for the active one.
 *
 * \param node_name The name of the node whose children to return.
 *
 * \param db_query_flags A bit field of #mdb_db_query_flags ORed together.
 *
 * \retval ret_binding_array An array of ::bn_binding containing the children
 * of the specified node.
 */
int mdb_iterate_binding(md_commit *commit, const mdb_db *db,
                        const char *node_name, 
                        mdb_db_query_flags_bf db_query_flags, 
                        bn_binding_array **ret_binding_array);

/**
 * A more flexible version of mdb_iterate_binding() which supports
 * advanced queries.  See ::bn_query_node_data for more detail on what
 * you can put in such a query.  Use bn_query_node_data_new() to create
 * a new structure, fill it out, and call this function.  It will take
 * ownership of the bn_query_node_data structure you pass.
 */
int mdb_iterate_binding_full_takeover(md_commit *commit, const mdb_db *db,
                                    mdb_db_query_flags_bf db_query_flags, 
                                    bn_query_node_data **inout_query_node_data,
                                    bn_binding_array **ret_binding_array);

int mdb_iterate_binding_cb(md_commit *commit, const mdb_db *db,
                           const char *node_name, 
                           mdb_db_query_flags_bf db_query_flags, 
                           mdb_iterate_binding_cb_func iterate_cb, 
                           void *cb_arg);

/**
 * Call the provided callback once for every node matching the 
 * binding name pattern provided.
 *
 * Note that it is not necessary to pass in the mdqf_sel_iterate_subtree
 * query flag.  This is added automatically by the function, as it is
 * required for this to function properly.
 */
int mdb_foreach_binding_cb(md_commit *commit, const mdb_db *db,
                           const char *pattern,
                           mdb_db_query_flags_bf db_query_flags, 
                           mdb_iterate_binding_cb_func iterate_cb, 
                           void *cb_arg);

/* names_to_add must be NULL terminated */
int mdb_get_child_binding(md_commit *commit, const mdb_db *db,
                          const char *names_to_add[],
                          const bn_binding *parent_binding,
                          mdb_db_query_flags_bf db_query_flags, 
                          bn_binding **ret_binding);

/**
 * Fetch a node that is the child of a provided binding, and return a
 * string representation of its value.  If the child cannot be
 * fetched, NULL is returned.
 */
int mdb_get_child_value_tstr(md_commit *commit, const mdb_db *db,
                             const char *child_rel_name,
                             const bn_binding *parent_binding,
                             mdb_db_query_flags_bf db_query_flags, 
                             tstring **ret_value);

int mdb_get_node_binding_fmt(md_commit *commit, const mdb_db *db, 
                             mdb_db_query_flags_bf db_query_flags, 
                             bn_binding **ret_binding,
                             const char *node_name_fmt, ...)
     __attribute__ ((format (printf, 5, 6)));

int mdb_get_node_attrib(md_commit *commit, const mdb_db *db,
                        const char *node_name, 
                        mdb_db_query_flags_bf db_query_flags,
                        bn_attribute_id attrib_id,
                        bn_attrib **ret_attrib);

int mdb_get_node_attrib_fmt(md_commit *commit, const mdb_db *db, 
                            mdb_db_query_flags_bf db_query_flags,
                            bn_attribute_id attrib_id,
                            bn_attrib **ret_attrib,
                            const char *node_name_fmt, ...)
     __attribute__ ((format (printf, 6, 7)));

int mdb_get_node_attrib_tstr_fmt(md_commit *commit,
                                 const mdb_db *db,
                                 mdb_db_query_flags_bf db_query_flags,
                                 bn_attribute_id attrib_id,
                                 uint32 *ret_flagged_type,
                                 tstring **ret_tstr_value,
                                 const char *node_name_fmt, ...)
     __attribute__ ((format (printf, 7, 8)));

int mdb_get_node_nth_child_binding(md_commit *commit, const mdb_db *db,
                                   const char *node_name,
                                   mdb_db_query_flags_bf db_query_flags, 
                                   uint32 child_num,
                                   bn_binding **ret_binding);

/*@}*/

/**
 * Return an array of all children of a specified node, in the
 * form of mdb_node objects.
 *
 * \param commit The opaque commit structure provided by the
 * management daemon to hold per-request context.
 *
 * \param db The name of the database to query, or NULL for the active one.
 *
 * \param node_name The name of the node whose children to return.
 *
 * \param db_query_flags A bit field of #mdb_db_query_flags ORed together.
 *
 * \retval ret_node_array An array of mdb_node containing the children
 * of the specified node.
 */
int mdb_iterate_node(md_commit *commit, const mdb_db *db,
                     const char *node_name, 
                     mdb_db_query_flags_bf db_query_flags, 
                     mdb_node_array **ret_node_array);

int mdb_iterate_node_cb(md_commit *commit, const mdb_db *db,
                        const char *node_name, 
                        mdb_db_query_flags_bf db_query_flags, 
                        mdb_iterate_node_cb_func iterate_cb, void *cb_arg);

/**
 * Read a binding from a source database, and set the same binding
 * into a destination database, potentially under a different node
 * name.  If you want to use the same node name, pass NULL for
 * dest_node_name.
 */
int mdb_copy_node_binding(md_commit *commit,
                          const mdb_db *src_db,
                          const char *src_node_name,
                          mdb_db *dest_db,
                          const char *dest_node_name);

/**
 * Like mdb_get_child_value_tstr(), instantiated for every native
 * non-string data type.  One important difference, however, is that
 * if the node cannot be found, lc_err_unexpected_null is returned.
 * (This is because unlike strings, there is no value that can be
 * returned that is not also a valid instance of the type.)
 *
 * XXX/EMT: we wish we could return a 'found' boolean instead, and not
 * use an error return code, which is supposed to be only for internal
 * errors.  Or at least use lc_err_not_found as the error code.
 * But we don't want to break callers, or define an alternate function.
 */
#define MDB_GET_CHILD_VALUE_PROTO(TYPE, RETVALDECL) \
int mdb_get_child_value_ ## TYPE(md_commit *commit, const mdb_db *db, \
                                 const char *child_rel_name, \
                                 const bn_binding *parent_binding, \
                                 mdb_db_query_flags_bf db_query_flags, \
                                 RETVALDECL)

#define MDB_GET_CHILD_VALUE_SUFFIX_PROTO(TYPE, SUFFIX, RETVALDECL)       \
int mdb_get_child_value_ ## TYPE ## _ ## SUFFIX(md_commit *commit, const mdb_db *db, \
                                 const char *child_rel_name, \
                                 const bn_binding *parent_binding, \
                                 mdb_db_query_flags_bf db_query_flags, \
                                 RETVALDECL)

#define MDB_GET_CHILD_VALUE_PROTO_SIMPLE(TYPE) \
        MDB_GET_CHILD_VALUE_PROTO(TYPE, TYPE *ret_value)

MDB_GET_CHILD_VALUE_PROTO_SIMPLE(uint8);
MDB_GET_CHILD_VALUE_PROTO_SIMPLE(int8);
MDB_GET_CHILD_VALUE_PROTO_SIMPLE(uint16);
MDB_GET_CHILD_VALUE_PROTO_SIMPLE(int16);
MDB_GET_CHILD_VALUE_PROTO_SIMPLE(uint32);
MDB_GET_CHILD_VALUE_PROTO_SIMPLE(int32);
MDB_GET_CHILD_VALUE_PROTO_SIMPLE(uint64);
MDB_GET_CHILD_VALUE_PROTO_SIMPLE(int64);
MDB_GET_CHILD_VALUE_PROTO_SIMPLE(float32);
MDB_GET_CHILD_VALUE_PROTO_SIMPLE(float64);
MDB_GET_CHILD_VALUE_PROTO_SIMPLE(tbool);
MDB_GET_CHILD_VALUE_PROTO_SIMPLE(char);
#ifndef TMSLIBS_DISABLE_IPV4_UINT32
MDB_GET_CHILD_VALUE_PROTO(ipv4addr, uint32 *);
#endif
MDB_GET_CHILD_VALUE_SUFFIX_PROTO(ipv4addr, bnipv4, bn_ipv4addr *);
MDB_GET_CHILD_VALUE_SUFFIX_PROTO(ipv4addr, inaddr, struct in_addr *);
MDB_GET_CHILD_VALUE_SUFFIX_PROTO(ipv4addr, inetaddr, bn_inetaddr *);
#ifndef TMSLIBS_DISABLE_IPV4_UINT32
MDB_GET_CHILD_VALUE_PROTO(ipv4prefix, uint64 *);
#endif
MDB_GET_CHILD_VALUE_SUFFIX_PROTO(ipv4prefix, bnipv4, bn_ipv4prefix *);
MDB_GET_CHILD_VALUE_PROTO(macaddr802, uint8 ret_value[6]);
MDB_GET_CHILD_VALUE_PROTO(ipv6addr, bn_ipv6addr *);
MDB_GET_CHILD_VALUE_SUFFIX_PROTO(ipv6addr, bnipv6, bn_ipv6addr *); /* Alias */
MDB_GET_CHILD_VALUE_SUFFIX_PROTO(ipv6addr, in6addr, struct in6_addr *);
MDB_GET_CHILD_VALUE_SUFFIX_PROTO(ipv6addr, inetaddr, bn_inetaddr *);
MDB_GET_CHILD_VALUE_PROTO(ipv6addrz, bn_ipv6addrz *);
MDB_GET_CHILD_VALUE_PROTO(ipv6prefix, bn_ipv6prefix *);
MDB_GET_CHILD_VALUE_PROTO(inetaddr, bn_inetaddr *);
MDB_GET_CHILD_VALUE_SUFFIX_PROTO(inetaddr, ipv4addr, bn_ipv4addr *);
MDB_GET_CHILD_VALUE_SUFFIX_PROTO(inetaddr, ipv6addr, bn_ipv6addr *);
MDB_GET_CHILD_VALUE_PROTO(inetaddrz, bn_inetaddrz *);
MDB_GET_CHILD_VALUE_SUFFIX_PROTO(inetaddrz, ipv6addrz, bn_ipv6addrz *);
MDB_GET_CHILD_VALUE_PROTO(inetprefix, bn_inetprefix *);
MDB_GET_CHILD_VALUE_SUFFIX_PROTO(inetprefix, ipv4prefix, bn_ipv4prefix *);
MDB_GET_CHILD_VALUE_SUFFIX_PROTO(inetprefix, ipv6prefix, bn_ipv6prefix *);
MDB_GET_CHILD_VALUE_PROTO(date, lt_time_sec *);
MDB_GET_CHILD_VALUE_PROTO(time_sec, lt_time_sec *);
MDB_GET_CHILD_VALUE_PROTO(time_ms, lt_time_ms *);
MDB_GET_CHILD_VALUE_PROTO(time_us, lt_time_us *);
MDB_GET_CHILD_VALUE_PROTO(datetime_sec, lt_time_sec *);
MDB_GET_CHILD_VALUE_PROTO(datetime_ms, lt_time_ms *);
MDB_GET_CHILD_VALUE_PROTO(datetime_us, lt_time_us *);
MDB_GET_CHILD_VALUE_PROTO(duration_sec, lt_dur_sec *);
MDB_GET_CHILD_VALUE_PROTO(duration_ms, lt_dur_ms *);
MDB_GET_CHILD_VALUE_PROTO(duration_us, lt_dur_us *);
MDB_GET_CHILD_VALUE_PROTO(btype, bn_type *);
MDB_GET_CHILD_VALUE_PROTO(attribute, bn_attribute_id *);
MDB_GET_CHILD_VALUE_PROTO(binary, tbuf **);
int mdb_get_child_value_oid(md_commit *commit, const mdb_db *db,
                            const char *child_rel_name,
                            const bn_binding *parent_binding,
                            mdb_db_query_flags_bf db_query_flags,
                            uint32 max_oid_len, 
                            uint32 ret_oid[], 
                            uint32 *ret_oid_len, 
                            tbool *ret_oid_abs);

/** @name Database Sets */

/*@{*/

/**
 * Create a binding and call mdb_set_node_binding for each member of 
 * the values array.
 */
int mdb_create_node_bindings(md_commit *commit, mdb_db *db,
                             const bn_str_value values[],
                             uint32 num_values);

int mdb_set_node_binding(md_commit *commit, mdb_db *db,
                         bn_set_subop subop,
                         mdb_db_set_flags_bf db_set_flags,
                         const bn_binding *binding);


int mdb_set_node_attribs(md_commit *commit, mdb_db *db,
                         bn_set_subop subop,
                         mdb_db_set_flags_bf db_set_flags,
                         const char *node_name, 
                         const bn_attrib_array *attribs);

int mdb_set_node_str(md_commit *commit, mdb_db *db, bn_set_subop subop,
                     mdb_db_set_flags_bf db_set_flags, bn_type btype,
                     const char *value, const char *name_format, ...)
     __attribute__ ((format (printf, 7, 8)));

int mdb_set_node_tbool(md_commit *commit, mdb_db *db, bn_set_subop subop,
                       mdb_db_set_flags_bf db_set_flags, const char *name,
                       tbool value);

/**
 * Delete all children of the specified node.
 */
int mdb_delete_child_nodes(md_commit *commit, mdb_db *db,
                           mdb_db_set_flags_bf db_set_flags,
                           const char *parent_name);

/* Reparent / rename */

/**
 * Rename a node, and/or move it to a different location in the tree.
 *
 * Unless called from an upgrade function (where registration
 * constraints are not enforced), this is usually only used to rename
 * nodes, since there is usually not another parent whose descendants
 * have exactly the same registration as the descendants of the
 * original parent.
 *
 * \param commit
 *
 * \param db
 *
 * \param subop Always pass bsso_reparent here.
 *
 * \param db_set_flags Bit field of #mdb_db_set_flags ORed together
 * (though currently none of these flags are relevant to reparent,
 * so you should always pass zero).
 *
 * \param source_node_name Absolute path of node to rename.  Note that
 * because the db is held in a tree data structure, this automatically
 * (though indirectly) impacts all of the binding's descendants as well.
 *
 * \param graft_under_name If the binding is to be reparented, this is
 * the absolute path to its new parent.  If the binding is only being 
 * renamed, pass NULL here.
 *
 * \param new_self_name If the binding is to be renamed, this the last
 * binding name component of its new name.  If the last component is 
 * to remain the same, pass NULL here.
 *
 * \param self_value_follows_name Should the value of the binding
 * being reparented/renamed be updated to reflect its new name?  This
 * should generally only be true of (a) you have passed something for
 * new_self_name, and (b) the binding node is a wildcard instance,
 * whose value must match the last component of its binding name.
 * 
 */
int mdb_reparent_node(md_commit *commit, mdb_db *db,
                      bn_set_subop subop,
                      mdb_db_set_flags_bf db_set_flags,
                      const char *source_node_name,
                      const char *graft_under_name,
                      const char *new_self_name,
                      tbool self_value_follows_name);

/**
 * Applicable to wildcard nodes of integer data type.  Call this
 * function on the parent of such a node to have the wildcard children
 * reparented (as necessary) to ensure that their numbers are all
 * consecutive.
 *
 * e.g. if "/a/b/ *" was of type uint32, and if the database had
 * "/a/b/1", "/a/b/3", and "/a/b/4", then reparents would be done to
 * renumber the latter two to be "/a/b/2" and "/a/b/3", respectively.
 *
 * \param commit
 *
 * \param db
 *
 * \param subop Always pass bsso_reparent here.
 *
 * \param db_set_flags Bit field of #mdb_db_set_flags ORed together
 * (though currently none of these flags are relevant to reparent,
 * so you should always pass zero).
 *
 * \param parent_node_name Name of parent of wildcard nodes to compact.
 * Note that the children must be of an unsigned integer data type.
 *
 * \param start_idx The index number to which to start the numbering
 * of the children.  Usually you would just pass 0 or 1 here.  NOTE:
 * we do not currently support shifting nodes UP in case there are any
 * numbered lower than this number.  This number is solely used right
 * now to determine what is the lowest we will renumber a node DOWN to.
 */
int mdb_reparent_compact_children(md_commit *commit, mdb_db *db,
                                  bn_set_subop subop, 
                                  mdb_db_set_flags_bf db_set_flags,
                                  const char *parent_node_name,
                                  uint32 start_idx);
                                  

/*@}*/

/** @name Whole Database Manipulation
 *
 * Note: the functions in this section are generally for
 * infrastructure use only.
 */

/*@{*/

/**
 * Create a new database.
 * \param db_name Name of the database.  This will be the filename it 
 * will be given when it is stored to disk.  It cannot have '/' in it,
 * since all config files go into the same directory.
 * \param reg_state Registration state for the nodes in the database,
 * generally derived from mgmtd module registrations.
 * \retval ret_db The newly-created database.
 */
int mdb_new(mdb_db **ret_db, const char *db_name,
            mdb_reg_state *reg_state);

/**
 * Free the memory associated with a database.
 * \param inout_db The database to be freed.  The caller's pointer is set
 * to NULL to avoid accidental references to the dangling pointer.
 */
int mdb_free(mdb_db **inout_db);

/**
 * Make a copy of a database.
 * \param old_db The database to be copied.
 * \retval ret_db The new database.  This is dynamically allocated and
 * must also be freed by the caller, separately from the original db.
 */
int mdb_duplicate(const mdb_db *old_db, mdb_db **ret_db);

/**
 * Create a new database whose contents are the initial config database.
 * \retval ret_db The newly-created database.
 */
int mdb_create_initial_db(mdb_db **ret_db);

/*@}*/

/*
 * Native type functions for getting values out of databases.  
 *
 * These are like mdb_get_node_binding(), except they return the native
 * type instead of a bn_binding.
 */


#define MDB_GET_NODE_VALUE_PROTO(TYPE, RETVALDECL) \
int mdb_get_node_value_ ## TYPE (md_commit *commit, const mdb_db *db, \
                                 const char *node_name, \
                                 mdb_db_query_flags_bf db_query_flags, \
                                 tbool *ret_found, \
                                 RETVALDECL)

#define MDB_GET_NODE_VALUE_SUFFIX_PROTO(TYPE, SUFFIX, RETVALDECL)        \
int mdb_get_node_value_ ## TYPE ## _ ## SUFFIX (md_commit *commit, const mdb_db *db, \
                                 const char *node_name, \
                                 mdb_db_query_flags_bf db_query_flags, \
                                 tbool *ret_found, \
                                 RETVALDECL)

#define MDB_GET_NODE_VALUE_PROTO_SIMPLE_NUMERIC(TYPE)                  \
    MDB_GET_NODE_VALUE_PROTO(TYPE, TYPE *ret_value)

#define MDB_GET_NODE_VALUE_PROTO_SIMPLE(TYPE, NTYPE)                   \
    MDB_GET_NODE_VALUE_PROTO(TYPE, NTYPE *ret_value)

MDB_GET_NODE_VALUE_PROTO_SIMPLE_NUMERIC(uint8);
MDB_GET_NODE_VALUE_PROTO_SIMPLE_NUMERIC(int8);
MDB_GET_NODE_VALUE_PROTO_SIMPLE_NUMERIC(uint16);
MDB_GET_NODE_VALUE_PROTO_SIMPLE_NUMERIC(int16);
MDB_GET_NODE_VALUE_PROTO_SIMPLE_NUMERIC(uint32);
MDB_GET_NODE_VALUE_PROTO_SIMPLE_NUMERIC(int32);
MDB_GET_NODE_VALUE_PROTO_SIMPLE_NUMERIC(uint64);
MDB_GET_NODE_VALUE_PROTO_SIMPLE_NUMERIC(int64);
MDB_GET_NODE_VALUE_PROTO_SIMPLE_NUMERIC(tbool);
MDB_GET_NODE_VALUE_PROTO_SIMPLE_NUMERIC(float32);
MDB_GET_NODE_VALUE_PROTO_SIMPLE_NUMERIC(float64);
MDB_GET_NODE_VALUE_PROTO_SIMPLE_NUMERIC(char);
#ifndef TMSLIBS_DISABLE_IPV4_UINT32
MDB_GET_NODE_VALUE_PROTO_SIMPLE(ipv4addr, uint32);
#endif
MDB_GET_NODE_VALUE_SUFFIX_PROTO(ipv4addr, bnipv4, bn_ipv4addr *);
MDB_GET_NODE_VALUE_SUFFIX_PROTO(ipv4addr, inaddr, struct in_addr *);
MDB_GET_NODE_VALUE_SUFFIX_PROTO(ipv4addr, inetaddr, bn_inetaddr *);
#ifndef TMSLIBS_DISABLE_IPV4_UINT32
MDB_GET_NODE_VALUE_PROTO_SIMPLE(ipv4prefix, uint64);
#endif
MDB_GET_NODE_VALUE_SUFFIX_PROTO(ipv4prefix, bnipv4, bn_ipv4prefix *);
MDB_GET_NODE_VALUE_PROTO(macaddr802, uint8 value[6]);
MDB_GET_NODE_VALUE_PROTO(ipv6addr, bn_ipv6addr *);
MDB_GET_NODE_VALUE_SUFFIX_PROTO(ipv6addr, bnipv6, bn_ipv6addr *); /* Alias */
MDB_GET_NODE_VALUE_SUFFIX_PROTO(ipv6addr, in6addr, struct in6_addr *);
MDB_GET_NODE_VALUE_SUFFIX_PROTO(ipv6addr, inetaddr, bn_inetaddr *);
MDB_GET_NODE_VALUE_PROTO(ipv6addrz, bn_ipv6addrz *);
MDB_GET_NODE_VALUE_PROTO(ipv6prefix, bn_ipv6prefix *);
MDB_GET_NODE_VALUE_PROTO(inetaddr, bn_inetaddr *);
MDB_GET_NODE_VALUE_SUFFIX_PROTO(inetaddr, ipv4addr, bn_ipv4addr *);
MDB_GET_NODE_VALUE_SUFFIX_PROTO(inetaddr, ipv6addr, bn_ipv6addr *);
MDB_GET_NODE_VALUE_PROTO(inetaddrz, bn_inetaddrz *);
MDB_GET_NODE_VALUE_SUFFIX_PROTO(inetaddrz, ipv6addrz, bn_ipv6addrz *);
MDB_GET_NODE_VALUE_PROTO(inetprefix, bn_inetprefix *);
MDB_GET_NODE_VALUE_SUFFIX_PROTO(inetprefix, ipv4prefix, bn_ipv4prefix *);
MDB_GET_NODE_VALUE_SUFFIX_PROTO(inetprefix, ipv6prefix, bn_ipv6prefix *);
MDB_GET_NODE_VALUE_PROTO_SIMPLE(date, lt_time_sec);
MDB_GET_NODE_VALUE_PROTO_SIMPLE(time_sec, lt_time_sec);
MDB_GET_NODE_VALUE_PROTO_SIMPLE(time_ms, lt_time_ms);
MDB_GET_NODE_VALUE_PROTO_SIMPLE(time_us, lt_time_us);
MDB_GET_NODE_VALUE_PROTO_SIMPLE(datetime_sec, lt_time_sec);
MDB_GET_NODE_VALUE_PROTO_SIMPLE(datetime_ms, lt_time_ms);
MDB_GET_NODE_VALUE_PROTO_SIMPLE(datetime_us, lt_time_us);
MDB_GET_NODE_VALUE_PROTO_SIMPLE(duration_sec, lt_dur_sec);
MDB_GET_NODE_VALUE_PROTO_SIMPLE(duration_ms, lt_dur_ms);
MDB_GET_NODE_VALUE_PROTO_SIMPLE(duration_us, lt_dur_us);
MDB_GET_NODE_VALUE_PROTO(btype, bn_type *);
MDB_GET_NODE_VALUE_PROTO(attribute, bn_attribute_id *);
MDB_GET_NODE_VALUE_PROTO(binary, tbuf **);
int mdb_get_node_value_oid(md_commit *commit, const mdb_db *db,
                           const char *node_name,
                           mdb_db_query_flags_bf db_query_flags,
                           tbool *ret_found,
                           uint32 max_oid_len, 
                           uint32 ret_oid[], 
                           uint32 *ret_oid_len, 
                           tbool *ret_oid_abs);

/**
 * Extract the commit and db out of an expression context structure.
 * This is for use by custom query expression functions, registered
 * using the graft points in mdb_adv_query.c, who want to access the
 * database.
 */
int mdb_get_query_context(const ltx_context *context, md_commit **ret_commit,
                          const mdb_db **ret_db);

/**
 * Tell if the specified database is a special, incremental db.  This is
 * only used internally.
 */
int mdb_db_incremental_get(const mdb_db *db, tbool *ret_incremental);

/**
 * Set the flag saying whether or not a database is a special,
 * incremental db.  This is only used internally, and should not be used
 * by customers.
 */
int mdb_db_incremental_set(mdb_db *db, tbool incremental);

/**
 * Tell if the specified database is set to do backups on save.
 */
int mdb_db_backup_get(const mdb_db *db, tbool *ret_do_backup);

/**
 * Set the flag saying whether or not a backup should be made whenever
 * the database is saved.
 */
int mdb_db_backup_set(mdb_db *db, tbool do_backup);

#ifdef __cplusplus
}
#endif

#endif /* __MDB_DB_H_ */
