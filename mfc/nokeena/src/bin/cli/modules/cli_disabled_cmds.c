

#include "common.h"
#include "climod_common.h"
#include "nkn_defs.h"
#include "file_utils.h"

int 
cli_disabled_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);


const int32 cli_disabled_cmds_init_order = INT32_MAX - 1;

int cli_disable_un_supported_pacifica_cmds(
	const lc_dso_info *info,
	const cli_module_context *context);

int 
cli_disabled_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = cli_unregister_command("mfd");
    bail_error(err);

    err = cli_unregister_command("no mfd");
    bail_error(err);

    err = cli_unregister_command("interface * zeroconf");
    bail_error(err);

    err = cli_unregister_command("no interface * zeroconf");
    bail_error(err);
    /*PR 855881
     *CRYPTO Feature is enabled,
     *Making the commands unavialable,
     *Removes the reverse map functionality
     *leading to nodes seen is show runn
     * */

#if 0
    err = cli_unregister_command("crypto");
    bail_error(err);

    err = cli_unregister_command("show crypto");
    bail_error(err);
#endif
    err = cli_unregister_command("namespace * origin-fetch offline fetch file-size");
    bail_error(err);

    err = cli_unregister_command("no namespace * origin-fetch offline fetch file-size");
    bail_error(err);

    err = cli_unregister_command("pub-point");
    bail_error(err);

    err = cli_unregister_command("no pub-point");
    bail_error(err);

    err = cli_unregister_command("show pub-point ");
    bail_error(err);

    err = cli_unregister_command("system debug * ");
    bail_error(err);


    /* Applies for Pacifica only */
    err = cli_disable_un_supported_pacifica_cmds(info, context);
    bail_error(err);

    // Unregister revmaps
    err = cli_revmap_ignore_bindings("/net/interface/config/*/addr/ipv4/zeroconf");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/mfd/licensing/config/mfd_licensed");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/mfd/licensing/config/vp_type03_licensed");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/network/config/threads");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/system/config/debug/level");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/namespace/*/uid");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/namespace/*/cache_inherit");
    bail_error(err);
#if 0
    err = cli_revmap_exclude_bindings("/nkn/nvsd/virtual_player/config/*");
    bail_error(err);

    err = cli_revmap_exclude_bindings("/nkn/nvsd/uri/config/*");
    bail_error(err);
#endif /*  0 */

    err = cli_revmap_ignore_bindings("/nkn/nvsd/origin_fetch/config/*");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/origin_fetch/config/*/partial-content");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/origin_fetch/config/*/offline/fetch/smooth-flow");
    bail_error(err);
 
    err = cli_revmap_ignore_bindings("/nkn/nvsd/origin_fetch/config/*/plain-text");
    bail_error(err);
 
    err = cli_revmap_ignore_bindings("/nkn/nvsd/origin_fetch/config/*/secure");
    bail_error(err);
 
    /* Exclude all the diskcache nodes other than the activated one */
    err = cli_revmap_ignore_bindings("/nkn/nvsd/diskcache/config/disk_id/*");
    bail_error(err);
    err = cli_revmap_ignore_bindings("/nkn/nvsd/diskcache/config/disk_id/*/auto_reformat");
    bail_error(err);
    err = cli_revmap_ignore_bindings("/nkn/nvsd/diskcache/config/disk_id/*/bay");
    bail_error(err);
    err = cli_revmap_ignore_bindings("/nkn/nvsd/diskcache/config/disk_id/*/block_disk_name");
    bail_error(err);
    err = cli_revmap_ignore_bindings("/nkn/nvsd/diskcache/config/disk_id/*/cache_enabled");
    bail_error(err);
    err = cli_revmap_ignore_bindings("/nkn/nvsd/diskcache/config/disk_id/*/commented_out");
    bail_error(err);
    err = cli_revmap_ignore_bindings("/nkn/nvsd/diskcache/config/disk_id/*/fs_partition");
    bail_error(err);
    err = cli_revmap_ignore_bindings("/nkn/nvsd/diskcache/config/disk_id/*/missing_after_boot");
    bail_error(err);
    err = cli_revmap_ignore_bindings("/nkn/nvsd/diskcache/config/disk_id/*/raw_partition");
    bail_error(err);
    err = cli_revmap_ignore_bindings("/nkn/nvsd/diskcache/config/disk_id/*/set_sector_count");
    bail_error(err);
    err = cli_revmap_ignore_bindings("/nkn/nvsd/diskcache/config/disk_id/*/stat_size_name");
    bail_error(err);
    err = cli_revmap_ignore_bindings("/nkn/nvsd/diskcache/config/disk_id/*/sub_provider");
    bail_error(err);
    err = cli_revmap_ignore_bindings("/nkn/nvsd/diskcache/config/disk_id/*/firmware_version");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/mfd/licensing/config/fms_licensed");
    bail_error(err);

    err = cli_revmap_exclude_bindings("/stat/nkn/stats/*");
    bail_error(err);

    err = cli_revmap_exclude_bindings("/nkn/nvsd/am/*");
    bail_error(err);

    err = cli_revmap_exclude_bindings("/nkn/nvsd/*");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/accesslog/config/upload/*");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/rtstream/config/live/server_port/**");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/origin_fetch/config/*/offline/fetch/size");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/pub-point/config/**");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/system/config/debug/mod");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/system/config/debug/level");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/network/config/tunnel-only");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/network/config/threads");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/network/config/policy");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/network/config/stack");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/network/config/afr_fast_start");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/origin_fetch/config/*/ingest-policy");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/pm/process/*/delete_trespassers");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/mfp/config/**");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nfs_mount/config/**");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/nvsd/mfp/status-time");
    bail_error(err);
bail:
    return err;
}



/* These are CLIs we want to suppress on Pacifica */
static const char *suppress_cmds[] = {
    "image",
    "no image",
    "boot",
    "no boot",
#if defined(PROD_FEATURE_CMC_SERVER) || defined(PROD_FEATURE_CMC_CLIENT)
    "cmc",
    "no cmc",
    "show cmc",
#endif
    "show images",
    "show bootvar",
    "show boot",
    "configuration revert factory | keep-cmc-local",
};

int cli_disable_un_supported_pacifica_cmds(
	const lc_dso_info *info,
	const cli_module_context *context)
{
    int err = 0;
    char *buff = NULL;
    uint32 len = 0;
    tstr_array *args = NULL;
    uint32 found_idx = 0;
    cli_command *ret_cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = lf_read_file("/proc/cmdline", &buff, &len);
    bail_error(err);

    err = ts_tokenize_str(buff, ' ', '\\', '"', 0, &args);
    bail_error(err);

    err = tstr_array_linear_search_str(args, "model=pacifica", 0, &found_idx);
    if (err == lc_err_not_found) {
	err = 0;
	goto bail;
    }

    if (!err && (found_idx > 0)) {
	err = cli_suppress_commands(suppress_cmds,
		sizeof(suppress_cmds)/sizeof(char *),
		true);
	bail_error(err);

	err = cli_get_command_registration("configuration revert factory", &ret_cmd);
	bail_error_null(err, ret_cmd);

	ret_cmd->cc_flags &= ~ccf_terminal;
    }
bail:
    safe_free(buff);
    tstr_array_free(&args);
    return err;
}
