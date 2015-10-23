

#include "dso.h"
#include "common.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_main.h"
#include "md_upgrade.h"
#include "proc_utils.h"
#include "file_utils.h"
#include "tpaths.h"


#include "nkn_common_config.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"

int md_cfg_files_init(const lc_dso_info *info, void *data);
static int
md_cfg_files_commit_apply(md_commit *commit,
		const mdb_db *old_db, const mdb_db *new_db,
		mdb_db_change_array *change_list, void *arg);

int
md_write_config_file(md_commit *commit,const  mdb_db * new_db, const char *file_name);

int
md_cfg_files_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;

    err = mdm_add_module("config-files", 1,
	    "/nkn/cfg-files", "/nkn/cfg-files/config", 0,
	    NULL, NULL,
	    NULL, NULL,
	    md_cfg_files_commit_apply, NULL,
	    0, 0,
	    NULL, NULL, NULL,
	    NULL, NULL, NULL,
	    NULL, NULL, &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		    "/nkn/cfg-files/config/files/*";
    node->mrn_value_type =	    bt_string;
    node->mrn_node_reg_flags =	    mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =	    mcf_cap_node_basic;
    node->mrn_bad_value_msg =	    "Invalid Filename";
    node->mrn_description =	    "Resource Mgr config nodes";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		    "/nkn/cfg-files/config/files/*/params/*";
    node->mrn_value_type =	    bt_string;
    node->mrn_node_reg_flags =	    mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =	    mcf_cap_node_basic;
    node->mrn_bad_value_msg =	    "Invalid Param name";
    node->mrn_description =	    "Cofig file param";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		    "/nkn/cfg-files/config/files/*/params/*/value";
    node->mrn_value_type =	    bt_string;
    node->mrn_initial_value =	    "";
    node->mrn_node_reg_flags =	    mrf_flags_reg_config_literal;
    node->mrn_cap_mask =	    mcf_cap_node_basic;
    node->mrn_description =	    "Config File Param value";
    err = mdm_add_node(module, &node);
    bail_error(err);


bail:
    return err;
}

static int
md_cfg_files_commit_apply(md_commit *commit,
		const mdb_db *old_db, const mdb_db *new_db,
		mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    uint32 num_changes = 0, i = 0;
    mdb_db_change *change = NULL;
    const char *file_name = NULL;

    lf_ensure_dir(NKN_CONFIG_OUTPUT_DIR, 0755);

    num_changes = mdb_db_change_array_length_quick(change_list);
    for (i = 0; i < num_changes; ++i) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	file_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
	if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 8, "nkn", "cfg-files", "config", "files", "*", "params", "*", "value")) {
	    err = md_write_config_file(commit, new_db, file_name );
	    bail_error(err);
	}
    }

bail:
    return err;

}

int
md_write_config_file(md_commit *commit, const mdb_db * new_db,
	const char *file_name)
{
    int err = 0, i = 0;
    node_name_t node_name = {0};
    tstring *filename_real = NULL, *param_value = NULL;
    char *filename_temp = NULL;
    int fd_temp = -1, num_params = 0;
    tstr_array *param_array = NULL, *name_parts = NULL;
    const char *param_name = NULL;

    err = lf_path_from_va_str(&filename_real, true, 2,
                              NKN_CONFIG_OUTPUT_DIR, file_name);
    bail_error(err);

    err = lf_temp_file_get_fd(ts_str(filename_real),
                              &filename_temp, &fd_temp);
    bail_error(err);

    err = md_conf_file_write_header(fd_temp, "md_cfg_files", "##");
    bail_error(err);

    snprintf(node_name, sizeof(node_name),
	    "/nkn/cfg-files/config/files/%s/params/*/value", file_name);

    err = mdb_get_matching_tstr_array(commit, new_db, node_name,
	    0, &param_array);
    bail_error(err);


    num_params = tstr_array_length_quick(param_array);
    bail_error(err);
    for (i =0; i < num_params; i++) {

	param_name = tstr_array_get_str_quick(param_array, i);

	ts_free(&param_value);
	err = mdb_get_node_value_tstr(commit, new_db, param_name, 0, NULL, &param_value);
	bail_error(err);

	tstr_array_free(&name_parts);

	err = bn_binding_name_to_parts(param_name, &name_parts, NULL);
	bail_error(err);

	err = lf_write_bytes_printf
	    (fd_temp, NULL,
	     "%s = %s\n", tstr_array_get_str_quick(name_parts, 6), ts_str_maybe_empty(param_value));
	bail_error(err);
    }


    err = lf_temp_file_activate_fd(ts_str(filename_real), filename_temp,
                                   &fd_temp,
                                   md_gen_conf_file_owner,
                                   md_gen_conf_file_group,
                                   md_gen_conf_file_mode, lao_backup_orig);
    bail_error(err);


bail:
    safe_close(&fd_temp);
    ts_free(&param_value);
    ts_free(&filename_real);
    safe_free(filename_temp);
    tstr_array_free(&param_array);
    return err;
}
