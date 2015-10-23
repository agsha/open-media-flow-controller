/*
 *
 * md_moni.h
 *
 *
 *
 */

#ifndef __MD_MONI_H_
#define __MD_MONI_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "bnode.h"
#include "bnode_proto.h"

/* ------------------------------------------------------------------------- */
/** \file md_moni.h Monitoring infrastructure
 * \ingroup gcl
 */

/* ========================================================================= */
/* Typedefs
 * =========================================================================
 */

/* Forward declaration of the monitoring infrastructure mapping structure */
struct mi_var_map;

/* ------------------------------------------------------------------------- */
/** A function to fetch the names of the children of a single binding
 * that is a parent of a wildcard node.  i.e. fetch the instances of 
 * the wildcard.
 *
 * \param reg_data The contents of the mvm_iterate_data field in the
 * mi_var_map structure that registered this node.
 *
 * \param var_map The monitoring variable map structure representing
 * the binding being requested.  e.g. you might retrieve the
 * mvm_selector field out of this, if the callback is to be used for
 * multiple monitoring nodes.
 *
 * \param node_name_parts The name of the node being requested, broken
 * down into component parts.
 *
 * \param num_node_name_parts The number of node name parts (the same as
 * would be returned by tstr_array_length_quick(node_name_parts)).
 *
 * \param node_name The full name of the node being requested.
 *
 * \retval wc_node_names An empty tstr_array provided to you, to which you 
 * should append the node names you have generated.
 */
typedef int (*mi_iterate_wc_names_func)(void *reg_data,
                                        struct mi_var_map *var_map,
                                        const tstr_array *node_name_parts,
                                        uint32 num_node_name_parts, 
                                        const char *node_name,
                                        tstr_array *wc_node_names);

/* ------------------------------------------------------------------------- */
/** A function to fetch the contents of a single binding.
 *
 * \param reg_data The contents of the mvm_get_data field in the
 * mi_var_map structure that registered this node.
 *
 * \param var_map The monitoring variable map structure representing
 * the binding being requested.  e.g. you might retrieve the
 * mvm_selector field out of this, if the callback is to be used for
 * multiple monitoring nodes.
 *
 * \param node_name_parts The name of the node being requested, broken
 * down into component parts.
 *
 * \param num_node_name_parts The number of node name parts (the same as
 * would be returned by tstr_array_length_quick(node_name_parts)).
 *
 * \param node_name The full name of the node being requested.
 *
 * \retval ret_binding The binding requested.  Note that
 * mi_var_map_get_data() may do most of the work for you here,
 * but you'd need to look up the base pointer for it to work off of.
 * Or optionally you may serve the binding yourself.
 */
typedef int (*mi_get_func)(void *reg_data,
                           struct mi_var_map *var_map,
                           const tstr_array *node_name_parts,
                           uint32 num_node_name_parts, const char *node_name,
                           bn_binding **ret_binding);

/* ------------------------------------------------------------------------- */
/** A function that can free any allocated resources from a called conversion
 *  function. The conversion function should pass the function pointer back
 *  to its caller.
 */
typedef void (*mi_conv_free_func)(void *src);

/* ------------------------------------------------------------------------- */
/** A function which given a pointer to some data, converts it into a
 *  pointer to the representation that matches the registered binding type.
 *  If the pointer to the returned data is not globally accessable and
 *  needs to be freed after the binding is created, this function should
 *  return a reference to a free function that will take the 'ret_data'
 *  pointer as an argument.
 *
 * \param reg_data The fixed data provided at registration time in
 * the mvm_conv_data field.
 *
 * \param src The pointer to the data to be converted.
 *
 * \retval ret_data The converted data.
 *
 * \retval ret_free_func A function to be called to free any resources
 * which were dynamically allocated during this call.
 */
typedef int (*mi_conversion_func)(void *reg_data, void *src, void **ret_data,
                                  mi_conv_free_func *ret_free_func);

/* ------------------------------------------------------------------------- */
/** Monitoring infrastructure per-node registration flags.
 */
typedef enum {
    mvmf_none = 0,

    /**
     * Register the node as a nonbarrier async query node (i.e. use
     * mrf_flags_reg_monitor_ext_literal_nonbarrier and
     * mrf_flags_reg_monitor_ext_wildcard_nonbarrier, instead of
     * mrf_flags_reg_monitor_ext_literal and
     * mrf_flags_reg_monitor_ext_wildcard.
     */
    mvmf_nonbarrier_async = 1 << 0,

} mi_var_map_flags;

/** Bit field of ::mi_var_map_flags ORed together */
typedef uint32 mi_var_map_flags_bf;

/* ------------------------------------------------------------------------- */
/** Monitoring infrastructure mapping structure
 */
typedef struct mi_var_map {
    /**
     * Binding name (full path) that gets its value by the associated
     * extraction functions registered below and provided for by the
     * library. This is the key for the structure.
     */
    const char *mvm_binding;

    /**
     * Type for the binding. If this is not the same as the associated
     * data, a conversion function should be provided.
     */
    bn_type mvm_btype;

    /**
     * Location in a structure that is associated with this binding
     * or the address of a global variable.
     * Care must be taken to ensure the offset is correct as it is
     * easy to reference random memory otherwise (especially with
     * pointer types like 'bt_string').
     * 
     * It is permissible to have the offset field ignored and 
     * have the binding name not associated with any run-time data.
     * The registered get function for the binding name will be called
     * when it is queried and any value can be populated in the return
     * binding (though the binding type should match).
     */
    uint_ptr mvm_offset;

    /**
     * The get function is responsible for extracting the data given
     * the above mappings and place the data in a binding.
     */
    mi_get_func mvm_get_func;

    /**
     * Data to be passed to mvm_get_func whenever it is called.
     */
    void *mvm_get_data;

    /**
     * A selector field that can be set to an arbitrary value.
     * Can be used in get and iterate functions for determining
     * which binding is being queried when one of these functions
     * is overloaded for multiple binding names. Zero should not
     * be used for this field, as that value should indicate no
     * selector has been set for the binding name.
     */
    int32 mvm_selector;

    /** Used to iterate over valid wildcard names */
    mi_iterate_wc_names_func mvm_iterate_func;

    /** Data to be passed to mvm_iterate_func whenever it is called */
    void *mvm_iterate_data;

    /**
     * If the type for the binding is not the same as what the
     * run-time state (via the offset) refers to, a conversion
     * function should be provided. The function is given the
     * location of data based on the offset and should return
     * a pointer to data that is of the type expected. If the
     * returned converted data is to be freed, a free function
     * should be returned as well.
     */
    mi_conversion_func mvm_conv_func;

    /**
     * Data to be passed to mvm_conv_func when it is called.
     */
    void *mvm_conv_data;

    /**
     * Only used on the 'mgmtd' side for registering the bindings.
     * Default to 'mcf_cap_node_basic' if not specified.
     */
    uint32 mvm_cap_mask;

    /*
     * XXXX/EMT: need to allow an ACL here.  Can't do it like a
     * regular mgmtd node (where a read-write ACL is built across
     * several calls) because these are defined in static
     * initializations.  Do we have to name a single const struct
     * which is pre-built for us?
     */

    /**
     * Flags to control how we register this node.
     */
    mi_var_map_flags_bf mvm_flags;

} mi_var_map;


/* ========================================================================= */
/* Macros to help in the use of the mapping structure 'mi_var_map'.
 * =========================================================================
 */

#define MI_FIELD_OFFSET(c_struct, field) \
    ((uint_ptr) (&((c_struct *) NULL)->field))

#define MI_GLOBAL_VAR(gvar) \
    ((uint_ptr) &(gvar))

#define MI_VAR_MAP_SIZE(var_map_array) \
    sizeof(var_map_array) / sizeof(var_map_array[0])


/* ========================================================================= */
/* Function prototypes for clients of the monitoring infrastructure
 * =========================================================================
 */

/* ------------------------------------------------------------------------- */
/** Any client of the monitoring infrastructure should have a global
 *  array of mapping structures ('mi_var_map's). This is used to map
 *  binding names to their associated data retrieval functions and type.
 *  This routine allocates the memory for such an array.
 */
int mi_total_var_map_create(array **total_var_map);


/* ------------------------------------------------------------------------- */
/** Cleans up resources in a variable mapping array
 */
int mi_total_var_map_free(array **inout_vmap);

/* ------------------------------------------------------------------------- */
/** Once an array of mapping structures exists, various variable maps can
 *  be added to it by this function. The 'var_map' parameter is a pointer
 *  to some number of mappings that are added to the array.
 */
int mi_var_map_add(array *total_var_map, mi_var_map *var_map,
                   uint32 var_map_size);

/* ------------------------------------------------------------------------- */
/** This function is used to retrieve the associated mapping structure for
 *  the given binding name.
 */
int mi_get_var_map(const char *node_name, array *total_var_map,
                   mi_var_map **ret_map);


/* ------------------------------------------------------------------------- */
/** This function does the extraction of the run-time data based on offsets
 *  and data pointers. A binding is created of the registered type.
 */
int mi_var_map_get_data(void *src, mi_var_map *var_map, const char *node_name,
                        bn_binding **ret_binding);


/* ------------------------------------------------------------------------- */
/** For clients that are external to mgmtd, this routine should be registered
 *  as the global_callback_query_request for the gcl session. The 'arg'
 *  parameter must be set to the global array of variable mapping structures.
 */
int mi_mgmt_handle_query_request(gcl_session *sess,  bn_request **inout_request,
                                 void *arg);


/* ------------------------------------------------------------------------- */
/** Conversion function that can be assigned to mvm_conv_func, when you
 * have a tstring field in a data structure, and want to export it as
 * a string binding.
 */
int mi_conv_tstring_string(void *reg_data, void *src, void **ret_data,
                           mi_conv_free_func *ret_free_func);


/* ------------------------------------------------------------------------- */
/** Conversion function that can be assigned to mvm_conv_func, when
 * you have an enum field in a data structure, and want to export it
 * as a string binding.  Set mvm_conv_data to point to the
 * lc_enum_string_map for your enum.
 */
int mi_conv_enum_string(void *reg_data, void *src, void **ret_data,
                        mi_conv_free_func *ret_free_func);


/* ------------------------------------------------------------------------- */
/** Just like mi_conv_enum_string(), except if no mapping can be found,
 * return the empty string, rather than "ERROR-NO-MAP".
 */
int mi_conv_enum_string_nomap_empty(void *reg_data, void *src, void **ret_data,
                                    mi_conv_free_func *ret_free_func);


/* ========================================================================= */
/* Function prototypes for internal functions of the  monitoring
 * infrastructure (i.e. not expected to be directly called by clients)
 * =========================================================================
 */

/* ------------------------------------------------------------------------- */
/** The main routine for retrieving runtime data. 'data' must be an array
 *  of mapping structures for the client that contains all of the binding
 *  names. If a binding name does not have a registered get function and
 *  an offset is supplied, it is assumed this is a reference to a global
 *  variable and data is attempted to be retrieved.
 */
int mi_get_handler(const char *node_name, void *data, 
                   const tstr_array *name_parts, bn_binding **ret_binding);

/* ------------------------------------------------------------------------- */
/** Similar to the get_handler.
 */
int mi_iterate_handler(const char *parent_name, void *data,
                       tstr_array **ret_wc_names, uint32 *ret_num_wcs);

#ifdef __cplusplus
}
#endif

#endif /* __MD_MONI_H_ */
