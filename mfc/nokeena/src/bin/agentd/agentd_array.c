/*
 * (C) Copyright 2015 Juniper Networks
 * All rights reserved.
 */

static const char rcsid[] = "$Id: agentd_array.c,v 1.57 2009/11/11 02:54:58 et Exp $";

#include "common.h"
#include "agentd_mgmt.h"
agentd_array_move_context cache_age_content_type;
agentd_array_move_context http_cl_resp_add_hdr;
agentd_array_move_context http_cl_resp_del_hdr;
agentd_array_move_context http_org_req_add_hdr;
agentd_array_move_context bond_interface;
agentd_array_move_context sshv2_authkey;
agentd_array_move_context smap_attachments;
agentd_array_move_context cache_index_hdr;
agentd_array_move_context license_key;   //Context for license key
agentd_array_move_context nameserver_ctxt;  //Context for nameserver configuration
agentd_array_move_context domainlist_ctxt;  // Context for domain-list configuration
agentd_array_move_context ipv6_ctxt;   // Context for interface ipv6 configuration
agentd_array_move_context tacacs_server_ctxt; //Context for TACACS server array

static int
agentd_array_append_modify_requests(bn_binding_array * barr, const char *idx_str,
                                 const char *root_name,
                                 const bn_binding_array *children);

static int
agentd_array_get_indices(md_client_context *mcc,
    const char *root_name, const char *db_name,
    int32 rev_before, int32 *ret_rev_after,
    uint32_array **ret_indices,
    uint32 *ret_code, tstring **ret_msg);

static int
agentd_array_set_element(md_client_context *context, bn_binding_array * barr,
                      const char *root_name, uint32 idx,
                      const bn_binding_array *children,
                      int32 rev_before, int32 *ret_rev_after,
                      uint32 *ret_code, tstring **ret_msg);


/******************************************************************************
 ** Functions
 *****************************************************************************/

/*
 * Many of these functions require multiple management requests to
 * perform their operations, and in most cases there could be trouble
 * if someone else modifies the database in the interim.  The basic
 * plan to avoid this problem is to have a revision ID on the database
 * that is incremented each time the database is changed.  All query or
 * set requests may optionally specify a required revision ID, where
 * the request is failed if the database no longer has that revision ID.
 * All query or set responses come with the revision ID the database had
 * after the operation was completed.
 *
 * Functions which themselves need to be atomic, but which have no
 * requirement to complete together with other operations, need no
 * change in API.  Most of the public agentd_array API falls into this
 * category.  Functions which are part of larger atomic operations
 * have hooks to allow the entire operation to be made atomic:
 *
 *   - rev_before ("in" parameter).  Revision number.  The revision
 *     number required for this operation, if any.  If the database
 *     does not match this number before starting the operation, the
 *     operation will fail.
 *
 *   - ret_rev_after ("out" parameter).  Pointer to a revision number.
 *     The revision number of the database after the operation was
 *     completed is returned here.  For query operations, this number
 *     will be the same as rev_before (if it was specified), unless
 *     the operation failed due to a revision ID mismatch, in which
 *     case it will have the new revision ID.
 */

/* ------------------------------------------------------------------------- */
int agentd_array_append(md_client_context *context, agentd_array_move_context *ctxt, bn_binding_array * barr,
                     const char *root_name, const bn_binding_array *children,
                     uint32 *ret_code, tstring **ret_msg)
{
    int err = 0;
    uint32_array *indices = NULL;
    uint32 new_idx = 0;
    uint32 lst_idx = 0;
    uint32 free_idx = 0;
    uint32 code = 0;
    uint32 num_deleted = 0;
    int32 rev = 0, new_rev = 0;
    uint32 num_elems = 0, i = 0;
    int32 last = -1;


    err = agentd_array_get_indices(context, root_name, NULL, rev, &new_rev,
                                &indices, &code, ret_msg);
    bail_error(err);

    if (code) {
        if (rev != new_rev && num_deleted > 0) {
#if 0
            err = agentd_add_extra_message
                (context, _("Duplicates of the new item were deleted, but "
                 "the new item was not added.\n"), ret_msg);
            bail_error(err);
#endif
        }
        goto bail;
    }

    /*
     * This is just to prevent us from getting 0 back as an unused
     * index, since we do not want to use that as an index.
     */
    err = uint32_array_sort(indices);
    bail_error(err);
    err = uint32_array_insert_sorted(indices, 0);
    bail_error(err);

    err = uint32_array_get_first_absent(indices, &new_idx);
    bail_error(err);

    new_idx += ctxt->mamc_start_index;

    err = uint32_array_get_max(indices, &lst_idx);
    for(i = 0; i< lst_idx; i++) {
	free_idx = uint32_array_get_quick(indices, i);
	if (new_idx == free_idx) {
	    new_idx++;
	}
    }

    /*If the new_index  is in a hole use it
     * else the new_idx + add_per_commit is the index to be used.
     * For now handling the  new_idx + add_per_commit.
     */
    err = agentd_array_set_element(context, barr, root_name, new_idx, children,
                                rev, &new_rev, &code, ret_msg);
    bail_error(err);

    if (code) {
        if (rev != new_rev && num_deleted > 0) {
#if 0
            err = agentd_add_extra_message
                (context, _("Duplicates of the new item were deleted, but "
                 "the new item was not added.\n"), ret_msg);
#endif
            bail_error(err);
        }
        goto bail;
    }
    ctxt->mamc_start_index++;

 bail:
    uint32_array_free(&indices);
    if (ret_code) {
        *ret_code = code;
    }
    return(err);
}

/* This function adds bindings to the last created agentd array element */
int agentd_array_set(md_client_context *context, agentd_array_move_context *ctxt, bn_binding_array * barr, 
                     const char *root_name, const bn_binding_array *children,
                     uint32 *ret_code, tstring **ret_msg)
{
    int err = 0;
    uint32 new_idx = 0, lst_idx = 0;
    int32 rev = 0, new_rev = 0;
    uint32 code = 0;
    uint32_array *indices = NULL;

    err = agentd_array_get_indices(context, root_name, NULL, rev, &new_rev,
                                 &indices, &code, ret_msg);
    bail_error(err);

    if (code) {
        if (rev != new_rev) {
            bail_error(err);
        }
        goto bail;
    }
    /*
     * This is just to prevent us from getting 0 back as an unused
     * index, since we do not want to use that as an index.
     */
    err = uint32_array_sort(indices);
    bail_error(err);
    err = uint32_array_insert_sorted(indices, 0);
    bail_error(err);

    err = uint32_array_get_max(indices, &lst_idx);
    bail_error(err);

    new_idx = lst_idx + ctxt->mamc_start_index;

    err = agentd_array_set_element(context, barr, root_name, new_idx, children,
                                rev, &new_rev, &code, ret_msg);
    bail_error(err);

    if (code) {
        if (rev != new_rev) {
            bail_error(err);
        }
        goto bail;
    }

 bail:
    uint32_array_free(&indices);
    if (ret_code) {
        *ret_code = code;
    }
    return(err);
}

static int
agentd_array_get_indices(md_client_context *mcc, 
    const char *root_name, const char *db_name,
    int32 rev_before, int32 *ret_rev_after,
    uint32_array **ret_indices,
    uint32 *ret_code, tstring **ret_msg)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    tstr_array *indices = NULL;
    tstring *idx_ts = NULL;
    uint32 num_indices = 0, i = 0, idx = 0;
    char *end = NULL;
    uint32 code = 0;
    int32 rev = 0;

    bail_null(root_name);
    bail_null(ret_indices);

    err = mdc_get_binding_children_ex
        (mcc, &code, NULL, true, &bindings, 0, root_name,
         db_name, rev_before, &rev);
    bail_error(err);

    if (code) {
        goto bail;
    }

    err = bn_binding_array_get_last_name_parts(bindings, &indices);
    bail_error(err);

    err = uint32_array_new(ret_indices);
    bail_error_null(err, *ret_indices);

    num_indices = tstr_array_length_quick(indices);
    for (i = 0; i < num_indices; ++i) {
        err = tstr_array_get(indices, i, &idx_ts);
        bail_error_null(err, idx_ts);
        idx = strtoul(ts_str(idx_ts), &end, 10);
        bail_require(*end == '\0');
        err = uint32_array_append(*ret_indices, idx);
        bail_error(err);
    }

 bail:
    bn_binding_array_free(&bindings);
    tstr_array_free(&indices);
    if (ret_code) {
        *ret_code = code;
    }
    if (ret_rev_after) {
        *ret_rev_after = rev;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
agentd_array_set_element(md_client_context *context, bn_binding_array * barr,
                      const char *root_name, uint32 idx,
                      const bn_binding_array *children,
                      int32 rev_before, int32 *ret_rev_after,
                      uint32 *ret_code, tstring **ret_msg)
{
    int err = 0;
    char *idx_str = NULL;

    bail_null(context);
    idx_str = smprintf("%d", idx);
    bail_null(idx_str);

    err = agentd_array_append_modify_requests(barr, idx_str, root_name, children);
    bail_error(err);

 bail:
    safe_free(idx_str);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
agentd_array_append_modify_requests(bn_binding_array * barr,const char *idx_str,
                                 const char *root_name,
                                 const bn_binding_array *children)
{
    int err = 0;
    bn_binding *old_bn = NULL, *new_bn = NULL;
    const tstring *old_name = NULL;
    char *new_name = NULL;
    char *old_value = NULL;
    uint32 num_bindings = 0, i = 0, old_type = 0;

    num_bindings = bn_binding_array_length_quick(children);
    for (i = 0; i < num_bindings; ++i) {
        err = bn_binding_array_get(children, i, &old_bn);
        bail_error_null(err, old_bn);

        err = bn_binding_get_name(old_bn, &old_name);
        bail_error_null(err, old_name);
        safe_free(new_name);
        new_name = smprintf("%s/%s/%s", root_name, idx_str, ts_str(old_name));
        bail_null(new_name);

        safe_free(old_value);
        err = bn_binding_get_str(old_bn, ba_value, bt_NONE, &old_type,
                                 &old_value);
        bail_error(err);

        bn_binding_free(&new_bn);
        err = bn_binding_new_str_autoinval
            (&new_bn, new_name, ba_value, bn_type_from_flagged_type(old_type),
             bn_type_flags_from_flagged_type(old_type), old_value);
        bail_error(err);

        err = bn_binding_array_append_takeover (barr, &new_bn);
        bail_error(err);
    }

 bail:
    safe_free(old_value);
    bn_binding_free(&new_bn);
    safe_free(new_name);
    return(err);
}


int
agentd_array_cleanup(void)
{
    int err = 0;
    memset(&cache_age_content_type, 0, sizeof(agentd_array_move_context));
    memset(&http_cl_resp_add_hdr, 0, sizeof(agentd_array_move_context));
    memset(&http_cl_resp_del_hdr, 0, sizeof(agentd_array_move_context));
    memset(&http_org_req_add_hdr, 0, sizeof(agentd_array_move_context));
    memset(&bond_interface, 0, sizeof(agentd_array_move_context));
    memset(&sshv2_authkey, 0, sizeof(agentd_array_move_context));
    memset(&smap_attachments, 0, sizeof(agentd_array_move_context));
    memset(&cache_index_hdr, 0, sizeof(agentd_array_move_context));
    memset(&license_key, 0, sizeof(agentd_array_move_context));
    memset(&nameserver_ctxt, 0, sizeof(agentd_array_move_context));
    memset(&domainlist_ctxt, 0, sizeof(agentd_array_move_context));
    memset(&ipv6_ctxt, 0, sizeof(agentd_array_move_context));
    memset(&tacacs_server_ctxt, 0, sizeof(agentd_array_move_context));

    return err;
}
