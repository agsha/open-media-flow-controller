/*
 *
 * mdb_changes.h
 *
 *
 *
 */

#ifndef __MDB_CHANGES_H_
#define __MDB_CHANGES_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif


#include "common.h"
#include "bnode.h"
#include "bnode_proto.h"
#include "mdb_db.h"

/**
 * \file mdb_changes.h API for working with database changes
 * \ingroup mdb
 *
 * This file contains definitions and APIs for working with records
 * that describe changes to database nodes.
 */

/**
 * Type of change to a database node.
 *
 * NOTE: mdb_db_change_type_map (in mdb_changes.c) must be kept in sync.
 */
typedef enum {
    mdct_NONE =   0,

    /** A new node was added to the database */
    mdct_add =    1,

    /** An existing node in the database was changed */
    mdct_modify = 2,

    /** A node was removed from the database */
    mdct_delete = 3,

    /**
     * Some change was made to one or more nodes matching the
     * registration pattern expressed by the name.  This will only 
     * show up on nodes registered with the mrf_change_no_notify flag.
     */
    mdct_unspecified = 4,

} mdb_db_change_type;

extern const lc_enum_string_map mdb_db_change_type_map[];

/**
 * Describes a change to a single node in the database.
 */
typedef struct mdb_db_change {

    /** Name of the node that was changed */
    tstring *mdc_name;

    /** Name of the node, broken down into parts */
    tstr_array *mdc_name_parts;

    /** Type of change for this node */
    mdb_db_change_type mdc_change_type;

    /** 
     * Attributes that the node had before the change.
     * If mdc_change_type is mdct_add, this will be NULL.
     */
    bn_attrib_array *mdc_old_attribs;

    /**
     * Attributes that the node has now, after the change.
     * If mdc_change_type is mdct_delete, this will be NULL.
     */
    bn_attrib_array *mdc_new_attribs;

    /**
     * Holds opaque information about the change, used only by
     * the infrastructure.
     */
    void *mdc_opaque; /* Of type 'mdb_db_change_extras *' */
} mdb_db_change;

TYPED_ARRAY_HEADER_NEW_NONE(mdb_db_change, mdb_db_change *);

/** Create a new array of 'mdb_db_change *' */
int mdb_db_change_array_new(mdb_db_change_array **ret_arr);

int mdb_db_change_compare(const mdb_db_change *node1, 
                          const mdb_db_change *node2);
int mdb_db_change_compare_for_tree(const void *node1, const void *node2);
int mdb_db_change_free(mdb_db_change **ret_node);
int mdb_db_change_dup(const mdb_db_change *node, mdb_db_change **ret_node);
int mdb_db_change_dup_for_tree(const void *src_value, void **ret_dest_value);
void mdb_db_change_free_for_tree(void *value);


/*
 * Generate a list of changes by computing the differences between two
 * databases.  This involves walking both databases, which could be
 * slow, but is the only option when the databases are independent.
 */
int mdb_compute_change_list(const mdb_db *old_db, const mdb_db *new_db,
                            mdb_db_change_array **ret_change_list);

/*
 * Generate a list of changes to a database since its last new layer
 * was pushed, by traversing the top layer.  This can be much faster
 * than mdb_compute_change_list() for large databases, since it only
 * needs to traverse the nodes that have changed.
 */
int mdb_compute_change_list_layered(mdb_db *db,
                                    mdb_db_change_array **ret_change_list);

#ifdef __cplusplus
}
#endif

#endif /* __MDB_CHANGES_H_ */
