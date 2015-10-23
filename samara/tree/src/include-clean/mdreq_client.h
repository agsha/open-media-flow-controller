/*
 *
 * mdreq_client.h
 *
 *
 *
 */

#ifndef __MDREQ_CLIENT_H_
#define __MDREQ_CLIENT_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file mdreq_client.h Wrappers to allow simple queries to the config db
 * without being a GCL client.
 * \ingroup lc
 *
 * The functions in this API query mgmtd by forking mdreq, specifying the
 * requested query on the command line, and capturing its output.
 * They are pretty rudimentary, but allow simple things to be done without
 * requiring a program to become a GCL client.
 */

#include "tstring.h"

/* ------------------------------------------------------------------------- */
/** Query a single node, and return its contents in string form,
 * regardless of its native data type.
 */
int mdrc_query_get(const char *node_name, tstring **ret_node_value);

/* ------------------------------------------------------------------------- */
/** Query a list of children of a specified node, and return the names of
 * all children in string form, regardless of their native data type.
 */
int mdrc_query_iterate(const char *parent_node_name,
                       tstr_array **ret_child_names);

#ifdef __cplusplus
}
#endif

#endif /* __MDREQ_CLIENT_H_ */
