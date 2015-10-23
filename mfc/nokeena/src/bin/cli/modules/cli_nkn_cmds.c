/*
 *
 * Filename:  cli_nkn_cmds.c
 * Date:      2008/10/23
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include "common.h"
#include <climod_common.h>
#include <dso.h>
#include <env_utils.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "tstring.h"
#include "random.h"
#include "cli_module.h"
#include "clish_module.h"
#include "file_utils.h"
#include "proc_utils.h"
#include "libnkncli.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"
#include <unistd.h>

#define MOD_RTMP_FMS	"mod-rtmp-fms"
#define MOD_SSLD	"mod-ssl"
#define MOD_WATCH_DOG   "watch-dog"
#define MOD_DELIVERY	"nvsd"
#define MOD_RTMP_ADMIN	"mod-rtmp-admin"
#define MOD_DMI         "mod-dmi"
#define	NKNCNT_FILE	"counters"
#define	NKNCNT_PATH	NKNCNT_BINARY_PATH
#define	NKNCNT_BINARY_PATH	"/opt/nkn/bin/nkncnt"
#define NKNCNT_FILE_PREFIX 	"/var/opt/tms/sysdumps/"
#define AM_GENERATE_LOG    "/opt/nkn/bin/am_dump"
#define FMS_DOWNLOADS_PATH	"/nkn/adobe/downloads"
#define	FMS_INSTALLSCRIPT_PATH	"/opt/nkn/bin/install_fms.sh"
#define FMS_PID_FILE		"/nkn/adobe/fms/fmsmaster.pid"
#define FMS_ADMIN_PID_FILE	"/nkn/adobe/fms/fmsadmin.pid"
#define SYSTEM_PASSWD_FILE	"/var/opt/tms/output/passwd"
#define SYSTEM_TMP_PASSWD_FILE	"/var/opt/tms/output/passwd.orig"
#define DMI_PID_FILE            "/var/run/libvirt/qemu/dmiadapter.pid"

#define ETHFLASH_LOOP_COUNT	"5"
#define MAX_LOOP_COUNT		25
#define WHITE_LIST		"/opt/nkn/bin/whitelist"

#define MOD_CRAWLER  "crawler"
const int32 cli_nkn_cmds_init_order = INT32_MAX - 3;
const char service_config_file_path[] = "/var/log/nkn/";

cli_completion_callback cli_ns_completion = NULL;
cli_help_callback cli_ns_help = NULL;

int cli_nkn_cmds_init(const lc_dso_info *info,
		    const cli_module_context *context);

/*
 *
 */
static int cli_reg_nkn_system_cmds(const lc_dso_info *info, const cli_module_context *context) ;
static int cli_reg_nkn_network_cmds(const lc_dso_info *info, const cli_module_context *context) ;
static int cli_reg_nkn_service_cmds(const lc_dso_info *info, const cli_module_context *context) ;

static int cli_reg_nkn_stats_cmds(const lc_dso_info *info, const cli_module_context *context) ;
static int cli_reg_nkn_mm_cmds(const lc_dso_info *info, const cli_module_context *context) ;

static int cli_reg_named_cmds(const lc_dso_info *info, const cli_module_context *context) ;
static int cli_reg_nkn_junos_re_cmds(const lc_dso_info *info, const cli_module_context *context);

static int
cli_reg_TM_extend_cmds(const lc_dso_info *info,
                    const cli_module_context *context);


static int cli_reg_nkn_ntp_override(const lc_dso_info *info, const cli_module_context *context) ;
static int cli_reg_nkn_version_override(const lc_dso_info *info, const cli_module_context *context) ;

static int 
cli_reg_nkn_snapshot_override(        const lc_dso_info *info,         const cli_module_context *context);
static int cli_reg_nkn_tech_supp_cmds(const lc_dso_info *info, const cli_module_context *context) ;

static int
cli_reg_nkn_rtsched_cmds(const lc_dso_info *info,
                    const cli_module_context *context);

int cli_web_listen_intf_show(const bn_binding_array *bindings,
                             uint32 idx, const bn_binding *binding,
                             const tstring *name,
                             const tstr_array *name_components,
                             const tstring *name_last_part,
                             const tstring *value, void *callback_data);

int
cli_nkn_set_rtsched_threads_value (void *data, const cli_command *cmd,
		                        const tstr_array *cmd_line,
			                const tstr_array *params);

int
cli_nkn_set_sched_threads_value (void *data, const cli_command *cmd,
		                        const tstr_array *cmd_line,
			                const tstr_array *params);

int cli_nkn_cmd_get_mem_stats(const char *mem_type_str,
                           uint32 *ret_mem_total, uint32 *ret_mem_used,
                           uint32 *ret_mem_free);

int
cli_nkn_set_sched_tasks_value (void *data, const cli_command *cmd,
		                        const tstr_array *cmd_line,
		                        const tstr_array *params);

int
cli_nkn_space_agent_set_cfg(void *data, const cli_command *cmd,
		                        const tstr_array *cmd_line,
			                const tstr_array *params);
int
cli_files_config_upload(void *data, const cli_command *cmd,
                    const tstr_array *cmd_line, const tstr_array *params);

static int
cli_files_config_upload_finish(const char *password, clish_password_result result,
                                           const cli_command *cmd, const tstr_array *cmd_line,
                                           const tstr_array *params, void *data,
                                           tbool *ret_continue);

int
cli_files_config_fetch(void *data, const cli_command *cmd,
                    const tstr_array *cmd_line, const tstr_array *params);

static int
cli_files_config_fetch_finish(const char *password, clish_password_result result,
                                           const cli_command *cmd, const tstr_array *cmd_line,
                                           const tstr_array *params, void *data,
                                           tbool *ret_continue);

int
cli_service_space_agent_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int
cli_nkn_service_revmap_cb(
	void *data,
	const cli_command *cmd,
	const bn_binding_array *bindings,
	const char *name,
	const tstr_array *name_parts,
	const char *value,
	const bn_binding *binding,
	cli_revmap_reply *ret_reply);

static int
cli_space_agent_revmap_cb (
	void *data,
	const cli_command *cmd,
	const bn_binding_array *bindings,
	const char *name,
	const tstr_array *name_parts,
	const char *value,
	const bn_binding *binding,
	cli_revmap_reply *ret_reply);


static int
cli_nkn_mm_show_cmd(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nkn_ssp_show_one_cust(
        const bn_binding_array *bindings,
        uint32 idx, const bn_binding *binding,
        const tstring *name, const tstr_array *name_components,
        const tstring *name_last_part,
        const tstring *value,
        void *callback_data);

int
cli_nkn_show_scheduler_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
int
cli_nkn_collect_counters_detail(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_status_module_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int
 cli_service_restart_stat_cb(				
	void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int
 cli_service_restart_stat_cb_with_url(				
	void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int cli_mod_fms_restart(void *data, const cli_command *cmd,
                       const tstr_array *cmd_line, const tstr_array *params);

static int cli_service_show(void *data, const cli_command *cmd,
                       const tstr_array *cmd_line, const tstr_array *params);

static int
cli_service_show_vmem(void *data, const cli_command *cmd,
            const tstr_array *cmd_line, const tstr_array *params);

static int cli_service_show_one(const bn_binding_array *bindings, uint32 idx,
                           const bn_binding *binding, const tstring *name,
                           const tstr_array *name_components,
                           const tstring *name_last_part,
                           const tstring *value, void *callback_data);
int
cli_nkn_list_ftpi_files(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);

/* set frequency , save file and durationi wrapper function */
static int
set_fre_sav_dur(
		tbool found,
		const char *pattern_str,
		const char *time_frequency,
		const char *time_duration,
		const char *file_name,
		tstr_array ** arr);


int
cli_nkn_show_stats_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nkn_show_counters_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

#if 1
/* Commented for bug# 4717 */
/* Uncommented for bug# 4997 */
int
cli_nkn_show_rtsp_counters_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
int
cli_nkn_show_http_counters_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
#endif

int
cli_nkn_show_common_counters(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nkn_show_thresholds_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_tech_support(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_tech_supp_disk(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_tech_supp_am(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int
cli_config_upload_finish(const char *password, clish_password_result result,
                         const cli_command *cmd, const tstr_array *cmd_line,
                         const tstr_array *params, void *data,
                         tbool *ret_continue);

static int
cli_tech_supp_am_upload_finish(const char *password, clish_password_result result,
                         const cli_command *cmd, const tstr_array *cmd_line,
                         const tstr_array *params, void *data,
                         tbool *ret_continue);

static int
md_clear_nvsd_samples(void *data, const cli_command *cmd,
            const tstr_array *cmd_line, const tstr_array *params);

static int
cli_network_print_restart_msg(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);

static int
cli_ssld_network_print_restart_msg(void *data, const cli_command *cmd,
                       const tstr_array *cmd_line,
                       const tstr_array *params);

int
cli_nkn_show_counters_internal_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_smartd_show_counters_internal_cb(
                void *data,
                const cli_command *cmd,
                const tstr_array *cmd_line,
                const tstr_array *params);

extern int
cli_std_exec(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);

static int
cli_show_mem_stats(void *data, const cli_command *cmd,
            const tstr_array *cmd_line, const tstr_array *params);

int
cli_nkn_ntp_server_set(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int
cli_reg_nkn_hostname_override(
        const lc_dso_info *info,
        const cli_module_context *context);
int
cli_nkn_version_copyright_set(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int
cli_app_fms_shell(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int
cli_fms_image_fetch(void *data, const cli_command *cmd,
                const tstr_array *cmd_line,
                const tstr_array *params);

static int
cli_fms_image_fetch_finish(const char *password, clish_password_result result,
                       const cli_command *cmd,
                       const tstr_array *cmd_line, const tstr_array *params,
                       void *data, tbool *ret_continue);
static int
cli_fms_image_fetch_internal(const char *url, const char *password,
                         const char *filename);

static int
cli_fms_image_install(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int
generate_file_name_for_counters(char **file_name);

static int
fms_status(const char *pid_path);

int cli_nkn_launch_program_bg(const char *abs_path, const tstr_array *argv,
		const char *working_dir,
		clish_termination_handler_func termination_handler,
		void *termination_handler_data);

static int
cli_run_ethflash(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
static int
cli_tech_supp_fix_fallback(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

static int cli_reg_nkn_cmm_cmds(const lc_dso_info *info, const cli_module_context *context);
int
cli_nkn_show_cmm_status(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

static int
cli_nkn_run_internal_cmd(void *data, 
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);
static int
cli_exe_shell_cmd(void *data,
                const cli_command *cmd,
                const tstr_array *cmd_line,
                const tstr_array *params);

static int
cli_system_nvsd_coredump_revmap_cb(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_system_nvsd_mod_dmi_revmap_cb(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nkn_change_service_status(void *data, const cli_command *cmd,
            const tstr_array *cmd_line, const tstr_array *params);

int
cli_file_upload_action(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_file_upload_action_finish(const char *password, clish_password_result result,
                         const cli_command *cmd, const tstr_array *cmd_line,
                         const tstr_array *params, void *data,
                         tbool *ret_continue);

int
cli_named_fwder(void *data, const cli_command *cmd,
		const tstr_array *cmd_line, const tstr_array *params);


static int cli_service_restart_mod_domain_filter (
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

/* Local Types */
enum {
	cid_service_show_global,
	cid_service_show_all,
	cid_service_show_one
};

enum {
	 service_running_status_enable = 0,
         service_running_status_disable = 1
};

enum {
	cid_full_counter_display = 0x1,
	cid_http_counter_display = 0x2,
	cid_rtsp_counter_display = 0x4
};

enum {
    cid_nameserver_add = 1,
    cid_nameserver_delete,
};

enum {
    cid_mod_include = 1,
    cid_mod_exclude,
};

/* Local Variables */
/* Removed samples total_cache_byte_count, total_disk_byte_count, total_origin_byte_count,Perportbytes, peroriginbytes, perdiskbytes.*/
const char* sample_clr_list[]={
                             "total_bytes",
                             "num_of_connections",
                             "cache_byte_count",
                             "origin_byte_count",
                             "disk_byte_count",
                             "http_transaction_count",
                             NULL};

cli_expansion_option cli_nkn_services[] = {
    {"mod-delivery", N_(""), (void*) MOD_DELIVERY},
    {"mod-ssl", N_(""), (void*) "ssld"},
    {"mod-ftp", N_(""), (void *) "ftpd"},
    {"mod-log", N_(""), (void*) "nknlogd"},
    {"mod-dmi", N_(""), (void*) MOD_DMI},
    {"mod-file-publisher", N_(""), (void*) "file_mfpd"},
    {"mod-live-publisher", N_(""), (void*) "live_mfpd"},
    {"mod-crawler", N_(""), (void*) MOD_CRAWLER},
    {"watch-dog", N_(""), (void*) MOD_WATCH_DOG},
    {"mod-stat", N_(""), (void *) "nkncnt"},
    {"mod-geodb", N_(""), (void *) "geodbd"},
    {NULL, NULL, NULL},
};


cli_expansion_option cli_nkn_processes[] = {
    /* cli keyword, help text, data associated, used for strcmp() */
    {"mod-ssl", N_("SSL Module"), (void*) "/nkn/nvsd/system/config/mod_ssl"},
    {"mod-ftp", N_("FTP Module"), (void *) "/nkn/nvsd/system/config/mod_ftp"},
    {"mod-crawler", N_("Crawler Module"), (void*) "/nkn/nvsd/system/config/mod_crawler"},
    {"mod-rest-api", N_("Rest-API module"), (void*) "/nkn/nvsd/system/config/mod_rapi"},
    {"mod-jcf", N_("JCF module"), (void*) "/nkn/nvsd/system/config/mod_ccf_dist"},
    {"mod-dmi", N_("DMI module"), (void*) "/nkn/nvsd/system/config/mod_dmi"},
    {NULL, NULL, NULL},
};

cli_expansion_option cli_set_default[] = {
	{ "-", N_("Display all (use defaults)"), NULL},
	{ NULL, NULL, NULL },
};

cli_expansion_option cli_nkn_nvsd_service_mods[] = {
	{"disk", N_("Disk status"), (void *)"preread"},
/* Not used as of now,This shows how it can be expanded for other modules */
//	{"mangement", N_("Management status"), (void *)"mgmt"},
//	{"network", N_("Network status"), (void *)"network"},
};

cli_expansion_option cli_set_collect_default[] = {
	{ "-", N_("Collect all counters details (use defaults)"), NULL},
	{ NULL, NULL, NULL },
};

cli_expansion_option cli_set_network_policy[] = {
	{ "0", N_(" FD Based Policy"), NULL},
	{ "1", N_(" Round Robin Policy"), NULL},
	{ NULL, NULL, NULL },
};
cli_expansion_option cli_set_network_stack[] = {
	{ "linux_kernel_stack", N_("Linux kernel stack"), NULL},
	{ "nokeena_user_space_stack", N_("Nokeena User space stack "), NULL},
	{ NULL, NULL, NULL },
};
enum {
	cid_reset_rtsched = 0,
	cid_set_rtsched = 1
};

enum {
	cid_reset_sched = 0,
	cid_set_sched = 1
};

enum {
	cid_set_port = 0,
	cid_reset_port = 1
};

enum {
	cid_agent_enable = 0,
	cid_agent_disable = 1
};

static const char *cli_file_snapshot_hidden_cmds[] = {
	"file snapshot",
	"file snapshot delete *",
	"file snapshot upload *",
	"show files snapshot ",
};


enum {
	reset_deadline_tasks = 0,
	set_deadline_tasks =1,
};
/* ------------------------------------------------------------------------- */
int
cli_nkn_cmds_init(const lc_dso_info *info,
                     const cli_module_context *context)
{
    int err = 0;
    tbool exist = false;

    if (context->cmc_mgmt_avail == false) {
        goto bail;
    }

    err = cli_reg_nkn_system_cmds(info,context);
    bail_error(err);

    /* Junos RE commands - Initialize only for pacifica */
    err = lf_test_is_regular("/etc/pacifica_hw", &exist);
    if (!err && exist) {
        err = cli_reg_nkn_junos_re_cmds(info, context);
        bail_error(err);
    }

    err = cli_reg_nkn_network_cmds(info,context);
    bail_error(err);

    err = cli_reg_nkn_service_cmds(info, context);
    bail_error(err);

    err = cli_reg_nkn_stats_cmds(info, context);
    bail_error(err);

    err = cli_reg_nkn_mm_cmds(info, context);
    bail_error(err);

    err = cli_reg_nkn_tech_supp_cmds(info, context);
    bail_error(err);

    err = cli_reg_nkn_ntp_override(info, context);
    bail_error(err);

    err = cli_reg_nkn_hostname_override(info, context);
    bail_error(err);

    err = cli_reg_nkn_version_override(info, context);
    bail_error(err);

    err = cli_reg_nkn_rtsched_cmds(info, context);
    bail_error(err);

    err = cli_reg_nkn_cmm_cmds(info, context);
    bail_error(err);
  err = cli_reg_nkn_snapshot_override(info, context);
  bail_error(err);

    err = cli_reg_named_cmds(info, context);
    bail_error(err);

    err = cli_reg_TM_extend_cmds(info, context);
    bail_error(err);

bail:
    return (err);
}


/*
 * System node is for system wide level configuration in nvsd.
 */

static int cli_show_system_cmds (
	void *data,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params)
{
	int err = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	err = cli_printf_query (_("system debug level   : "
			"#/nkn/nvsd/system/config/debug/level#\n"));
	bail_error(err);

	err = cli_printf_query (_("system debug mod     : "
			"#/nkn/nvsd/system/config/debug/mod#\n"));
        bail_error(err);

bail:
	return err;
}

static int
cli_show_system_mirror_status(void *data, const cli_command *cmd,
            const tstr_array *cmd_line, const tstr_array *params)
{
        int err = 0;
        int num_params = 0;
        tstr_array *argv = NULL;

        UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(cmd);
        UNUSED_ARGUMENT(params);

        /* Get the number of parameters first */
        num_params = tstr_array_length_quick(cmd_line);
        lc_log_basic(LOG_DEBUG, _("Num params:%d\n"), num_params);

        err = tstr_array_new ( &argv , NULL);
        bail_error (err);
        err = tstr_array_append_str (argv, "/opt/nkn/bin/mirror_status.sh");
        bail_error (err);

	if (num_params == 6) {
	    err = tstr_array_append_str (argv, "-v");
	    bail_error (err);
	}

        err = nkn_clish_run_program_fg("/opt/nkn/bin/mirror_status.sh", argv, NULL, NULL, NULL);
        bail_error(err);

bail:
        tstr_array_free(&argv);
        return err;
} /* end of cli_show_system_mirror_status() */

static int
cli_system_mirror_resync(void *data, const cli_command *cmd,
            const tstr_array *cmd_line, const tstr_array *params)
{
        int err = 0;
        int num_params = 0;
        tstr_array *argv = NULL;

        UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(cmd);
        UNUSED_ARGUMENT(cmd_line);

        /* Get the number of parameters first */
        num_params = tstr_array_length_quick(cmd_line);
        lc_log_basic(LOG_DEBUG, _("Num params:%d\n"), num_params);

        err = tstr_array_new ( &argv , NULL);
        bail_error (err);
        err = tstr_array_append_str (argv, "/opt/nkn/bin/mirror_resync.sh");
        bail_error (err);
	if (num_params == 5) {
	    err = tstr_array_append_str (argv, "-v");
	    bail_error (err);
	}

        err = nkn_clish_run_program_fg("/opt/nkn/bin/mirror_resync.sh", argv, NULL, NULL, NULL);
        bail_error(err);

bail:
        tstr_array_free(&argv);
        return err;
} /* end of cli_system_mirror_resync() */

int
cli_junos_re_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply);

int
cli_junos_re_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *rev_cmd = NULL, *t_username = NULL, *t_password = NULL;

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &t_username, "/nkn/junos-re/auth/username");
    bail_error_null(err, t_username);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &t_password, "/nkn/junos-re/auth/password");
    bail_error_null(err, t_password);

    err = ts_new_sprintf(&rev_cmd, "junos-re auth username %s password %s",
			ts_str(t_username), ts_str(t_password));

    err = tstr_array_append_takeover(ret_reply->crr_cmds,&rev_cmd);
    bail_error(err);

#define CONSUME_REVMAP_NODES(c) \
    {\
        err = tstr_array_append_sprintf(ret_reply->crr_nodes, c);\
        bail_error(err);\
    }

    CONSUME_REVMAP_NODES("/nkn/junos-re/auth/username");
    CONSUME_REVMAP_NODES("/nkn/junos-re/auth/password");

bail:
    ts_free(&rev_cmd);
    ts_free(&t_username);
    ts_free(&t_password);
    return err;
}

static int
cli_reg_nkn_junos_re_cmds(const lc_dso_info *info,
                    const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str      = "junos-re";
    cmd->cc_help_descr     = N_("Junos RE configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "junos-re auth";
    cmd->cc_help_descr     = N_("Junos RE authentication configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "junos-re auth username";
    cmd->cc_help_descr     = N_("Junos RE authentication username configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "junos-re auth username *";
    cmd->cc_help_exp       = N_("<username>");
    cmd->cc_help_exp_hint  = N_("provide username of RE");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "junos-re auth username * password";
    cmd->cc_help_descr     = N_("Junos RE authentication password configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "junos-re auth username * password *";
    cmd->cc_help_exp       = N_("<password>");
    cmd->cc_help_exp_hint  = N_("provide password of RE");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/junos-re/auth/username";
    cmd->cc_exec_value     = "$1$";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_name2     = "/nkn/junos-re/auth/password";
    cmd->cc_exec_value2    = "$2$";
    cmd->cc_exec_type2     = bt_string;
    cmd->cc_revmap_type    = crt_manual;
    cmd->cc_revmap_callback = cli_junos_re_revmap;
    cmd->cc_revmap_order   = cro_junos_re;
    CLI_CMD_REGISTER;

bail:
    return err;
}

static int
cli_reg_nkn_system_cmds(const lc_dso_info *info,
                    const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str = "system";
    cmd->cc_flags          = ccf_hidden;
    cmd->cc_help_descr     = N_("system wide level configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "system debug";
    cmd->cc_flags          = ccf_hidden;
    cmd->cc_help_descr     = N_("set debug configurations");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "system debug command";
    cmd->cc_help_descr     = N_("set debug command");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "system debug command *";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_help_exp       = N_("<command>");
    cmd->cc_help_exp_hint  = N_("run a gdb command. - program may crash"); 
    cmd->cc_exec_callback =  cli_nkn_run_internal_cmd;
    cmd->cc_revmap_type =       	crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "system debug level";
    cmd->cc_help_descr     = N_("set debug level");
    cmd->cc_flags          = ccf_unavailable;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "system debug level *";
    cmd->cc_flags          = ccf_terminal | ccf_unavailable;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_help_exp       = N_("<level>");
    cmd->cc_help_exp_hint  = N_("\n1: SEVERE\n2: ERROR\n3: WARNING\n4: MESSAGE\n5-7: DEBUG");
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/nvsd/system/config/debug/level";
    cmd->cc_exec_type      = bt_uint32;
    cmd->cc_exec_value     = "$1$";
    cmd->cc_revmap_type =       	crt_none;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str = "system debug mod";
    cmd->cc_help_descr     = N_("set debug mod");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "system debug mod *";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_help_exp       = N_("<mod>");
    cmd->cc_help_exp_hint  = N_("\n0x0000000000000001 MOD_NETWORK"
				"\n0x0000000000000002 MOD_HTTP"
				"\n0x0000000000000004 MOD_DM2"
				"\n0x0000000000000008 MOD_MM"
				"\n0x0000000000000010 MOD_BM"
				"\n0x0000000000000020 MOD_CE"
				"\n0x0000000000000040 MOD_SCHED"
				"\n0x0000000000000080 MOD_SSP"
				"\n0x0000000000000100 MOD_HTTPHDRS"
				"\n0x0000000000000200 MOD_FQUEUE"
				"\n0x0000000000000400 MOD_SYSTEM"
				"\n0x0000000000000800 MOD_FM"
				"\n0x0000000000001000 MOD_OM"
				"\n0x0000000000002000 MOD_OOMGR"
				"\n0x0000000000004000 MOD_TCP"
				"\n0x0000000000008000 MOD_TFM"
				"\n0x0000000000010000 MOD_NAMESPACE"
				"\n0x0000000000020000 MOD_NFS"
				"\n0x0000000000040000 MOD_RTSP"
                		"\n0x0000000000080000 MOD_AM"
                		"\n0x0000000000100000 MOD_MM_PROMOTE"
                		"\n0x0000000000200000 MOD_FUSE"
                		"\n0x0000000000400000 MOD_VPEMGR"
// Bug# 3914           		"\n0x0000000000800000 MOD_GM" 
                		"\n0x0000000001000000 MOD_DASHBOARD"
                		"\n0x0000000002000000 MOD_CACHELOG"
                		"\n0x0000000004000000 MOD_CP"
				"\n"
                		"\n0x4000000000000000 MOD_TRACE"
                		"\n0x8000000000000000 MOD_CACHE"
				"\n0x0000000020000000 MOD_CLUSTER"
				"\n0xFFFFFFFFFFFFFFFF MOD_ANY");

    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/nvsd/system/config/debug/mod";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "$1$";
    cmd->cc_revmap_type =       	crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "show system";
    cmd->cc_flags 	   = ccf_terminal;
    cmd->cc_capab_required = ccp_query_rstr_curr;
    cmd->cc_help_descr     = N_("Display system configuration");
    cmd->cc_exec_callback  = cli_show_system_cmds;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "show system mirror";
    cmd->cc_help_descr     = N_("Display Mirror configuration and status");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "show system mirror boot-device";
    cmd->cc_help_descr     = N_("Display Mirror configuration and status");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "show system mirror boot-device status";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_query_basic_curr;
    cmd->cc_help_descr     = N_("Display Mirror configuration and status");
    cmd->cc_exec_callback  = cli_show_system_mirror_status;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "show system mirror boot-device status detail";
    cmd->cc_flags          = ccf_terminal | ccf_hidden;
    cmd->cc_capab_required = ccp_query_basic_curr;
    cmd->cc_help_descr     = N_("Display Mirror configuration and status");
    cmd->cc_exec_callback  = cli_show_system_mirror_status;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "system mirror";
    cmd->cc_help_descr     = N_("Rebuild the boot device mirror");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "system mirror boot-device";
    cmd->cc_help_descr     = N_("Rebuild the boot device mirror");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "system mirror boot-device resync";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_help_descr     = N_("Rebuild the boot device mirror");
    cmd->cc_exec_callback  = cli_system_mirror_resync;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "system mirror boot-device resync detail";
    cmd->cc_flags          = ccf_terminal | ccf_hidden;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_help_descr     = N_("Rebuild the boot device mirror");
    cmd->cc_exec_callback  = cli_system_mirror_resync;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "application";
    cmd->cc_help_descr     = N_("Configure 3rd party applications");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "application fms";
    cmd->cc_help_descr     = N_("FMS configuration");
    cmd->cc_flags 	   = ccf_hidden | ccf_unavailable;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "application fms shell";
    cmd->cc_flags 	   = ccf_terminal | ccf_hidden;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_help_descr     = N_("Shell to configure FMS");
    cmd->cc_exec_callback  = cli_app_fms_shell;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "application fms download";
    cmd->cc_help_descr     = N_("FMS download option");
    CLI_CMD_REGISTER

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "application fms download *";
    cmd->cc_flags          = ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required = ccp_action_priv_curr;
    cmd->cc_help_exp =          N_("<URL>");
    cmd->cc_help_exp_hint =     N_("scp/http/ftp URL");
    cmd->cc_exec_callback  = cli_fms_image_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "application fms install";
    cmd->cc_help_descr     = N_("FMS installation option");
    CLI_CMD_REGISTER


    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "application fms install *";
    cmd->cc_magic          = cfi_fmsfiles;
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_priv_curr;
    cmd->cc_help_exp       = cli_msg_filename;
    cmd->cc_help_use_comp  = true;
    cmd->cc_comp_callback  = cli_file_completion;
    cmd->cc_exec_callback  = cli_fms_image_install;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	    = "application mfp ";
    cmd->cc_help_descr	    = "configures mfp application";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	    = "application mfp *";
    cmd->cc_help_exp	    = "<disk name>";
    cmd->cc_help_exp_hint   = "mfp disk configuration";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	    = "application mfp * attach";
    cmd->cc_help_descr	    = "attach mfp application to disk";
    cmd->cc_flags	    = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/nfs_mount/config/$1$/type";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "disk";
    cmd->cc_exec_name2      = "/nkn/nfs_mount/config/$1$/mountpath";
    cmd->cc_exec_type2      = bt_string;
    cmd->cc_exec_value2     = "/nkn/pers/ps_1/MFP/ingest";
    cmd->cc_revmap_order   = cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	    = "application mfp * detach";
    cmd->cc_help_descr	    = "detach mfp application to disk";
    cmd->cc_flags	    = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_exec_operation = cxo_delete;
    cmd->cc_exec_name      = "/nkn/nfs_mount/config/$1$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	   = "system generic";
    cmd->cc_help_descr	   = N_("system generic command");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	   = "system generic command";
    cmd->cc_help_descr	   = N_("system generic command configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "system generic command *";
    cmd->cc_help_exp       = N_("<shell command>");
    cmd->cc_help_exp_hint  = N_("Enter the shell command to be executed - \"ps -aef\"");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_exec_callback  = cli_exe_shell_cmd;
    CLI_CMD_REGISTER;

bail:
    return err;
}


static int cli_show_network_hidden_cmds(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
        int err = 0;
        bn_binding_array *bindings = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

        err = mdc_get_binding_children(
                        cli_mcc,
                        NULL,
                        NULL,
                        false,
                        &bindings,
                        false,
                        true,
                        "/nkn");
        bail_error(err);

        err = cli_printf_query (_("network total running threads         : "
                        "#/nkn/nvsd/network/config/threads#\n"));
        bail_error(err);

        err = cli_printf_query (_("network threads load balance policy   : "
                        "#/nkn/nvsd/network/config/policy#\n"));
        bail_error(err);

        err = cli_printf_query (_("network running stack                 : "
                        "#/nkn/nvsd/network/config/stack#\n"));
        bail_error(err);

        err = cli_printf_query (_("network faststart (sec)               : "
                        "#/nkn/nvsd/network/config/afr_fast_start#\n"));
        bail_error(err);

        err = cli_printf_query (_("network tunnel-only Enabled           : "
                        "#/nkn/nvsd/network/config/tunnel-only#\n"));
        bail_error(err);

bail:
        bn_binding_array_free(&bindings);

        return err;
}


static int
cli_reg_TM_extend_cmds(const lc_dso_info *info,
                    const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "configuration service";
    cmd->cc_help_descr =	    "Service configuration (such as Policy Engine script, server map and SSL certificates";
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =		    "configuration service files";
    cmd->cc_help_descr =	    "Service configuration files";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "configuration service files upload";
    cmd->cc_help_descr =	    "Upload service configuration files";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "configuration service files fetch";
    cmd->cc_help_descr =	    "Fetch service configuration files";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "configuration service files upload *";
    cmd->cc_flags |=                ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_help_exp =              cli_msg_url_alt_ul;
    cmd->cc_help_exp_hint =         cli_msg_url_alt_ul_hint;
    cmd->cc_exec_callback =         cli_files_config_upload;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "configuration service files fetch *";
    cmd->cc_flags |=                ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_help_exp =              cli_msg_url_alt_dl;
    cmd->cc_help_exp_hint =         cli_msg_url_alt_dl_hint;
    cmd->cc_exec_callback =         cli_files_config_fetch;
    CLI_CMD_REGISTER;

bail:
    return err;

}

static int
cli_reg_nkn_network_cmds(const lc_dso_info *info,
                    const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    /* "network" is registered elsewhere */
    CLI_CMD_NEW;
    cmd->cc_words_str = "network tunnel-only";
    cmd->cc_help_descr     = N_("Config tunnel-only debug feature");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "network tunnel-only enable";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_help_descr     = N_("Enables tunnel-only debug feature");
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/nvsd/network/config/tunnel-only";
    cmd->cc_exec_type      = bt_bool;
    cmd->cc_exec_value     = "true";
    cmd->cc_revmap_order   = cro_mgmt;
    cmd->cc_revmap_type    = crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "network tunnel-only disable";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_help_descr     = N_("Disables tunnel-only debug feature");
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/nvsd/network/config/tunnel-only";
    cmd->cc_exec_type      = bt_bool;
    cmd->cc_exec_value     = "false";
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "network threads";
    cmd->cc_flags          = ccf_hidden;
    cmd->cc_help_descr     = N_("create network threads");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "network threads *";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_help_exp       = N_("<integer>");
    cmd->cc_help_exp_hint  = N_("\n1-16: Number of threads to be created");
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/nvsd/network/config/threads";
    cmd->cc_exec_type      = bt_uint32;
    cmd->cc_exec_value     = "$1$";
    cmd->cc_exec_callback  = cli_network_print_restart_msg;
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "network policy";
    cmd->cc_flags          = ccf_hidden;
    cmd->cc_help_descr     = N_("set network threads policy");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "network policy *";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_help_exp       = N_("<policy_ID>");
    cmd->cc_help_exp_hint  = N_("Select a policy id from below");
    cmd->cc_help_options 	=	cli_set_network_policy;
    cmd->cc_help_num_options 	=       sizeof(cli_set_network_policy) / 
	    				sizeof(cli_expansion_option);
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/nvsd/network/config/policy";
    cmd->cc_exec_type      = bt_uint32;
    cmd->cc_exec_value     = "$1$";
    cmd->cc_exec_callback  = cli_network_print_restart_msg;
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "network stack";
    cmd->cc_flags          = ccf_hidden;
    cmd->cc_help_descr     = N_("choose a network stack");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "network stack *";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_help_exp       = N_("<string>");
    cmd->cc_help_exp_hint  = N_("Select stack type from below");
    cmd->cc_help_options 	=	cli_set_network_stack;
    cmd->cc_help_num_options 	=       sizeof(cli_set_network_stack) / 
	    				sizeof(cli_expansion_option);
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/nvsd/network/config/stack";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "$1$";
    cmd->cc_exec_callback  = cli_network_print_restart_msg;
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "network faststart";
    cmd->cc_help_descr     = N_("set session assured flow rate fast start");
    cmd->cc_flags =         ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "network faststart *";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_help_exp       = N_("<seconds>");
    cmd->cc_help_exp_hint  = N_("The first \"n\" seconds will deliver video faster");
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/nvsd/network/config/afr_fast_start";
    cmd->cc_exec_type      = bt_uint32;
    cmd->cc_exec_value     = "$1$";
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "show network internal";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_query_rstr_curr;
    cmd->cc_help_descr     = N_("Display network configuration");
    cmd->cc_exec_callback  = cli_show_network_hidden_cmds;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "network ssld-threads";
    cmd->cc_flags          = ccf_hidden;
    cmd->cc_help_descr     = N_("create network threads");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "network ssld-threads *";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_help_exp       = N_("<integer>");
    cmd->cc_help_exp_hint  = N_("\n1-4: Number of threads to be created");
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/ssld/config/network/threads";
    cmd->cc_exec_type      = bt_uint32;
    cmd->cc_exec_value     = "$1$";
    cmd->cc_exec_callback  = cli_ssld_network_print_restart_msg;
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

bail:
    return err;
}


static int
cli_reg_nkn_service_cmds(const lc_dso_info *info,
                    const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);


    CLI_CMD_NEW;
    cmd->cc_words_str	   = "service start";
    cmd->cc_help_descr     = N_("start a service");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	   = "service restart";
    cmd->cc_help_descr     = N_("restart a service");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	   = "service stop";
    cmd->cc_help_descr     = N_("terminate a service");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service restart mod-delivery";
    cmd->cc_help_descr     = N_("restart delivery service");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_rstr_curr;
#if 0
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/pm/actions/restart_process";
    cmd->cc_exec_name      = "process_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "nvsd";
#endif
    cmd->cc_exec_callback  = md_clear_nvsd_samples;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service start *";
    cmd->cc_help_exp =      N_("<service name>");
    cmd->cc_help_term =     N_("Enable service configuration and status");
    cmd->cc_help_exp_hint = N_("Enable service specific configuration and status");
    cmd->cc_help_options =  cli_nkn_processes;
    cmd->cc_help_num_options = sizeof(cli_nkn_processes)/sizeof(cli_expansion_option);
    cmd->cc_magic          = service_running_status_enable;
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_exec_callback = cli_nkn_change_service_status;
    cmd->cc_revmap_names   = "/nkn/nvsd/system/config/*";
    cmd->cc_revmap_type    = crt_manual;
    cmd->cc_revmap_order   = cro_pm;
    cmd->cc_revmap_callback= cli_nkn_service_revmap_cb;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service restart mod-ssl";
    cmd->cc_help_descr     = N_("restart ssl service");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/pm/actions/restart_process";
    cmd->cc_exec_name      = "process_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "ssld";
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str      = "service stop *";
    cmd->cc_help_exp =      N_("<service name>");
    cmd->cc_help_term =     N_("Disable service configuration and status");
    cmd->cc_help_exp_hint = N_("Disable service specific configuration and status");
    cmd->cc_help_options =  cli_nkn_processes;
    cmd->cc_help_num_options = sizeof(cli_nkn_processes)/sizeof(cli_expansion_option);
    cmd->cc_magic	    = service_running_status_disable;
    cmd->cc_flags	    = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_revmap_type	    = crt_none;
    cmd->cc_exec_callback   = cli_nkn_change_service_status;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service restart mod-log";
    cmd->cc_help_descr     = N_("restart mod-log service");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/pm/actions/restart_process";
    cmd->cc_exec_name      = "process_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "nknlogd";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service restart mod-ftp";
    cmd->cc_help_descr     = N_("restart pre-stage ftp service");
    cmd->cc_flags          = ccf_terminal | ccf_hidden;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/pm/actions/restart_process";
    cmd->cc_exec_name      = "process_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "ftpd";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service restart mod-oom";
    cmd->cc_help_descr     = N_("restart offline OM service");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/pm/actions/restart_process";
    cmd->cc_exec_name      = "process_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "nknoomgr";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service stop mod-stat";
    cmd->cc_help_descr     = N_("terminate mod-stat[nkncnt] service");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/pm/actions/terminate_process";
    cmd->cc_exec_name      = "process_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "nkncnt";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service restart mod-stat";
    cmd->cc_help_descr     = N_("restart mod-stat[nkncnt] service");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_exec_callback  = cli_service_restart_stat_cb;
    CLI_CMD_REGISTER;

     CLI_CMD_NEW;
    cmd->cc_words_str      = "service restart mod-stat url";
    cmd->cc_help_descr     = N_("Specify the url to copy counters");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str      = "service restart mod-stat url *";
    cmd->cc_flags          = ccf_terminal|ccf_sensitive_param|ccf_ignore_length;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_help_exp       = N_("url");
    cmd->cc_help_exp_hint  = N_("[scp|sftp|ftp]://<username>:<password>@<hostname>/<path>");
    cmd->cc_exec_callback  = cli_service_restart_stat_cb_with_url;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service restart mod-stat flush";
    cmd->cc_help_descr     = N_("restart mod-stat[nkncnt] service");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/pm/actions/send_signal";
    cmd->cc_exec_name      = "process_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "nkncnt";
    cmd->cc_exec_name2      = "signal_name";
    cmd->cc_exec_type2      = bt_string;
    cmd->cc_exec_value2     = "SIGHUP";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service restart "MOD_RTMP_FMS ;
    cmd->cc_help_descr     = N_("restart FMS RTMP service");
    cmd->cc_flags          = ccf_terminal | ccf_unavailable;
    cmd->cc_capab_required = ccp_query_basic_curr;
    cmd->cc_exec_callback  = cli_mod_fms_restart;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service restart "MOD_RTMP_ADMIN ;
    cmd->cc_help_descr     = N_("restart RTMP admin service");
    cmd->cc_flags          = ccf_terminal | ccf_unavailable;
    cmd->cc_capab_required = ccp_query_basic_curr;
    cmd->cc_exec_callback  = cli_mod_fms_restart;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service status" ;
    cmd->cc_help_descr     = "Define the behavior of status";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service status mod-delivery" ;
    cmd->cc_help_descr     = "Define the behavior of status for mod-delivery";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service status mod-delivery include" ;
    cmd->cc_help_descr     = "Include a module in"
				    " deciding delivery's current status";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service status mod-delivery include *" ;
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_magic	   = cid_mod_include;
    cmd->cc_exec_callback  = cli_nvsd_status_module_config;
    cmd->cc_help_options   = cli_nkn_nvsd_service_mods;
    cmd->cc_help_num_options =
		     sizeof(cli_nkn_nvsd_service_mods)/sizeof(cli_expansion_option);
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service status mod-delivery exclude" ;
    cmd->cc_help_descr     = "Excluded a module in"
				    " deciding delivery's current status";

    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service status mod-delivery exclude *" ;
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_magic	   = cid_mod_exclude;
    cmd->cc_exec_callback  = cli_nvsd_status_module_config;
    cmd->cc_help_options   = cli_nkn_nvsd_service_mods;
    cmd->cc_help_num_options =
		     sizeof(cli_nkn_nvsd_service_mods)/sizeof(cli_expansion_option);
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str      = "service junos-agent" ;
    cmd->cc_help_descr     = "Enable or disable junos-agent for management from JSCD";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service junos-agent restart" ;
    cmd->cc_help_descr     = "Enable junos-agent for management from JSCD";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/pm/actions/restart_process";
    cmd->cc_exec_name      = "process_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "ssh_tunnel";
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str      = "service junos-agent enable" ;
    cmd->cc_help_descr     = "Enable junos-agent for management from JSCD";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_magic	   = cid_agent_enable;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_exec_callback  = cli_service_space_agent_cb;
    cmd->cc_revmap_names   = "/pm/process/ssh_tunnel/auto_launch";
    cmd->cc_revmap_order   = cro_mgmt;
    cmd->cc_revmap_callback= cli_space_agent_revmap_cb;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service junos-agent disable" ;
    cmd->cc_help_descr     = "Disable junos-agent for management from JSCD";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_magic	   = cid_agent_disable;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_exec_callback  = cli_service_space_agent_cb;
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service junos-agent port" ;
    cmd->cc_help_descr     = "Specify the port on which junos-agent will listen";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service junos-agent port *" ;
    cmd->cc_help_exp       = "<port>";
    cmd->cc_help_exp_hint  = "Specify the port on which junos-agent will listen";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_exec_name	   = "/nkn/mgmt-if/config/dmi-port";
    cmd->cc_exec_type	   = bt_uint32;
    cmd->cc_exec_value	   = "$1$";
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_revmap_names   = "/nkn/mgmt-if/config/dmi-port";
    cmd->cc_revmap_order   = cro_mgmt;
    cmd->cc_revmap_callback= cli_space_agent_revmap_cb;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str      = "no service junos-agent" ;
    cmd->cc_help_descr     = "Reset junos-agent settings";
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str      = "no service junos-agent port" ;
    cmd->cc_help_descr     = "Reset the port on which junos-agent will listen";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_exec_name	   = "/nkn/mgmt-if/config/dmi-port";
    cmd->cc_exec_type	   = bt_uint32;
    cmd->cc_exec_operation = cxo_reset;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_magic	   = cid_reset_port;
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str      = "service bypass-on-failure" ;
    cmd->cc_help_descr     = N_("enable/disable bypass when delivery engine fails");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service bypass-on-failure mod-delivery" ;
    cmd->cc_help_descr     = N_("enable/disable bypass when delivery fails");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service bypass-on-failure mod-delivery enable" ;
    cmd->cc_help_descr     = N_("enable bypassing when delivery fails");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/nvsd/system/config/bypass";
    cmd->cc_exec_value     = "1";
    cmd->cc_exec_type      = bt_uint32;
    cmd->cc_revmap_type    = crt_auto;
    cmd->cc_revmap_order   = cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service bypass-on-failure mod-delivery enable optimized" ;
    cmd->cc_help_descr     = N_("enable bypassing when delivery fails (optimized)");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/nvsd/system/config/bypass";
    cmd->cc_exec_value     = "2";
    cmd->cc_exec_type      = bt_uint32;
    cmd->cc_revmap_type    = crt_auto;
    cmd->cc_revmap_order   = cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service bypass-on-failure mod-delivery disable" ;
    cmd->cc_help_descr     = N_("disable bypassing when delivery fails");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/nvsd/system/config/bypass";
    cmd->cc_exec_value     = "0";
    cmd->cc_revmap_type    = crt_none;
    cmd->cc_exec_type      = bt_uint32;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str      = "service snapshot" ;
    cmd->cc_help_descr     = N_("enable/disable snapshot when service crashes");
    cmd->cc_revmap_type    = crt_manual;
    cmd->cc_revmap_order   = cro_mgmt;
    cmd->cc_revmap_callback =   cli_system_nvsd_coredump_revmap_cb;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service snapshot mod-delivery" ;
    cmd->cc_help_descr     = N_("enable/disable snapshot (default: disabled)");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service snapshot mod-delivery enable" ;
    cmd->cc_help_descr     = N_("enable snapshot (restart could be slow)");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/nvsd/system/config/coredump";
    cmd->cc_exec_value     = "1";
    cmd->cc_exec_type      = bt_uint32;
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;
#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str      = "service start "MOD_DMI ;
    cmd->cc_help_descr     = N_("Starts the service that interfaces with JUNOS Space");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/nvsd/system/config/mod_dmi";
    cmd->cc_exec_value     = "1";
    cmd->cc_exec_type      = bt_uint32;
    cmd->cc_revmap_type    = crt_manual;
    cmd->cc_revmap_order   = cro_mgmt;
    cmd->cc_revmap_callback =   cli_system_nvsd_mod_dmi_revmap_cb;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service stop "MOD_DMI ;
    cmd->cc_help_descr     = N_("Stop the service that interfaces with JUNOS Space");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/nvsd/system/config/mod_dmi";
    cmd->cc_exec_value     = "0";
    cmd->cc_exec_type      = bt_uint32;
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;
#endif


    CLI_CMD_NEW;
    cmd->cc_words_str      = "service snapshot mod-delivery disable" ;
    cmd->cc_help_descr     = N_("disable snapshot (no snapshot for debug)");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/nvsd/system/config/coredump";
    cmd->cc_exec_value     = "0";
    cmd->cc_exec_type      = bt_uint32;
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "service flash-led";
    cmd->cc_help_descr     = N_("Flash the ethernet LEDs in sequence");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service flash-led ethernet";
    cmd->cc_help_descr     = N_("Flash the ethernet LEDs in sequence");
    cmd->cc_flags 	   = ccf_terminal;
    cmd->cc_capab_required = ccp_query_basic_curr;
    cmd->cc_exec_callback  = cli_run_ethflash;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service flash-led ethernet *";
    cmd->cc_help_exp       = N_("<count>");
    cmd->cc_help_exp_hint  = "Number of times to loop through the ports(range 1-25)";
    cmd->cc_flags 	   = ccf_terminal;
    cmd->cc_capab_required = ccp_query_basic_curr;
    cmd->cc_exec_callback  = cli_run_ethflash;
    CLI_CMD_REGISTER;
   
    CLI_CMD_NEW;
    cmd->cc_words_str      = "service restart mod-file-publisher";
    cmd->cc_help_descr     = N_("restart file-mfpd service");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/pm/actions/restart_process";
    cmd->cc_exec_name      = "process_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "file_mfpd";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service restart mod-live-publisher";
    cmd->cc_help_descr     = N_("restart live-mfpd service");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/pm/actions/restart_process";
    cmd->cc_exec_name      = "process_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "live_mfpd";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service restart mod-domain-filter";
    cmd->cc_help_descr     = N_("restart mod-domain-filter service");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_rstr_curr;
    cmd->cc_exec_callback  = cli_service_restart_mod_domain_filter;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service stop mod-file-publisher";
    cmd->cc_help_descr     = N_("Terminate file-mfpd service");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/pm/actions/terminate_process";
    cmd->cc_exec_name      = "process_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "file_mfpd";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service stop mod-live-publisher";
    cmd->cc_help_descr     = N_("Terminate live-mfpd service");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/pm/actions/terminate_process";
    cmd->cc_exec_name      = "process_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "live_mfpd";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service stop "MOD_RTMP_FMS ;
    cmd->cc_help_descr     = N_("Terminate FMS RTMP service");
    cmd->cc_flags          = ccf_terminal | ccf_unavailable;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/nkn/nvsd/services/actions/stop";
    cmd->cc_exec_name      = "service_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = MOD_RTMP_FMS;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service stop "MOD_RTMP_ADMIN ;
    cmd->cc_help_descr     = N_("Terminate RTMP admin service");
    cmd->cc_flags          = ccf_terminal | ccf_unavailable;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/nkn/nvsd/services/actions/stop";
    cmd->cc_exec_name      = "service_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = MOD_RTMP_ADMIN;
    CLI_CMD_REGISTER;

    /* CRAWLER CLI*/

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service restart mod-crawler";
    cmd->cc_help_descr     = N_("Stop and Start the crawler service");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/pm/actions/restart_process";
    cmd->cc_exec_name      = "process_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = MOD_CRAWLER;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "show service";
    cmd->cc_help_descr     = N_("Display service-specific configuration and status");
    cmd->cc_help_term       = N_("Show all service configuration and status");
    cmd->cc_magic 	   = cid_service_show_all;
    cmd->cc_flags 	   = ccf_terminal;
    cmd->cc_capab_required = ccp_query_basic_curr;
    cmd->cc_exec_callback  = cli_service_show;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =     "show service *";
    cmd->cc_help_exp =      N_("<service name>");
    cmd->cc_help_term =     N_("Display service configuration and status");
    cmd->cc_help_exp_hint = N_("Show service specific configuration and status");
    cmd->cc_help_options =  cli_nkn_services;
    cmd->cc_help_num_options = sizeof(cli_nkn_services)/sizeof(cli_expansion_option);
    cmd->cc_magic 	   = cid_service_show_one;
    cmd->cc_flags =           ccf_terminal;
    cmd->cc_capab_required = ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_service_show;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =     "show service * vmem";
    cmd->cc_help_descr =    N_("Show Service memory status");
    cmd->cc_flags =         ccf_terminal | ccf_unavailable;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_service_show_vmem;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =     "show memory stats";
    cmd->cc_help_descr =    N_("Display monitored process memory stats");
    cmd->cc_flags =         ccf_terminal | ccf_unavailable;
    cmd->cc_capab_required = ccp_query_basic_curr;
    cmd->cc_exec_callback = cli_show_mem_stats;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings_va(5,
		    "/nkn/nvsd/system/config/coredump",
		    "/nkn/nvsd/system/config/bypass",
		    "/nkn/nvsd/system/config/debug/command",
		    "/nkn/ssld/config/network/threads",
		    "/pm/nokeena/nkncnt/config/upload/url");

    bail_error(err);

    err = cli_revmap_hide_bindings("/pm/process/nvsd/environment/set/LD_LIBRARY_PATH/value");
    bail_error(err);
    err = cli_revmap_hide_bindings("/pm/process/ssld/auto_launch");
    bail_error(err);
    err = cli_revmap_hide_bindings("/pm/process/crawler/auto_launch");
    bail_error(err);
    err = cli_revmap_hide_bindings("/pm/process/ftpd/auto_launch");
    bail_error(err);
    err = cli_revmap_hide_bindings("/pm/process/agentd/auto_launch");
    bail_error(err);
    err = cli_revmap_hide_bindings("/pm/process/agentd/launch_params/1/param");
    bail_error(err);
    err = cli_revmap_hide_bindings("/pm/process/ssh_tunnel/auto_launch");
    bail_error(err);
    err = cli_revmap_hide_bindings("/pm/process/jcfg-server/auto_launch");
    bail_error(err);


bail:
    return err;
}

int
cli_nkn_ssp_show_one_cust(
        const bn_binding_array *bindings,
        uint32 idx, const bn_binding *binding,
        const tstring *name, const tstr_array *name_components,
        const tstring *name_last_part,
        const tstring *value,
        void *callback_data)
{
    int err = 0;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(callback_data);

    err = cli_printf("  %s\n", ts_str(value));
    bail_error(err);


bail:
    return err;
}


int
cli_nkn_list_ftpi_files(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions)
{
    int err = 0;
    const char *home_dir = NULL, *name = NULL;
    int num_names = 0, i = 0;
    tstr_array *names = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    // Get home dir
    home_dir = lc_getenv("HOME");
    bail_null(home_dir);

    if (strlen(home_dir) == 0) {
        goto bail;
    }

    err = cli_file_get_filenames(home_dir, &names);
    bail_error_null(err, names);

    num_names = tstr_array_length_quick(names);
    for(i = 0; i < num_names; i++) {
        name = tstr_array_get_str_quick(names, i);
        bail_null(name);
        if (name[0] == '.')
            continue;
        if(lc_is_prefix(ts_str(curr_word), name, false)) {
            err = tstr_array_append_str(ret_completions, name);
            bail_error(err);
        }
    }


bail:
    tstr_array_free(&names);
    return 0;
}


int
cli_reg_nkn_stats_cmds(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;
    void *cli_nvsd_ps_ptr = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = cli_get_symbol("cli_nvsd_namespace_cmds", "cli_ns_name_completion",
            &cli_nvsd_ps_ptr);
    cli_ns_completion = cli_nvsd_ps_ptr;
    bail_error_null(err, cli_ns_completion);

    cli_nvsd_ps_ptr = NULL;
    err = cli_get_symbol("cli_nvsd_namespace_cmds", "cli_ns_name_help",
            &cli_nvsd_ps_ptr);
    cli_ns_help = cli_nvsd_ps_ptr;
    bail_error_null(err, cli_ns_help);

    CLI_CMD_NEW;
    cmd->cc_words_str =     "show counters";
    cmd->cc_help_descr =    N_("Display counters");
    cmd->cc_flags =         ccf_terminal;
    cmd->cc_capab_required = ccp_query_basic_curr;
    cmd->cc_exec_callback = cli_nkn_show_counters_cb;
    cmd->cc_magic =	   cid_full_counter_display;
    cmd->cc_exec_data =     NULL;
    CLI_CMD_REGISTER;
#if 1
    /* Commented for bug# 4717 */
    /* Commented for bug# 4997 */
    CLI_CMD_NEW;
    cmd->cc_words_str =     "show counters http";
    cmd->cc_help_descr =    N_("Display HTTP counters");
    cmd->cc_flags =         ccf_terminal;
    cmd->cc_capab_required = ccp_query_basic_curr;
    cmd->cc_exec_callback = cli_nkn_show_http_counters_cb;
    cmd->cc_magic =	    cid_http_counter_display;
    cmd->cc_exec_data =     NULL;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =     "show counters rtsp";
    cmd->cc_help_descr =    N_("Display RTSP counters");
    cmd->cc_flags =         ccf_terminal;
    cmd->cc_capab_required = ccp_query_basic_curr;
    cmd->cc_exec_callback = cli_nkn_show_rtsp_counters_cb;
    cmd->cc_magic =	    cid_rtsp_counter_display;
    cmd->cc_exec_data =     NULL;
    CLI_CMD_REGISTER;

#endif

    CLI_CMD_NEW;
    cmd->cc_words_str 		=	"show counters internal";
    cmd->cc_help_descr 		=	N_("Show all nvsd counters");
    cmd->cc_flags 		=	ccf_terminal | ccf_hidden;
    cmd->cc_capab_required 	= 	ccp_query_basic_curr;
    cmd->cc_exec_callback 	= 	cli_nkn_show_counters_internal_cb;
    cmd->cc_exec_data 		=     	NULL;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=     	"show counters internal *";
    cmd->cc_help_exp 		=      	N_("<name>");
    cmd->cc_help_exp_hint 	= 	N_("Counter pattern name");
    cmd->cc_help_options 	=	cli_set_default;
    cmd->cc_help_num_options 	=       sizeof(cli_set_default) /
						sizeof(cli_expansion_option);
    cmd->cc_comp_type 		=	cct_use_help_options;
    cmd->cc_flags 		=	ccf_terminal;
    cmd->cc_capab_required 	= 	ccp_query_basic_curr;
    cmd->cc_exec_callback 	= 	cli_nkn_show_counters_internal_cb;
    cmd->cc_exec_data 		=     	NULL;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str           =    "show counters internal smart";
    cmd->cc_help_descr          =    N_("Self-Monitoring, Analysis and Reporting Technology (SMART) counters for disks");
    cmd->cc_flags               =    ccf_terminal;
    cmd->cc_capab_required      =    ccp_query_basic_curr;
    cmd->cc_exec_callback       =    cli_smartd_show_counters_internal_cb;
    cmd->cc_exec_data           =    NULL;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str           =    "show counters internal smart *";
    cmd->cc_help_exp            =    N_("<counter-pattern>");
    cmd->cc_help_exp_hint       =    N_("Specify a pattern for the counter to be displayed(for ex., glob_)");
    cmd->cc_flags               =    ccf_terminal;
    cmd->cc_capab_required      =    ccp_query_basic_curr;
    cmd->cc_exec_callback       =    cli_smartd_show_counters_internal_cb;
    cmd->cc_exec_data           =    NULL;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=	"collect ";
    cmd->cc_help_descr 		=	N_("Collect counter data ");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=	"collect counters ";
    cmd->cc_help_descr 		=	N_("Collect counter data ");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=	"collect counters detail";
    cmd->cc_help_descr 		=	N_("Collect counter data ");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=     	"collect counters detail *";
    cmd->cc_help_exp 		=      	N_("<name>");
    cmd->cc_help_exp_hint 	= 	N_("Counter pattern name");
    cmd->cc_help_options 	=	cli_set_collect_default;
    cmd->cc_help_num_options 	=       sizeof(cli_set_collect_default) /
						sizeof(cli_expansion_option);
//    cmd->cc_flags 		=	ccf_var_args;
    cmd->cc_comp_type 		=	cct_use_help_options;
//    cmd->cc_capab_required 	= 	ccp_query_basic_curr;
//    cmd->cc_exec_callback 	= 	cli_nkn_show_counters_internal_cb;
    cmd->cc_exec_data 		=     	NULL;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str		=	"collect counters detail *  frequency";
    cmd->cc_help_descr		=	N_("configure time interval to loop");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=     	"collect counters detail  * frequency *";
    cmd->cc_help_exp 		=     	N_("<seconds>");
    cmd->cc_help_exp_hint 	= 	N_("time interval in seconds to loop");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str		=	"collect counters detail *  frequency * duration";
    cmd->cc_help_descr		=	N_("configure duration for looping the counters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=     	"collect counters detail *  frequency * duration *";
    cmd->cc_help_exp 		=     	N_("<seconds>");
    cmd->cc_help_exp_hint 	= 	N_("Duration for looping the counters");
    cmd->cc_flags 		=	ccf_terminal;
    cmd->cc_capab_required 	= 	ccp_query_basic_curr;
    cmd->cc_exec_callback 	= 	cli_nkn_collect_counters_detail;
    cmd->cc_exec_data 		=     	NULL;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=     	"show counters internal * save";
    cmd->cc_help_descr 		=     	N_("To save counters to a file");
    cmd->cc_flags 		=	ccf_terminal;
    cmd->cc_capab_required 	= 	ccp_query_basic_curr;
    cmd->cc_exec_callback 	= 	cli_nkn_show_counters_internal_cb;
    cmd->cc_exec_data 		=     	NULL;
    CLI_CMD_REGISTER;



    CLI_CMD_NEW;
    cmd->cc_words_str =     "show statistics";
    cmd->cc_help_descr =    N_("Show key Statistics");
    cmd->cc_flags =         ccf_terminal;
    cmd->cc_capab_required = ccp_query_basic_curr;
    cmd->cc_exec_callback = cli_nkn_show_stats_cb;
    cmd->cc_exec_data =     NULL;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =    "reset counters";
    cmd->cc_help_descr =   N_("Reset the Statistics counters");
    cmd->cc_flags =	   ccf_terminal;
    cmd->cc_capab_required = ccp_query_rstr_curr;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/stat/nkn/actions/reset-counter";
    cmd->cc_exec_data = NULL;
    CLI_CMD_REGISTER;

    /* Not registering the reset counters http | https | namespace
    commands. The functionality will be added later. Will remove this
    once the functionality is implemented */
    return err; 

    CLI_CMD_NEW;
    cmd->cc_words_str =    "reset counters http";
    cmd->cc_help_descr =   N_("Reset the HTTP related statistics");
    cmd->cc_flags =        ccf_terminal;
    cmd->cc_capab_required = ccp_query_rstr_curr;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/stat/nkn/actions/reset-http-counter";
    cmd->cc_exec_data = NULL;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =    "reset counters https";
    cmd->cc_help_descr =   N_("Reset the HTTPS related statistics");
    cmd->cc_flags =        ccf_terminal;
    cmd->cc_capab_required = ccp_query_rstr_curr;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/stat/nkn/actions/reset-https-counter";
    cmd->cc_exec_data = NULL;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =    "reset counters namespace";
    cmd->cc_help_descr =   N_("Reset statistics for all namespaces");
    cmd->cc_flags =        ccf_terminal;
    cmd->cc_capab_required = ccp_query_rstr_curr;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/stat/nkn/actions/reset-namespace-counter";
    cmd->cc_exec_data = NULL;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =    "reset counters namespace *";
    cmd->cc_help_descr =   N_("Reset the namespace related statistics");
    cmd->cc_flags =        ccf_terminal;
    cmd->cc_capab_required = ccp_query_rstr_curr;
    cmd->cc_help_callback =     cli_ns_help;
    cmd->cc_comp_callback =     cli_ns_completion;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/stat/nkn/actions/reset-namespace-counter";
    cmd->cc_exec_name =             "namespace_name";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_value =            "$1$";
    CLI_CMD_REGISTER;



#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =     "show thresholds";
    cmd->cc_help_descr =    N_("Show key thresholds");
    cmd->cc_flags =         ccf_terminal;
    cmd->cc_capab_required = ccp_query_basic_curr;
    cmd->cc_exec_callback = cli_nkn_show_thresholds_cb;
    cmd->cc_exec_data =     NULL;
    CLI_CMD_REGISTER;
#endif

bail:
    return err;
}

int
cli_nkn_show_http_counters_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    bn_binding *binding = NULL;
    uint64 cl_redir_err1=0, cl_redir_err2=0, cl_redir_errs=0;
    uint32 code = 0;
    tstr_array *port_ids = NULL;
    tstr_array *namespace_ids = NULL;
    uint32 total_http = 0, total_http_ipv6 = 0;

    uint64_t sas_tot_obj = 0;
    uint64_t sata_tot_obj = 0;
    uint64_t ssd_tot_obj = 0;
    uint64_t sas_ing_obj = 0, sas_del_obj = 0;
    uint64_t sata_ing_obj = 0, sata_del_obj = 0;
    uint64_t ssd_ing_obj = 0, ssd_del_obj = 0;

    if(cmd->cc_magic == cid_http_counter_display) {
	    err = cli_nkn_show_common_counters(data, cmd, cmd_line, params);
	    bail_error(err);
    }


    err = mdc_get_binding(cli_mcc, &code, NULL, false, &binding,
            "/stat/nkn/nvsd/num_conx_ipv6", NULL);
    bail_error(err);
    if (binding) {
        err = bn_binding_get_uint32(binding, ba_value, NULL, &total_http_ipv6);
        bn_binding_free(&binding);
        bail_error(err);
    }
    err = mdc_get_binding(cli_mcc, &code, NULL, false, &binding,
            "/stat/nkn/nvsd/num_connections", NULL);
    bail_error(err);

    if (binding) {
        err = bn_binding_get_uint32(binding, ba_value, NULL, &total_http);
        bail_error(err);
        bn_binding_free(&binding);
    }

    total_http +=total_http_ipv6;

    err = cli_printf(_("\n%-50s: %u\n"), "Total number of Active HTTP Connections", (unsigned int) total_http);
    bail_error(err);


    err = cli_printf_query(_("%-50s: " "#/stat/nkn/nvsd/current/num_http_req# \n"),
		    "Total number of HTTP Connections");
    bail_error(err);

    // TODO Collect statistics on port basis

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/nvsd/current/num_http_transaction# \n"),
		    "Total number of HTTP Transactions");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/response/current/200/count# \n"),
		    "Total number of HTTP 200 responses");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/response/current/206/count# \n"),
		    "Total number of HTTP 206 responses");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/response/current/302/count# \n"),
		    "Total number of HTTP 302 responses");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/response/current/304/count# \n"),
		    "Total number of HTTP 304 responses");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/response/current/400/count# \n"),
		    "Total number of HTTP 400 responses");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/response/current/404/count# \n"),
		    "Total number of HTTP 404 responses");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/response/current/416/count# \n"),
		    "Total number of HTTP 416 responses");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/response/current/500/count# \n"),
		    "Total number of HTTP 500 responses");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/response/current/501/count# \n"),
		    "Total number of HTTP 501 responses");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/response/current/503/count# \n"),
		    "Total number of HTTP 503 responses");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/response/current/504/count# \n"),
		    "Total number of HTTP 504 responses");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/response/current/timeouts# \n"),
		    "Total number of HTTP Timeouts");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/aborts/current/tot_well_finished# \n"),
		    "Total HTTP Well finished count");
    bail_error(err);

#if 0
    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/http/aborts/tot_well_finished" ,NULL);
    bail_error(err);

    if (binding ) {
        err = bn_binding_get_uint32(binding,ba_value,NULL,&val);
        bail_error(err);
        bn_binding_free(&binding);
    }

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/nvsd/num_http_transaction" ,NULL);
    bail_error(err);

    if (binding ) {
        err = bn_binding_get_uint64(binding,ba_value,NULL,&lval1);
        bail_error(err);
        bn_binding_free(&binding);
    }

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/nvsd/num_connections" ,NULL);
    bail_error(err);

    if (binding ) {
        err = bn_binding_get_uint32(binding,ba_value,NULL,&val2);
        bail_error(err);
        bn_binding_free(&binding);
    }

    err = cli_printf(_("HTTP Aborts	: %d\n"),val1-val-val2);
    bail_error(err);
#else
    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/current/cache_hit_count# \n"),
		    "Total Number of Cache-Hits");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/aborts/current/mm_stat_err# \n"),
		    "Total Number of Cache-Miss");
    bail_error(err);

    err = cli_printf(_("\n%-50s:\n"),"HTTP TUNNEL STATS");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/tunnel/current/total_connections# \n")
		    , "Total Connections");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/tunnel/active_connections# \n")
		    , "Total Active Connections");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/tunnel/total_http09_requests# \n")
		   , "Total Number of HTTP 0.9 requests");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/nvsd/current/tunnel_byte_count# \n")
		    , "Total Bytes Served");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/tunnel/current/error_count# \n")
		    , "Total Errors");
    bail_error(err);

    err = cli_printf(_("%-50s:\n"),"ERROR COUNTERS");
    bail_error(err);
#endif
#if 0
    err = cli_printf_query(_("    Number of Error timeouts with task    : "
                "#/stat/nkn/http/aborts/error_timeout_withtask# \n"));
    bail_error(err);
#endif

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/aborts/current/http_schd_err_get_data# \n"),
		    "    Number of Scheduler Errors on get data");
    bail_error(err);


    err = cli_printf_query(_("%-50s: " "#/stat/nkn/http/aborts/current/task_deadline_missed# \n"),
		    "    Number of HTTP deadline missed tasks");
    bail_error(err);

#if 0
    err = cli_printf_query(_("    Number of Cache Request Errors        : "
                "#/stat/nkn/http/aborts/cache_req_err# \n"));
    bail_error(err);
#endif

    err = cli_printf(_("%-50s:\n"),"PROXY ERRORS");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/proxy/current/om_err_conn_failed#\n"),
		    "    OM Error Connection Failed Count");
    bail_error(err);
//
//	Commented out as fix for Bug 3828
//    err = cli_printf_query(_("%-50s: " "#/stat/nkn/proxy/om_err_get# \n"),
//		    "    OM Get Error Count");
//    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/proxy/current/http_err_parse# \n"),
		    "    HTTP Parse Error Count");
    bail_error(err);

    err = cli_printf(_("%-50s:\n"),"Cluster L7 statistics");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/nvsd/current/tot_num_cl_redirect# \n"),
		    "    Total number of Redirects");
    bail_error(err);

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/nvsd/current/cl_redirect_int_err1" ,NULL);
    bail_error(err);

    if (binding ) {
        err = bn_binding_get_uint64(binding,ba_value,NULL,&cl_redir_err1);
        bail_error(err);
        bn_binding_free(&binding);
       }
    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/nvsd/current/cl_redirect_int_err2" ,NULL);
    bail_error(err);

    if (binding ) {
        err = bn_binding_get_uint64(binding,ba_value,NULL,&cl_redir_err2);
        bail_error(err);
        bn_binding_free(&binding);
       }

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/nvsd/current/cl_redirect_int_errs" ,NULL);
    bail_error(err);

    if (binding ) {
        err = bn_binding_get_uint64(binding,ba_value,NULL,&cl_redir_errs);
        bail_error(err);
        bn_binding_free(&binding);
       }

    err = cli_printf(_("    Total number of Redirect errors	          : %ld\n"),cl_redir_err1+cl_redir_err2+cl_redir_errs);
    bail_error(err);


    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/tier/sas/ingested_objects" ,NULL);
    bail_error(err);

    if (binding ) {
	    err = bn_binding_get_uint64(binding,ba_value,NULL,&sas_ing_obj);
	    bail_error(err);
	    bn_binding_free(&binding);
    }

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/tier/sata/ingested_objects" ,NULL);
    bail_error(err);

    if (binding ) {
	    err = bn_binding_get_uint64(binding,ba_value,NULL,&sata_ing_obj);
	    bail_error(err);
	    bn_binding_free(&binding);
    }

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/tier/ssd/ingested_objects" ,NULL);
    bail_error(err);

    if (binding ) {
	    err = bn_binding_get_uint64(binding,ba_value,NULL,&ssd_ing_obj);
	    bail_error(err);
	    bn_binding_free(&binding);
    }


    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/tier/sas/deleted_objects" ,NULL);
    bail_error(err);

    if (binding ) {
	    err = bn_binding_get_uint64(binding,ba_value,NULL,&sas_del_obj);
	    bail_error(err);
	    bn_binding_free(&binding);
    }

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/tier/sata/deleted_objects" ,NULL);
    bail_error(err);

    if (binding ) {
	    err = bn_binding_get_uint64(binding,ba_value,NULL,&sata_del_obj);
	    bail_error(err);
	    bn_binding_free(&binding);
    }

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/tier/ssd/deleted_objects" ,NULL);
    bail_error(err);

    if (binding ) {
	    err = bn_binding_get_uint64(binding,ba_value,NULL,&ssd_del_obj);
	    bail_error(err);
	    bn_binding_free(&binding);
    }

    sas_tot_obj = sas_ing_obj - sas_del_obj;
    sata_tot_obj = sata_ing_obj - sata_del_obj;
    ssd_tot_obj = ssd_ing_obj - ssd_del_obj;

    err = cli_printf(_("%-50s:\n"),"DISK CACHE COUNTERS");
    bail_error(err);

    err = cli_printf(_("%-50s: %lu\n"), "    Total Objects", sata_tot_obj + sas_tot_obj + ssd_tot_obj);
    bail_error(err);

    err = cli_printf(_("%-50s: %lu\n"), "    Total Objects cached on SAS", sas_tot_obj);
    bail_error(err);

    err = cli_printf(_("%-50s: %lu\n"), "    Total Objects cached on SSD", ssd_tot_obj);
    bail_error(err);

    err = cli_printf(_("%-50s: %lu\n"), "    Total Objects cached on SATA", sata_tot_obj);
    bail_error(err);

bail:
    tstr_array_free(&port_ids);
    tstr_array_free(&namespace_ids);
    bn_binding_free(&binding);
    return err;
}
#if 1
/* Commented for Bug# 4717 */
/* Commented for Bug# 4997 */
int
cli_nkn_show_rtsp_counters_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    bn_binding *binding = NULL;
    uint32 code = 0;
    tstr_array *port_ids = NULL;
    tstr_array *namespace_ids = NULL;
    uint64 total_rtsp = 0;
    uint32 rtsp200resp = 0;
    uint32 rtsp400resp = 0;
    uint32 rtsp404resp = 0;
    uint32 rtsp500resp = 0;
    uint32 rtsp501resp = 0;

    if(cmd->cc_magic == cid_rtsp_counter_display) {
	    err = cli_nkn_show_common_counters(data, cmd, cmd_line, params);
	    bail_error(err);
    }

    err = mdc_get_binding(cli_mcc, &code, NULL, false, &binding,
            "/stat/nkn/nvsd/num_rtsp_connections", NULL);
    bail_error(err);
    if (binding) {
        err = bn_binding_get_uint64(binding, ba_value, NULL, &total_rtsp);
        bail_error(err);
        bn_binding_free(&binding);
    }


    err = cli_printf(_("\n%-50s: %llu\n"), "Total number of Active RTSP Connections", (long long unsigned int) total_rtsp);
    bail_error(err);

    err = mdc_get_binding(cli_mcc, &code, NULL, false, &binding,
		    "/stat/nkn/rtsp/response/current/200/count",
		    NULL);
    bail_error(err);
    if(binding) {
	    err = bn_binding_get_uint32(binding, ba_value, NULL, &rtsp200resp);
	    bail_error(err);
	    bn_binding_free(&binding);
    }

    err = mdc_get_binding(cli_mcc, &code, NULL, false, &binding,
		    "/stat/nkn/rtsp/response/current/200/count",
		    NULL);
    bail_error(err);
    if(binding) {
	    err = bn_binding_get_uint32(binding, ba_value, NULL, &rtsp200resp);
	    bail_error(err);
	    bn_binding_free(&binding);
    }


    err = mdc_get_binding(cli_mcc, &code, NULL, false, &binding,
		    "/stat/nkn/rtsp/response/current/400/count",
		    NULL);
    bail_error(err);
    if(binding) {
	    err = bn_binding_get_uint32(binding, ba_value, NULL, &rtsp400resp);
	    bail_error(err);
	    bn_binding_free(&binding);
    }

    err = mdc_get_binding(cli_mcc, &code, NULL, false, &binding,
		    "/stat/nkn/rtsp/response/current/404/count",
		    NULL);
    bail_error(err);
    if(binding) {
	    err = bn_binding_get_uint32(binding, ba_value, NULL, &rtsp404resp);
	    bail_error(err);
	    bn_binding_free(&binding);
    }

    err = mdc_get_binding(cli_mcc, &code, NULL, false, &binding,
		    "/stat/nkn/rtsp/response/current/500/count",
		    NULL);
    bail_error(err);
    if(binding) {
	    err = bn_binding_get_uint32(binding, ba_value, NULL, &rtsp500resp);
	    bail_error(err);
	    bn_binding_free(&binding);
    }

    err = mdc_get_binding(cli_mcc, &code, NULL, false, &binding,
		    "/stat/nkn/rtsp/response/current/501/count",
		    NULL);
    bail_error(err);
    if(binding) {
	    err = bn_binding_get_uint32(binding, ba_value, NULL, &rtsp501resp);
	    bail_error(err);
	    bn_binding_free(&binding);
    }

    err = cli_printf_query(_("%-50s: " "%u\n"),
		    "Total number of RTSP Transactions",
		    rtsp200resp + rtsp400resp + rtsp404resp + rtsp500resp + rtsp501resp);
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/nvsd/current/num_rtsp_udp_pkt_fwd#" " \n"),
		    "Total Number of RTP Packets Forwarded");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "%u\n"),
		    "Total number of RTSP 200 responses", rtsp200resp);
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "%u\n"),
		    "Total number of RTSP 400 responses", rtsp400resp);
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "%u\n"),
		    "Total number of RTSP 404 responses", rtsp404resp);
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "%u\n"),
		    "Total number of RTSP 500 responses", rtsp500resp);
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "%u\n\n"),
		    "Total number of RTSP 501 responses", rtsp501resp);
    bail_error(err);


    err = cli_printf(_("%-50s \n"),"RTCP Statistics ");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/nvsd/current/glob_rtcp_tot_udp_packets_fwd#" "  \n"),
		    "Total Number of RTCP Packets Forwarded");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/nvsd/current/glob_rtcp_tot_udp_sr_packets_sent#" " \n"),
		    "Total Number of Sender Report sent");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/nvsd/current/glob_rtcp_tot_udp_rr_packets_sent#" " \n"),
		    "Total Number of Receiver Report sent");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/nvsd/current/glob_rtcp_tot_udp_packets_recv#" "  \n"),
		    "Total Number of RTCP Packets Received");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/nvsd/current/glob_tot_rtcp_sr#" " \n"),
		    "Total Number of Sender Report Received");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/nvsd/current/glob_tot_rtcp_rr#" " \n\n"),
		    "Total Number of Receiver Report Received");
    bail_error(err);

    err = cli_printf(_("%-50s \n"),"PROXY ERRORS");
    bail_error(err);
#if 0
    err = cli_printf_query(_("%-50s: " "#/stat/nkn/proxy/current/om_err_conn_failed#" " \n"),
		    "    OM Error Connection Failed Count");
    bail_error(err);
#endif
    err = cli_printf_query(_("%-50s: " "#/stat/nkn/rtsp/current/parse_error_count#" " \n\n"),
		    "    RTSP Parse Error Count");
    bail_error(err);

bail:
    tstr_array_free(&port_ids);
    tstr_array_free(&namespace_ids);
    bn_binding_free(&binding);
    return err;
}
#endif

int
cli_nkn_show_stats_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    bn_binding *binding = NULL;
    uint64 val;
    const char *port_id = NULL;
    tstr_array *port_ids = NULL;
    uint32 num_samples = 0 , i = 0 , code = 0;
    tstring *interface_id = NULL;
    double dDivisor = 1000000.00;
    double dMBValue =0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);
    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
		             "/stats/state/chd/total_byte_rate/node/\\/stat\\/nkn\\/nvsd\\/total_byte_rate/last/value" ,NULL);
    bail_error(err);

    if (binding ) {
	err = bn_binding_get_uint64(binding,ba_value,NULL,&val);
	bail_error(err);
	bn_binding_free(&binding);
    }

    dMBValue = val/dDivisor;

    err = cli_printf(_("%-50s: %-25.4lf\n"), "Current Bandwidth (MB/Sec)", dMBValue);
    bail_error(err);

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
		             "/stats/state/chd/cache_byte_rate/node/\\/stat\\/nkn\\/nvsd\\/cache_byte_rate/last/value" ,NULL);
    bail_error(err);

    if (binding ) {
	err = bn_binding_get_uint64(binding,ba_value,NULL,&val);
	bail_error(err);
	bn_binding_free(&binding);
    }

    dMBValue = val/dDivisor;

    err = cli_printf_query(_("%-50s: %-25.4lf\n"), "Current Cache Bandwidth (MB/Sec)", dMBValue);
    bail_error(err);

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
		             "/stats/state/chd/disk_byte_rate/node/\\/stat\\/nkn\\/nvsd\\/disk_byte_rate/last/value" ,NULL);
    bail_error(err);

    if (binding ) {
	err = bn_binding_get_uint64(binding,ba_value,NULL,&val);
	bail_error(err);
	bn_binding_free(&binding);
    }

    dMBValue = val/dDivisor;

    err = cli_printf_query(_("%-50s: %-25.4lf\n"),"Current Disk Bandwidth (MB/Sec)", dMBValue);
    bail_error(err);

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
		             "/stats/state/chd/origin_byte_rate/node/\\/stat\\/nkn\\/nvsd\\/origin_byte_rate/last/value" ,NULL);
    bail_error(err);

    if (binding ) {
	err = bn_binding_get_uint64(binding,ba_value,NULL,&val);
	bail_error(err);
	bn_binding_free(&binding);
    }

    dMBValue = val/dDivisor;

    err = cli_printf_query(_("%-50s: %-25.4lf\n"),"Current Origin Bandwidth (MB/Sec)",dMBValue);
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stats/state/chd/connection_rate/node/\\/stat\\/nkn\\/nvsd\\/connection_rate/last/value# \n"),
			    "Avg Number of Connections Per Sec");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stats/state/chd/http_transaction_rate/node/\\/stat\\/nkn\\/nvsd\\/http_transaction_rate/last/value# \n"),
	    		    "Avg HTTP Transactions per sec");
    bail_error(err);

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
		             "/stat/nkn/proxy/cur_proxy_rate" ,NULL);
    bail_error(err);

    if (binding ) {
	err = bn_binding_get_uint64(binding,ba_value,NULL,&val);
	bail_error(err);
	bn_binding_free(&binding);
    }

    dMBValue = val/dDivisor;

    err = cli_printf_query(_("%-50s: %-25.4lf\n"),"Current Proxy Rate in this sec (MB/Sec)", dMBValue);
    bail_error(err);

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
		             "/stats/state/chd/perdiskbyte_rate/node/\\/stat\\/nkn\\/nvsd\\/perdiskbyte_rate/last/value" ,NULL);
    bail_error(err);

    if (binding ) {
	err = bn_binding_get_uint64(binding,ba_value,NULL,&val);
	bail_error(err);
	bn_binding_free(&binding);
    }

    dMBValue = val/dDivisor;

    err = cli_printf_query(_("%-50s: %-25.4lf\n"),"Per Disk Bandwidth (MB/Sec)", dMBValue);
    bail_error(err);

    err = cli_printf(_("%-50s:\n"),"Per port statistics");
    bail_error(err);

    err = mdc_get_binding_children_tstr_array(cli_mcc,NULL,NULL,&port_ids,"/stats/state/chd/perportbyte_rate/node",NULL);
    bail_error(err);

    num_samples = tstr_array_length_quick(port_ids);
    for ( i = 0 ; i < num_samples; i++) {
	    val = 0;
            port_id = tstr_array_get_str_quick(port_ids,i);
            bail_null(port_id);
            err = bn_binding_name_to_parts_va(port_id,false,1,3,&interface_id);

            err = mdc_get_binding_fmt(cli_mcc,NULL,NULL,false,&binding,
	        	NULL,"/stats/state/chd/perportbyte_rate/node/\\/stat\\/nkn\\/nvsd\\/%s\\/perportbyte_rate/last/value",ts_str(interface_id));
            bail_error(err);

	    if(binding) {
            	err = bn_binding_get_uint64(binding,ba_value,NULL,&val);
            	bail_error(err);
		err = cli_printf(_("%-30s%-20s: %lu\n"),"    Tx Bytes Per Sec On",ts_str(interface_id),val);
                bail_error(err);
	    }

    }

bail:
    bn_binding_free(&binding);
    tstr_array_free(&port_ids);
    return err;
}


int
cli_nkn_show_thresholds_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_printf_query(_("Bandwidth Usage Error threshold      : "
		"#/stats/config/alarm/total_byte_rate/rising/error_threshold# Bytes\n"));
    bail_error(err);

    err = cli_printf_query(_("Bandwidth Usage Clear threshold      : "
		"#/stats/config/alarm/total_byte_rate/rising/clear_threshold# Bytes\n"));
    bail_error(err);

    err = cli_printf_query(_("Current Bandwidth Usage    	   : "
                "#/stats/state/chd/total_byte_rate/node/\\/stat\\/nkn\\/nvsd\\/total_byte_rate/last/value# \n"));
    bail_error(err);


    err = cli_printf_query(_("Connection Rate Error threshold      : "
		"#/stats/config/alarm/connection_rate/rising/error_threshold# \n"));
    bail_error(err);

    err = cli_printf_query(_("Connection Rate Clear threshold      : "
		"#/stats/config/alarm/connection_rate/rising/clear_threshold# \n"));
    bail_error(err);

    err = cli_printf_query(_("Current Connection Rate		   : "
                "#/stats/state/chd/connection_rate/node/\\/stat\\/nkn\\/nvsd\\/connection_rate/last/value# \n"));
    bail_error(err);

    err = cli_printf_query(_("Cache Byte Rate Error threshold      : "
		"#/stats/config/alarm/avg_cache_byte_rate/rising/error_threshold# Bytes\n"));
    bail_error(err);

    err = cli_printf_query(_("Cache Byte Rate Clear threshold      : "
		"#/stats/config/alarm/avg_cache_byte_rate/rising/clear_threshold# Bytes\n"));
    bail_error(err);

    err = cli_printf_query(_("Current Cache Byte Rate		   : "
                "#/stats/state/chd/avg_cache_byte_rate/node/\\/stat\\/nkn\\/nvsd\\/avg_cache_byte_rate/last/value# \n"));
    bail_error(err);


    err = cli_printf_query(_("Origin Byte Rate Error threshold     : "
		"#/stats/config/alarm/avg_origin_byte_rate/rising/error_threshold# Bytes\n"));
    bail_error(err);

    err = cli_printf_query(_("Origin Byte Rate Clear threshold     : "
		"#/stats/config/alarm/avg_origin_byte_rate/rising/clear_threshold# Bytes\n"));
    bail_error(err);

    err = cli_printf_query(_("Current Origin Byte Rate		   : "
                "#/stats/state/chd/avg_origin_byte_rate/node/\\/stat\\/nkn\\/nvsd\\/avg_origin_byte_rate/last/value# \n"));
    bail_error(err);

    err = cli_printf_query(_("Disk Byte Rate Error threshold       : "
		"#/stats/config/alarm/avg_disk_byte_rate/rising/error_threshold# Bytes\n"));
    bail_error(err);

    err = cli_printf_query(_("Disk Byte Rate Clear threshold       : "
		"#/stats/config/alarm/avg_disk_byte_rate/rising/clear_threshold# Bytes\n"));
    bail_error(err);

    err = cli_printf_query(_("Current Disk Byte Rate		   : "
                "#/stats/state/chd/avg_disk_byte_rate/node/\\/stat\\/nkn\\/nvsd\\/avg_disk_byte_rate/last/value# \n"));
    bail_error(err);


    err = cli_printf_query(_("HTTP transaction Rate Error threshold: "
		"#/stats/config/alarm/http_transaction_rate/rising/error_threshold# \n"));
    bail_error(err);

    err = cli_printf_query(_("HTTP transaction Rate Clear threshold: "
		"#/stats/config/alarm/http_transaction_rate/rising/clear_threshold# \n"));
    bail_error(err);

    err = cli_printf_query(_("HTTP Transaction Rate	   : "
                "#/stats/state/chd/http_transaction_rate/node/\\/stat\\/nkn\\/nvsd\\/http_transaction_rate/last/value# \n"));
    bail_error(err);


bail:
    return err;
}


static int
cli_reg_nkn_mm_cmds(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-mgr";
    cmd->cc_help_descr =        N_("configure Media Manager options");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-mgr provider";
    cmd->cc_help_descr =        N_("Configure MM provider options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-mgr provider threshold";
    cmd->cc_help_descr =        N_("Configure MM provide threshold options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-mgr provider threshold high";
    cmd->cc_help_descr =        N_("Configure High threshold");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-mgr provider threshold high *";
    cmd->cc_help_exp =          N_("<number>");
    cmd->cc_help_exp_hint =     N_("Number between 0 and 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/mm/config/provider/threshold/high";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-mgr provider threshold low";
    cmd->cc_help_descr =        N_("Configure Low threshold");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-mgr provider threshold low *";
    cmd->cc_help_exp =          N_("<number>");
    cmd->cc_help_exp_hint =     N_("Number between 0 and 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/mm/config/provider/threshold/low";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_mgmt;
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-mgr evict-time";
    cmd->cc_help_descr =        N_("Configure Eviction thread time");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-mgr evict-time *";
    cmd->cc_help_exp =          N_("<time>");
    cmd->cc_help_exp_hint =     N_("Time between 60 (second) and 86400 (second)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/mm/config/evict_thread_time";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_order =      cro_mgmt;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;
#endif
    CLI_CMD_NEW;
    cmd->cc_words_str =         "show media-mgr";
    cmd->cc_help_descr =        N_("Display Media Manager configuration");
    cmd->cc_flags =             ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_nkn_mm_show_cmd;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings_va(1, 
		    "/nkn/nvsd/mm/config/evict_thread_time");
    bail_error(err);

bail:
    return err;
}

int
cli_nkn_mm_show_cmd(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    cli_printf_query(_("Provider Threshold\n"));
    cli_printf_query(_("             High : "
                "#/nkn/nvsd/mm/config/provider/threshold/high#\n"));
    cli_printf_query(_("             Low  : "
                "#/nkn/nvsd/mm/config/provider/threshold/low#\n"));
#if 0
    cli_printf_query(_("Thread Evict Time : "
                "#/nkn/nvsd/mm/config/evict_thread_time# (seconds) \n"));
#endif

    return err;
}



static cli_execution_callback cli_config_show = NULL;
#define csc_normal      0x001  /* Show all commands except any defaults      */
#define csc_full        0x002  /* Show all commands except hidden defaults   */
#define csc_all         0x003  /* Show all commands                          */
#define csc_exclude_mask       0x0000000F

#define csc_saved       0x010  /* Use active saved database                  */
#define csc_running     0x020  /* Use current running database               */
#define csc_initial     0x030  /* Use initial database                       */
#define csc_other       0x040  /* Use other saved database                   */
#define csc_db_mask            0x000000F0

#define csc_root        0x100  /* Use all nodes in the database              */
#define csc_subtree     0x200  /* Only use nodes under specified root        */
#define csc_nodes_mask         0x00000F00


static int cli_reg_nkn_tech_supp_cmds(const lc_dso_info *info, const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;
    void *callback = NULL;

    UNUSED_ARGUMENT(context);

    err = lc_dso_module_get_symbol(info->ldi_context,
            "cli_config_cmds", "cli_config_show",
            &callback);
    bail_error_null(err, callback);
    cli_config_show = (cli_execution_callback)(callback);

    CLI_CMD_NEW;
    cmd->cc_words_str =         "tech-support";
    cmd->cc_help_descr =        N_("Generate a system report for Juniper Tech Support use.");
    //cmd->cc_flags =             ccf_terminal;
    //cmd->cc_magic =             csc_normal | csc_running | csc_root;
    //cmd->cc_capab_required =    ccp_query_rstr_curr;
    //cmd->cc_exec_callback =     cli_config_show;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "tech-support *";
    cmd->cc_flags =             ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =    ccp_set_basic_curr;
    cmd->cc_help_exp =          cli_msg_url;
    cmd->cc_exec_callback =     cli_tech_support;
    cmd->cc_magic =             csc_normal | csc_running | csc_root;
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str	    =	"tech-support * backtrace";
    cmd->cc_flags	    =	ccf_terminal | ccf_hidden ;
    cmd->cc_capab_required  =	ccp_set_basic_curr;
    cmd->cc_help_descr	    =   N_("Uploads all available backtraces to remote server");
    cmd->cc_exec_callback   =	cli_file_upload_action;
    cmd->cc_exec_data	    =	(void *)"/nkn/debug/action/upload_bt";
    cmd->cc_magic =             csc_normal | csc_running | csc_root;
    CLI_CMD_REGISTER;
#endif
    CLI_CMD_NEW;
    cmd->cc_words_str =         "tech-support * disk-cache";
    cmd->cc_flags =             ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =    ccp_set_basic_curr;
    cmd->cc_help_descr =        N_("Dump Disk-Cache information");
    cmd->cc_exec_callback =     cli_tech_supp_disk;
    cmd->cc_magic =             csc_normal | csc_running | csc_root;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "tech-support * am";
    cmd->cc_flags =             ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =    ccp_set_basic_curr;
    cmd->cc_help_descr =        N_("Dump AM log");
    cmd->cc_exec_callback =     cli_tech_supp_am;
    cmd->cc_magic =             csc_normal | csc_running | csc_root;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "tech-support fix-fallback";
    cmd->cc_flags =             ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =    ccp_set_basic_curr;
    cmd->cc_help_descr =        N_("Fix the configuration file for fallback");
    cmd->cc_exec_callback =     cli_tech_supp_fix_fallback;
    CLI_CMD_REGISTER;

bail:
    return err;
}


/* ------------------------------------------------------------------------- */
// Shamelessly copied from cli_config_cmds.c
static int
cli_config_upload_finish(const char *password, clish_password_result result,
                         const cli_command *cmd, const tstr_array *cmd_line,
                         const tstr_array *params, void *data,
                         tbool *ret_continue)
{
    int err = 0;
    const char *remote_url = NULL;
    bn_binding_array *bindings = NULL;
    tstring *dump_path = NULL, *dump_name = NULL, *dump_dir = NULL;
    uint32 code = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(ret_continue);
    /*
     * We have nothing to clean up, so we really only care about this
     * if we got a successful password entry.
     */
    if (result != cpr_ok) {
        goto bail;
    }



    err = mdc_send_action_with_bindings_and_results(cli_mcc,
            &code, NULL, "/debug/generate/dump", NULL, &bindings);
    bail_error(err);

    if (bindings) {
        err = bn_binding_array_get_value_tstr_by_name(bindings,
                "dump_path", NULL, &dump_path);
        bail_error(err);
    }

    if (dump_path) {
        err = lf_path_last(ts_str(dump_path), &dump_name);
        bail_error_null(err, dump_name);

        err = lf_path_parent(ts_str(dump_path), &dump_dir);
        bail_error(err);

        remote_url = tstr_array_get_str_quick(params, 0);
        bail_null(remote_url);

        if (password) {
            err = mdc_send_action_with_bindings_str_va(cli_mcc,
                    NULL, NULL,
                    "/file/transfer/upload", 3,
                    "local_path", bt_string, ts_str(dump_path),
                    //"local_dir", bt_string, ts_str(dump_dir),
                    //"local_filename", bt_string, ts_str(dump_name),
                    "remote_url", bt_string, remote_url,
                    "password", bt_string, password);
            bail_error(err);
        }
        else {
            err = mdc_send_action_with_bindings_str_va(cli_mcc,
                    NULL, NULL,
                    "/file/transfer/upload", 3,
                    "local_dir", bt_string, ts_str(dump_dir),
                    "local_filename", bt_string, ts_str(dump_name),
                    "remote_url", bt_string, remote_url);
            bail_error(err);
        }
    }
    else if (code == 0) {

    }
    else {
        lc_log_basic(LOG_WARNING, _("System dump generation returned code %d"), code);
    }



 bail:
    bn_binding_array_free(&bindings);
    ts_free(&dump_path);
    ts_free(&dump_name);
    ts_free(&dump_dir);
    return(err);
}



int
cli_tech_support(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *remote_url = NULL;

    UNUSED_ARGUMENT(data);

    remote_url = tstr_array_get_str_quick(params, 0);
    bail_null(remote_url);

    if (clish_is_shell()) {
        err = clish_maybe_prompt_for_password_url
                    (remote_url, cli_config_upload_finish,
                     cmd, cmd_line, params, NULL);
        bail_error(err);
    }
    else {
        err = cli_config_upload_finish(NULL, cpr_ok, cmd, cmd_line, params,
                NULL, NULL);
        bail_error(err);
    }


bail:
    return err;
}



static int
cli_tech_supp_disk_container_upload_finish(const char *password, clish_password_result result,
                                           const cli_command *cmd, const tstr_array *cmd_line,
                                           const tstr_array *params, void *data,
                                           tbool *ret_continue)
{
    int err = 0;
    const char *remote_url = NULL;
    bn_binding_array *bindings = NULL;
    tstring *dump_path = NULL, *dump_name = NULL, *dump_dir = NULL;
    uint32 code = 0;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(ret_continue);

    /*
     * We have nothing to clean up, so we really only care about this
     * if we got a successful password entry.
     */
    if (result != cpr_ok) {
        goto bail;
    }

    err = mdc_send_action_with_bindings_and_results(cli_mcc,
            &code,
            NULL,
            "/nkn/debug/generate/dump",
            NULL,
            &bindings);
    bail_error(err);

    if(bindings) {
        err = bn_binding_array_get_value_tstr_by_name(
                bindings, "dump_path", NULL, &dump_path);
        bail_error(err);
    }

    if (dump_path) {
        err = lf_path_last(ts_str(dump_path), &dump_name);
        bail_error_null(err, dump_name);

        err = lf_path_parent(ts_str(dump_path), &dump_dir);
        bail_error(err);

        remote_url = tstr_array_get_str_quick(params, 0);
        bail_null(remote_url);

        if (password) {
            err = mdc_send_action_with_bindings_str_va(cli_mcc,
                    NULL, NULL,
                    "/file/transfer/upload", 3,
                    "local_path", bt_string, ts_str(dump_path),
                    //"local_dir", bt_string, ts_str(dump_dir),
                    //"local_filename", bt_string, ts_str(dump_name),
                    "remote_url", bt_string, remote_url,
                    "password", bt_string, password);
            bail_error(err);
        }
        else {
            err = mdc_send_action_with_bindings_str_va(cli_mcc,
                    NULL, NULL,
                    "/file/transfer/upload", 3,
                    "local_dir", bt_string, ts_str(dump_dir),
                    "local_filename", bt_string, ts_str(dump_name),
                    "remote_url", bt_string, remote_url);
            bail_error(err);
        }
    }
    else if (code == 0) {

    }
    else {
        lc_log_basic(LOG_WARNING, _("System dump generation returned code %d"), code);
    }



bail:
    bn_binding_array_free(&bindings);
    ts_free(&dump_path);
    ts_free(&dump_name);
    ts_free(&dump_dir);
    return err;
}

int
cli_tech_supp_disk(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{

    int err = 0;
    const char *remote_url = NULL;

    UNUSED_ARGUMENT(data);

    remote_url = tstr_array_get_str_quick(params, 0);
    bail_null(remote_url);

    if (clish_is_shell()) {
        err = clish_maybe_prompt_for_password_url
                    (remote_url, cli_tech_supp_disk_container_upload_finish,
                     cmd, cmd_line, params, NULL);
        bail_error(err);
    }
    else {
        err = cli_tech_supp_disk_container_upload_finish(NULL, cpr_ok, cmd, cmd_line, params,
                NULL, NULL);
        bail_error(err);
    }


bail:
    return err;
}

static int
cli_show_mem_stats(void *arg, const cli_command *cmd,
            const tstr_array *cmd_line, const tstr_array *params)
{
    int err = 0;
    uint32 i = 0, j = 0;
    const char *proc_names[] = {"nvsd", "nknlogd", "mgmtd", "statsd", 
        "nkndashboard", "nknoomgr", "nknvpemgr", "httpd", "nkncache_evictd"};
    bn_binding *bn_last = NULL;
    uint32 last_id = 0;
    char *p = NULL;
    tstring *peak = NULL, *data = NULL, *resident = NULL, *lock = NULL;

    UNUSED_ARGUMENT(arg);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    
    p = smprintf("/stats/state/sample/proc_mem/node/\\/stat\\/nkn\\/vmem\\/statsd\\/peak" );
    bail_null(p);
    
    err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &bn_last, NULL, 
            "%s/last/instance_id", p);
    bail_error(err);

    if (bn_last == NULL) {
        cli_printf_error(_("no data to show"));
        goto bail;
    }
    err = bn_binding_get_uint32(bn_last, ba_value, 0, &last_id);
    bail_error(err);

    j = (last_id <= 10) ? 0 : (last_id - 10);

    cli_printf(_("%-20s  %-15s %-10s %-10s %-10s %-10s\n\n"),
                "TIME",
                "Process",
                "VmPeak",
                "VmData",
                "VmRSS",
                "VmLock");

    for(; j < last_id; ++j) {
        for(i = 0; i < sizeof(proc_names)/sizeof(const char *); ++i) {
            safe_free(p);
            p = smprintf("/stats/state/sample/proc_mem/node/\\/stat\\/nkn\\/vmem\\/%s\\", proc_names[i]);

            err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &peak, NULL, "%s/peak/instance/%d/value", p, j);
            bail_error(err);

            err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &data, NULL, "%s/data/instance/%d/value", p, j);
            bail_error(err);

            err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &resident, NULL, "%s/resident/instance/%d/value", p, j);
            bail_error(err);

            err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &lock, NULL, "%s/lock/instance/%d/value", p, j);
            bail_error(err);

            cli_printf_query(_(
                        "#%s/data/instance/%d/time# \t"
                        "%-15s %-10s %-10s %-10s %-10s\n"),
                        p, j,
                        proc_names[i],
                        ts_str(peak),
                        ts_str(data),
                        ts_str(resident),
                        ts_str(lock));
            ts_free(&peak);
            ts_free(&data);
            ts_free(&resident);
            ts_free(&lock);

        }

        cli_printf(_("\n"));

    }


bail:
    safe_free(p);
    ts_free(&peak);
    ts_free(&data);
    ts_free(&resident);
    ts_free(&lock);
    bn_binding_free(&bn_last);
    return err;
}

static int
cli_service_show_vmem(void *data, const cli_command *cmd,
            const tstr_array *cmd_line, const tstr_array *params)
{
    int err = 0;
    char *name = NULL;
    uint32 i = 0;
    char *p = NULL, *d = NULL, *r = NULL, *l = NULL;
    bn_binding *bn_last = NULL;
    uint32 last_id = 0;
    tbool found = false;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    err = cli_expansion_option_get_data(cli_nkn_services, -1, tstr_array_get_str_quick(params, 0), (void **) &name, &found);
    bail_error(err);

    if (!found || (name == NULL)) {
        cli_printf_error(_("Unknown module name"));
        goto bail;
    }

    //name = tstr_array_get_str_quick(params, 0);
    //bail_null(name);

    p = smprintf("/stats/state/sample/proc_mem/node/\\/stat\\/nkn\\/vmem\\/%s\\/peak", (name));
    bail_null(p);

    d = smprintf("/stats/state/sample/proc_mem/node/\\/stat\\/nkn\\/vmem\\/%s\\/data", (name));
    bail_null(d);

    r = smprintf("/stats/state/sample/proc_mem/node/\\/stat\\/nkn\\/vmem\\/%s\\/resident", (name));
    bail_null(r);

    l = smprintf("/stats/state/sample/proc_mem/node/\\/stat\\/nkn\\/vmem\\/%s\\/lock", (name));
    bail_null(l);

    err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &bn_last, NULL, 
            "%s/last/instance_id", p);
    bail_error(err);

    if (bn_last == NULL) {
        cli_printf_error(_("no data to show"));
        goto bail;
    }
    err = bn_binding_get_uint32(bn_last, ba_value, 0, &last_id);
    bail_error(err);



    cli_printf(_(
                "\tTIME\t\t"
                "VmPeak (kB)\t"
                "VmData (kB)\t"
                "VmRSS  (kB)\t"
                "VmLock (kB)\n"));
    i = (last_id < 120)? 0 : (last_id - 120);
    for (; i < last_id; i++) {

        cli_printf_query(_(
                    "#%s/instance/%d/time#"
                    "\t #%s/instance/%d/value#"
                    "\t #%s/instance/%d/value#"
                    "\t #%s/instance/%d/value#"
                    "\t #%s/instance/%d/value#\n"), 
                p, i, 
                p, i, 
                d, i,
                r, i,
                l, i);
    }

bail:
    safe_free(p);
    safe_free(d);
    safe_free(r);
    safe_free(l);
    bn_binding_free(&bn_last);
    return err;
}

/* ------------------------------------------------------------------------- *
	Function : fms_status()
	Purpose : FMS had pid files that store the process's pid. But to
	check if the process is running we have read the pid from the pid
	file and check if the pid exists. This function does that to 
	figure out if a FMS process is running.

	Returns: 0 if process exists and non zero otherwise
 * ------------------------------------------------------------------------- */
static int
fms_status(const char *pid_path)
{ 
	char	sz_buf [48];
	int	n_size = 0;
	int	fms_pid = 0;
	int	ret_val  = 0;
	FILE	*fp_pidfile = NULL;
	struct stat	stat_info;
	
	/* Sanity test */
	if (!pid_path) return 1;

	/* Now open the pid file */
	fp_pidfile = fopen (pid_path, "r");
	if (NULL == fp_pidfile)
	{
		/* No pid file hence no process */
		return 2;
	}

	/* Now read the PID */
	memset (sz_buf, 0, sizeof(sz_buf));
	n_size = fread (sz_buf, sizeof(char), sizeof(sz_buf), fp_pidfile);
	if (n_size <= 0)
	{
		/* Empty PID file - should never happen */
		fclose(fp_pidfile);
		return 3;
	}

	/* Close the FILE */
	fclose(fp_pidfile);

	/* We assume the buffer has the PID */
	fms_pid = atoi(sz_buf);

	/* Now easiest way to check if this is valid is to check if
	 * /proc/<pid> exists. */
	sprintf (sz_buf, "/proc/%d", fms_pid);

	ret_val = stat (sz_buf, &stat_info);

	return (ret_val);

} /* end of fms_status() */


int
cli_web_listen_intf_show(const bn_binding_array *bindings,
                         uint32 idx, const bn_binding *binding,
                         const tstring *name,
                         const tstr_array *name_components,
                         const tstring *name_last_part,
                         const tstring *value, void *callback_data)
{
    int err = 0;
    tstring *interface = NULL;

    if (idx == 0) {
        err = cli_printf(_("     Listen Interfaces:\n"));
        bail_error(err);
    }

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &interface);
    bail_error(err);

    err = cli_printf(_("        Interface:             %s\n"), ts_str(interface));
    bail_error(err);

 bail:
    ts_free(&interface);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
cli_service_show(void *data, const cli_command *cmd,
            const tstr_array *cmd_line, const tstr_array *params)
{
    int err = 0;
    uint32 num_procs = 0;
    bn_binding_array *bindings = NULL;
    char *spec = NULL;
    tstring *state = NULL;
    tstring *proc_name = NULL;
    char *name = NULL;
    tbool found = false;
    int ret_val = 0;
    char *uptime_str = NULL;
    bn_binding *uptime_binding = NULL;
    tbool ret_ambiguous;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);

    switch (cmd->cc_magic) {

    case cid_service_show_global:
        err = cli_printf_query
            (_(
             "Initial relaunch delay: "
             "#/pm/failure/relaunch_delay_initial# ms\n"

             "Relaunch delay factor: "
             "#/pm/failure/relaunch_delay_factor#\n"

             "Maximum relaunch delay: "
             "#/pm/failure/relaunch_delay_max# ms\n"

             "Relaunch delay reset threshold: "
             "#/pm/failure/relaunch_reset_threshold# ms\n"));
        bail_error(err);

        err = cli_printf_query
             (_(
             "Maximum core size: "
             "#/pm/failure/core_size_limit#\n"

             "Maximum disk space for snapshots: "
             "#/pm/failure/snapshot_max_disk_use#\n"

             "Maximum %% of /var space for snapshots: "
             "#/pm/failure/snapshot_max_disk_use_pct#\n"

             "Minimum disk space to leave free after snapshots: "
             "#/pm/failure/snapshot_min_disk_free#\n"

             "Minimum %% of /var space to leave free after snapshots: "
             "#/pm/failure/snapshot_min_disk_free_pct#\n"

             "Core poll interval: "
             "#/pm/core_poll_freq# sec\n"

             "Liveness polling: "
             "#/pm/failure/liveness/enable#\n"

             "Liveness poll interval: "
             "#/pm/failure/liveness/poll_freq# sec\n"

             "Liveness poll grace period: "
             "#/pm/failure/liveness/grace_period# sec\n"

             "Shutdown delay: "
             "#/pm/shutdown_delay# ms\n"

             "Initial crash scan delay: "
             "#/pm/failure/crash_initial_delay# sec\n"));
        bail_error(err);
        break;

    case cid_service_show_all:
        /*
         * Do query separately so we get "/pm/monitor" tree too.
         */
        err = mdc_get_binding_children
            (cli_mcc,
             NULL, NULL, true, &bindings, false, true, "/pm");
        bail_error(err);
        err = mdc_foreach_binding_prequeried
            (bindings, "/pm/process/*", NULL, cli_service_show_one,
             NULL, &num_procs);
        bail_error(err);

        if (num_procs == 0) {
            err = cli_printf_error(_("No processes configured.\n"));
            bail_error(err);
        }
        break;

    case cid_service_show_one:
        err = tstr_array_get(params, 0, &proc_name);
        bail_error_null(err, proc_name);

        err = cli_expansion_option_get_data_ex(cli_nkn_services, -1, ts_str(proc_name), (void**) &name, &found, &ret_ambiguous);
        bail_error(err);
        if ( !found || (name == NULL)) {
            cli_printf_error(_("Unknown module name."));
            goto bail;
        }

	 if (ret_ambiguous) {
        	cli_printf_error(_("Ambiguous command \"%s\"."),
             		tstr_array_get_str_quick(params, 0));
		 cli_printf("\ntype \"%s %s %s?\" for help", tstr_array_get_str_quick(cmd_line, 0), 
			tstr_array_get_str_quick(cmd_line, 1),  tstr_array_get_str_quick(cmd_line, 2) );
        	goto bail;
    }

	/* Check if it is the FMS services */
	if (!strcmp(name, MOD_RTMP_FMS)) {
	    /*Avoid printing FMS related details */
	    goto bail;
		cli_printf("Service : FMS RTMP Server\n");
		/* Check based on the PID file */
		ret_val = fms_status (FMS_PID_FILE);
		if (ret_val)
			cli_printf("Current status: stopped\n\n");
		else
			cli_printf("Current status: running\n\n");

		/* Done, hence exit the routine */
		goto bail;
	}
	else if (!strcmp(name, MOD_RTMP_ADMIN)) {
	    /*Avoid printing FMS related details*/
	    goto bail;
		cli_printf("Service : FMS RTMP Admin Server\n");
		/* Check based on the PID file */
		ret_val = fms_status (FMS_ADMIN_PID_FILE);
		if (ret_val)
			cli_printf("Current status: stopped\n\n");
		else
			cli_printf("Current status: running\n\n");

		/* Done, hence exit the routine */
		goto bail;
	}
        else if (!strcmp(name, MOD_DMI)) {
                uint32 num_intfs = 0;
                cli_printf("Service : DMI service\n");
                /* Check based on the PID file */
                ret_val = fms_status (DMI_PID_FILE);
                if (ret_val)
                        cli_printf("    DMI status:                stopped\n");
                else
                        cli_printf("    DMI status:                running\n");

                err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &state,
                                 "/pm/monitor/process/agentd/state", NULL);
                bail_error(err);
                err = cli_printf("    Agentd status:             %s\n",ts_str(state));
                bail_error(err);
                ts_free(&state);
                state = NULL;

                err = cli_printf_query
                        (_(
                           "    Management interface:      "
                           "#/nkn/mgmt-if/config/if-name#\n")
                        );
                bail_error(err);

                err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &state,
                                 "/web/httpd/listen/enable", NULL);
                bail_error(err);
                err = cli_printf("    Web listen enabled:        %s\n",ts_str(state));
                bail_error(err);
                
                err = mdc_foreach_binding(cli_mcc,
                              "/web/httpd/listen/interface/*", NULL,
                              cli_web_listen_intf_show, NULL, &num_intfs);
                bail_error(err);

                if (num_intfs == 0) {
                    err = cli_printf(_("       No Listen Interfaces.\n"));
                    bail_error(err);
                }
                err = mdc_get_binding_children
                      (cli_mcc,
                      NULL, NULL, true, &bindings, false, true, "/pm/monitor/process/ssh_tunnel");
                bail_error(err);
                cli_printf_prequeried(bindings,
                  _("    JunOS-agent State:\n"
                    "      Current status:          #/pm/monitor/process/ssh_tunnel/state#\n"
                    "      PID:                     #/pm/monitor/process/ssh_tunnel/pid#\n"
                    "      Num. failures:           #/pm/monitor/process/ssh_tunnel/num_failures#\n"
                    "      Last launched:           #n:r~"
                    "/pm/monitor/process/ssh_tunnel/last_launched/time#\n"
                    "      Last terminated:         #n:r~"
                    "/pm/monitor/process/ssh_tunnel/last_terminated/time#\n"
                    "      Next launch:             #n:r~"
		    "/pm/monitor/process/ssh_tunnel/next_launch/time#\n"));



                /* Done, hence exit the routine */
                goto bail;
        }
	else if (!strcmp(name, MOD_WATCH_DOG)) {
		cli_printf("Service : Watch-Dog\n");
		/* Check based on the node */
		err = cli_printf_query
			(_(
			   "Watch-dog Enabled   : "
			   "#/pm/process/nvsd/liveness/enable#\n"

			   "Restart Enabled     : "
			   "#/nkn/nvsd/watch_dog/config/restart#\n"

			   "Hung Count          : "
			   "#/pm/process/nvsd/liveness/hung_count#\n"

			   "Polling Interval    : "
			   "#/pm/failure/liveness/poll_freq# sec\n"

			   "Polling Grace Period: "
			   "#/pm/failure/liveness/grace_period# sec\n")
			);
		bail_error(err);

		/* Done, hence exit the routine */
		goto bail;
	}
	else if (!strcmp(name, MOD_DELIVERY)) {
		lt_dur_ms uptime = 0;
                tstring *snapshot = NULL;
                tbool b_snapshot = false;

		err = mdc_get_binding(cli_mcc, NULL, NULL, false, &uptime_binding,
				"/pm/monitor/process/nvsd/uptime", NULL);
		bail_error(err);

		if(uptime_binding) {
			/* Check based on the node */
			err = bn_binding_get_duration_ms(uptime_binding, ba_value, NULL,
					&uptime);
			bail_error(err);

			err = lt_dur_ms_to_counter_str(uptime, &uptime_str);
			bail_error(err);
		}

                err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &snapshot,
                                "/nkn/nvsd/system/config/coredump", NULL);
                bail_error(err);
                bail_null(snapshot);

                if(strcmp(ts_str(snapshot),"1")==0){
                    b_snapshot = true;
                }

		err = cli_printf_query("%-50s\n", "Service :  mod-delivery (MFC)");
		bail_error(err);

		err = cli_printf_query("%-50s " " #e:N.A.~/nkn/nvsd/services/monitor/state/nvsd/global#\n", "   Current status:  ");
		bail_error(err);

		err = cli_printf_query("%-50s " " #e:N.A.~/nkn/nvsd/services/monitor/state/nvsd/mgmt#\n", "      Management status:  ");
		bail_error(err);

		err = cli_printf_query("%-50s " " #e:N.A.~/nkn/nvsd/services/monitor/state/nvsd/network#\n", "      Network status:  ");
		bail_error(err);

		err = cli_printf_query("%-50s " " #e:N.A.~/nkn/nvsd/services/monitor/state/nvsd/preread/global#\n", "      Disk-Cache status:  ");
		bail_error(err);

		err = cli_printf_query("%-50s " " #e:N.A.~/nkn/nvsd/system/config/init/preread#\n", "         Include Disk pre-read status:  ");
		bail_error(err);

		err = cli_printf_query("%-50s " " #e:N.A.~/nkn/nvsd/services/monitor/state/nvsd/preread/sas#\n", "         SAS Dictionary generation:  ");
		bail_error(err);

		err = cli_printf_query("%-50s " " #e:N.A.~/nkn/nvsd/services/monitor/state/nvsd/preread/sata#\n", "         SATA Dictionary generation:  ");
		bail_error(err);

		err = cli_printf_query("%-50s " " #e:N.A.~/nkn/nvsd/services/monitor/state/nvsd/preread/ssd#\n", "         SSD Dictionary generation:  ");
		bail_error(err);

		err = cli_printf_query("%-50s " " #e:N.A.~/pm/monitor/process/nvsd/num_failures#\n", "   Num. failures:  ");
		bail_error(err);

                err = cli_printf_query("%-50s " " %s\n", "   Snapshot generation:  ", b_snapshot ? "Enabled" : "Disabled");
                bail_error(err);

		err = cli_printf_query("%-50s " " #e:N.A.~/pm/monitor/process/nvsd/last_terminated/time#\n", "   Last terminated:  ");
		bail_error(err);

		err = cli_printf_query("%-50s " " %s\n", "   Uptime:  ", uptime_str? uptime_str:"N/A");
		bail_error(err);


		/* Done, hence exit the routine */
		goto bail;

	}

		spec = smprintf("/pm/process/%s", (char *) name);//ts_str(proc_name));
        bail_null(spec);

        /*
         * Do query separately so we get "/pm/monitor" tree too.
         */
        err = mdc_get_binding_children
            (cli_mcc,
             NULL, NULL, true, &bindings, false, true, "/pm");
        bail_error(err);
        err = mdc_foreach_binding_prequeried
            (bindings, spec, NULL, cli_service_show_one, NULL, &num_procs);
        bail_error(err);
        if (num_procs == 0) {
            err = cli_printf_error(_("Unknown process name.\n"));
            bail_error(err);
        }
        break;

    default:
        bail_force(lc_err_unexpected_case);
        break;
    }

 bail:
    safe_free(spec);
    safe_free(uptime_str);
    ts_free(&state);
    bn_binding_free(&uptime_binding);
    bn_binding_array_free(&bindings);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
cli_service_show_one(const bn_binding_array *bindings, uint32 idx,
                const bn_binding *binding, const tstring *name,
                const tstr_array *name_components,
                const tstring *name_last_part,
                const tstring *value, void *callback_data)
{
    int err = 0;
    const char *nn = NULL; /* node name */
    const char *pn = NULL; /* process name */
    const bn_binding *uptime_binding = NULL;
    lt_dur_ms uptime = 0;
    char *uptime_name = NULL, *uptime_str = NULL;
    tstring *descr = NULL;
    char *pn_full = NULL;

    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(callback_data);

    nn = ts_str(name);
    bail_null(nn);

    pn = ts_str(name_last_part);
    bail_null(pn);

    /* Check if it one of the interested process */
    if (strcmp (pn, "nvsd") && strcmp (pn, "nknlogd")
    		&& strcmp (pn, "ftpd")
    		&& strcmp (pn, "watchdog")
    		&& strcmp (pn, "live_mfpd")
		&& strcmp (pn, "file_mfpd")
    		&& strcmp (pn, "nkncnt")
    		&& strcmp (pn, MOD_CRAWLER)
    		&& strcmp (pn, "geodbd")
		&& strcmp (pn, "ssld"))
        goto bail;


    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &descr, NULL,
                                   "/pm/process/%s/description", pn);
    bail_error(err);

    if (descr && ts_num_chars(descr) > 0) {
        if (!strcmp (pn, "nvsd"))
            pn_full = smprintf("mod-delivery (%s)", ts_str(descr));
	else if (!strcmp (pn, "nknlogd"))
            pn_full = smprintf("mod-log (%s)", ts_str(descr));
	else if (!strcmp (pn, "ftpd"))
            pn_full = smprintf("mod-ftp (%s)", ts_str(descr));
	else if (!strcmp (pn, "file_mfpd"))
            pn_full = smprintf("mod-file-publisher (%s)", ts_str(descr));
	else if (!strcmp (pn, "live_mfpd"))
            pn_full = smprintf("mod-live-publisher (%s)", ts_str(descr));
	else if (!strcmp (pn, "nkncnt"))
            pn_full = smprintf("mod-stat (%s)", ts_str(descr));
	else if (!strcmp (pn, "ssld"))
            pn_full = smprintf("mod-ssld (%s)", ts_str(descr));
  	else if (!strcmp (pn, "watchdog"))
                pn_full = smprintf("watchdog");
  	else if (!strcmp (pn, MOD_CRAWLER))
                pn_full = smprintf("mod-crawler");
  	else if (!strcmp (pn, "geodbd"))
                pn_full = smprintf("geodb-daemon");
        bail_null(pn_full);
    }
    else {
        if (!strcmp (pn, "nvsd"))
            pn_full = strdup ("delivery");
	else if (!strcmp (pn, "nknlogd"))
            pn_full = strdup("logd");

	else if (!strcmp (pn, "ftpd"))
            pn_full = strdup("pre-stage ftp");
	else if (!strcmp (pn, "file_mfpd"))
            pn_full = strdup("file-mfpd");
	else if (!strcmp (pn, "live_mfpd"))
            pn_full = strdup("live_mfpd");
	else if (!strcmp (pn, "nkncnt"))
            pn_full = smprintf("mod-stat (Counter Collection)");
  	else if (!strcmp (pn, "geodbd"))
                pn_full = smprintf("geodb-daemon");
	else if (!strcmp (pn, MOD_CRAWLER))
            pn_full = smprintf("mod-crawler");

        bail_null(pn_full);
    }

    uptime_name = smprintf("/pm/monitor/process/%s/uptime", pn);
    bail_null(uptime_name);

    err = bn_binding_array_get_binding_by_name
        (bindings, uptime_name, &uptime_binding);
    bail_error(err);

    if (uptime_binding) {
        err = bn_binding_get_duration_ms(uptime_binding, ba_value, NULL,
                                         &uptime);
        bail_error(err);

        err = lt_dur_ms_to_counter_str(uptime, &uptime_str);
        bail_error(err);
    }

    else {
        uptime_str = strdup(_("N/A"));
        bail_null(uptime_str);
    }

    if (idx > 0) {
        err = cli_printf("\n");
        bail_error(err);
    }

    err = cli_printf_prequeried
        (bindings,
         _("Service : %s\n"
           "   Current status:  #/pm/monitor/process/%s/state#\n"
           "   Num. failures:   #/pm/monitor/process/%s/num_failures#\n"
           "   Last terminated: #/pm/monitor/process/%s/last_terminated/"
                                                                      "time#\n"
           "   Uptime:          %s\n"),
         pn_full, pn, pn, pn, uptime_str);
    bail_error(err);

    /* XXX/EMT: maybe cli_printf_prequeried() et al. could automatically
     * use pretty-printing funcs for time duration types...
     */

 bail:
    ts_free(&descr);
    safe_free(pn_full);
    safe_free(uptime_name);
    safe_free(uptime_str);
    return(err);
}
static int
md_clear_nvsd_samples(void *data, const cli_command *cmd,
            const tstr_array *cmd_line, const tstr_array *params)
{
    int err = 0;
    int i = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    while(sample_clr_list[i] != NULL) {
        err = mdc_send_action_with_bindings_str_va(
                cli_mcc, NULL, NULL, "/stats/actions/clear/sample", 1,
                "id", bt_string, sample_clr_list[i]);
        bail_error(err);
        i++;
    }

    err = mdc_send_action_with_bindings_str_va(
                cli_mcc, NULL, NULL, "/pm/actions/restart_process", 1,
                "process_name", bt_string, "nvsd");
    bail_error(err);

    err = mdc_send_action_with_bindings_str_va(
                cli_mcc, NULL, NULL, "/pm/actions/restart_process", 1,
                "process_name", bt_string, "ssld");
    bail_error(err);

    err = mdc_send_action_with_bindings_str_va(
                cli_mcc, NULL, NULL, "/pm/actions/restart_process", 1,
                "process_name", bt_string, "jpsd");
    bail_error(err);

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
int
cli_network_print_restart_msg(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
    int err = 0;

    /* Call the standard cli processing */
    err = cli_std_exec(data, cmd, cmd_line, params);
    bail_error(err);

    /* Print the restart message */
    err = cli_printf("warning: if command successful, please restart mod-delivery service for change to take effect\n");
    bail_error(err);

 bail:
    return(err);
}
static int 
set_fre_sav_dur( 
		tbool found,
		const char *pattern_str, 
		const char *time_frequency, 
		const char *time_duration,
		const char *file_name,
		tstr_array **arr)
{
	int err = 0;

	err = tstr_array_new ( arr , NULL);
	bail_error (err);
	err = tstr_array_append_str (*arr, NKNCNT_PATH);
	bail_error (err);
	if (time_frequency)
	{
		err = tstr_array_append_str (*arr, "-t");	
		bail_error(err);
		err = tstr_array_append_str (*arr, time_frequency);	
		bail_error(err);
	} else {
		err = tstr_array_append_str (*arr, "-t");	
		bail_error(err);
		err = tstr_array_append_str (*arr, "0");	
		bail_error(err);
	}

	if (time_duration)
	{
		err = tstr_array_append_str (*arr, "-l");	
		bail_error(err);
		err = tstr_array_append_str (*arr, time_duration);	
		bail_error(err);
	} else {
		err = tstr_array_append_str (*arr, "-l");	
		bail_error(err);
		err = tstr_array_append_str (*arr, "0");	
		bail_error(err);
	}
	if (file_name)
	{
		err = tstr_array_append_str (*arr, "-S");	
		bail_error(err);
		err = tstr_array_append_str (*arr, file_name);	
		bail_error(err);
	}
	if (found)
	{
		err = tstr_array_append_str (*arr, "-s");
		bail_error(err);
		err = tstr_array_append_str (*arr, "");
		bail_error(err);
	} else {
		err = tstr_array_append_str (*arr, "-s");
		bail_error(err);
		err = tstr_array_append_str (*arr, pattern_str); 
		bail_error(err);
	}

bail:
	return err;

}

static int
generate_file_name_for_counters(
				char **file_name) 
{


	int err = 0;
	tstring *hostname = NULL;
	time_t timeval = 0;
	struct tm *timeinfo = NULL;
	char time_string_buffer[40] = { 0 };
	
        err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &hostname,
                          "/system/hostname", NULL);
        bail_error(err);
        bail_null_msg(hostname,"host name is null some thing is wrong");

	/* get current time */
	time(&timeval);
	timeinfo = localtime (&timeval);

	/* format current time to yearmonthday-hourminutesecond format */
	strftime (time_string_buffer, sizeof(time_string_buffer), "%y%m%d-%H%M%S",timeinfo);
	*file_name = smprintf("%s%s-%s-%s.txt", NKNCNT_FILE_PREFIX, NKNCNT_FILE, ts_str(hostname), time_string_buffer);
	bail_null_msg(*file_name,"filename generated is null");			

bail:
	ts_free(&hostname);
	return err;

}

int
cli_nkn_show_counters_internal_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	int num_params = 0;
	const char *pattern_str;
	tstr_array *argv = NULL;
	char *name = NULL;
	tbool found = false;
	char *file_name = NULL;
	char *grep_pattern = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(params);

	/* Get the number of parameters first */
	num_params = tstr_array_length_quick(cmd_line);

	/* Check if it is "show counters internal" */

	if (num_params == 3)
	{
		err = set_fre_sav_dur(
				false , "", NULL, NULL, NULL, &argv);
		bail_error(err);
		err = nkn_clish_run_program_fg(NKNCNT_BINARY_PATH, argv, NULL, NULL, NULL);
		bail_error(err);
	} else if (num_params == 4)
	{
		pattern_str = tstr_array_get_str_quick(cmd_line, 3);
		err = cli_expansion_option_get_data(cli_set_default, -1, pattern_str, (void **) &name, &found);
		bail_error(err);
		err = set_fre_sav_dur(
				found, pattern_str, NULL, NULL, NULL, &argv);
		bail_error(err);

		err = nkn_clish_run_program_fg(NKNCNT_BINARY_PATH, argv, NULL, NULL, NULL);
		bail_error(err);
	} else if (num_params > 4) {

		pattern_str = tstr_array_get_str_quick(cmd_line, 3);
		err = cli_expansion_option_get_data(cli_set_default, -1, pattern_str,
									(void **) &name, &found);
		bail_error(err);
		/* only save is set */
		err = generate_file_name_for_counters(&file_name);
		bail_error(err);
		err = set_fre_sav_dur(
			found, pattern_str, NULL,
			NULL, file_name, &argv);
		bail_error(err);
		lc_log_basic(LOG_NOTICE,_("Counter data dump filename: %s\n"), file_name);
	 	//err = nkn_clish_run_program_fg(NKNCNT_PATH, argv, NULL, NULL, NULL);
	        err = cli_nkn_launch_program_bg(NKNCNT_PATH, argv, NULL, NULL, NULL);
		bail_error(err);
	}

bail:
    safe_free(file_name);
    safe_free(name);
    safe_free(grep_pattern);
    tstr_array_free(&argv);
    return err;

} /* end of cli_nkn_show_counters_internal_cb() */

int
cli_smartd_show_counters_internal_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)

{
        int err = 0;
        int num_params = 0;
        tstr_array *argv = NULL;
        char *name = NULL;
        const char *grep_pattern = NULL;

        UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(cmd);
        UNUSED_ARGUMENT(params);

        /* Get the number of parameters first */
        num_params = tstr_array_length_quick(cmd_line);
        lc_log_basic(LOG_DEBUG, _("Num params:%d\n"), num_params);

        err = tstr_array_new ( &argv , NULL);
        bail_error (err);
        err = tstr_array_append_str (argv, "/opt/nkn/bin/nkncnt");
        bail_error (err);
        err = tstr_array_append_str (argv, "-P");
        bail_error (err);
        err = tstr_array_append_str (argv, "/opt/nkn/sbin/nsmartd");
        bail_error (err);

        if(num_params == 4) {  /*sh counters internal smart*/
                /*Just the global level stats is needed*/
                err = tstr_array_append_str (argv, "-s");
                bail_error (err);
                err = tstr_array_append_str (argv, "");
                bail_error (err);
        } else if (num_params == 5) { /*sh counters internal smart <counter-name>*/
                grep_pattern = tstr_array_get_str_quick(cmd_line, 4);
                bail_null(grep_pattern);

                err = tstr_array_append_str (argv, "-s");
                bail_error (err);
                err = tstr_array_append_str (argv, grep_pattern);
                bail_error (err);


        }
        err = tstr_array_append_str (argv, "-t");
        bail_error (err);
        err = tstr_array_append_str (argv, "0");
        bail_error (err);

        err = nkn_clish_run_program_fg("/opt/nkn/bin/nkncnt", argv, NULL, NULL, NULL);
        bail_error(err);

bail:
        safe_free(name);
        tstr_array_free(&argv);
        return err;
}



cli_execution_callback cli_show_version = NULL;
static int
cli_reg_nkn_version_override(
        const lc_dso_info *info, 
        const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;
	void *callback = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	err = lc_dso_module_get_symbol(info->ldi_context,
		"cli_diag_cmds", "cli_diag_show_version",
		&callback); 
	bail_error(err);

	if (callback) {
		cli_show_version = (cli_execution_callback)(callback);
	}
	else
		goto bail;
	err = cli_get_command_registration("show version", &cmd);
	bail_error_null(err, cmd);

	cmd->cc_exec_callback = cli_nkn_version_copyright_set;

bail:
	return err;
}



int 
cli_nkn_version_copyright_set(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;

	err = cli_printf
            (_("%s"), nokeena_copyright);
	bail_error(err);

        if (cli_show_version) {
	     err = cli_show_version(data, cmd, cmd_line, params);
	     bail_error(err);
        }

bail:
	return err;
}


static int 
cli_reg_nkn_ntp_override(
        const lc_dso_info *info, 
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = cli_get_command_registration("ntp server *", &cmd);
    bail_error_null(err, cmd);

    cmd->cc_help_exp =          N_("<hostname (FQDN) or IP address>");
    cmd->cc_exec_callback =     cli_nkn_ntp_server_set;

bail:
    return err;
}

static int
cli_reg_nkn_hostname_override(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = cli_get_command_registration("hostname *", &cmd);
    bail_error_null(err, cmd);

    cmd->cc_exec_callback =     cli_network_print_restart_msg;

bail:
    return err;
}

int 
cli_nkn_ntp_server_set(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *name = NULL;
    char *node = NULL;
    struct hostent *hent = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    name = tstr_array_get_str_quick(params, 0);
    bail_null(name);


    /* Test if user entered an IP address or a hostname */
    if (!lia_str_is_ipv4addr_quick(name)) {
	
        hent = gethostbyname(name);
        if (NULL == hent) {
            cli_printf_error(_("error: unable to resolve host '%s'"), name);
            err = 0;
            goto bail;
        }

        if (hent->h_addr_list[0] != NULL) {
            name = inet_ntoa(*(struct in_addr*)(hent->h_addr_list[0]));
        }
        else {
            err = lc_err_generic_error;
            bail_error_msg(err, "error: unable to resolve host '%s'", name);
        }
    }

    node = smprintf("/ntp/server/address/%s", name);
    bail_null(node);

    err = mdc_create_binding(cli_mcc, NULL, NULL, 
            bt_hostname, 
            name,
            node);
    bail_error(err);

bail:
    safe_free(node);
    return err;
}

int
cli_tech_supp_am(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *remote_url = NULL;

    UNUSED_ARGUMENT(data);

    remote_url = tstr_array_get_str_quick(params, 0);
    bail_null(remote_url);

    if (clish_is_shell()) {
        err = clish_maybe_prompt_for_password_url
                    (remote_url, cli_tech_supp_am_upload_finish,
                     cmd, cmd_line, params, NULL);
        bail_error(err);
    }
    else {
        err = cli_tech_supp_am_upload_finish(NULL, cpr_ok, cmd, cmd_line, params,
                NULL, NULL);
        bail_error(err);
    }


bail:
    return err;
}


static int
cli_files_config_upload_finish(const char *password, clish_password_result result,
                                           const cli_command *cmd, const tstr_array *cmd_line,
                                           const tstr_array *params, void *data,
                                           tbool *ret_continue)
{
    int err = 0;
    const char *remote_url = NULL;
    bn_binding_array *bindings = NULL;
    tstring  *result_str = NULL;
    uint32 code = 0;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;
    str_value_t cfg_file_name = {0};
    str_value_t cfg_full_file_name = {0};

    UNUSED_ARGUMENT(password);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(ret_continue);

    if (result != cpr_ok) {
	    goto bail;
    }

    lt_time_us cur_time = lt_curr_time_us();

    snprintf(cfg_file_name, sizeof(cfg_file_name), "config_files_%li.tgz", cur_time);
    snprintf(cfg_full_file_name, sizeof(cfg_full_file_name), "%s/%s",
		    service_config_file_path,cfg_file_name);



    err = mdc_send_action_with_bindings_and_results_va(cli_mcc,
		    &code,
		    NULL,
		    "/nkn/debug/generate/config_files",
		    &bindings,
		    2,
		    "cmd", bt_uint16, "1",
		    "file_name", bt_string, cfg_full_file_name);
    bail_error(err);

    if(bindings) {
        err = bn_binding_array_get_value_tstr_by_name(
                bindings, "Result", NULL, &result_str);
        bail_error(err);
    }

    if(code) {
	lc_log_basic(LOG_NOTICE, "Unable to generate config files");
	goto bail;
    }

    if (ts_equal_str(result_str, "Files compressed.\n", false)) {

	
	remote_url = tstr_array_get_str_quick(params, 0);
	bail_null(remote_url);

        if (password) {
            err = mdc_send_action_with_bindings_str_va(cli_mcc,
                    &ret_err, &ret_msg,
                    "/file/transfer/upload", 3,
                    "local_path", bt_string, cfg_full_file_name,
                    "remote_url", bt_string, remote_url,
                    "password", bt_string, password);
            bail_error(err);
        }
        else {
            err = mdc_send_action_with_bindings_str_va(cli_mcc,
                    &ret_err, &ret_msg,
                    "/file/transfer/upload", 3,
                    "local_dir", bt_string, service_config_file_path,
                    "local_filename", bt_string, cfg_file_name,
                    "remote_url", bt_string, remote_url);
            bail_error(err);
        }
	if(!ret_err) {
	    err = cli_printf("Config file %s uploaded.\n", cfg_file_name);
	    bail_error(err);
	} else {
	    cli_printf_error("Unable to upload config file %s.\n", cfg_file_name);
	}
    } else {
	cli_printf_error("%s", ts_str(result_str));
        lc_log_basic(LOG_WARNING, _("Config file upload returned code %d"), code);
    }


bail:
    bn_binding_array_free(&bindings);
    lf_remove_file(cfg_full_file_name);
    ts_free(&ret_msg);
    ts_free(&result_str);
    return err;
}

static int
cli_tech_supp_am_upload_finish(const char *password, clish_password_result result,
                                           const cli_command *cmd, const tstr_array *cmd_line,
                                           const tstr_array *params, void *data,
                                           tbool *ret_continue)
{
    int err = 0;
    const char *remote_url = NULL;
    bn_binding_array *bindings = NULL;
    tstring  *dump_path = NULL, *dump_name = NULL, *dump_dir = NULL;
    uint32 code = 0;
    const char *rank = NULL;

    UNUSED_ARGUMENT(password);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(ret_continue);

    /*
     * We have nothing to clean up, so we really only care about this
     * if we got a successful password entry.
     */
    if (result != cpr_ok) {
        goto bail;
    }
#if 1
    err = mdc_send_action_with_bindings_and_results(cli_mcc,
            &code,
            NULL,
            "/nkn/debug/generate/amrankdump",
            NULL,
            &bindings);
    bail_error(err);

    if(bindings) {
        err = bn_binding_array_get_value_tstr_by_name(
                bindings, "dump_path", NULL, &dump_path);
        bail_error(err);
    }
#endif

    if (NULL == rank) {
        // Default case
        rank = "-1";
    }
#if 0
    err = mdc_send_action_with_bindings_str_va
            (cli_mcc, &ret_err,&ret_msg,"/nkn/nvsd/am/actions/rank",
            1, "rank", bt_int32, rank);
    bail_error(err);

    if (ret_err != 0) {
        cli_printf(_("Error in executing command.\n"));
        goto bail;
    } 
    	
    dump_path = "/var/log/nkn/amrank";
    dump_dir = "/var/log/nkn";
    dump_name = "amrank";
    fp = fopen("/var/log/nkn/amrank","w");
    bail_null(fp);
    fprintf(fp,"%s\n",ts_str(ret_msg));
    fclose(fp);
#endif	

#if 1
    if (dump_path) {
        err = lf_path_last(ts_str(dump_path), &dump_name);
        bail_error_null(err, dump_name);

        err = lf_path_parent(ts_str(dump_path), &dump_dir);
        bail_error(err);

        remote_url = tstr_array_get_str_quick(params, 0);
        bail_null(remote_url);

        if (password) {
            err = mdc_send_action_with_bindings_str_va(cli_mcc,
                    NULL, NULL,
                    "/file/transfer/upload", 3,
                    "local_path", bt_string, ts_str(dump_path),
                    //"local_dir", bt_string, ts_str(dump_dir),
                    //"local_filename", bt_string, ts_str(dump_name),
                    "remote_url", bt_string, remote_url,
                    "password", bt_string, password);
            bail_error(err);
        }
        else {
            err = mdc_send_action_with_bindings_str_va(cli_mcc,
                    NULL, NULL,
                    "/file/transfer/upload", 3,
                    "local_dir", bt_string, ts_str(dump_dir),
                    "local_filename", bt_string, ts_str(dump_name),
                    "remote_url", bt_string, remote_url);
            bail_error(err);
        }
    }
    else if (code == 0) {

    }
    else {
        lc_log_basic(LOG_WARNING, _("System dump generation returned code %d"), code);
    }

#endif

bail:
    bn_binding_array_free(&bindings);
    return err;
}

/* ----------------------------------------------------------------------- */
static int 
cli_app_fms_shell(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	lc_launch_params *lp=NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	err = clish_prepare_to_exec();
	complain_error(err);

	err = lc_launch_params_get_defaults(&lp);
	bail_error(err);


	err = ts_new_str(&(lp->lp_path), "/usr/sbin/chroot");
	bail_error(err);

	err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 2,
					"/usr/sbin/chroot", "/nkn/adobe");
	bail_error(err);

	lp->lp_fork = false;
	lp->lp_env_opts = eo_clear_all;
	lp->lp_tolerate_errors = true;

	/* Set the limited environment variables */
	err = tstr_array_new_va_str(&(lp->lp_env_set_names), NULL, 2,
					"SHELL", "PATH");
	bail_error(err);
	err = tstr_array_new_va_str(&(lp->lp_env_set_values), NULL, 2,
					"/bin/sh", "/bin");
	bail_error(err);

	err = lc_launch(lp, NULL);
	bail_error(err);

bail:
	lc_launch_params_free(&lp);
	return err;
}

/* ----------------------------------------------------------------------- */
static int 
cli_mod_fms_restart(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	const char *name = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(params);

	/* Get the 3rd command line string */
	name = tstr_array_get_str_quick(cmd_line, 2);
	bail_null(name);

	/* Now check if it is FMS service restart */
	if (!strcmp(name, MOD_RTMP_FMS))
		system ("/nkn/adobe/fms/fmsmgr server fms restart 2> /dev/null");
	else if (!strcmp(name, MOD_RTMP_ADMIN))
		system ("/nkn/adobe/fms/fmsmgr adminserver restart 2> /dev/null");
bail:
	return err;

} /* end of cli_mod_fms_restart */


/* ------------------------------------------------------------------------- */
static int
cli_fms_image_fetch_internal(const char *url, const char *password,
                         const char *filename)
{
    int err = 0;
    bn_request *req = NULL;
    uint16 db_file_mode = 0;
    char *db_file_mode_str = NULL;

    err = bn_action_request_msg_create(&req, "/file/transfer/download");
    bail_error(err);

    bail_null(url);
    err = bn_action_request_msg_append_new_binding
        (req, 0, "remote_url", bt_string, url, NULL);
    bail_error(err);

    if (password) {
        err = bn_action_request_msg_append_new_binding
            (req, 0, "password", bt_string, password, NULL);
        bail_error(err);
    }

    err = bn_action_request_msg_append_new_binding
        (req, 0, "local_dir", bt_string, FMS_DOWNLOADS_PATH, NULL);
    bail_error(err);

    if (filename) {
        err = bn_action_request_msg_append_new_binding
            (req, 0, "local_filename", bt_string, filename, NULL);
        bail_error(err);
    }

    err = bn_action_request_msg_append_new_binding
        (req, 0, "allow_overwrite", bt_bool, "true", NULL);
    bail_error(err);

    /*
     * If we're not in the CLI shell, we won't be displaying progress,
     * so there's no need to track it.
     *
     * XXX/EMT: we should make up our own progress ID, and start and
     * end our own operation, and tell the action about it with the
     * progress_image_id parameter.  This is so we can report errors
     * that happen outside the context of the action request.  But
     * it's not a big deal for now, since almost nothing happens
     * outside of this one action, so errors are unlikely.
     */
    if (clish_progress_is_enabled()) {
        err = bn_action_request_msg_append_new_binding
            (req, 0, "track_progress", bt_bool, "true", NULL);
        bail_error(err);
        err = bn_action_request_msg_append_new_binding
            (req, 0, "progress_oper_type", bt_string, "image_download", NULL);
        bail_error(err);
        err = bn_action_request_msg_append_new_binding
            (req, 0, "progress_erase_old", bt_bool, "true", NULL);
        bail_error(err);
    }

    db_file_mode = 0600;
    db_file_mode_str = smprintf("%d", db_file_mode);
    bail_null(db_file_mode_str);

    err = bn_action_request_msg_append_new_binding
        (req, 0, "mode", bt_uint16, db_file_mode_str, NULL);
    bail_error(err);

    if (clish_progress_is_enabled()) {
        err = clish_send_mgmt_msg_async(req);
        bail_error(err);
        err = clish_progress_track(req, NULL, 0, NULL, NULL);
        bail_error(err);
    }
    else {
        err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
        bail_error(err);
    }

 bail:
    safe_free(db_file_mode_str);
    bn_request_msg_free(&req);
    return(err);
} /* end of  cli_fms_image_fetch_internal() */


/* ------------------------------------------------------------------------- */
static int
cli_fms_image_fetch_finish(const char *password, clish_password_result result,
                       const cli_command *cmd,
                       const tstr_array *cmd_line, const tstr_array *params,
                       void *data, tbool *ret_continue)
{
    int err = 0;
    const char *remote_url = NULL;
    const char *filename = NULL;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(ret_continue);
    UNUSED_ARGUMENT(data);

    /*
     * We have nothing to clean up, so we really only care about this
     * if we got a successful password entry.
     */
    if (result != cpr_ok) {
        goto bail;
    }

    remote_url = tstr_array_get_str_quick(params, 0);
    bail_null(remote_url);

    filename = tstr_array_get_str_quick(params, 1);
    /* May be NULL */

#ifdef cli_default_image_filename
    cid = cmd->cc_magic;
    if (cid == cid_image_fetch_default) {
        filename = cli_default_image_filename;
    }
#endif

    err = cli_fms_image_fetch_internal(remote_url, password, filename);
    bail_error(err);

 bail:
    return(err);
} /* end of cli_fms_image_fetch_finish() */


/* ------------------------------------------------------------------------- */
static int
cli_fms_image_fetch(void *data, const cli_command *cmd,
                const tstr_array *cmd_line,
                const tstr_array *params)
{
    int err = 0;
    const char *remote_url = NULL;

    UNUSED_ARGUMENT(data);

    remote_url = tstr_array_get_str_quick(params, 0);
    bail_null(remote_url);

    if (clish_is_shell()) {
        err = clish_maybe_prompt_for_password_url
           (remote_url, cli_fms_image_fetch_finish, cmd, cmd_line, params, NULL);
        bail_error(err);
    }
    else {
        err = cli_fms_image_fetch_finish(NULL, cpr_ok, cmd, cmd_line, params, 
                                     NULL, NULL);
        bail_error(err);
    }

 bail:
    return(err);
} /* end of cli_fms_image_fetch() */

/* ------------------------------------------------------------------------- */
static int 
fms_install_terminate(void *data)
{
    tbool file_exists = false;
    UNUSED_ARGUMENT(data);

    /* not checking return value of lf_test_exists() */
    lf_test_exists(SYSTEM_TMP_PASSWD_FILE, &file_exists);
    if (file_exists == true) {
	/* we must move it back to the correct place, ignoring error code */
	lf_rename_file(SYSTEM_TMP_PASSWD_FILE, SYSTEM_PASSWD_FILE);
    }
    /* 
     * we cannot say, installation passed/failed. function should
     * return success even if failed
     */
    return 0;
}


static int 
cli_fms_image_install(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	const char *image_name = NULL;
	char *image_path = NULL;
	tstr_array *argv = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	/* Get the image name */
	image_name = tstr_array_get_str_quick(params, 0);
	bail_null(image_name);

	err = tstr_array_new (&argv , NULL);
	bail_error (err);
	err = tstr_array_append_str (argv, FMS_INSTALLSCRIPT_PATH);
	bail_error (err);

	image_path = smprintf ("%s/%s", FMS_DOWNLOADS_PATH, image_name);
	err = tstr_array_append_str (argv, image_path);	
	bail_error(err);

	/* make sure to delete temporary file */
	unlink(SYSTEM_TMP_PASSWD_FILE);

	err = nkn_clish_run_program_fg(FMS_INSTALLSCRIPT_PATH, argv, NULL, fms_install_terminate , NULL);
	bail_error(err);

bail:
    safe_free(image_path);
    tstr_array_free(&argv);
    return err;

} /* end of cli_fms_image_install() */

static int
cli_reg_nkn_rtsched_cmds(const lc_dso_info *info,
                    const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str	   = "scheduler";
    /*cmd->cc_flags          = ccf_hidden; bug# 8077*/
    cmd->cc_help_descr     = N_("Configure the number of tasks and threads");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "no scheduler";
    /*cmd->cc_flags          = ccf_hidden; bug# 8077 */
    cmd->cc_help_descr     = N_("Reset the number of tasks and threads");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	   = "scheduler threads";
    cmd->cc_help_descr     = N_("Configure the number of threads");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "no scheduler threads";
    cmd->cc_help_descr     = N_("Reset the number of  threads");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	   = "scheduler tasks";
    cmd->cc_help_descr     = N_("Configure the number of tasks per scheduler");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "no scheduler tasks";
    cmd->cc_help_descr     = N_("Reset the number of  tasks");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	   = "scheduler tasks deadline";
    cmd->cc_help_descr     = N_("Configure the number of tasks");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "no scheduler tasks deadline";
    cmd->cc_help_descr     = N_("Reset the number of deadline tasks");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_reset;
    cmd->cc_exec_callback  = cli_nkn_set_sched_tasks_value;
    cmd->cc_magic	   = reset_deadline_tasks;
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	   = "scheduler tasks deadline *";
    cmd->cc_help_exp       = N_("<number>");
    cmd->cc_help_exp_hint  = N_("Configure the number of deadline tasks. Default: 256 "
				"Valid Input Values: 256 or 512 or 1024");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_callback  = cli_nkn_set_sched_tasks_value;
    cmd->cc_magic	   = set_deadline_tasks;
    cmd->cc_revmap_type    = crt_manual;
    cmd->cc_revmap_order   = cro_scheduler;
    cmd->cc_revmap_names   = "/nkn/nvsd/system/config/sched/tasks";
    cmd->cc_revmap_cmds	   = "scheduler tasks deadline $v$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "scheduler threads realtime";
    cmd->cc_help_descr     = N_("Configure the number of realtime threads");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "no scheduler threads realtime ";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_help_descr	   = N_("Reset the number of realtime threads");
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_reset;
    cmd->cc_exec_callback  = cli_nkn_set_rtsched_threads_value;
    cmd->cc_magic	   = cid_reset_rtsched;
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "scheduler threads realtime *";
    cmd->cc_flags          = ccf_terminal ;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_help_exp       = N_("<number>");
    cmd->cc_help_exp_hint  = N_("Set the number of realtime threads (1-8)");
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_callback  = cli_nkn_set_rtsched_threads_value;
    cmd->cc_magic	   = cid_set_rtsched;
    cmd->cc_revmap_type    = crt_manual;
    cmd->cc_revmap_order   = cro_scheduler;
    cmd->cc_revmap_names   = "/nkn/nvsd/system/config/rtsched/threads";
    cmd->cc_revmap_cmds	   = "scheduler threads realtime $v$";
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "scheduler threads deadline ";
    cmd->cc_help_descr     = N_("Configure the number of deadline threads");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "no scheduler threads deadline ";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_help_descr	   = N_("Reset the number of deadline threads");
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_callback  = cli_nkn_set_sched_threads_value;
    cmd->cc_magic	   = cid_reset_sched;
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "scheduler threads deadline *";
    cmd->cc_flags          = ccf_terminal ;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_help_exp       = N_("<number>");
    cmd->cc_help_exp_hint  = N_("Set the number of deadline threads. Valid input value: 0 - 8");
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_callback  = cli_nkn_set_sched_threads_value;
    cmd->cc_magic	    = cid_set_sched;
    cmd->cc_revmap_type    = crt_manual;
    cmd->cc_revmap_order   = cro_scheduler;
    cmd->cc_revmap_names   = "/nkn/nvsd/system/config/sched/threads";
    cmd->cc_revmap_cmds	   = "scheduler threads deadline $v$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	    = "show scheduler";
    cmd->cc_help_descr	    = N_("Show Cluster manager status");
    cmd->cc_flags	    = ccf_terminal;
    cmd->cc_capab_required  = ccp_query_basic_curr;
    cmd->cc_exec_callback   = cli_nkn_show_scheduler_config;
    cmd->cc_revmap_type	    = crt_none;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings_va(5,
		    "/nkn/nvsd/system/config/rtsched/threads",
		    "/nkn/nvsd/system/config/sched/threads",
		    "/nkn/nvsd/system/config/sched/tasks",
		   "/nkn/nvsd/system/config/init/network",
		   "/nkn/nvsd/system/config/init/preread");

bail:
    return err;
}


int
cli_nkn_set_rtsched_threads_value(void *data, const cli_command *cmd,
		                        const tstr_array *cmd_line,
			                const tstr_array *params)
{
	int err = 0;
	uint32 code = 0;
	tstring *msg = NULL;
	const char *no_of_threads = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd_line);

	switch(cmd->cc_magic)
	{
	case cid_reset_rtsched:
		err = mdc_set_binding(cli_mcc,
				&code,
				&msg,
				bsso_reset,
				bt_uint32,
				"0",
				"/nkn/nvsd/system/config/rtsched/threads");
		bail_error(err);
		break;
	case cid_set_rtsched:
		no_of_threads = tstr_array_get_str_quick(params, 0);
		bail_null(no_of_threads);
		err = mdc_set_binding(cli_mcc,
				&code,
				&msg,
				bsso_modify,
				bt_uint32,
				no_of_threads,
				"/nkn/nvsd/system/config/rtsched/threads");
		bail_error(err);
		if ( code != 0){
			cli_printf(_("Error:%s\n"), ts_str(msg));
		}
		break;
	}
			
	cli_printf("warning: if command successful, please restart "
		"mod-delivery service for change to take effect\n");
bail:
	ts_free(&msg);
	return err;
}


int
cli_nkn_set_sched_tasks_value(void *data, const cli_command *cmd,
		                        const tstr_array *cmd_line,
		                        const tstr_array *params)
{
	int err = 0;
	uint32 code = 0;
	tstring *msg = NULL;
	const char *no_of_tasks = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(params);
	UNUSED_ARGUMENT(cmd_line);
	
	switch(cmd->cc_magic)
	{
	case reset_deadline_tasks:
		err = mdc_set_binding(cli_mcc,
				&code,
				&msg,
				bsso_reset,
				bt_uint32,
				"256",
				"/nkn/nvsd/system/config/sched/tasks");
		bail_error(err);
		break;
	case set_deadline_tasks:
		no_of_tasks = tstr_array_get_str_quick(params, 0);
		bail_null(no_of_tasks);
		err = mdc_set_binding(cli_mcc,
				&code,
				&msg,
				bsso_modify,
				bt_uint32,
				no_of_tasks,
				"/nkn/nvsd/system/config/sched/tasks");
		bail_error(err);
		if ( code != 0){
			cli_printf(_("Error:%s\n"), ts_str(msg));
		}
		break;
	}

	cli_printf("warning: if command successful, please service restart "
		"mod-delivery for change to take effect\n");
bail:
	ts_free(&msg);
	return err;
}

#if 1 // Commented out by RAMANAND on 23rd April 2010 as there are 
	// multiple definition of this function. 
	/* Uncommented by Varsha for Bug 4997 (30-Apr-2010) as this function will
	 * in-turn call HTTP counter and RTSP counter function. The
	 * multiple definition was because the HTTP counter display
	 * alone was enabled initially and named as
	 * cli_nkn_show_counters for bug 4717.
	 */
int
cli_nkn_show_counters_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;

    err = cli_nkn_show_common_counters(data, cmd, cmd_line, params);
    bail_error(err);
    
    err = cli_nkn_show_http_counters_cb(data, cmd, cmd_line, params);
    bail_error(err);

    err = cli_nkn_show_rtsp_counters_cb(data, cmd, cmd_line, params);
    bail_error(err);
bail:
    return err;
}
#endif

int
cli_nkn_show_scheduler_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(params);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(cmd);

    err = cli_printf(_("Scheduler Configurations\n"));
    bail_error(err);

    err = cli_printf_query(_("	Number of realtime threads: "
			"#/nkn/nvsd/system/config/rtsched/threads#\n"));
    bail_error(err);

    err = cli_printf_query(_("	Number of deadline threads: "
			"#/nkn/nvsd/system/config/sched/threads#\n"));
    bail_error(err);

    err = cli_printf_query(_("	Number of deadline tasks: "
			"#/nkn/nvsd/system/config/sched/tasks#\n"));
    bail_error(err);

    bail:
	return err;
}

int
cli_nkn_show_common_counters(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    bn_binding *binding = NULL;
    uint32 tmpval = 0;
    uint64 lval = 0,lval1 = 0 ;
    uint64 ltunnelbyte = 0 , ltotalbyte = 0, rtotalbyte = 0, rvodtotalbyte = 0;
    uint32 code = 0;
    const char *port_id = NULL;
    tstr_array *port_ids = NULL;
    const char *namespace_id = NULL;
    tstr_array *namespace_ids = NULL;
    uint32 num_samples = 0 , i = 0;
    uint64_t total_conns = 0, total_rtsp = 0,
	   ipv4_conx = 0, ipv6_conx = 0, total_server_conx = 0;
    uint32 total_http = 0, total_http_ipv6 = 0;;
    float32 totalbw = 0;
    uint64 totalbwbps = 0;
    const char *bname ="/stats/state/chd/intf_util/node/\\/net\\/interface\\/global\\/state\\/stats\\/rxtx\\/bytes_per_sec/last/value";

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = mdc_get_binding(cli_mcc, &code, NULL, false, &binding, 
            "/stat/nkn/nvsd/num_connections", NULL);
    bail_error(err);
    if (binding) {
        err = bn_binding_get_uint32(binding, ba_value, NULL, &total_http);
        bail_error(err);
        bn_binding_free(&binding);
    }

    err = mdc_get_binding(cli_mcc, &code, NULL, false, &binding,
            "/stat/nkn/nvsd/num_conx_ipv6", NULL);
    bail_error(err);
    if (binding) {
        err = bn_binding_get_uint32(binding, ba_value, NULL, &total_http_ipv6);
        bn_binding_free(&binding);
        bail_error(err);
    }
    err = mdc_get_binding(cli_mcc, &code, NULL, false, &binding, 
            "/stat/nkn/nvsd/num_rtsp_connections", NULL);
    bail_error(err);
    if (binding) {
        err = bn_binding_get_uint64(binding, ba_value, NULL, &total_rtsp);
        bail_error(err);
        bn_binding_free(&binding);
    }

    total_conns = total_rtsp + total_http + total_http_ipv6;


    err = cli_printf(_("%-50s: %llu\n"), "Total number of Open Connections", (long long unsigned int) total_conns);
    bail_error(err);

    bn_binding_free(&binding);
    err = mdc_get_binding(cli_mcc, NULL, NULL, false, &binding,
	    "/stat/nkn/http/server/active/ipv4_conx", NULL);
    bail_error(err);

    err = bn_binding_get_uint64(binding, ba_value, NULL, &ipv4_conx);
    bail_error(err);

    bn_binding_free(&binding);
    err = mdc_get_binding(cli_mcc, NULL, NULL, false, &binding,
	    "/stat/nkn/http/server/active/ipv6_conx", NULL);
    bail_error(err);

    err = bn_binding_get_uint64(binding, ba_value, NULL, &ipv6_conx);
    bail_error(err);

    total_server_conx = ipv6_conx + ipv4_conx;
    err = cli_printf("Total number of Open Server Connections           :"
	    " %lu\n", total_server_conx);
    bail_error(err);

   if(cmd->cc_magic != cid_rtsp_counter_display) {
    err = cli_printf_query(_("%-50s: "
                "#/stat/nkn/nvsd/current/cache_byte_count#"
                " Bytes\n"),"Total Bytes served from RAM cache" );
    bail_error(err);
    }
    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/nvsd/current/origin_byte_count" ,NULL);
    bail_error(err);

    if (binding ) {
        err = bn_binding_get_uint64(binding,ba_value,NULL,&lval);
        bail_error(err);
        bn_binding_free(&binding);
    }

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/nvsd/current/tunnel_byte_count" ,NULL);
    bail_error(err);

    if (binding ) {
        err = bn_binding_get_uint64(binding,ba_value,NULL,&ltunnelbyte);
        bail_error(err);
        bn_binding_free(&binding);
    }

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/nvsd/current/nfs_byte_count" ,NULL);
    bail_error(err);

    if (binding ) {
        err = bn_binding_get_uint64(binding,ba_value,NULL,&lval1);
        bail_error(err);
        bn_binding_free(&binding);
    }

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/nvsd/current/total_rtsp_byte_count" ,NULL);
    bail_error(err);

    if (binding ) {
        err = bn_binding_get_uint64(binding,ba_value,NULL,&rtotalbyte);
        bail_error(err);
        bn_binding_free(&binding);
    }
    
    err = cli_printf(_("%-50s:\n"),"Origin Server Counters");
    bail_error(err);

    if(cmd->cc_magic != cid_rtsp_counter_display) {
    err = cli_printf(_("%-50s: %ld Bytes\n"),"    Total Bytes served from Origin Server",lval+lval1+ltunnelbyte+rtotalbyte);
    bail_error(err);

    err = cli_printf_query(_("%-50s: %ld Bytes\n"),"    Total Bytes served from HTTP Origin Server",lval+ltunnelbyte);              
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/nvsd/current/nfs_byte_count#"" Bytes\n"),
		    "    Total Bytes served from NFS Origin Server");              
    bail_error(err);
    }

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/nvsd/current/total_byte_count" ,NULL);
    bail_error(err);

    if (binding ) {
        err = bn_binding_get_uint64(binding,ba_value,NULL,&ltotalbyte);
        bail_error(err);
        bn_binding_free(&binding);
    }

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
	"/stat/nkn/nvsd/current/total_rtsp_vod_byte_count" ,NULL);
    bail_error(err);

    if (binding ) {
        err = bn_binding_get_uint64(binding,ba_value,NULL,&rvodtotalbyte);
        bail_error(err);
        bn_binding_free(&binding);
    }

    err = cli_printf_query(_("%-50s: %ld Bytes\n"),"    Total Bytes served from RTSP Origin Server",rtotalbyte);              
    bail_error(err);

    if(cmd->cc_magic != cid_rtsp_counter_display) {
    err = cli_printf_query(_("%-50s: " "#/stat/nkn/nvsd/current/disk_byte_count#"" Bytes\n"),
		    "Total Bytes served from Disk cache");
    bail_error(err);
    }

    err = cli_printf_query(_("%-50s: %ld Bytes\n"),"Total Bytes served",ltotalbyte+rtotalbyte+rvodtotalbyte);
    bail_error(err);

#if defined(BZ_10975)
    // BZ 10975
    //BW
    /*
     * get the bandwith directly from chd nodes, instead of mgmgtd nodes,
     * as mgmgtd nodes, again fetches it from statsd nodes, which causes
     * message voilation
     */
    err = mdc_get_binding(cli_mcc,NULL,NULL,false,&binding, bname,NULL);
    bail_error(err);

    if(binding) {
	err = bn_binding_get_uint64(binding,ba_value,NULL,&totalbwbps);
	bail_error(err);
	bn_binding_free(&binding);
	/* convert it bits per seconds, convert it to mega bit per second */
        totalbwbps = totalbwbps * 8;
        totalbw = (totalbwbps/1000000.0f);
    }// else print zero

    //err = cli_printf_query(_("%-50s: %.3f Mbps\n"),"Total Bandwidth",totalbw);
    //bail_error(err);
#endif // BZ_10975

    // TODO Collect statistics on port basis

    /*! TODO : No counter availabe in FQ module */
    err = cli_printf_query(_("%-50s: " "#/stat/nkn/ingest/current/fetch_count# \n"),
		    "Ingest Fetch Count");
    bail_error(err);
#if 0 /*6552 */
    err = cli_printf_query(_("%-50s: " "#/stat/nkn/ingest/current/fetch_bytes# \n"),
		    "Ingest Bytes Fetched");
    bail_error(err);
#endif
    err = cli_printf_query(_("%-50s: " "#/stat/nkn/disk_global/current/read_count# \n"),
		    "Total Disk Read Operations");
    bail_error(err);

    err = cli_printf_query(_("%-50s: "  "#/stat/nkn/disk_global/current/write_count# \n"),
		    "Total Disk Write Operations");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/ssp/num_of_seeks# \n"),
		    "Virtual Player Number of Seeks");
    bail_error(err);
#if 0
    err = cli_printf(_("Virtual Player Number of Profile Changes  : %s\n"),"NA");
    bail_error(err);
#endif

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/ssp/hash_veri_errs# \n"),
		    "Virtual Player Hash Verification Failed Errors");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/num_of_ports# \n"),
		    "Total Number of ports");
    bail_error(err);

    err = mdc_get_binding_children_tstr_array(cli_mcc,NULL,NULL,&port_ids,"/stat/nkn/port",NULL);
    bail_error(err);

    num_samples = tstr_array_length_quick(port_ids);
    for ( i = 0 ; i < num_samples; i++) {
        port_id = tstr_array_get_str_quick(port_ids,i);

        err = mdc_get_binding_fmt(cli_mcc,NULL,NULL,false,&binding,
                NULL,"/stat/nkn/port/%s/total_sessions",port_id);
        bail_error(err);

        err = bn_binding_get_uint32(binding,ba_value,NULL,&tmpval);
        bail_error(err);

        err = cli_printf(_("%-30s%-20s: %d\n"), "Active Connections on Port",port_id,tmpval);
        bail_error(err);
        bn_binding_free(&binding);

    }

    err = mdc_get_binding_children_tstr_array(cli_mcc,NULL,NULL, 
                     &namespace_ids, "/stat/nkn/namespace",NULL);
    bail_error(err);

    num_samples = tstr_array_length_quick(namespace_ids);
    for ( i = 0 ; i < num_samples; i++) {
        namespace_id = tstr_array_get_str_quick(namespace_ids, i);

	if (namespace_id && strcmp(namespace_id, "mfc_probe")) {
	    err = mdc_get_binding_fmt(cli_mcc,NULL,NULL,false,&binding,
		    NULL,"/stat/nkn/namespace/%s/gets", namespace_id);
	    bail_error(err);

	    err = bn_binding_get_uint64(binding, ba_value,NULL, &lval1);
	    bail_error(err);

	    err = cli_printf(_("%-34s %-15s: %lu\n"),"Number of Requests on Namespace",
		    namespace_id, lval1);
	    bail_error(err);
	    bn_binding_free(&binding);
	}

    }

bail:
    tstr_array_free(&port_ids);
    tstr_array_free(&namespace_ids);
    bn_binding_free(&binding);
    return err;
}
int cli_nkn_launch_program_bg(const char *abs_path, const tstr_array *argv,
		const char *working_dir,
		clish_termination_handler_func termination_handler,
		void *termination_handler_data)
{
	int err = 0;
	uint32_t i;
	uint32_t param_count = 0;
	tstring *stdout_output = NULL;
	lc_launch_params *lp = NULL;
	lc_launch_result lr;

	UNUSED_ARGUMENT(working_dir);
	UNUSED_ARGUMENT(termination_handler);
	UNUSED_ARGUMENT(termination_handler_data);

	/* Initialize launch params */

	err = lc_launch_params_get_defaults(&lp);
	bail_error_null(err, &lp);

	param_count = tstr_array_length_quick(argv);

	err = ts_new_str(&(lp->lp_path), abs_path);
	bail_error(err);

	err = tstr_array_new(&(lp->lp_argv), 0);
	bail_error(err);
	
	err = tstr_array_insert_str(lp->lp_argv, 0, abs_path);
	bail_error(err);

	for(i = 1; i < param_count; i++) {
		const char *param_value = NULL;
		param_value = tstr_array_get_str_quick(argv, i);
		if(!param_value)
			break;
		err = tstr_array_insert_str(lp->lp_argv, i, param_value);
		bail_error(err);
	}

	lp->lp_wait = false;
	lp->lp_env_opts = eo_preserve_all;
	lp->lp_log_level = LOG_INFO;

	lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture_nowait;
	lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture_nowait;

	err = lc_launch(lp, &lr);
	bail_error(err);

	//stdout_output = lr.lr_captured_output;
	//lc_log_basic(LOG_NOTICE, _(" Counter log file: %s \n"), ts_str(stdout_output));

bail: 
	return err;
	ts_free(&stdout_output);
	lc_launch_params_free(&lp);



}

/* ------------------------------------------------------------------------ */
static int
cli_run_ethflash(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    int num_params = 0;
    int count_value = 0;
    const char *loop_count = ETHFLASH_LOOP_COUNT;
    tstr_array *cmd_params = NULL;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(data);

    /* Get the number of parameters first */
    num_params = tstr_array_length_quick(cmd_line);

    if (num_params == 4) {
        /* Get the loop count if it exists in the parameter */
	loop_count = tstr_array_get_str_quick(params, 0);
	if (!loop_count)
	    loop_count = ETHFLASH_LOOP_COUNT;
    }

    /* Validate loop count */
    count_value = atoi(loop_count);

    if ((count_value < 1) || (count_value > MAX_LOOP_COUNT))
    {
            cli_printf_error(_("error: invalid count (%s) (valid range 1-%d)"),
	                     loop_count, MAX_LOOP_COUNT);
            goto bail;
    }

    /* Create the param list for the system call */
    /* Passing 5 the number of times to cycle through the ports */
    err = tstr_array_new_va_str(&cmd_params, NULL, 2,
			"/opt/nkn/bin/ethflash", loop_count);

   /* Run the ethflash script that should flash the ethernet ports in order */
   err = nkn_clish_run_program_fg("/opt/nkn/bin/ethflash", cmd_params, NULL, NULL, NULL);
   bail_error(err);

bail:
    return err;
}

int
cli_nkn_set_sched_threads_value(void *data, const cli_command *cmd,
		                        const tstr_array *cmd_line,
			                const tstr_array *params)
{
	int err = 0;
	uint32 code = 0;
	tstring *msg = NULL;
	const char *no_of_threads = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	switch(cmd->cc_magic)
	{
	case cid_reset_sched:
		err = mdc_set_binding(cli_mcc,
				&code,
				&msg,
				bsso_reset,
				bt_uint32,
				"0",
				"/nkn/nvsd/system/config/sched/threads");
		bail_error(err);
		break;
	case cid_set_sched:
		no_of_threads = tstr_array_get_str_quick(params, 0);
		bail_null(no_of_threads);
		err = mdc_set_binding(cli_mcc,
				&code,
				&msg,
				bsso_modify,
				bt_uint32,
				no_of_threads,
				"/nkn/nvsd/system/config/sched/threads");
		bail_error(err);
		if ( code != 0){
			cli_printf(_("Error:%s\n"), ts_str(msg));
		}
		break;
	}
	cli_printf("warning: if command successful, please restart "
		"mod-delivery service for change to take effect\n");
bail:
	ts_free(&msg);
	return err;
}

int
cli_nkn_space_agent_set_cfg(void *data, const cli_command *cmd,
		                        const tstr_array *cmd_line,
			                const tstr_array *params)
{
	int err = 0;
	const char *port = NULL;
	str_value_t sock_pair = {0};
	uint64_t port_num = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd_line);

	switch(cmd->cc_magic)
	{
	case cid_set_port:
		port = tstr_array_get_str_quick(params, 0);
		bail_null(port);

		port_num = strtoul(port, NULL, 0);
		if((port_num == 0 )|| (port_num > 65535)) {
		    cli_printf_error("Invalid port number");
		    goto bail;
		}
		/* Check if the number is valid */

		snprintf(sock_pair, sizeof(sock_pair), "%s:10.84.77.10:22", port );
		err = mdc_set_binding(cli_mcc,
				NULL,
				NULL,
				bsso_modify,
				bt_string,
				sock_pair,
				"/pm/process/ssh_tunnel/launch_params/11/param");
		bail_error(err);
		break;
	case cid_reset_port:
		err = mdc_set_binding(cli_mcc,
				NULL,
				NULL,
				bsso_reset,
				bt_string,
				"8022:10.84.77.10:22",
				"/pm/process/ssh_tunnel/launch_params/11/param");
		bail_error(err);
		break;

	}
	cli_printf("warning: if command successful, please restart "
		"junos-agent service for change to take effect\n");
bail:
	return err;
}


static int cli_reg_nkn_cmm_cmds(const lc_dso_info *info, const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	CLI_CMD_NEW;
	cmd->cc_words_str = 	"show cluster-manager";
	cmd->cc_help_descr = 	N_("Show Cluster manager status");
	cmd->cc_help_term = 	N_("Show Cluster manager status");
	cmd->cc_flags = 	ccf_terminal;
	cmd->cc_capab_required = ccp_query_basic_curr;
	cmd->cc_exec_callback = cli_nkn_show_cmm_status;
	cmd->cc_revmap_type = 	crt_none;
	cmd->cc_exec_data = 	(void *) 1;
	cmd->cc_magic =	1;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 	"show cluster-manager iterations";
	cmd->cc_help_descr = 	N_("Show cluster manager status continuously.");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 	"show cluster-manager iterations *";
	cmd->cc_help_exp = 	N_("<iterations>");
	cmd->cc_help_exp_hint = N_("Number of times to iterate or loop the display");
	cmd->cc_flags = 	ccf_terminal;
	cmd->cc_capab_required = ccp_query_basic_curr;
	cmd->cc_exec_callback = cli_nkn_show_cmm_status;
	cmd->cc_revmap_type = 	crt_none;
	cmd->cc_exec_data = 	NULL;
	cmd->cc_magic =	1;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 	"show cluster-manager iterations * nkncmmstatus";
	cmd->cc_help_descr = 	N_("Show cluster manager for nkncmmstatus.");
	cmd->cc_flags = 	ccf_terminal;
	cmd->cc_capab_required = ccp_query_basic_curr;
	cmd->cc_exec_callback = cli_nkn_show_cmm_status;
	cmd->cc_revmap_type = 	crt_none;
	cmd->cc_exec_data = 	NULL;
	cmd->cc_magic =	2;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 	"show cluster-manager iterations nkncmmstatus";
	cmd->cc_help_descr = 	N_("Show cluster manager status continuously.");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 	"show cluster-manager iterations nkncmmstatus -r";
	cmd->cc_help_descr = 	N_("Show cluster manager for nkncmmstatus.");
	cmd->cc_flags = 	ccf_terminal;
	cmd->cc_capab_required = ccp_query_basic_curr;
	cmd->cc_exec_callback = cli_nkn_show_cmm_status;
	cmd->cc_revmap_type = 	crt_none;
	cmd->cc_exec_data = 	NULL;
	cmd->cc_magic =	3;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 	"show cluster-manager iterations * nkncmmstatus -r";
	cmd->cc_help_descr = 	N_("Show cluster manager for nkncmmstatus.");
	cmd->cc_flags = 	ccf_terminal;
	cmd->cc_capab_required = ccp_query_basic_curr;
	cmd->cc_exec_callback = cli_nkn_show_cmm_status;
	cmd->cc_revmap_type = 	crt_none;
	cmd->cc_exec_data = 	NULL;
	cmd->cc_magic =	4;
	CLI_CMD_REGISTER;

bail:
	return err;
}

int
cli_nkn_show_cmm_status(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *iterator = "1"; // default run once
	int32 status = 0;

	UNUSED_ARGUMENT(cmd_line);

	if (data == NULL) {
		iterator = tstr_array_get_str_quick(params, 0);
	}

	if (cmd->cc_magic == 1)
	{
		err = lc_launch_quick_status(&status, NULL, false, 4,
			"/opt/nkn/bin/nkncmmstatus",
			"/opt/nkn/bin/nkncmmstatus",
			"-i", iterator);
		bail_error(err);
	}else if (cmd->cc_magic == 2)
	{
		err = lc_launch_quick_status(&status, NULL, false, 5,
			"/opt/nkn/bin/nkncmmstatus",
			"/opt/nkn/bin/nkncmmstatus",
			"-i", iterator, "nkncmmstatus");
		bail_error(err);
	}else if (cmd->cc_magic == 3)
	{
		err = lc_launch_quick_status(&status, NULL, false, 4,
			"/opt/nkn/bin/nkncmmstatus",
			"/opt/nkn/bin/nkncmmstatus",
			"nkncmmstatus", "-r");
		bail_error(err);
	}else if (cmd->cc_magic == 4)
	{
		err = lc_launch_quick_status(&status, NULL, false, 6,
			"/opt/nkn/bin/nkncmmstatus",
			"/opt/nkn/bin/nkncmmstatus",
			"-i", iterator, "nkncmmstatus", "-r");
		bail_error(err);
	}
bail:
	return err;
}

/* ------------------------------------------------------------------ */
static int
cli_tech_supp_fix_fallback(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	int32 status = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	/* Launch the script that would copy the backed up
	 * config files to a new copy that has the nokeenamfd name
	 * in addition to the one with junipermfc
	 */

	err = lc_launch_quick_status(&status, NULL, false, 1, 
			"/opt/nkn/bin/fix-fallback.sh");
	bail_error(err);
bail:
	return err;
}

static int
cli_nkn_run_internal_cmd(void *data, 
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	int status = 0;
	int32 pid = 0;
	char *c_pid = NULL;
	tstring *t_pid = NULL;
	const char *gcmd = NULL;
	int glen = 0;
	bn_binding *binding = NULL;
	tstring *t_out = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	gcmd = tstr_array_get_str_quick(params, 0);
	glen = strlen(gcmd);

	err = mdc_get_binding(cli_mcc, NULL, NULL, false, &binding, "/pm/monitor/process/nvsd/pid", NULL);
	bail_error(err);

	if (binding) {
		err = bn_binding_get_int32(binding, ba_value, NULL, &pid);
		bail_error(err);
	}
	else {
		err = lc_launch_quick_status(&status, &t_pid, false, 2,
				"/sbin/pidof",
				"nvsd");
		bail_error(err);

		if (t_pid && ts_length(t_pid) > 0) {
			c_pid = smprintf("%s", ts_str(t_pid));
		} else {
			cli_printf_error(_("No nvsd running or couldnt find the pid.\n"));
			goto bail;
		}

	}

	err = lf_write_file("/vtmp/gdb.script", 0666, tstr_array_get_str_quick(params, 0), true, glen);
	bail_error(err);

	c_pid = smprintf("%d", pid);

	err = lc_launch_quick_status(&status, &t_out, false,6,
			"/usr/bin/gdb",
			"--batch-silent",
			"-p",
			c_pid,
			"-x",
			"/vtmp/gdb.script");
	bail_error(err);
	lf_remove_file("/vtmp/gdb.script");

	if (t_out && ts_length(t_out) > 0)
		cli_printf(_("%s\n"), ts_str(t_out));

bail:
	ts_free(&t_out);
	bn_binding_free(&binding);
	safe_free(c_pid);
	ts_free(&t_pid);
	return err;
}


static int
cli_exe_shell_cmd(void *data,
                const cli_command *cmd,
                const tstr_array *cmd_line,
                const tstr_array *params)

{
	int err = 0;
    	const char *shell_cmd = NULL;
	tstr_array *shell_cmd_parts = NULL;
	const char *cmd_only = NULL;
        tstr_array *lines = NULL;
        const char *line = NULL;
	int num_lines = 0, i = 0;
	tbool found = false;
	int32 status = 0;
    	tstring *ret_output = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	shell_cmd = tstr_array_get_str_quick(params, 0);
        bail_null(shell_cmd);

	err = ts_tokenize_str(shell_cmd, ' ', '\\', '"', 0, &shell_cmd_parts);
        bail_error(err);

	cmd_only = tstr_array_get_str_quick(shell_cmd_parts, 0);

	err = lf_read_file_lines(WHITE_LIST, &lines);
	bail_error(err);

	num_lines = tstr_array_length_quick(lines);

	for ( i = 0 ; i < num_lines; ++i) {
        	line = tstr_array_get_str_quick(lines, i);
        	bail_null(line);
	
		if (strstr(line, cmd_only) != NULL) {
			found = true;
			break;
		} 
	}

	if (found){
		err = lc_launch_quick_status(&status, &ret_output, true, 2, "/opt/nkn/bin/generic_shell_cmd.py", shell_cmd);
	        bail_error(err);
		cli_printf("%s", ts_str(ret_output));
	}else{
		cli_printf_error("error : unsupported system command <%s>", shell_cmd);
	}
bail:
	return err;
}


int
cli_nkn_collect_counters_detail(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	int num_params = 0;
	const char *pattern_str;
	tstr_array *argv = NULL;
	char *name = NULL;
	tbool found = false;
	const char *time_frequency = NULL;
	const char *time_duration = NULL;
	char *file_name = NULL;
	const char *grep_pattern = NULL;
	char *grep_pattern_ptr = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(params);

	/* Get the number of parameters first */
	num_params = tstr_array_length_quick(cmd_line);

	/* Check if it is "show counters internal" */

	if (num_params == 3)
	{
		err = set_fre_sav_dur(
				false , "_", NULL, NULL, NULL, &argv);
		bail_error(err);
		err = nkn_clish_run_program_fg(NKNCNT_BINARY_PATH, argv, NULL, NULL, NULL);
		bail_error(err);
	} else if (num_params == 4)
	{
		pattern_str = tstr_array_get_str_quick(cmd_line, 3);
		err = cli_expansion_option_get_data(cli_set_default, -1, pattern_str, (void **) &name, &found);
		bail_error(err);
		err = set_fre_sav_dur(
				found, pattern_str, NULL, NULL, NULL, &argv);
		bail_error(err);

		err = nkn_clish_run_program_fg(NKNCNT_BINARY_PATH, argv, NULL, NULL, NULL);
		bail_error(err);
	} else if (num_params > 4) {

		pattern_str = tstr_array_get_str_quick(cmd_line, 3);
		err = cli_expansion_option_get_data(cli_set_default, -1, pattern_str,
									(void **) &name, &found);
		bail_error(err);
		if ( lc_is_prefix(tstr_array_get_str_quick(cmd_line, 4) , "frequency", false))
		{
			time_frequency = tstr_array_get_str_quick(cmd_line, 5); /* frequency <*> */
			bail_null_msg ( time_frequency,
					"time frequency for nkn counter looping is null/not valid");
			time_duration = tstr_array_get_str_quick(cmd_line, 7); /* duration <*> */
			bail_null_msg (time_duration,
			"duration for looping nkn counter is null/not valid");
			if (num_params >= 8)
			{
				/* frequency , save and duration is set */
				err = generate_file_name_for_counters(&file_name);
				bail_error(err);
				err = set_fre_sav_dur(
					found, pattern_str, time_frequency,
					time_duration, file_name, &argv);
				bail_error(err);
			} else {
				/* frequency and duration is set */
				err = set_fre_sav_dur(
					found, pattern_str, time_frequency,
					time_duration, NULL, &argv);
				bail_error(err);
			}
		} else if (lc_is_prefix(tstr_array_get_str_quick(cmd_line, 4), "duration", false))
		{
			time_duration = tstr_array_get_str_quick(cmd_line, 5); /* duration <*> */
			bail_null_msg (time_duration,
					"duration for looping nkn counter is null/not valid");
			time_frequency = tstr_array_get_str_quick(cmd_line, 7); /* frequency <*> */
			bail_null_msg (time_frequency,
					"time frequency for nkn counter looping is null/not valid");
			if (num_params >= 8)
			{
				/* frequency , save and duration is set */
				err = generate_file_name_for_counters(&file_name);
				bail_error(err);
				err = set_fre_sav_dur(
					found, pattern_str, time_frequency,
					time_duration, file_name, &argv);
				bail_error(err);

			} else {
				/* frequency and duration is set */
				err = set_fre_sav_dur(
					found, pattern_str, time_frequency,
					time_duration, NULL, &argv);
				bail_error(err);

			}
		} else {
			if (num_params > 5)
			{
				if ( lc_is_prefix(tstr_array_get_str_quick(cmd_line, 5) ,
								"frequency", false))
				{
					time_frequency = tstr_array_get_str_quick(cmd_line, 6); /* frequency <*> */
				 	bail_null_msg (time_frequency,
							"time frequency for nkn counter looping is null/not valid");
					time_duration = tstr_array_get_str_quick(cmd_line, 8); /* duration <*> */
					bail_null_msg (time_duration,
							"duration for looping nkn counter is null/not valid");
					/* frequency , save and duration is set */
					err = generate_file_name_for_counters(&file_name);
					bail_error(err);
					err = set_fre_sav_dur(
						found, pattern_str, time_frequency,
						time_duration, file_name, &argv);
					bail_error(err);
				} else if ( lc_is_prefix(tstr_array_get_str_quick(cmd_line,5),
								"duration", false))
				{
					time_duration = tstr_array_get_str_quick(cmd_line, 6); /* duration <*> */
					bail_null_msg (time_duration,
							"duration for looping nkn counter is null/not valid");
					time_frequency = tstr_array_get_str_quick(cmd_line, 8); /* frequency <*> */
					bail_null_msg (time_frequency,
							"time frequency for nkn counter looping is null/not valid");
					/* frequency , save and duration is set */
					err = generate_file_name_for_counters(&file_name);
					bail_error(err);
					err = set_fre_sav_dur(
						found, pattern_str, time_frequency,
						time_duration, file_name, &argv);
					bail_error(err);
				}
			}
		}
		lc_log_basic(LOG_NOTICE,_("Counter data dump filename: %s\n"), file_name);
		/*BUG  7146 - Moving the launch of nkncnt as background to mgmtd context.
		 * Currently because it is in the CLI context, when CLI exits, the nkncnt gets killed
		 */
		if(pattern_str) {
		    if(found) {
			grep_pattern = "";
		    }else {
			grep_pattern_ptr = smprintf("%s", pattern_str);
			grep_pattern = grep_pattern_ptr;
		    }
		}
		err = mdc_send_action_with_bindings_str_va(cli_mcc,
			NULL, NULL,
			"/nkn/debug/generate/collect-counters", 4,
			"frequency", bt_string, time_frequency,
			"duration", bt_string, time_duration,
			"filename", bt_string, file_name,
			"pattern", bt_string, grep_pattern);
		bail_error(err);
	}

bail:
    safe_free(file_name);
    safe_free(name);
    safe_free(grep_pattern_ptr);
    tstr_array_free(&argv);
    return err;

} /* end of cli_nkn_show_counters_internal_cb() */

/*---------------------------------------------------------------------------*/
static int
cli_space_agent_revmap_cb (
	void *data,
	const cli_command *cmd,
	const bn_binding_array *bindings,
	const char *name,
	const tstr_array *name_parts,
	const char *value,
	const bn_binding *binding,
	cli_revmap_reply *ret_reply)
{
	int err =0;
	char *cmd_str = NULL;
	int port = 0;
	int res = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	//UNUSED_ARGUMENT(name);
	UNUSED_ARGUMENT(name_parts);
	UNUSED_ARGUMENT(binding);
	UNUSED_ARGUMENT(bindings);

	if (!strcmp(name, "/nkn/mgmt-if/config/dmi-port")) {

		/* Build the command */
		cmd_str = smprintf("service junos-agent port %s", value);
		bail_null(cmd_str);

		/* Add to the revmap */
		err = tstr_array_append_str_takeover(ret_reply->crr_cmds, &cmd_str);
		bail_error(err);

		err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
		bail_error(err);
	} else if (!strcmp(name, "/pm/process/ssh_tunnel/auto_launch")) {

		if(!strcmp(value, "true")) {

			cmd_str = smprintf("service junos-agent enable");
			bail_null(cmd_str);

			err = tstr_array_append_str_takeover(ret_reply->crr_cmds, &cmd_str);
			bail_error(err);
		}
	}
bail:
	return err;
}

static int
cli_system_nvsd_coredump_revmap_cb (
	void *data,
	const cli_command *cmd,
	const bn_binding_array *bindings,
	const char *name,
	const tstr_array *name_parts,
	const char *value,
	const bn_binding *binding,
	cli_revmap_reply *ret_reply)
{
	int err =0;
	char *cmd_str = NULL;
	tstring *nvsd_coredump = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(name);
	UNUSED_ARGUMENT(name_parts);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	/* Get the coredump flag value */
	err = bn_binding_array_get_value_tstr_by_name(bindings,
			"/nkn/nvsd/system/config/coredump", NULL,
			&nvsd_coredump);
	bail_error_null(err, nvsd_coredump);

	/* Build the command */
	if (!strcmp (ts_str(nvsd_coredump), "1")) // Value 1 means enabled
		cmd_str = smprintf("service snapshot mod-delivery enable\n");
	else  // any other value treat it as disable (could be 0 or 2)
		cmd_str = smprintf("# service snapshot mod-delivery disable\n");
	bail_null(cmd_str);

	/* Add to the revmap */
	err = tstr_array_append_str_takeover(ret_reply->crr_cmds, &cmd_str);
	bail_error(err);

	/* Consume all nodes */
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, 
				"/nkn/nvsd/system/config/coredump");
	bail_error(err);

bail:
	ts_free(&nvsd_coredump);
	return err;
}

static int
cli_system_nvsd_mod_dmi_revmap_cb (
        void *data,
        const cli_command *cmd,
        const bn_binding_array *bindings,
        const char *name,
        const tstr_array *name_parts,
        const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply)
{
        int err =0;
        char *cmd_str=NULL;
        tstring *nvsd_mod_dmi = NULL;

        UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(cmd);
        UNUSED_ARGUMENT(name);
        UNUSED_ARGUMENT(name_parts);
        UNUSED_ARGUMENT(value);
        UNUSED_ARGUMENT(binding);
        
        cmd_str=(char *)malloc(255 * sizeof(char));
        bail_null(cmd_str);
        memset(cmd_str,'\0',255);

        /* Get the coredump flag value */
        err = bn_binding_array_get_value_tstr_by_name(bindings,
                        "/nkn/nvsd/system/config/mod_dmi", NULL,
                        &nvsd_mod_dmi);
        bail_error_null(err, nvsd_mod_dmi);

        /* Build the command */
        if (!strcmp (ts_str(nvsd_mod_dmi), "true")) // Value 1 means enabled
                snprintf(cmd_str,255,"service start mod-dmi\n");
        else  // any other value treat it as disable (could be 0 or 2)
                snprintf(cmd_str,255,"# service stop mod-dmi\n");

        /* Add to the revmap */
        err = tstr_array_append_str_takeover(ret_reply->crr_cmds, &cmd_str);
        bail_error(err);

        /* Consume all nodes */
        err = tstr_array_append_sprintf(ret_reply->crr_nodes,
                                "/nkn/nvsd/system/config/mod_dmi");
        bail_error(err);

bail:
        safe_free(cmd_str);
        ts_free(&nvsd_mod_dmi);
        return err;
}

static int
cli_nkn_change_service_status(void *data, const cli_command *cmd,
            const tstr_array *cmd_line, const tstr_array *params)
{
    int err = 0;
    tstring *proc_name = NULL;
    char *name = NULL;
    tbool found = false;
    int ret_val = 0;
    tbool ret_ambiguous;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);
    switch(cmd->cc_magic) {
	case  service_running_status_enable:

	    err = tstr_array_get(params, 0, &proc_name);
	    bail_error_null(err, proc_name);

	    err = cli_expansion_option_get_data_ex(cli_nkn_processes, -1,
		    ts_str(proc_name), (void**) &name, &found, &ret_ambiguous);
	    bail_error(err);
	    if ( !found || (name == NULL)) {
		cli_printf_error(_("Unknown module name."));
		goto bail;
	    }

	    if (ret_ambiguous) {
		cli_printf_error(_("Ambiguous command \"%s\"."),
			tstr_array_get_str_quick(params, 0));
		cli_printf("\ntype \"%s %s %s?\" for help",
			tstr_array_get_str_quick(cmd_line, 0),
			tstr_array_get_str_quick(cmd_line, 1),
			tstr_array_get_str_quick(cmd_line, 2));
		goto bail;
	    }
	    err = mdc_set_binding(cli_mcc, NULL, NULL,
		    bsso_modify,
		    bt_bool,
		    "true",
		    name);
	    bail_error(err);
#if 0
	    if (!strcmp(name,  "mod_ssl")) {
		err = mdc_set_binding(cli_mcc, NULL, NULL,
			bsso_modify,
			bt_bool,
			"true",
			"/nkn/nvsd/system/config/mod_ssl");
		bail_error(err);
	    } else if (!strcmp(name, "mod_ftp")) {
		err = mdc_set_binding(cli_mcc, NULL, NULL,
			bsso_modify,
			bt_bool,
			"true",
			"/nkn/nvsd/system/config/mod_ftp");
		bail_error(err);
	    } else if (!strcmp(name, "mod_crawler")) {
		err = mdc_set_binding(cli_mcc, NULL, NULL,
			bsso_modify,
			bt_bool,
			"true",
			"/nkn/nvsd/system/config/mod_crawler");
		bail_error(err);
	    }
#endif
	    break;

	case  service_running_status_disable:
	    err = tstr_array_get(params, 0, &proc_name);
	    bail_error_null(err, proc_name);

	    err = cli_expansion_option_get_data_ex(cli_nkn_processes, -1,
		    ts_str(proc_name), (void**) &name, &found, &ret_ambiguous);
	    bail_error(err);
	    if ( !found || (name == NULL)) {
		cli_printf_error(_("Unknown module name."));
		goto bail;
	    }

	    if (ret_ambiguous) {
		cli_printf_error(_("Ambiguous command \"%s\"."),
			tstr_array_get_str_quick(params, 0));
		cli_printf("\ntype \"%s %s %s?\" for help",
			tstr_array_get_str_quick(cmd_line, 0),
			tstr_array_get_str_quick(cmd_line, 1),
			tstr_array_get_str_quick(cmd_line, 2) );
		goto bail;
	    }
	    err = mdc_set_binding(cli_mcc, NULL, NULL,
		    bsso_modify,
		    bt_bool,
		    "false",
		    name);
	    bail_error(err);
#if 0
	    if (!strcmp(name, "mod_ssl")) {
		err = mdc_set_binding(cli_mcc, NULL, NULL,
			bsso_modify,
			bt_bool,
			"false",
			"/nkn/nvsd/system/config/mod_ssl");
		bail_error(err);
	    } else if (!strcmp(name, "mod_ftp")) {
		err = mdc_set_binding(cli_mcc, NULL, NULL,
			bsso_modify,
			bt_bool,
			"false",
			"/nkn/nvsd/system/config/mod_ftp");
		bail_error(err);
	    } else if (!strcmp(name, "mod_crawler")) {
		err = mdc_set_binding(cli_mcc, NULL, NULL,
			bsso_modify,
			bt_bool,
			"false",
			"/nkn/nvsd/system/config/mod_crawler");
		bail_error(err);
	    }
#endif
	    break;
	default:
	    bail_force(lc_err_unexpected_case);
	    break;
    }
bail:
    return err;
}

/*
 * this function is generic function that can be used to trigger any action
 * need uploading uploading a file. action must be specified as cc_exec_data
 * 1. first tries to findout if url needs password or not
 * 2. send a action command to mgmtd to do needed processing i.e. generate a
 *	file, mgmtd must return dump_file binding for uploading the file to
 *	the url provided.
 */
int
cli_file_upload_action(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{

    int err = 0;
    const char *remote_url = NULL;

    remote_url = tstr_array_get_str_quick(params, 0);

    bail_null(remote_url);
    bail_null(data);

    if (clish_is_shell()) {
        err = clish_maybe_prompt_for_password_url(remote_url,
		cli_file_upload_action_finish,
		cmd,
		cmd_line,
		params,
		data);
        bail_error(err);
    } else {
        err = cli_file_upload_action_finish(NULL,
		cpr_ok,
		cmd,
		cmd_line,
		params,
                data,
		NULL);
        bail_error(err);
    }

bail:
    return err;
}

/*
 * This function is called as callback from cli_file_upload_action(), this is
 * responsible for sending the action to mgmgtd along with the password and
 * remote url and uploading the file to provided url.
 */
int
cli_file_upload_action_finish(const char *password, clish_password_result result,
                         const cli_command *cmd, const tstr_array *cmd_line,
                         const tstr_array *params, void *data,
                         tbool *ret_continue)
{
    unsigned int ret_err = 0;
    int err = 0;
    unsigned int code = 0;
    const char *action = data, *remote_url = NULL;
    tstring *dump_path = NULL;
    bn_binding_array *bindings = NULL;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);
    UNUSED_ARGUMENT(ret_continue);

    if (result != cpr_ok) {
	goto bail;
    }

    bail_null(data);

    /* send the action  to the mgmtd */
    err = mdc_send_action_with_bindings_and_results(cli_mcc,
            &code, NULL,action, NULL, &bindings);
    bail_error(err);

    /* dump_path must be absolute  path */
    if (bindings) {
        err = bn_binding_array_get_value_tstr_by_name(bindings,
                "dump_path", NULL, &dump_path);
        bail_error(err);
    } else {
	err = 1;
	goto bail;
    }
    if (0 == strcmp(ts_str(dump_path), "")) {
	/* there is no file provided by mgmtd */
	err = 0;
	goto bail;
    }
    remote_url = tstr_array_get_str_quick(params, 0);
    bail_null(remote_url);

    /*  upload the file */
    if (password) {
	err = mdc_send_action_with_bindings_str_va(cli_mcc, &ret_err, NULL,
		"/file/transfer/upload", 3,
		"local_path", bt_string, ts_str(dump_path),
		"remote_url", bt_string, remote_url,
		"password", bt_string, password);
    } else {
	err = mdc_send_action_with_bindings_str_va(cli_mcc, &ret_err, NULL,
		"/file/transfer/upload", 2,
		"local_path", bt_string, ts_str(dump_path),
		"remote_url", bt_string, remote_url);
    }
    bail_error(err);
    bail_error(ret_err);

bail:
    bn_binding_array_free(&bindings);
    ts_free(&dump_path);
    return err;
}
static int cli_service_restart_stat_cb (
	void *data,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params)
{
	int err = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);
	/* Set the auto-launch to enable & trigger the action for restarting mod-stat*/

	err = mdc_set_binding(cli_mcc,
			NULL,
			NULL,
			bsso_modify,
			bt_bool,
			"true",
			"/pm/process/nkncnt/auto_launch");
	bail_error(err);

	err = mdc_send_action_with_bindings_str_va(cli_mcc,
		NULL, NULL,
		"/pm/actions/restart_process", 1,
		"process_name", bt_string, "nkncnt");
	bail_error(err);

bail:
	return err;
}

static int cli_service_restart_stat_cb_with_url (
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
        int err = 0;

     const char *remote_url = NULL;
     UNUSED_ARGUMENT(data);

     remote_url = tstr_array_get_str_quick(params, 0);

     bail_null(remote_url);

     bail_error(err);
     err = mdc_set_binding(cli_mcc,
                           NULL,
                           NULL,
                           bsso_modify,
                           bt_string,
                           remote_url,
                           "/pm/nokeena/nkncnt/config/upload/url");
     bail_error(err);


     /* Set the auto-launch to enable & trigger the action for restarting mod-stat with URL */

     err = mdc_send_action_with_bindings_str_va(cli_mcc,NULL, NULL,
  					        "/pm/actions/restart_process", 1,
				                "process_name", bt_string, "nkncnt");
     bail_error(err);

bail:
    return err;

}

int
cli_reg_named_cmds(const lc_dso_info *info,
		const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str = "ip name-server forward-to";
    cmd->cc_help_descr =    N_("Configure forwarding name server options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "no ip name-server forward-to";
    cmd->cc_help_descr =    N_("Clear certain forwarding name server options");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str = "ip name-server forward-to address";
    cmd->cc_help_descr = N_("Configure IP address of a server to which DNS queries are forwarded to");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "no ip name-server forward-to address";
    cmd->cc_help_descr = N_("Remove a name server address");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"ip name-server forward-to address *";
    cmd->cc_help_exp =		N_("<ip-address>");
    cmd->cc_help_exp_hint =	N_("An Iv4 address");
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_basic_curr;
    cmd->cc_magic =		cid_nameserver_add;
    cmd->cc_exec_callback =	cli_named_fwder;
    cmd->cc_revmap_names =	"/nkn/named/config/forward-to/*/address";
    cmd->cc_revmap_type =	crt_manual;
    cmd->cc_revmap_order =	cro_ip;
    cmd->cc_revmap_cmds =	"ip name-server forward-to address $v$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"no ip name-server forward-to address *";
    cmd->cc_help_exp =		N_("<ip-address>");
    cmd->cc_help_use_comp =	true;
    cmd->cc_comp_type =		cct_matching_values;
    cmd->cc_comp_pattern =	"/nkn/named/config/forward-to/*/address";
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_basic_curr;
    cmd->cc_magic =		cid_nameserver_delete;
    cmd->cc_exec_callback =	cli_named_fwder;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings_va(1,
		    "/nkn/named/config/forward-to/*");
    bail_error(err);

bail:
    return err;
}



int
cli_named_fwder(void *data, const cli_command *cmd,
		const tstr_array *cmd_line, const tstr_array *params)
{
    int err = 0;

    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);
    UNUSED_ARGUMENT(data);

    switch(cmd->cc_magic) {
	case cid_nameserver_add:
	    err = mdc_array_append_single(cli_mcc,
		    "/nkn/named/config/forward-to", "address", bt_ipv4addr,
		    tstr_array_get_str_quick(params, 0), true, NULL,
		    NULL, NULL);
	    bail_error(err);
	    break;
	case cid_nameserver_delete:
	    err = mdc_array_delete_by_value_single(cli_mcc,
		    "/nkn/named/config/forward-to", true, "address",
		    bt_ipv4addr, tstr_array_get_str_quick(params, 0),
		    NULL, NULL, NULL);
	    bail_error(err);
	    break;
	default:
	    bail_force(lc_err_unexpected_case);
    }

bail:
    return err;
}
/* End of cli_nkn_cmds.c */
static int 
cli_reg_nkn_snapshot_override(
        const lc_dso_info *info, 
        const cli_module_context *context)
{
    int err = 0;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);


    err =cli_hide_unhide_commands(cli_file_snapshot_hidden_cmds, sizeof(cli_file_snapshot_hidden_cmds)/sizeof(const char *), false);
    bail_error(err);
bail:
    return err;
}

/****************************************************************************/
/* Function : cli_service_restart_mod_domain_filter                         */
/* Purpose  : Restart the mod-domain-filter process                         */
/****************************************************************************/
static int cli_service_restart_mod_domain_filter (
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
        int err = 0;
	tbool t_enabled = false;
	const char* process1 = NULL;
	const char* process2 = NULL;

        UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(cmd);
        UNUSED_ARGUMENT(cmd_line);
        UNUSED_ARGUMENT(params);

        /*Check whether the mod-domain-filter service is running*/
	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, &t_enabled,
                    "/nkn/l4proxy/config/enable", NULL);
        bail_error(err);

	if (t_enabled) {
		/*if proxyd is enabled restart nvsd first to release the port 80 and restart proxyd next*/
		process1 = "nvsd";
		process2 = "proxyd";
	}
	else {
		/*if proxyd is disabled restart proxyd first to release the port 80 and restart nvsd next*/
		process1 = "proxyd";
                process2 = "nvsd";
	}

        err = mdc_send_action_with_bindings_str_va(cli_mcc,
                NULL, NULL,
                "/pm/actions/restart_process", 1,
                "process_name", bt_string, process1);
        bail_error(err);

	/*Sleep for 5 seconds between the process restarts*/
	sleep(5);

	err = mdc_send_action_with_bindings_str_va(cli_mcc,
                NULL, NULL,
                "/pm/actions/restart_process", 1,
                "process_name", bt_string, process2);
        bail_error(err);

bail:
        return err;
}

int
cli_service_space_agent_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);


    switch(cmd->cc_magic) {
    case cid_agent_enable:
	err = mdc_set_binding(cli_mcc, NULL, NULL,
		    bsso_modify,
		    bt_bool,
		    "true",
		    "/pm/process/ssh_tunnel/auto_launch");
	bail_error(err);
	break;
    case cid_agent_disable:
	err = mdc_set_binding(cli_mcc, NULL, NULL,
		    bsso_modify,
		    bt_bool,
		    "false",
		    "/pm/process/ssh_tunnel/auto_launch");
	bail_error(err);
    }
bail:
    return err;
}


int
cli_nvsd_status_module_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 idx = 0, num_parms = 0;
    const char *module = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    num_parms = tstr_array_length_quick(params);
    for(idx = 0; idx < num_parms; ++idx) {
	tbool ret_found = false;
	void *ret_data = NULL;
	/* check if its present in the list */
	module = tstr_array_get_str_quick(params, idx);
	bail_null(module);

	err = cli_expansion_option_get_data(cli_nkn_nvsd_service_mods, -1,
					module, &ret_data, &ret_found);
	bail_error(err);

	if(!ret_found) {
	    err = cli_printf_error("Illegal module name %s", module);
	    bail_error(err);
	    goto bail;
	}

     }

    /* get if its an include/exculde based on the magivc */
  
    /* If we reached up to this line, then all params are valid interfaces
     */
    num_parms = tstr_array_length_quick(params);
    for(idx = 0; idx < num_parms; idx++) {
	node_name_t mod_nd = {0};
	void *ret_data = NULL;
	tbool ret_found = false;
	const char *nd_name = NULL;
        module = tstr_array_get_str_quick(params, idx);
        bail_null(module);

	err = cli_expansion_option_get_data(cli_nkn_nvsd_service_mods, -1,
					module, &ret_data, &ret_found);
	bail_error_null(err, ret_data);

	nd_name = (const char *)ret_data;

        snprintf(mod_nd, sizeof(mod_nd), "/nkn/nvsd/system/config/init/%s", nd_name);

        /* Create the node */
	if (cmd->cc_magic == cid_mod_include) {
	    err = mdc_set_binding(cli_mcc, NULL, NULL,
		    bsso_modify,
		    bt_bool,
		    "true",
		    mod_nd);
	    bail_error(err);
	}
	/* Delete the node */
	else {
	    err = mdc_set_binding(cli_mcc, NULL, NULL,
		    bsso_modify,
		    bt_bool,
		    "false",
		    mod_nd);
	    bail_error(err);

	}
    }

bail:
    return err;
}
int
cli_ssld_network_print_restart_msg(void *data, const cli_command *cmd,
                       const tstr_array *cmd_line,
                       const tstr_array *params)
{
    int err = 0;

    /* Call the standard cli processing */
    err = cli_std_exec(data, cmd, cmd_line, params);
    bail_error(err);

    /* Print the restart message */
    err = cli_printf("warning: if command successful, please restart mod-ssl service for change to take effect\n");
    bail_error(err);

 bail:
    return(err);
}


int
cli_files_config_upload(void *data, const cli_command *cmd,
                    const tstr_array *cmd_line, const tstr_array *params)
{
    int err = 0;
    const char *remote_url = NULL;

    UNUSED_ARGUMENT(data);

    remote_url = tstr_array_get_str_quick(params, 0);
    bail_null(remote_url);

    if (clish_is_shell()) {
	    err = clish_maybe_prompt_for_password_url
		    (remote_url, cli_files_config_upload_finish, cmd, cmd_line, params,
		     NULL);
	    bail_error(err);
    }
    else {
	    err = cli_files_config_upload_finish(NULL, cpr_ok, cmd, cmd_line, params,
			    NULL, NULL);
	    bail_error(err);
    }

bail:
    return(err);

}


int
cli_files_config_fetch(void *data, const cli_command *cmd,
                    const tstr_array *cmd_line, const tstr_array *params)
{
    int err = 0;
    const char *remote_url = NULL;

    UNUSED_ARGUMENT(data);

    remote_url = tstr_array_get_str_quick(params, 0);
    bail_null(remote_url);

    if (clish_is_shell()) {
	    err = clish_maybe_prompt_for_password_url
		    (remote_url, cli_files_config_fetch_finish, cmd, cmd_line, params,
		     NULL);
	    bail_error(err);
    }
    else {
	    err = cli_files_config_fetch_finish(NULL, cpr_ok, cmd, cmd_line, params,
			    NULL, NULL);
	    bail_error(err);
    }

bail:
    return(err);

}

static int
cli_files_config_fetch_finish(const char *password, clish_password_result result,
                                           const cli_command *cmd, const tstr_array *cmd_line,
                                           const tstr_array *params, void *data,
                                           tbool *ret_continue)
{
    int err = 0;
    const char *remote_url = NULL;
    bn_binding_array *bindings = NULL;
    tstring  *result_str = NULL;
    uint32 code = 0;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;
    bn_request *req = NULL;
    str_value_t cfg_file_name = {0};
    str_value_t cfg_full_file_name = {0};

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(ret_continue);

    if (result != cpr_ok) {
	goto bail;
    }

    lt_time_us cur_time = lt_curr_time_us();

    snprintf(cfg_file_name, sizeof(cfg_file_name), "config_files_%li.tgz", cur_time);
    snprintf(cfg_full_file_name, sizeof(cfg_full_file_name), "%s/%s", service_config_file_path,
			cfg_file_name);

    remote_url = tstr_array_get_str_quick(params, 0);
    bail_null(remote_url);

    err = bn_action_request_msg_create(&req, "/file/transfer/download");
    bail_error(err);

    err = bn_action_request_msg_append_new_binding
	(req, 0, "remote_url", bt_string, remote_url, NULL);
    bail_error(err);

    if (password) {
	err = bn_action_request_msg_append_new_binding
	    (req, 0, "password", bt_string, password, NULL);
	bail_error(err);
    }

    err = bn_action_request_msg_append_new_binding
	(req, 0, "local_dir", bt_string, service_config_file_path, NULL);
    bail_error(err);


    err = bn_action_request_msg_append_new_binding
	(req, 0, "local_filename", bt_string, cfg_file_name, NULL);
    bail_error(err);

    err = bn_action_request_msg_append_new_binding
	(req, 0, "allow_overwrite", bt_bool, "true", NULL);
    bail_error(err);

    /*
     * If we're not in the CLI shell, we won't be displaying progress,
     * so there's no need to track it.
     *
     * XXX/EMT: we should make up our own progress ID, and start and
     * end our own operation, and tell the action about it with the
     * progress_image_id parameter.  This is so we can report errors
     * that happen outside the context of the action request.  But
     * it's not a big deal for now, since almost nothing happens
     * outside of this one action, so errors are unlikely.
     */
    if (clish_progress_is_enabled()) {
	err = bn_action_request_msg_append_new_binding
	    (req, 0, "track_progress", bt_bool, "true", NULL);
	bail_error(err);
	err = bn_action_request_msg_append_new_binding
	    (req, 0, "progress_oper_type", bt_string, "image_download", NULL);
	bail_error(err);
	err = bn_action_request_msg_append_new_binding
	    (req, 0, "progress_erase_old", bt_bool, "true", NULL);
	bail_error(err);
    }

    err = bn_action_request_msg_append_new_binding
	(req, 0, "mode", bt_uint16, "0600", NULL);
    bail_error(err);

    if (clish_progress_is_enabled()) {
	err = clish_send_mgmt_msg_async(req);
	bail_error(err);
	err = clish_progress_track(req, NULL, 0, &ret_err, &ret_msg);
	bail_error(err);
    }
    else {
	err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, &ret_err, &ret_msg);
	bail_error(err);
    }

    if(ret_err) {
	lc_log_basic(LOG_NOTICE, "Downloading service config files failed.");
	goto bail;
    }

    err = mdc_send_action_with_bindings_and_results_va(cli_mcc,
	    &code,
	    NULL,
	    "/nkn/debug/generate/config_files",
	    &bindings,
	    2,
	    "cmd", bt_uint16, "2",
	    "file_name", bt_string , cfg_full_file_name);
    bail_error(err);

    if(bindings) {
	err = bn_binding_array_get_value_tstr_by_name(
		bindings, "Result", NULL, &result_str);
	bail_error(err);
	err = cli_printf("%s", ts_str(result_str));
	bail_error(err);
    }

    if(code)
	lc_log_basic(LOG_NOTICE, "Unable to deploy the config files.");

bail:
    bn_binding_array_free(&bindings);
    lf_remove_file(cfg_full_file_name);
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    ts_free(&result_str);
    return err;
}

static int
cli_nkn_service_revmap_cb(
	void *data,
	const cli_command *cmd,
	const bn_binding_array *bindings,
	const char *name,
	const tstr_array *name_parts,
	const char *value,
	const bn_binding *binding,
	cli_revmap_reply *ret_reply)
{
    int err = 0;
    unsigned int i = 0;
    tstring *rev_cmd = NULL;
    const char *module = NULL;
    tbool service_status = false;

    for (i = 0; i < sizeof(cli_nkn_processes)/sizeof(cli_expansion_option); i++) {
	if((cli_nkn_processes[i].ceo_data != NULL)
		&& (0 == strcmp(name, (char *)cli_nkn_processes[i].ceo_data))) {
	    module = cli_nkn_processes[i].ceo_option;
	    break;
	}
    }

    if (module) {
	if (0 == strcmp(value, "true")) {
	    service_status = true;
	} else {
	    service_status = false;
	}

	err = ts_new_sprintf(&rev_cmd, "service %s %s",
		service_status ? "start" : "stop", module);
	bail_error(err);

	if (rev_cmd != NULL) {
	    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	    bail_error(err);
	}
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
	bail_error(err);
    }

bail:
    ts_free(&rev_cmd);
    return err;
}
