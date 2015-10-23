/*
 *
 * mdb_reg.h
 *
 *
 *
 */

#ifndef __MDB_REG_H_
#define __MDB_REG_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "mdb_db.h"

/* ------------------------------------------------------------------------- */
/**
 * Get all the initial attributes for a given registration node.
 *
 * \param db An open configuration database with registration state
 *
 * \param node_name The node to retrieve.  This may contain stars (*'s)
 *  at wildcard levels of the registered name.
 *
 * \retval ret_initial_attribs All the registered initial attributes for
 * the given node.  This will be NULL in cases where there are no
 * initial attributes registered, and no error will be returned.
 */
int mdr_reg_get_node_initial_attribs(const mdb_db *db,
                               const char *node_name,
                               const bn_attrib_array **ret_initial_attribs);

/* ------------------------------------------------------------------------- */
/**
 * Get all the initial attributes for a given registration node, by name parts.
 *
 * \param db An open configuration database with registration state
 *
 * \param node_name_parts The node to retrieve.  This may contain stars (*'s)
 *  at wildcard levels of the registered name.
 *
 * \retval ret_initial_attribs All the registered initial attributes for
 * the given node.  This will be NULL in cases where there are no
 * initial attributes registered, and no error will be returned.
 */
int mdr_reg_get_node_initial_attribs_parts(const mdb_db *db,
                               const tstr_array *node_name_parts,
                               const bn_attrib_array **ret_initial_attribs);


/* ------------------------------------------------------------------------- */
/**
 * Get the initial value for a given registration node.
 *
 * \param db An open configuration database with registration state
 *
 * \param node_name_parts The node to retrieve.  This may contain stars (*'s)
 *  at wildcard levels of the registered name.
 *
 * \retval ret_initial_value The registered initial value value atribute for
 * the given node.  This will be NULL in cases where there is no
 * initial value attribute registered, and no error will be returned.
 */
int mdr_reg_get_node_initial_value(const mdb_db *db,
                                   const char *node_name,
                                   const bn_attrib **ret_initial_value);

/* ------------------------------------------------------------------------- */
/**
 * Get the initial value for a given registration node, by name parts.
 *
 * \param db An open configuration database with registration state
 *
 * \param node_name_parts The node to retrieve.  This may contain stars (*'s)
 *  at wildcard levels of the registered name.
 *
 * \retval ret_initial_value The registered initial value value
 * attribute for the given node.  This will be NULL in cases where there
 * is no initial value attribute registered, and no error will be
 * returned.
 */
int mdr_reg_get_node_initial_value_parts(const mdb_db *db,
                                         const tstr_array *node_name_parts,
                                         const bn_attrib **ret_initial_value);


/* ------------------------------------------------------------------------- */
/**
 * Get the registered initial value for a given registration node.
 *
 * \param db An open configuration database with registration state
 *
 * \param node_name_parts The node to retrieve.  This may contain stars (*'s)
 *  at wildcard levels of the registered name.
 *
 * \retval ret_value_type The registered type of the given node.  This
 * will be bt_NONE in cases where there is no such node, or no
 * registered type, and no error will be returned.
 */
int mdr_reg_get_node_value_type(const mdb_db *db,
                                const char *node_name,
                                bn_type *ret_value_type);

/* ------------------------------------------------------------------------- */
/**
 * Get the registered initial value for a given registration node.
 *
 * \param db An open configuration database with registration state
 *
 * \param node_name_parts The node to retrieve.  This may contain stars (*'s)
 *  at wildcard levels of the registered name.
 *
 * \retval ret_value_type The registered type of the given node.  This
 * will be bt_NONE in cases where there is no such node, or no
 * registered type, and no error will be returned.
 */
int mdr_reg_get_node_value_type_parts(const mdb_db *db,
                                      const tstr_array *node_name_parts,
                                      bn_type *ret_value_type);

#ifdef __cplusplus
}
#endif

#endif /* __MDB_REG_H_ */
