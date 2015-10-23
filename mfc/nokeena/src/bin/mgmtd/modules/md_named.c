/*
 * Copyright (C) 2011, Juniper Networks Inc.
 * All rights reserved.
 */

/*
 * Author:  Dhruva Narasimhan
 * When:    5/20/2011
 * What:    PM module for legacy daemon - named (BIND9)
 */


#include "common.h"
#include "file_utils.h"
#include "proc_utils.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_mod_reg.h"



/*-----------------------------------------------------------------------------
 * GLOBAL VARIABLES
 *---------------------------------------------------------------------------*/
const bn_str_value md_named_initial_values[] = {
    {"/auth/passwd/user/named/enable", bt_bool, "true"},
    {"/auth/passwd/user/named/password", bt_string, "*"},
    {"/auth/passwd/user/named/uid", bt_uint32, "25"},
    {"/auth/passwd/user/named/gid", bt_uint32, "25"},
    {"/auth/passwd/user/named/gecos", bt_string, "Named"},
    {"/auth/passwd/user/named/home_dir", bt_string, "/var/named"},
    {"/auth/passwd/user/named/shell", bt_string, "/sbin/nologin"},
    {"/auth/passwd/user/named/newly_created", bt_bool, "false"},
};


static int
md_named_commit_apply(md_commit *commit,
		      const mdb_db *old_db, const mdb_db *new_db,
		      mdb_db_change_array *change_list, void *arg);

static int
md_named_write_forwarders_conf(md_commit *commit, const mdb_db *db);

static int
md_named_reload_conf(md_commit *commit, const mdb_db *db);
/*-----------------------------------------------------------------------------
 * FUNCTION DECLARATIONS
 *---------------------------------------------------------------------------*/
int md_named_init(const lc_dso_info *info, void *data);



static int md_named_add_initial(md_commit *commit,
				mdb_db *db,
				void *arg)
{
    int err = 0;
    int size = sizeof(md_named_initial_values) / sizeof(bn_str_value);

    err = mdb_create_node_bindings(commit, db, md_named_initial_values, size);
    bail_error(err);

bail:
    return(err);
}


int md_named_init(const lc_dso_info *info,
		  void *data)
{
    int err = 0;

    md_module *module = NULL;
    md_reg_node *node = NULL;

    err = mdm_add_module(
            "named",
            1,
            "/nkn/named",
	    NULL,
            0,
            NULL,
	    NULL,
	    NULL,
	    NULL,
	    md_named_commit_apply,
	    NULL,
	    0,
	    0,
            NULL,
	    NULL,
	    md_named_add_initial,
	    NULL,
            NULL,
	    NULL,
	    NULL,
	    NULL,
	    &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		"/nkn/named/config/forward-to/*";
    node->mrn_value_type =	bt_uint32;
    node->mrn_limit_wc_count_max = 3;
    node->mrn_bad_value_msg =	N_("Can only configure upto 3 forwarders ");
    node->mrn_node_reg_flags =  mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =	mcf_cap_node_basic;
    node->mrn_description =	"Ordered list of nameservers to use";
    node->mrn_audit_descr =	N_("forwarder $v$");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		"/nkn/named/config/forward-to/*/address";
    node->mrn_value_type =	bt_ipv4addr;
    node->mrn_initial_value =	"0.0.0.0";
    node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
    node->mrn_cap_mask =	mcf_cap_node_basic;
    node->mrn_description =	"Address of a nameserver to use for DNS";
    node->mrn_audit_descr =	N_("address");
    err = mdm_add_node(module, &node);
    bail_error(err);



bail:
    return err;
}

static int
md_named_commit_apply(md_commit *commit,
		      const mdb_db *old_db, const mdb_db *new_db,
		      mdb_db_change_array *change_list, void *arg)
{
    int err = 0;

    err = md_named_write_forwarders_conf(commit, new_db);
    bail_error(err);

    err = md_named_reload_conf(commit, new_db);
    bail_error(err);

bail:
    return err;
}

static int
md_named_reload_conf(md_commit *commit, const mdb_db *db)
{
    int err = 0;
    int32 code = 0;


    err = lc_launch_quick(&code,
			  NULL,
			  2,
			  "/usr/sbin/rndc",
			  "reload");
    bail_error(err);

bail:
    return err;
}

static int
md_named_get_nameservers(md_commit *commit, const mdb_db *db, tstr_array **ret_nameservers)
{
    int err = 0;
    bn_binding_array *barr = NULL;
    tstr_array *tmp = NULL;
    tstring *elem = NULL;
    uint32 i, num_elems;
    bn_binding *binding = NULL;


    *ret_nameservers = NULL;

    err = mdb_iterate_binding(commit, db, "/nkn/named/config/forward-to",
			      mdqf_sel_iterate_subtree | mdqf_sel_iterate_leaf_nodes_only,
			      &barr);
    bail_error(err);

    bn_binding_array_dump("named", barr, LOG_NOTICE);

    num_elems = bn_binding_array_length_quick(barr);

    err = tstr_array_new(&tmp, NULL);
    bail_error_null(err, tmp);

    for(i = 0;  i < num_elems; i++) {
	err = bn_binding_array_get(barr, i, &binding);
	bail_error_null(err, binding);

	err = bn_binding_get_tstr(binding, ba_value,
				  bt_ipv4addr, NULL, &elem);
	bail_error_null(err, elem);

	err = tstr_array_append_takeover(tmp, &elem);
	elem = NULL;
	bail_error(err);
    }
    *ret_nameservers = tmp;

bail:

    bn_binding_array_free(&barr);
    return err;
}

static int
md_named_write_forwarders_conf(md_commit *commit, const mdb_db *db)
{
    int err = 0;
    uint32 i, num_elems;
    char *fwders_temp = NULL;
    int fwders_fd = -1;
    int32 bytes_written = 0;
    tstring *ip_list = NULL;
    tstr_array *nameserver = NULL;
    tstring *ts = NULL;


    err = lf_temp_file_get_fd("/var/named/chroot/etc/named-forwarders.conf",
			      &fwders_temp, &fwders_fd);
    bail_error(err);

    /* get the nameserver list */
    err = md_named_get_nameservers(commit, db, &nameserver);
    bail_error(err);

    if (nameserver == NULL) {
	goto bail;
    }

    num_elems = tstr_array_length_quick(nameserver);

    err = ts_new(&ip_list);
    bail_error(err);

    /* Build the entire file:
     *	    forwarders { @IpList@ };
     */
    err = ts_append_sprintf(ip_list,
			    "	    forwarders { ");
    bail_error(err);

    for (i = 0; i < num_elems; i++) {
	ts = tstr_array_get_quick(nameserver, i);
	err = ts_append_sprintf(ip_list, "%s; ", ts_str(ts));
	bail_error(err);
    }

    err = ts_append_sprintf(ip_list, " };\n");
    bail_error(err);


    /* Write the file now. */
    err = lf_write_bytes_tstr(fwders_fd, ip_list, &bytes_written);
    bail_error(err);

    err = lf_temp_file_activate_fd("/var/named/chroot/etc/named-forwarders.conf",
				   fwders_temp,
				   &fwders_fd,
				   25,
				   25,
				   0644,
				   lao_backup_orig);
    bail_error(err);

bail:
    ts_free(&ip_list);
    tstr_array_free(&nameserver);
    return err;
}


