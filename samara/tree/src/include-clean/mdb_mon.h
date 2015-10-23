/*
 *
 * mdb_mon.h
 *
 *
 *
 */

#ifndef __MDB_MON_H_
#define __MDB_MON_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif


#include "common.h"
#include "mdb_db.h"
#include "tree.h"


int mdb_mon_iterate_binding(md_commit *commit, const mdb_db *db,
                            const char *node_name, 
                            mdb_db_query_flags_bf db_query_flags, 
                            bn_binding_array **ret_binding_array);

int mdb_mon_iterate_node(md_commit *commit, const mdb_db *db,
                         const char *node_name, 
                         mdb_db_query_flags_bf db_query_flags, 
                         mdb_node_array **ret_node_array);


typedef int (*mdb_mon_iterate_node_cb_func)(md_commit *commit, 
                                            const mdb_db *db, 
                                            const mdb_node *node,
                                            void *arg);

typedef int (*mdb_mon_iterate_binding_cb_func)(md_commit *commit, 
                                               const mdb_db *db, 
                                               const bn_binding *binding,
                                               void *arg);

int mdb_mon_iterate_node_cb(md_commit *commit, const mdb_db *db,
                            const char *node_name, 
                            mdb_db_query_flags_bf db_query_flags, 
                            mdb_mon_iterate_node_cb_func iterate_cb,
                            void *cb_arg);


int mdb_mon_iterate_binding_cb(md_commit *commit, const mdb_db *db,
                               const char *node_name, 
                               mdb_db_query_flags_bf db_query_flags, 
                               mdb_mon_iterate_binding_cb_func iterate_cb,
                               void *cb_arg);





int mdb_query_target_mon_get(mdb_query_target_type target_type,
                             int32 binding_num,
                             uint32 node_id,
                             const char *node_name,
                             const tstr_array *name_parts, 
                             mdb_node_reg *reg_node,
                             mdb_query_state *inout_query_state);


int mdb_query_target_mon_iterate_names(mdb_query_target_type target_type,
                                       int32 binding_num,
                                       uint32 node_id,
                                       const char *node_name,
                                       const tstr_array *name_parts, 
                                       mdb_node_reg *reg_node,
                                       mdb_query_state *inout_query_state,
                                       tstr_array **ret_child_names);


#ifdef __cplusplus
}
#endif

#endif /* __MDB_MON_H_ */
