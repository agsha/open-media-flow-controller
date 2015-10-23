/*
 *
 * mdb_dbbe.h
 *
 *
 *
 */

#ifndef __MDB_DBBE_H_
#define __MDB_DBBE_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ============================================================================
 * NOTE: this API is NOT for use by mgmtd modules!  It is for the use
 * of the mgmtd infrastructure.
 *
 * The only case where a module might use this API is to read from the
 * manufacturing database.  But it should not use it on the config db,
 * or use it for writing in any case.
 * ============================================================================
 */

#include "common.h"
#include "mdb_db.h"
#include "tree.h"
#include "ltree.h"

typedef enum {
    dbbet_none = 0,
    dbbet_file_mem_tree = 1,
    dbbet_mem_tree,
} mdb_dbbe_type;


/*
 * Meta info about the db
 *
 * XXX/EMT: currently none of these fields are ever used.
 */
typedef struct mdb_dbbe_info {
    mdb_dbbe_type mdi_type;
    uint32 mdi_type_format_revision;
    uint32 mdi_db_node_count;
    uint32 mdi_db_revision_id;
    uint32 mdi_db_modify_time;
    uint8 mdi_db_hash[32];
} mdb_dbbe_info;

/*
 * Db header flags.  Later these could potentially be used for
 * compression, format type, etc.  Right now mdff_format_binary is the
 * only specified flag, and is always used.
 */
typedef enum {
    mdff_UNUSED_1 =        1 << 1,
    mdff_UNUSED_2 =        1 << 2,

    /* This is the only flag normally used, and should always be set */
    mdff_format_binary =   1 << 3,

    mdff_UNUSED_4 =        1 << 4,

    /*
     * This is an internal flag used by clusterd to represent an
     * incremental database, which may have adds, modifies and deletes
     * on top of some baseline db.
     */
    mdff_type_incremental = 1 << 9,

} mdb_dbbe_fh_flags;


static const char mdf_file_ident[] = "TMS_CONF\032\004";
static const char mdf_file_pad[] = "\n\r\r\n";
enum { mdf_file_version = 1};

typedef struct mdb_dbbe_file_header {
    uint8  mfh_ident[12];      /* Always "TMS_CONF..\0\0". */
    uint32 mfh_version;
    uint32 mfh_file_length;   /* total file length in bytes */
    uint32 mfh_hdr_length;
    uint32 mfh_fh_flags;      /* Used for compression, format */

    uint32 mfh_node_count;
    uint32 mfh_uncompressed_length;
    uint32 mfh_revision_id;
    uint8 mfh_hash[32];
    uint8 mfh_pad[4];
} __attribute__((packed)) mdb_dbbe_file_header;

static const char mdb_dbbe_active_name[] = "active";
static const char mdb_dbbe_initial_name[] = "initial";
static const char mdb_dbbe_active_bak_name[] = "active.bak";

int mdb_dbbe_new_init(mdb_db *inout_db, const char *db_name);
int mdb_dbbe_free(mdb_db *inout_db);
int mdb_dbbe_duplicate(const mdb_db *old_db, mdb_db *ret_db);

/**
 * Create a new mdb_db that will represent a set of potential changes
 * to the provided mdb_db, and temporarily modify the provided mdb_db
 * so it will not "see" those changes.  Specifically, this entails:
 *   - Set the provided db as read-only.
 *   - Set the provided db to use the current top layer as its topmost
 *     layer for all subsequent calls, until further notice.
 *   - Push a new layer onto the stack in the ltree of the provided db.
 *   - Create a new db that is a clone of the provided db, except that
 *     it has a reference to the provided db's ltree, instead of its
 *     own copy.  This db will know it doesn't own its ltree.
 *
 * The result is that you now have two mdb_db structures, each with a
 * different view onto the same database.  When queried, the original
 * sees only a snapshot of its state at the time this API was called;
 * and the original cannot be modified.  The new mdb_db can be modified,
 * and sees all of the modifications.
 *
 * NOTE: because the original mdb_db is modified, you cannot just
 * clean up by freeing the new mdb_db.  You must clean up by calling
 * mdb_dbbe_finish_pending_changes(), which includes freeing the new
 * mdb_db.
 */
int mdb_dbbe_begin_pending_changes(mdb_db *orig_db, mdb_db **ret_changes_db);

/**
 * Clean up the temporary situation created by
 * mdb_dbbe_begin_pending_changes().
 *
 * If we are accepting the pending changes, we merge the new layer
 * created by mdb_dbbe_begin_pending_changes() down into the layer
 * below, making these changes permanent, and indistinguishable from
 * the nodes that existed before.
 *
 * If we are rejecting the changes, we pop this layer off the stack
 * and free it, without merging any changes.  The database goes back
 * to the exact state it was in before
 * mdb_dbbe_begin_pending_changes() was called.
 *
 * In either case, we restore orig_db to its original state in other
 * senses, i.e. making it read/write, and telling it to view whatever
 * is its topmost layer.  We also free the temp_db returned by
 * mdb_dbbe_begin_pending_changes().  We call it "temp_db" instead of
 * "changes_db" because mdb_pivot_pending_changes() may have changed
 * its disposition (XXX/EMT: abstraction break!)
 */
int mdb_dbbe_finish_pending_changes(mdb_db *orig_db, mdb_db **inout_temp_db,
                                    tbool accept_changes);

/**
 * Compact all layers in a database into a single layer.  This is
 * similar to what mdb_dbbe_finish_pending_changes() does (with
 * 'accept_changes' set to 'true'), except:
 *
 *   - That function expects two mdb_db structs, which both point to the
 *     same underlying database (one is dependent on the other); whereas
 *     this function expects to operate on a single independent mdb_db.
 *     This function will fail if this mdb_db does not own the underlying
 *     dbbe (mbe_own_tree must be 'true').
 *
 *   - That function assumes there is only one outstanding layer to be 
 *     compacted.  This one compacts as many layers as there are.
 */
int mdb_dbbe_compact_layers(mdb_db *db);

int mdb_dbbe_set_active_db(const char *db_name);
int mdb_dbbe_get_active_db(tstring **ret_db_name);

/*
 * NOTE: mdb_dbbe_open_flags_map (in mdb_dbbe.c) must be kept in sync.
 */
typedef enum {
    mdbof_none = 0,
    
    /**
     * When opening the database, all attributes of the following
     * types should be treated specially:
     *   - bt_ipv6addr
     *   - bt_ipv6addrz
     *   - bt_ipv6prefix
     *   - bt_inetaddr
     *   - bt_inetaddrz
     *   - bt_inetprefix
     *
     * These attributes will be converted to binary in the same manner
     * as they would have been if we had not recognized their bn_type
     * value.  This is meant to be used to convert a database which
     * might contain these new data types into a form which can be
     * read by older releases which would not otherwise be able to
     * open the file (Eucalyptus Update 4 without Overlay 12;
     * Eucalyptus Update 3 without Overlay 4; or Eucalyptus Update 2
     * and earlier).
     *
     * Specifically, we do what mdb_dbbe_save_unrecog_attrib() does:
     *   - Convert the binary representation of the attribute into a 
     *     bt_binary attribute.
     *   - Set the ::btf_unknown_converted type flag on this attribute.
     *   - Set the ::ba_unknown_attribs attribute on this binding, to 
     *     flag that there are unknown attributes present.
     */
    mdbof_ipv6_attribs_to_binary = 1 << 0,

    /**
     * By default (unless mdbof_ipv6_attribs_to_binary is set),
     * whenever we read an attribute with the ::btf_unknown_converted
     * type flag set, try to convert it back into a real attribute, as
     * long as we recognize the data type.  If we are able to convert
     * all such attributes on a given binding, delete the
     * ::ba_unknown_attribs attribute on that binding (if it exists).
     * This should fully undo the transformation performed by
     * ::mdof_ipv6_attribs_to_binary.
     *
     * This flag tells us NOT to do that.
     */
    mdbof_no_restore_unknown_attribs = 1 << 1,

    /*
     * By default, the returned db is created empty, and the specified
     * file is read into it.  If this flag is set, an existing database
     * must be provided in the db parameter, and the bindings in the
     * specified file are applied on top of the existing database.  The
     * bindings from the file may include nodes where the value (either
     * in ba_value or ba_encrypted_value) is marked deleted with
     * btf_deleted , and these nodes will be removed in the returned db.
     *
     * The database used in this case could be a normal saved db, or one
     * that had been marked incremental.
     */
    mdbof_overlay = 1 << 2,

} mdb_dbbe_open_flags;

typedef uint32 mdb_dbbe_open_flags_bf;

extern const lc_enum_string_map mdb_dbbe_open_flags_map[];

int mdb_dbbe_open(mdb_db **ret_db, mdb_reg_state *reg_state,
                  const char *db_name);

/*
 * Note that if mdbof_overlay is set in flags, '*inout_db' must be an
 * existing db to overlay on top of.  Otherwise 'inout_db' is purely a
 * return parameter.
 */
int mdb_dbbe_open_full(mdb_db **inout_db, mdb_reg_state *reg_state,
                       const char *db_name, mdb_dbbe_open_flags_bf flags);

int mdb_dbbe_close(mdb_db **db);
int mdb_dbbe_save(mdb_db *db);
int mdb_dbbe_save_to(mdb_db *db, const char *db_name, tbool switch_to);
int mdb_dbbe_create_empty_db(mdb_db *ret_db, const char *db_name);
int mdb_dbbe_get_db_info(const char *db_name, 
                         tbool *ret_found,
                         mdb_dbbe_info **ret_db_info);

/**
 * Get the contents of the database as a tbuffer, with exactly the
 * contents that would be saved to disk if we did mdb_dbbe_save().
 */
int mdb_dbbe_get_tbuf(const mdb_db *db, tbuf **ret_tb);

/*
 * Each db has a change id, which changes each time the nodes inside the
 * db are operated on by a change operation.  Note that it may change
 * when a set is done on a node for the same value that the node already
 * has.  The id space is shared between all the db's in this process.
 */
int mdb_dbbe_get_change_id(const mdb_db *db, uint64 *ret_change_id);

int mdb_dbbe_get_node(const md_commit *commit, const mdb_db *db,
                      const char *node_name, 
                      const mdb_node **ret_node);

/*
 * Look up a dbbe node by name parts.
 *
 * To use an ltree continuation cookie from a previous call, pass it
 * in 'cont_cookie'.  The node_name_parts here must be identical to the
 * node_name_parts from when the cookie was created, except for the last
 * node name part.
 *
 * NOTE: callers outside libmdb should always pass NULL for cont_cookie
 * and ret_new_cont_cookie.
 */
int mdb_dbbe_get_node_parts(const md_commit *commit, const mdb_db *db, 
                            const tstr_array *node_name_parts,
                            ltree_cont_cookie *cont_cookie,
                            ltree_cont_cookie **ret_new_cont_cookie,
                            const mdb_node **ret_node);

int mdb_dbbe_get_node_depth_parts(const md_commit *commit, const mdb_db *db,
                                  const tstr_array *node_name_parts,
                                  tbool get_partial,
                                  const mdb_node **ret_node,
                                  int32 *ret_match_depth);

int mdb_dbbe_get_num_children(const md_commit *commit, const mdb_db *db, 
                              const char *node_name, 
                              uint32 *ret_num_children);

int mdb_dbbe_iterate_names(const md_commit *commit, const mdb_db *db,
                           const char *node_name, 
                           mdb_db_query_flags_bf db_query_flags,
                           tbool full_names,
                           tstr_array **ret_name_array);

int mdb_dbbe_iterate_names_parts(const md_commit *commit, const mdb_db *db,
                                 const tstr_array *node_name_parts,
                                 mdb_db_query_flags_bf db_query_flags, 
                                 tbool full_names,
                                 tstr_array **ret_name_array);

typedef int (*mdb_dbbe_diff_cb_func)(const mdb_node *node1,
                                     const mdb_node *node2,
                                     void *arg);

int mdb_dbbe_diff(const md_commit *commit, const mdb_db *db1,
                  const char *node_name1, const mdb_db *db2,
                  const char *node_name2, mdb_db_query_flags_bf db_query_flags,
                  mdb_dbbe_diff_cb_func iterate_cb, void *cb_arg);

typedef int (*mdb_dbbe_dtl_cb_func)(const char *bname, 
                                    const tstr_array *bname_parts,
                                    const bn_attrib_array *old_attribs,
                                    const bn_attrib_array *new_attribs,
                                    tbool is_modified, tbool is_deleted,
                                    tbool maybe_dups,
                                    const mdb_node_reg *node_reg,
                                    void *arg);

/*
 * Like mdb_dbbe_diff(), except calls back the callback function for
 * every node difference expressed by the top layer of the provided
 * mdb_db, vs. the lower layers.  This will only work if the provided
 * mdb_db has at least two layers.
 */
int mdb_dbbe_diff_top_layer(const mdb_db *db, 
                            mdb_dbbe_dtl_cb_func iterate_cb, void *cb_arg);

/**
 * Make the subtree specified by root_name in dest_db match that found
 * in src_db.  That includes deleting it if src_db doesn't have that
 * subtree at all.
 */
int mdb_dbbe_sync_subtree(md_commit *commit,
                          const mdb_db *src_db, mdb_db *dest_db,
                          const char *root_name);

typedef int (*mdb_dbbe_iterate_node_cb_func)(const mdb_node *binding_node,
                                             void *reg_node_always_null,
                                             void *arg);

int mdb_dbbe_iterate_node(const md_commit *commit, const mdb_db *db,
                          const char *node_name,
                          mdb_db_query_flags_bf db_query_flags,
                          mdb_dbbe_iterate_node_cb_func iterate_cb,
                          void *cb_arg);


int mdb_dbbe_iterate_node_parts(const md_commit *commit, const mdb_db *db,
                                const tstr_array *node_name_parts,
                                mdb_db_query_flags_bf db_query_flags,
                                mdb_dbbe_iterate_node_cb_func iterate_cb,
                                void *cb_arg);

int mdb_dbbe_change_binding(md_commit *commit, mdb_db *db, 
                            bn_set_subop subop,
                            mdb_db_set_flags_bf db_set_flags,
                            const bn_binding *binding);

int mdb_dbbe_delete_node(md_commit *commit, mdb_db *db, 
                         const char *node_name);

int mdb_dbbe_update_reg(mdb_db *db);

/*
 * Query the read-only bit on this database.
 */
int mdb_dbbe_read_only_get(const mdb_db *db, tbool *ret_is_read_only);

/*
 * Set the read-only bit on this database.
 *
 * CAUTION: this is mainly for internal use only.  Do not set this value
 * unless you know what you're doing!  For example, some of the APIs which
 * use ltrees, like mdb_dbbe_begin_pending_changes() et al., set this flag
 * and expect it to remain how they left it.
 */
int mdb_dbbe_read_only_set(const mdb_db *db, tbool is_read_only);


#ifdef __cplusplus
}
#endif

#endif /* __MDB_DBBE_H_ */
