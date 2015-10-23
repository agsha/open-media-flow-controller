/*
 * Filename :   cli_nvsd_diskcache_cmds.c
 * Date     :   2008/12/07
 * Author   :   Dhruva Narasimhan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include <ctype.h>
#include <strings.h>
#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "file_utils.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "proc_utils.h"
#include "libnkncli.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"

/* -------------------------------------------------------------------------- */
cli_expansion_option cli_disk_type_options[] = {
	{"SATA",
	 N_("Cache type SATA"),
	 NULL},

	{"SAS",
	 N_("Cache type SAS"),
	 NULL},

	{"SSD",
	 N_("Cache type SSD"),
	 NULL},

	{NULL, NULL, NULL}
};

enum {
	cid_3ware_controller = 1,
};


enum {
        cc_disk_format = 1,
        cc_disk_format_sb = 2
};

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int 
cli_nvsd_diskcache_cmds_init(
        const lc_dso_info   *info,
        const cli_module_context *context);

int 
cli_nvsd_diskcache_op(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


int
cli_nvsd_diskcache_list(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int cli_nvsd_diskcontroller(
	void *data, 
	const cli_command *cmd, 
	const tstr_array *cmd_line, 
	const tstr_array *params);

int 
cli_diskcache_validate(
        const char *label,
        tbool *ret_valid);

int 
cli_diskcache_get_disks(
        tstr_array **ret_labels,
        tbool hide_display);

static int
cli_diskcache_create(
        const tstr_array *params);
static int
cli_diskcache_delete(
        const tstr_array *params);

int
cli_diskcache_help(
        void *data, 
        cli_help_type help_type,
        const cli_command *cmd, 
        const tstr_array *cmd_line,
        const tstr_array *params, 
        const tstring *curr_word,
        void *context);

int 
cli_diskcache_completion(void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);


int 
cli_diskcache_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_diskcache_stub(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_diskcache_type(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nvsd_diskcache_show_evict(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nvsd_diskcache_show(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nvsd_diskcache_show_freeblock_threshold(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_diskcache_show_cache_tier(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
int
cli_nvsd_diskcache_show_group_read(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_diskcache_show_generic_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int 
cli_nvsd_print_disk_info(
	const char* t_diskname,
	const char* t_diskid,
	uint64_t* p_free_mbytes);

static int 
cli_nvsd_print_disk_info_in_oneline(
	const char* t_diskname,
	const char* t_diskid,
	uint64_t* p_free_mbytes);

static int 
cli_nvsd_diskcache_format(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nvsd_disk_metadata_upload(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_nvsd_diskcache_sas_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_diskcache_sata_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


int
cli_nvsd_diskcache_ssd_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int 
cli_nvsd_diskcache_status_revmap_cb (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int 
cli_nvsd_ramcache_status_revmap_cb (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);


static int
cli_nvsd_diskcache_sas_inwm_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);

static int
cli_nvsd_diskcache_sata_inwm_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);
static int
cli_nvsd_diskcache_ssd_inwm_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);

static int
cli_nvsd_diskcache_dict_inwm_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);

static int
cli_nvsd_diskcache_sas_exwm_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);

static int
cli_nvsd_diskcache_sata_exwm_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);
static int
cli_nvsd_diskcache_ssd_exwm_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);

static int
cli_nvsd_diskcache_sata_timed_evict_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);

static int
cli_nvsd_diskcache_sas_timed_evict_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);

static int
cli_nvsd_diskcache_ssd_timed_evict_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);

int
cli_nvsd_diskcache_evict_wm_external(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int
cli_disk_metadata_upload_finish(const char *password, clish_password_result result,
				const cli_command *cmd, const tstr_array *cmd_line,
				const tstr_array *params, void *data,
				tbool *ret_continue);

/*---------------------------------------------------------------------*/
int 
cli_nvsd_diskcache_cmds_init(
        const lc_dso_info   *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    // TODO: Add GETTEXT stuff here


    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache";
    cmd->cc_help_descr =        N_("Configure media-caches");
    cmd->cc_revmap_order =      cro_media_cache;
    cmd->cc_revmap_callback =   cli_nvsd_ramcache_status_revmap_cb;
    CLI_CMD_REGISTER;

    /*---------------------------------------------------------------------*/

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache evict-thread-frequency";
    cmd->cc_flags =             ccf_hidden;
    cmd->cc_help_descr =        N_("Configure a media-cache");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache evict-thread-frequency *";
    cmd->cc_help_exp =          N_("<seconds>");
    cmd->cc_help_exp_hint =     N_("seconds");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/evict_thread_freq";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction";
    cmd->cc_help_descr =        N_("Configure a media-cache internal watermark");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no media-cache internal-eviction";
    cmd->cc_help_descr =        N_("Negate media-cache internal eviction parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sas";
    cmd->cc_help_descr =        N_("Configure a media-cache internal watermark for SAS");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no media-cache internal-eviction sas";
    cmd->cc_help_descr =        N_("Negate media-cache internal eviction tier parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sas watermark";
    cmd->cc_help_descr =        N_("Configure a media-cache internal watermark for SAS");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sas watermark *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("High watermark Level between 1 to 100");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sas watermark * *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Low watermark Level between 1 to 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/watermark/internal/sas/high";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint16;
    cmd->cc_exec_name2 =        "/nkn/nvsd/diskcache/config/watermark/internal/sas/low";
    cmd->cc_exec_value2 =       "$2$";
    cmd->cc_exec_type2 =        bt_uint16;
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_media_cache;
    cmd->cc_revmap_callback =   cli_nvsd_diskcache_sas_inwm_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sas percent-disks";
    cmd->cc_help_descr =        N_("Keyword to specify percent of disks within a tier chosen for eviction");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sas percent-disks *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Number between 1 to 100 (will be rounded-up to integral number of disks)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/internal/sas/percent-disks";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint16;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sata";
    cmd->cc_help_descr =        N_("Configure a media-cache internal watermark for SATA");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no media-cache internal-eviction sata";
    cmd->cc_help_descr =        N_("Negate media-cache internal eviction tier parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sata watermark";
    cmd->cc_help_descr =        N_("Configure a media-cache internal watermark for SATA");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sata watermark *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("High watermark Level between 1 to 100");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sata watermark * *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Low watermark Level between 1 to 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/watermark/internal/sata/high";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint16;
    cmd->cc_exec_name2 =        "/nkn/nvsd/diskcache/config/watermark/internal/sata/low";
    cmd->cc_exec_value2 =       "$2$";
    cmd->cc_exec_type2 =        bt_uint16;
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_media_cache;
    cmd->cc_revmap_callback =   cli_nvsd_diskcache_sata_inwm_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sata percent-disks";
    cmd->cc_help_descr =        N_("Keyword to specify percent of disks within a tier chosen for eviction");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sata percent-disks *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Number between 1 to 100 (will be rounded-up to integral number of disks)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/internal/sata/percent-disks";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint16;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction ssd";
    cmd->cc_help_descr =        N_("Configure a media-cache internal watermark for SSD");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no media-cache internal-eviction ssd";
    cmd->cc_help_descr =        N_("Negate media-cache internal eviction tier parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction ssd watermark";
    cmd->cc_help_descr =        N_("Configure a media-cache internal watermark for SSD");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction ssd watermark *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("High watermark Level between 1 to 100");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction ssd watermark * *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Low watermark Level between 1 to 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/watermark/internal/ssd/high";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint16;
    cmd->cc_exec_name2 =        "/nkn/nvsd/diskcache/config/watermark/internal/ssd/low";
    cmd->cc_exec_value2 =       "$2$";
    cmd->cc_exec_type2 =        bt_uint16;
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_media_cache;
    cmd->cc_revmap_callback =   cli_nvsd_diskcache_ssd_inwm_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction ssd percent-disks";
    cmd->cc_help_descr =        N_("Keyword to specify percent of disks within a tier chosen for eviction");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction ssd percent-disks *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Number between 1 to 100 (will be rounded-up to integral number of disks)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/internal/ssd/percent-disks";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint16;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sata start-time";
    cmd->cc_help_descr =        N_("Configure the start-time for SATA internal eviction");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no media-cache internal-eviction sata start-time";
    cmd->cc_help_descr =        N_("Reset the start-time for SATA internal eviction");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/internal/watermark/sata/evict_time";
    cmd->cc_exec_type =         bt_time;
    cmd->cc_exec_name2 =        "/nkn/nvsd/diskcache/config/internal/watermark/sata/time_low_pct";
    cmd->cc_exec_type2 =        bt_uint16;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sata start-time *";
    cmd->cc_help_exp =          N_("<hh:mm>");
    cmd->cc_help_exp_hint =     N_("Evict start time in 24 hour format (localtime of MFC)");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sata start-time * watermark";
    cmd->cc_help_descr =        N_("Configure a low watermark for time based internal eviction");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sata start-time * watermark *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Low watermark Level between 1 to 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/internal/watermark/sata/evict_time";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_time;
    cmd->cc_exec_name2 =        "/nkn/nvsd/diskcache/config/internal/watermark/sata/time_low_pct";
    cmd->cc_exec_value2 =       "$2$";
    cmd->cc_exec_type2 =        bt_uint16;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    cmd->cc_revmap_callback =   cli_nvsd_diskcache_sata_timed_evict_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sas start-time";
    cmd->cc_help_descr =        N_("Configure the start-time for SAS internal eviction");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no media-cache internal-eviction sas start-time";
    cmd->cc_help_descr =        N_("Reset the start-time for SAS internal eviction");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/internal/watermark/sas/evict_time";
    cmd->cc_exec_type =         bt_time;
    cmd->cc_exec_name2 =        "/nkn/nvsd/diskcache/config/internal/watermark/sas/time_low_pct";
    cmd->cc_exec_type2 =        bt_uint16;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sas start-time *";
    cmd->cc_help_exp =          N_("<hh:mm>");
    cmd->cc_help_exp_hint =     N_("Evict start time in 24 hour format (localtime of MFC)");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sas start-time * watermark";
    cmd->cc_help_descr =        N_("Configure a low watermark for time based internal eviction");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction sas start-time * watermark *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Low watermark Level between 1 to 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/internal/watermark/sas/evict_time";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_time;
    cmd->cc_exec_name2 =        "/nkn/nvsd/diskcache/config/internal/watermark/sas/time_low_pct";
    cmd->cc_exec_value2 =       "$2$";
    cmd->cc_exec_type2 =        bt_uint16;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    cmd->cc_revmap_callback =   cli_nvsd_diskcache_sas_timed_evict_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction ssd start-time";
    cmd->cc_help_descr =        N_("Configure the start-time for SSD internal eviction");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction ssd start-time *";
    cmd->cc_help_exp =          N_("<hh:mm>");
    cmd->cc_help_exp_hint =     N_("Evict start time in 24 hour format (localtime of MFC)");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no media-cache internal-eviction ssd start-time";
    cmd->cc_help_descr =        N_("Reset the start-time for SSD internal eviction");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/internal/watermark/ssd/evict_time";
    cmd->cc_exec_type =         bt_time;
    cmd->cc_exec_name2 =        "/nkn/nvsd/diskcache/config/internal/watermark/ssd/time_low_pct";
    cmd->cc_exec_type2 =        bt_uint16;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction ssd start-time * watermark";
    cmd->cc_help_descr =        N_("Configure a low watermark for time based internal eviction");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction ssd start-time * watermark *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Low watermark Level between 1 to 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/internal/watermark/ssd/evict_time";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_time;
    cmd->cc_exec_name2 =        "/nkn/nvsd/diskcache/config/internal/watermark/ssd/time_low_pct";
    cmd->cc_exec_value2 =       "$2$";
    cmd->cc_exec_type2 =        bt_uint16;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    cmd->cc_revmap_callback =   cli_nvsd_diskcache_ssd_timed_evict_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction dictionary";
    cmd->cc_help_descr =        N_("Configure a media-cache internal watermark");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction dictionary watermark";
    cmd->cc_help_descr =        N_("Configure a media-cache internal watermark for dictionary based eviction");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction dictionary watermark *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("High watermark Level between 1 to 100");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache internal-eviction dictionary watermark * *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Low watermark Level between 1 to 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/watermark/internal/dict/high";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint16;
    cmd->cc_exec_name2 =        "/nkn/nvsd/diskcache/config/watermark/internal/dict/low";
    cmd->cc_exec_value2 =       "$2$";
    cmd->cc_exec_type2 =        bt_uint16;
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_media_cache;
    cmd->cc_revmap_callback =   cli_nvsd_diskcache_dict_inwm_revmap;
    CLI_CMD_REGISTER;



    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache external-eviction";
    cmd->cc_help_descr =        N_("Configure a media-cache external watermark");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache external-eviction sas";
    cmd->cc_help_descr =        N_("Configure a media-cache external watermark for SAS");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache external-eviction sas watermark";
    cmd->cc_help_descr =        N_("Configure a media-cache external watermark for SAS");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache external-eviction sas watermark *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("High watermark Level between 1 to 100");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache external-eviction sas watermark * *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Low watermark Level between 1 to 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/watermark/external/sas/high";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint16;
    cmd->cc_exec_name2 =        "/nkn/nvsd/diskcache/config/watermark/external/sas/low";
    cmd->cc_exec_value2 =       "$2$";
    cmd->cc_exec_type2 =        bt_uint16;
    cmd->cc_revmap_type =	crt_manual;
    cmd->cc_revmap_order =      cro_media_cache;
    cmd->cc_revmap_callback =   cli_nvsd_diskcache_sas_exwm_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache external-eviction sata";
    cmd->cc_help_descr =        N_("Configure a media-cache external watermark for SATA");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache external-eviction sata watermark";
    cmd->cc_help_descr =        N_("Configure a media-cache external watermark for SATA");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache external-eviction sata watermark *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("High watermark Level between 1 to 100");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache external-eviction sata watermark * *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Low watermark Level between 1 to 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/watermark/external/sata/high";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint16;
    cmd->cc_exec_name2 =        "/nkn/nvsd/diskcache/config/watermark/external/sata/low";
    cmd->cc_exec_value2 =       "$2$";
    cmd->cc_exec_type2 =        bt_uint16;
    cmd->cc_revmap_type =		crt_manual;
    cmd->cc_revmap_order =      cro_media_cache;
    cmd->cc_revmap_callback =   cli_nvsd_diskcache_sata_exwm_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache external-eviction ssd";
    cmd->cc_help_descr =        N_("Configure a media-cache external watermark for SSD");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache external-eviction ssd watermark";
    cmd->cc_help_descr =        N_("Configure a media-cache external watermark for SSD");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache external-eviction ssd watermark *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("High watermark Level between 1 to 100");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache external-eviction ssd watermark * *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Low watermark Level between 1 to 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/watermark/external/ssd/high";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint16;
    cmd->cc_exec_name2 =        "/nkn/nvsd/diskcache/config/watermark/external/ssd/low";
    cmd->cc_exec_value2 =       "$2$";
    cmd->cc_exec_type2 =        bt_uint16;
    cmd->cc_revmap_type =		crt_manual;
    cmd->cc_revmap_order =      cro_media_cache;
    cmd->cc_revmap_callback =   cli_nvsd_diskcache_ssd_exwm_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk";
    cmd->cc_help_descr =        N_("Configure a media-cache");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk upgrade-firmware";
    cmd->cc_help_descr =        N_("Upgrade firmware on Intel SSD drive");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_action_name =  "/nkn/nvsd/diskcache/actions/upgrade-firmware";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk upgrade-firmware download";
    cmd->cc_help_descr =        N_("Download and update firmware for SSD drives");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk upgrade-firmware download *";
    cmd->cc_help_exp =          N_("<url>");
    cmd->cc_help_exp_hint =     N_("Provide the download url for the update firmware");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_action_name =  "/nkn/nvsd/diskcache/actions/upgrade-firmware-generic";
    cmd->cc_exec_name =         "option";
    cmd->cc_exec_value =        "download";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_exec_name2 =        "path";
    cmd->cc_exec_value2 =       "$1$";
    cmd->cc_exec_type2 =        bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk upgrade-firmware download * safe";
    cmd->cc_help_descr =        N_("Update firmware only during a reboot");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_action_name =  "/nkn/nvsd/diskcache/actions/upgrade-firmware-generic";
    cmd->cc_exec_name =         "option";
    cmd->cc_exec_value =        "download-reboot";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_exec_name2 =        "path";
    cmd->cc_exec_value2 =       "$1$";
    cmd->cc_exec_type2 =        bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk cache-tier";
    cmd->cc_help_descr =        N_("Configure a media-cache cache-tier");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk cache-tier sas";
    cmd->cc_help_descr =        N_("Configure a media-cache cache-tier for sas");

    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk cache-tier ssd";
    cmd->cc_help_descr =        N_("Configure a media-cache cache-tier for ssd");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk cache-tier sata";
    cmd->cc_help_descr =        N_("Configure a media-cache cache-tier for sata");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk cache-tier sas admission";
    cmd->cc_help_descr =        N_("Configure a media-cache cache-tier for sas");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk cache-tier ssd admission";
    cmd->cc_help_descr =        N_("Configure a media-cache cache-tier for ssd");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk cache-tier sata admission";
    cmd->cc_help_descr =        N_("Configure a media-cache cache-tier for sata");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk cache-tier sas admission threshold";
    cmd->cc_help_descr =        N_("Configure a media-cache cache-tier for sas");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk cache-tier ssd admission threshold";
    cmd->cc_help_descr =        N_("Configure a media-cache cache-tier for ssd");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk cache-tier sata admission threshold";
    cmd->cc_help_descr =        N_("Configure a media-cache cache-tier for sata");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk cache-tier sata admission threshold *";
    cmd->cc_help_exp =          N_("<interger>");
    cmd->cc_help_exp_hint =     N_("Enter between 5 and 1250");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/sata_adm_thres";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk cache-tier ssd admission threshold *";
    cmd->cc_help_exp =          N_("<interger>");
    cmd->cc_help_exp_hint =     N_("Enter between 5 and 1250");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/ssd_adm_thres";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk cache-tier sas admission threshold *";
    cmd->cc_help_exp =          N_("<interger>");
    cmd->cc_help_exp_hint =     N_("Enter between 5 and 1250");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/sas_adm_thres";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk mount-new";
    cmd->cc_help_descr =        N_("Mount for new disk for disk cache");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/nkn/nvsd/diskcache/actions/newdisk";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     "";
    cmd->cc_flags =             ccf_terminal  | ccf_prefix_mode_root;
    cmd->cc_capab_required =    ccp_set_priv_curr;
    cmd->cc_help_callback =     cli_diskcache_help;
    cmd->cc_comp_callback =     cli_diskcache_completion;
    cmd->cc_exec_callback =     cli_diskcache_enter_mode;
    CLI_CMD_REGISTER;

# if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk * meta-data";
    cmd->cc_help_descr =        N_("Configure media-cache metadata compression");
    cmd->cc_flags = 		ccf_unavailable;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk * meta-data compression";
    cmd->cc_help_descr =        N_("Config the metadata compression  percentage value");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk * meta-data compression *";
    cmd->cc_help_exp =          N_("<positive number>");
    cmd->cc_help_exp_hint =     N_("Config the metadata compression  percentage value");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/disk_id/$1$/metadata_comp_per";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint16;
    cmd->cc_revmap_order =      cro_media_cache; 
    cmd->cc_revmap_callback =   cli_nvsd_comp_revmap;
    cmd->cc_revmap_names =      "/nkn/nvsd/diskcache/config/disk_id/*/metadata_comp_per";
    CLI_CMD_REGISTER;
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no media-cache";
    cmd->cc_help_descr =        N_("Negate media-cache parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no media-cache disk";
    cmd->cc_help_descr =        N_("Negate media-cache parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no media-cache disk *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     "";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_help_callback =     cli_diskcache_help;
    cmd->cc_comp_callback =     cli_diskcache_completion;
    cmd->cc_exec_callback =     cli_diskcache_stub;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk * no";
    cmd->cc_help_descr =        N_("Clear certain media-cache settings");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_req_prefix_count =  3;
    CLI_CMD_REGISTER;

    /*---------------------------------------------------------------------*/

    /*---------------------------------------------------------------------*/

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk * disk-type";
    cmd->cc_help_descr =        N_("Configure disk's type");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk * disk-type sata";
    cmd->cc_help_descr =        N_("Set the type of the disk to SATA");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_diskcache_type;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk * disk-type sas";
    cmd->cc_help_descr =        N_("Set the type of the disk to SAS");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_diskcache_type;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk * disk-type ssd";
    cmd->cc_help_descr =        N_("Set the type of the disk to SSD");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_diskcache_type;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk * status";
    cmd->cc_help_descr =        N_("Configure disk status");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk * status active";
    cmd->cc_help_descr =        N_("Activate media-cache");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/nkn/nvsd/diskcache/actions/activate";
    cmd->cc_exec_name =         "name";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_name2 =        "activate";
    cmd->cc_exec_type2 =	bt_bool;
    cmd->cc_exec_value2 =       "true";
    cmd->cc_revmap_order =      cro_media_cache;
    cmd->cc_revmap_callback =   cli_nvsd_diskcache_status_revmap_cb;
    cmd->cc_revmap_names =      "/nkn/nvsd/diskcache/config/disk_id/*/activated";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk * status inactive";
    cmd->cc_help_descr =        N_("Deactivate media-cache");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/nkn/nvsd/diskcache/actions/activate";
    cmd->cc_exec_name =         "name";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_name2 =        "activate";
    cmd->cc_exec_type2 =	bt_bool;
    cmd->cc_exec_value2 =       "false";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk * cache";
    cmd->cc_help_descr =        N_("Configure cache settings");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk * cache enable";
    cmd->cc_help_descr =        N_("Enable media-cache");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/nkn/nvsd/diskcache/actions/cacheable";
    cmd->cc_exec_name =         "name";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_name2 =        "cacheable";
    cmd->cc_exec_type2 =	bt_bool;
    cmd->cc_exec_value2 =       "true";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk * cache disable";
    cmd->cc_help_descr =        N_("Disable media-cache");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/nkn/nvsd/diskcache/actions/cacheable";
    cmd->cc_exec_name =         "name";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_name2 =        "cacheable";
    cmd->cc_exec_type2 =	bt_bool;
    cmd->cc_exec_value2 =       "false";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk * format";
    cmd->cc_help_descr =        N_("Format a media cache");
    cmd->cc_magic       =       cc_disk_format;
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_callback =     cli_nvsd_diskcache_format;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk * format small-block";
    cmd->cc_help_descr =        N_("Format disk to optimize for a very large number of objects");
    cmd->cc_magic    	=	cc_disk_format_sb; 
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_callback =     cli_nvsd_diskcache_format;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk * repair";
    cmd->cc_help_descr =        N_("Repair a media cache");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_action_name =  "/nkn/nvsd/diskcache/actions/repair";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_exec_name =         "diskname";
    CLI_CMD_REGISTER;

#if 0 /*Bug 9753*/
    /*-------------------------------------------------------------------------------*/
    /* BZ 2903 : add block free thresholds for individual tiers */
    /*-------------------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache free-block ";
//    cmd->cc_flags =             ccf_hidden; /*Bug# 9754 */
    cmd->cc_help_descr =        N_("Configure a media-cache internal free-block parameter");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache free-block threshold";
//    cmd->cc_flags =             ccf_hidden; /*Bug# 9754 */
    cmd->cc_help_descr =        N_("Configure a media-cache internal free-block threshold");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache free-block threshold sas";
    cmd->cc_help_descr =        N_("Configure a media-cache internal free-block threshold for SAS");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache free-block threshold sas *";
    cmd->cc_help_exp =          N_("<percent>");
    cmd->cc_help_exp_hint =     N_("0, 25, 50, 75, 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/free-block/threshold/internal/sas";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_int8;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache free-block threshold sata";
    cmd->cc_help_descr =        N_("Configure a media-cache internal free-block threshold for SATA");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache free-block threshold sata *";
    cmd->cc_help_exp =          N_("<percent>");
    cmd->cc_help_exp_hint =     N_("0, 25, 50, 75, 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/free-block/threshold/internal/sata";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_int8;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache free-block threshold ssd";
    cmd->cc_help_descr =        N_("Configure a media-cache internal free-block threshold for SSD");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache free-block threshold ssd *";
    cmd->cc_help_exp =          N_("<percent>");
    cmd->cc_help_exp_hint =     N_("0, 25, 50, 75, 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/free-block/threshold/internal/ssd";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_int8;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    /*---------------------------------------------------------------------*/
#endif

    /*-------------------------------------------------------------------------------*/
    /* BZ 6421 : add block sharing thresholds for individual tiers */
    /*-------------------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk block-sharing";
    cmd->cc_flags =             ccf_hidden;
    cmd->cc_help_descr =        N_("Configure a media-cache internal block-sharing parameter");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk block-sharing threshold";
    cmd->cc_flags =             ccf_hidden;
    cmd->cc_help_descr =        N_("Configure a media-cache internal block-sharing threshold");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk block-sharing threshold sas";
    cmd->cc_help_descr =        N_("Configure a media-cache internal block-sharing threshold for SAS");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk block-sharing threshold sas *";
    cmd->cc_help_exp =          N_("<percent>");
    cmd->cc_help_exp_hint =     N_("0, 25, 50, 75, 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/block-sharing/threshold/internal/sas";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_int8;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk block-sharing threshold sata";
    cmd->cc_help_descr =        N_("Configure a media-cache internal block-sharing threshold for SATA");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk block-sharing threshold sata *";
    cmd->cc_help_exp =          N_("<percent>");
    cmd->cc_help_exp_hint =     N_("0, 25, 50, 75, 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/block-sharing/threshold/internal/sata";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_int8;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk block-sharing threshold ssd";
    cmd->cc_help_descr =        N_("Configure a media-cache internal block-sharing threshold for SSD");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk block-sharing threshold ssd *";
    cmd->cc_help_exp =          N_("<percent>");
    cmd->cc_help_exp_hint =     N_("0, 25, 50, 75, 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/block-sharing/threshold/internal/ssd";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_int8;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    /*---------------------------------------------------------------------*/

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"media-cache disk meta-data";
    cmd->cc_help_descr =        N_("Upload disk meta-data");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"media-cache disk meta-data upload";
    cmd->cc_help_descr =        N_("Upload disk meta-data");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"media-cache disk meta-data upload *";
    cmd->cc_help_exp =          N_("<scp://username:password@hostname/path>");
    cmd->cc_flags = 		ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_callback =     cli_nvsd_disk_metadata_upload;
    CLI_CMD_REGISTER;

    /*---------------------------------------------------------------------*/

    /*---------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str = 	"media-cache disk dictionary";
    cmd->cc_help_descr = 	N_("Configure Dictionary parameters for disk caches");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"media-cache disk dictionary pre-load";
    cmd->cc_help_descr = 	N_("Configure dictionary pre-load options");
    CLI_CMD_REGISTER;

    /*
     * following command creates "/config/nkn/cache.no_preread" in the system,
     */
    CLI_CMD_NEW;
    cmd->cc_words_str = 	"media-cache disk dictionary pre-load enable";
    cmd->cc_help_descr = 	N_("Enable pre-load of cache meta data");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set	;
    cmd->cc_exec_name =		"/nkn/nvsd/diskcache/config/pre-load/enable";
    cmd->cc_exec_value =	"true";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_revmap_type = 	crt_auto;
    cmd->cc_revmap_order =	cro_media_cache;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"media-cache disk dictionary pre-load disable";
    cmd->cc_help_descr = 	N_("Disable pre-load of cache meta data");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set	;
    cmd->cc_exec_name =		"/nkn/nvsd/diskcache/config/pre-load/enable";
    cmd->cc_exec_value =	"false";
    cmd->cc_exec_type =		bt_bool;
    cmd->cc_revmap_type = 	crt_auto;
    cmd->cc_revmap_order =	cro_media_cache;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"media-cache disk dictionary pre-load stop";
    cmd->cc_help_descr = 	N_("Stop pre-load of cache meta data");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_action_rstr_curr;
    cmd->cc_exec_operation =	cxo_action;
    cmd->cc_exec_action_name =	"/nkn/nvsd/diskcache/actions/pre-read-stop";
    CLI_CMD_REGISTER;
    /*---------------------------------------------------------------------*/

    /*---------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "show media-cache";
    cmd->cc_help_descr =        N_("Display media-caches");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show media-cache disk";
    cmd->cc_help_descr =        N_("Display disk caches");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show media-cache disk list";
    cmd->cc_help_descr =        N_("List available disk caches");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_nvsd_diskcache_list;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show media-cache disk *";
    cmd->cc_help_exp =          N_("<name>");
    cmd->cc_help_exp_hint =     N_("Disk cache name");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_help_callback =     cli_diskcache_help;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_comp_callback =     cli_diskcache_completion;
    cmd->cc_exec_callback =     cli_nvsd_diskcache_show;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "show media-cache evict-thread-frequency";
    cmd->cc_help_descr =        N_("Show media cache eviction thread frequency");
    cmd->cc_flags =             ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_nvsd_diskcache_show_evict;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show media-cache internal-eviction-watermark";
    cmd->cc_help_descr =        N_("Show media cache eviction internal watermarks");
    cmd->cc_flags =             ccf_terminal|ccf_ignore_length;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_action_name =  "/nkn/nvsd/diskcache/actions/show-internal-watermarks";
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "show media-cache external-eviction-watermark";
    cmd->cc_help_descr =        N_("Show media cache eviction external watermarks");
    cmd->cc_flags =             ccf_terminal|ccf_ignore_length;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_nvsd_diskcache_evict_wm_external;
    CLI_CMD_REGISTER;
#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =         "show media-cache free-block";
    cmd->cc_help_descr =        N_("Display free-block thresholdof disk caches");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show media-cache free-block threshold";
    cmd->cc_help_descr =        N_("Show media cache free-block threshold");
    cmd->cc_flags =             ccf_terminal ;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_nvsd_diskcache_show_freeblock_threshold;
    CLI_CMD_REGISTER;
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show media-cache cache-tier";
    cmd->cc_help_descr =        N_("Show media cache cache-tier");
    cmd->cc_flags =             ccf_terminal ;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_nvsd_diskcache_show_cache_tier;
    CLI_CMD_REGISTER;

    /* Bug 9853
    CLI_CMD_NEW;
    cmd->cc_words_str =         "show media-cache group-read";
    cmd->cc_help_descr =        N_("Show media cache group read");
    cmd->cc_flags =             ccf_terminal ;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_nvsd_diskcache_show_group_read;
    CLI_CMD_REGISTER;
     */
    CLI_CMD_NEW;
    cmd->cc_words_str =         "show media-cache generic-config";
    cmd->cc_help_descr =        N_("Show media cache generic-config");
    cmd->cc_flags =             ccf_terminal ;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_nvsd_diskcache_show_generic_config;
    CLI_CMD_REGISTER;
#if 0    /* Bug 9853 */

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk group-read";
    cmd->cc_help_descr =        N_("set media cache group-read");
/*    cmd->cc_flags =             ccf_hidden ; bug# 7030 */
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk group-read sas";
    cmd->cc_help_descr =        N_("set media cache group-read for SAS");
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk group-read sata";
    cmd->cc_help_descr =        N_("set media cache group-read for SATA");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk group-read ssd";
    cmd->cc_help_descr =        N_("set media cache group-read for SSD");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk group-read sata enable";
    cmd->cc_help_descr =        N_("enable group-read for SATA");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/sata/group-read/enable";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk group-read sata disable";
    cmd->cc_help_descr =        N_("disable group-read for SATA");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/sata/group-read/enable";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk group-read sas enable";
    cmd->cc_help_descr =        N_("enable group-read for SAS");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/sas/group-read/enable";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk group-read sas disable";
    cmd->cc_help_descr =        N_("disable group-read for SAS");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/sas/group-read/enable";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk group-read ssd enable";
    cmd->cc_help_descr =        N_("enable group-read for SSD");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/ssd/group-read/enable";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk group-read ssd disable";
    cmd->cc_help_descr =        N_("disable group-read for SATA");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/ssd/group-read/enable";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER 
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show media-cache controller";
    cmd->cc_help_descr =        N_("Display disk cache controllers");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show media-cache controller 3ware";
    cmd->cc_help_descr =        N_("Display disk cache controller 3ware");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_nvsd_diskcontroller;
    cmd->cc_magic =             cid_3ware_controller;
    CLI_CMD_REGISTER;


    /*---------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk actions ";
    cmd->cc_help_descr =        N_("Configure disk's actions");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk actions rate-limit";
    cmd->cc_help_descr =        N_("Configure disk's actions rate-limit");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk actions rate-limit enable";
    cmd->cc_help_descr =        N_("Enable disk's actions rate-limit");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/cmd/rate_limit";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk actions rate-limit disable";
    cmd->cc_help_descr =        N_("Disable disk's actions rate-limit");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/cmd/rate_limit";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk write-size";
    cmd->cc_help_descr =        N_("Configure the disk write size");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "media-cache disk write-size *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Minimum size of data to buffer before writing to disk (in bytes)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/diskcache/config/write-size";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER;

    /*---------------------------------------------------------------------*/
    err = cli_revmap_ignore_bindings_va(10,
            "/nkn/nvsd/diskcache/config/evict_thread_freq",
            "/nkn/nvsd/diskcache/config/free-block/threshold/internal/*",
            "/nkn/nvsd/diskcache/config/block-sharing/threshold/internal/*",
            "/nkn/nvsd/diskcache/config/free-block/threshold/internal/sas",
            "/nkn/nvsd/diskcache/config/free-block/threshold/internal/sata",
            "/nkn/nvsd/diskcache/config/free-block/threshold/internal/ssd",
            "/nkn/nvsd/diskcache/config/sas/group-read/enable",
            "/nkn/nvsd/diskcache/config/sata/group-read/enable",
            "/nkn/nvsd/diskcache/config/ssd/group-read/enable",
            "/nkn/nvsd/diskcache/config/cmd/rate_limit");
    bail_error(err);

bail:
    return err;
}

int 
cli_nvsd_diskcache_show_evict(
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
    err = cli_printf_query(_(
                ": "
                "#/nkn/nvsd/diskcache/config/evict_thread_freq# (seconds)\n"));
    bail_error(err);

bail:
    return err;
}

#if 0
int 
cli_nvsd_diskcache_show_freeblock_threshold(
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

    err = cli_printf_query(_(
                "SAS free block threshold: "
                "#/nkn/nvsd/diskcache/config/free-block/threshold/internal/sas# percent\n"));
    err = cli_printf_query(_(
                "SATA free block threshold: "
                "#/nkn/nvsd/diskcache/config/free-block/threshold/internal/sata# percent\n"));
    err = cli_printf_query(_(
                "SSD free block threshold: "
                "#/nkn/nvsd/diskcache/config/free-block/threshold/internal/ssd# percent\n"));
    bail_error(err);

bail:
    return err;
}
#endif

int
cli_nvsd_diskcache_show_cache_tier(
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

    err = cli_printf_query(_(
                "SAS cache-tier admission threshold  : "
                "#/nkn/nvsd/diskcache/config/sas_adm_thres# \n"));
    bail_error(err);

    err = cli_printf_query(_(
                "SATA cache-tier admission threshold : "
                "#/nkn/nvsd/diskcache/config/sata_adm_thres# \n"));
    bail_error(err);

    err = cli_printf_query(_(
                "SSD cache-tier admission threshold  : "
                "#/nkn/nvsd/diskcache/config/ssd_adm_thres# \n"));
    bail_error(err);

bail:
    return err;
}

/* Bug 9853
int
cli_nvsd_diskcache_show_group_read(
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

    err = cli_printf_query(_(
                "SAS group read enabled  : "
                "#/nkn/nvsd/diskcache/config/sas/group-read/enable# \n"));
    bail_error(err);

    err = cli_printf_query(_(
                "SATA group read enabled : "
                "#/nkn/nvsd/diskcache/config/sata/group-read/enable# \n"));
    bail_error(err);

    err = cli_printf_query(_(
                "SSD group read enabled  : "
                "#/nkn/nvsd/diskcache/config/ssd/group-read/enable# \n"));
    bail_error(err);

bail:
    return err;
}
*/

int
cli_nvsd_diskcache_show_generic_config(
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

    err = cli_printf_query(_(
                "media-cache disk dictionary preload enabled  : "
                "#/nkn/nvsd/diskcache/config/pre-load/enable# \n"));
    bail_error(err);

    err = cli_printf_query(_(
                "media-cache disk rate-limit enabled          : "
                "#/nkn/nvsd/diskcache/config/cmd/rate_limit# \n"));
    bail_error(err);

bail:
    return err;
}

/*---------------------------------------------------------------------*/
int 
cli_nvsd_diskcache_show(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    char *r = NULL;
    char *node_name = NULL;
    const char *t_diskname = NULL;
    tstring *t_diskid = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    /* Send the disk name and get the disk id */
    t_diskname = tstr_array_get_str_quick(params, 0);
    bail_null(t_diskname);

    node_name = smprintf ("/nkn/nvsd/diskcache/monitor/diskname/%s/disk_id", t_diskname);

    /* Now get the disk_id */
    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_diskid, node_name, NULL);
    bail_error(err);
    safe_free (node_name);

    if (!t_diskid)
    {
    	cli_printf(_(
                "error: invalid disk name <%s>\n"), t_diskname);
	goto bail;
    }

    /* Now print info about the disk */
    cli_printf(_(
                "\nDisk Cache Configuration & Status:\n\n"));
    cli_nvsd_print_disk_info (t_diskname, ts_str(t_diskid), NULL);

bail:
    safe_free(r);
    return err;
}
/*---------------------------------------------------------------------*/
int 
cli_nvsd_diskcache_op(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    tbool valid = false;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);

    switch(cmd->cc_exec_operation)
    {
    case cxo_set:
        cli_printf("Adding disk @ bay %s with name %s\n",
                tstr_array_get_str_quick(params, 1),
                tstr_array_get_str_quick(params, 0));

        // Check if label exists
        //err = cli_diskcache_validate(tstr_array_get_str_quick(params, 0),
                //&valid);
        //bail_error(err);

        if (!valid) {
            err = cli_diskcache_create(params);
            bail_error(err);
        }
        else {
            cli_printf_error(_("media-cache label - %s - already exists\n"),
                    tstr_array_get_str_quick(params, 0));
        }
        break;

    case cxo_delete:
        cli_printf("Deleteing disk @ bay %s with name %s\n",
                tstr_array_get_str_quick(params, 1),
                tstr_array_get_str_quick(params, 0));

        // Check if the label exists
        err = cli_diskcache_validate(tstr_array_get_str_quick(params, 0), 
               &valid);
        bail_error(err);

        if (valid) {
            err = cli_diskcache_delete(params);
            bail_error(err);
        }
        else {
            cli_printf_error(_("Unknown media-cache label - %s\n"),
                    tstr_array_get_str_quick(params, 0));
        }
        break;
    default:
        break;
    }

bail:
    return err;

}


// Create the disk_id node and children.
// Assumes that the label doesnt exist - MUST pre-validate the label
static int
cli_diskcache_create(
        const tstr_array *params)
{
    int err = 0;
    char *disk_id_binding = NULL;
    char *bay_binding = NULL;

    bail_null(params);

    disk_id_binding = smprintf("/nkn/nvsd/diskcache/config/disk_id/%s",
            tstr_array_get_str_quick(params, 0));

    err = mdc_create_binding(
            cli_mcc, 
            NULL,
            NULL,
            bt_string,
            tstr_array_get_str_quick(params, 0),
            disk_id_binding);
    bail_error(err);

    bay_binding = smprintf("/nkn/nvsd/diskcache/config/disk_id/%s/bay",
            tstr_array_get_str_quick(params, 0));

    err = mdc_set_binding(
            cli_mcc,
            NULL,
            NULL,
            bsso_modify,
            bt_uint16,
            tstr_array_get_str_quick(params, 1),
            bay_binding);
bail:
    safe_free(disk_id_binding);
    safe_free(bay_binding);
    return err;
}



// Delete the entire disk_id and its children !!
// This is IRRECOVERABLE
// MUST pre-validate the label to check if it exists or not
static int
cli_diskcache_delete(
        const tstr_array *params)
{
    int err = 0;
    char *disk_id_binding = NULL;

    bail_null(params);

    disk_id_binding = smprintf("/nkn/nvsd/diskcache/config/disk_id/%s",
            tstr_array_get_str_quick(params, 0));
    err = mdc_delete_binding(
            cli_mcc,
            NULL,
            NULL,
            disk_id_binding);

    bail_error(err);

bail:
    safe_free(disk_id_binding);
    return err;
}

int
cli_nvsd_diskcache_list(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 idx = 0;
    uint32 num_disks = 0;
    tstr_array *disk_name_list = NULL;
    uint64_t t_total_free_mbytes = 0;
    tstring *t_diskid = NULL;
    const char *t_disk_name = NULL;
    uint64_t t_free_mbytes = 0;
    uint32 ret_err = 0;
    node_name_t node_name = {0};

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    /* Get the disk list */

    err = cli_diskcache_get_disks(&disk_name_list, false);
    bail_error(err);

    err = tstr_array_length(disk_name_list, &num_disks);
    bail_error(err);

    cli_printf_query(_("%8s%14s%10s%8s%8s%12s%8s\n"),
                "Device","Type","Tier","Active","Cache","Free Space","State");
    cli_printf_query(_("%8s%14s%10s%8s%8s%12s%8s\n"),
                "------","----","----","------","-----","----------","-----");
    for (idx = 0; idx < num_disks; idx++)
    {
	char cp_diskname [8]; // stores string of format : dc_???
	t_disk_name = tstr_array_get_str_quick(disk_name_list, idx);

	/* Instead of using the list as is creating the disk name in sequence
	 * as we know disk name format as the default list is sorted 
	 * alphabetically and that does not work when we have more than 9 disks
	 */
	safe_strlcpy(cp_diskname, t_disk_name );

	/* Now get the disk_id */
	snprintf (node_name,sizeof(node_name),
		"/nkn/nvsd/diskcache/monitor/diskname/%s/disk_id", cp_diskname);

	err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_diskid, node_name, NULL);
	bail_error(err);

	if (t_diskid == NULL) {
	    continue;
	} else {
	    tstring *t_owner = NULL;
	    node_name_t owner_node = {0};
	    int next = 0;

	    snprintf(owner_node, sizeof(owner_node),
		    "/nkn/nvsd/diskcache/config/disk_id/%s/owner", ts_str(t_diskid));
	    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
		    &t_owner, owner_node, NULL);
	    bail_error(err);
	    if (!ts_equal_str(t_owner, "DM2", false)) {
		next = 1;
	    } else {
		/* do nothing */
	    }
	    ts_free(&t_owner);
	    if (next) {
		next = 0;
		ts_free(&t_diskid);
		continue;
	    }
	}
	/* Now print info about the disk */
	err = cli_nvsd_print_disk_info_in_oneline (cp_diskname, ts_str(t_diskid), &t_free_mbytes);
	bail_error(err);

	t_total_free_mbytes += t_free_mbytes;
    }

    cli_printf(_(
                "\nTotal Free Space: %ld MiB\n\n"), t_total_free_mbytes);
    cli_printf(_(" '*' as suffix in the Device denotes the unsupported type of disk"));
    cli_printf(_("\n"));

bail:
    return err;
}


int 
cli_diskcache_validate(
        const char *label,
        tbool *ret_valid)
{
    int err = 0;
    tstr_array *labels = NULL;
    uint32 idx = 0;

    bail_null(ret_valid);
    *ret_valid = false;

    err = cli_diskcache_get_disks(&labels, false);
    bail_error(err);

    err = tstr_array_linear_search_str(labels, label, 0, &idx);
    if(err != lc_err_not_found) {
        bail_error(err);
        *ret_valid = true;
    }
    else {
        err = cli_printf_error(_("Unknown media-cache %s\n"), label);
        bail_error(err);
    }

bail:
    tstr_array_free(&labels);
    return err;
}



int 
cli_diskcache_get_disks(
        tstr_array **ret_labels,
        tbool hide_display)
{
    int err = 0;
    tstr_array *labels = NULL;

    UNUSED_ARGUMENT(hide_display);

    bail_null(ret_labels);
    *ret_labels = NULL;

    err = mdc_get_binding_children_tstr_array(cli_mcc,
            NULL, NULL, &labels, 
            "/nkn/nvsd/diskcache/monitor/diskname", NULL);
    bail_error_null(err, labels);

    *ret_labels = labels;
    labels = NULL;
bail:
    tstr_array_free(&labels);
    return err;
}

int
cli_diskcache_help(
        void *data, 
        cli_help_type help_type,
        const cli_command *cmd, 
        const tstr_array *cmd_line,
        const tstr_array *params, 
        const tstring *curr_word,
        void *context)
{
    int err = 0;
    tstr_array *labels = NULL;
    const char *label = NULL;
    int i = 0, num_names = 0;


    switch(help_type)
    {
    case cht_termination:
        if(cli_prefix_modes_is_enabled() && cmd->cc_help_term_prefix) {
            err = cli_add_termination_help(context, 
                    GT_(cmd->cc_help_term_prefix, cmd->cc_gettext_domain));
            bail_error(err);
        }
        else {
            err = cli_add_termination_help(context,
                    GT_(cmd->cc_help_term, cmd->cc_gettext_domain));
            bail_error(err);
        }
        break;

    case cht_expansion:
        if(cmd->cc_help_exp) {
            err = cli_add_expansion_help(context,
                    GT_(cmd->cc_help_exp, cmd->cc_gettext_domain),
                    GT_(cmd->cc_help_exp_hint, cmd->cc_gettext_domain));
            bail_error(err);
        }

        err = tstr_array_new(&labels, NULL);
        bail_error(err);

        err = cli_diskcache_completion(data, cmd, cmd_line, params, 
                curr_word, labels);
        bail_error(err);

        num_names = tstr_array_length_quick(labels);
        for(i = 0; i < num_names; ++i) {
            label = tstr_array_get_str_quick(labels, i);
            bail_null(label);
            
            err = cli_add_expansion_help(context, label, NULL);
            bail_error(err);
        }
        break;
    
    default:
        bail_force(lc_err_unexpected_case);
        break;
    }

bail:
    tstr_array_free(&labels);
    return err;
}


/*---------------------------------------------------------------------*/
int 
cli_diskcache_completion(void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions)
{
    int err = 0;
    tstr_array *labels = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_diskcache_get_disks(&labels, true);
    bail_error(err);

    err = tstr_array_concat(ret_completions, &labels);
    bail_error(err);

    err = tstr_array_delete_non_prefixed(ret_completions, ts_str(curr_word));
    bail_error(err);

bail:
    tstr_array_free(&labels);
    return err;
}


/*---------------------------------------------------------------------*/
int 
cli_diskcache_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    tbool valid = false;

    UNUSED_ARGUMENT(data);

    if (cli_prefix_modes_is_enabled()) {
        err = cli_diskcache_validate(tstr_array_get_str_quick(params, 0),
                &valid);
        bail_error(err);

        if (valid) {
            err = cli_prefix_enter_mode(cmd, cmd_line);
            bail_error(err);
        }
    }
    else {
        err = cli_print_incomplete_command_error(cmd, cmd_line);
        bail_error(err);
    }
bail:
    return err;
}

/*---------------------------------------------------------------------*/
int 
cli_diskcache_stub(
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

    bail_error(err);
bail:
    return err;
}

/*---------------------------------------------------------------------*/
int 
cli_diskcache_type(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0, i;
	char *node_name = NULL;
	char *cmd_str = NULL;
	tstring *t_diskid = NULL;
	tstring *t_subprovider = NULL;
	const char *cp_diskname = NULL;
	const char *cp_cachetype = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);

	/* Get the disk name from the params */
	cp_diskname = tstr_array_get_str_quick(params, 0);
	bail_null(cp_diskname);

	/* Get the cache type from the params */
	cp_cachetype = tstr_array_get_str_quick(cmd_line, 4);
	bail_null(cp_cachetype);

	/* Now get the correct disk type strings as CLI allows substrings */
	/* Eg. CLI allows sat as the keyword match for sata and ss for ssd */
	i = 0;
	while (NULL != cli_disk_type_options[i].ceo_option)
	{
		/* Check if the string matches */
		if (!strncasecmp(cp_cachetype, cli_disk_type_options[i].ceo_option,
						strlen(cp_cachetype)))
		{
			cp_cachetype = cli_disk_type_options[i].ceo_option;
			break;
		}

		/* Else check the next type */
		i++;
	}

	/* Now get the disk_id */
	node_name = smprintf ("/nkn/nvsd/diskcache/monitor/diskname/%s/disk_id", cp_diskname);

	err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_diskid, node_name, NULL);
	if (err || !t_diskid)
	{
		cli_printf_error(_("error: failed to find  disk '%s'\n"), cp_diskname);
		bail_error(err);
	}
	bail_null(t_diskid);
	safe_free(node_name);

	/* Now get the sub-provider value to check if it is UNKNOWN */
	node_name = smprintf ("/nkn/nvsd/diskcache/config/disk_id/%s/sub_provider", ts_str(t_diskid));

	err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_subprovider, node_name, NULL);
	if (err | !t_subprovider)
	{
		cli_printf_error(_("error: failed to get current cache-type for disk name '%s'\n"), cp_diskname);
		bail_error(err);
	}
	bail_null(t_subprovider);

	/* Now update the sub_provider node and the config text file */
	/* First the sub_provider node binding */
	err = mdc_modify_binding (cli_mcc, NULL, NULL, bt_string,
				cp_cachetype, node_name);
	if (err)
	{
		cli_printf_error(_("error: failed to modify the cache-type"));
		bail_error(err);
	}

	/* Now the change the config file /config/nkn/diskcache-startup.info */
	cmd_str = smprintf ("/opt/nkn/bin/update_cachetype.sh %s %s %s", ts_str(t_diskid), ts_str(t_subprovider), cp_cachetype);

	system(cmd_str);

	cli_printf (_("cache type successfully modified\nIMPORTANT NOTE: please RESTART 'mod-delivery' for the change to take effect\n"));
bail:
    if (err)
        cli_printf_error(_("error: command failed, please check system log"));
    safe_free(node_name);
    ts_free (&t_diskid);
    ts_free (&t_subprovider);
    return err;
}

/*---------------------------------------------------------------------*/
static int 
cli_nvsd_print_disk_info(const char* t_diskname, const char* t_diskid, uint64_t* p_free_mbytes)
{
    int err = 0;
    char *node_name = NULL;
    tstring *t_free_blocks = NULL;
    tstring *t_block_size = NULL;
    tstring *t_disk_state_check = NULL;
    tstring *t_firmware_version = NULL;
    uint64_t	t_free_bytes, t_free_mbytes ;
    int print_firmware_version = 0;

    /* Now get the firmware version */
    node_name = smprintf ("/nkn/nvsd/diskcache/config/disk_id/%s/firmware_version", t_diskid);

    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_firmware_version, node_name, NULL);
    bail_error(err);
    safe_free(node_name);
    bail_null(t_firmware_version);

    if (strcmp(ts_str(t_firmware_version), "NA"))
	print_firmware_version = 1;

    /* Now get the block_size */
    node_name = smprintf ("/stat/nkn/disk/%s/block_size", t_diskname);

    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_block_size, node_name, NULL);
    bail_error(err);
    safe_free(node_name);
    bail_null(t_block_size);

    /* Now get the free_blocks */
    node_name = smprintf ("/stat/nkn/disk/%s/free_blocks", t_diskname);

    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_free_blocks, node_name, NULL);
    bail_error(err);
    bail_null(t_free_blocks);

    /* Now calculate */
    t_free_bytes = (uint64_t)atoi(ts_str(t_block_size)) * (uint64_t)atoi(ts_str(t_free_blocks));
    t_free_mbytes = t_free_bytes /(1024 * 1024);

    /* Return the free Mbytes */
    if (p_free_mbytes)
    	*p_free_mbytes = t_free_mbytes;

    /* Print the output */
    cli_printf_query(_(
                "   Device Name/Type: #/nkn/nvsd/diskcache/monitor/disk_id/%s/diskname#"), t_diskid);
    cli_printf_query(_(
                "/#/nkn/nvsd/diskcache/monitor/disk_id/%s/disktype#\n"), t_diskid);
    cli_printf("   Serial Number: %s\n", t_diskid);
    if (print_firmware_version)
	cli_printf_query(_(
                "   Firmware version: #/nkn/nvsd/diskcache/config/disk_id/%s/firmware_version#\n"), t_diskid);
    cli_printf_query(_(
                "   Cache Tier: #/nkn/nvsd/diskcache/monitor/disk_id/%s/tier#\n"), t_diskid);
    cli_printf_query(_(
                "   Activated: #/nkn/nvsd/diskcache/config/disk_id/%s/activated#\n"), t_diskid);
    cli_printf_query(_(
                "   Cache Enabled: #/nkn/nvsd/diskcache/config/disk_id/%s/cache_enabled#\n\n"), t_diskid);

    node_name = smprintf ("/nkn/nvsd/diskcache/monitor/disk_id/%s/diskstate", t_diskid);
    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_disk_state_check, node_name, NULL);
    bail_error(err);
    safe_free(node_name);
    bail_null(t_disk_state_check);

    if (ts_length(t_disk_state_check) > 0 && ts_equal_str(t_disk_state_check, "cache running", false))
    cli_printf_query(_(
                "   Free Space: %ld MiB\n"), t_free_mbytes);
    else
    cli_printf_query(_(
                "   Free Space:    -   \n"));

    cli_printf_query(_(
                "   Disk State : #/nkn/nvsd/diskcache/monitor/disk_id/%s/diskstate#\n"), t_diskid);
    /*cli_printf_query(_(
                "   Meta-data Compression Percentage : #/nkn/nvsd/diskcache/config/disk_id/%s/metadata_comp_per#\n"), t_diskid);*/
    cli_printf_query(_(
                "-------------------------------------\n"));
    cli_printf(_("\n"));
#if 0
    cli_printf_query(_(
                "   Free Pages: #/stat/nkn/disk/%s/free_pages#\n"), ts_str(t_diskname));
    cli_printf_query(_(
                "   Free Blocks: #/stat/nkn/disk/%s/free_blocks#\n"), ts_str(t_diskname));
    cli_printf_query(_(
                "   Page Size: #/stat/nkn/disk/%s/page_size#\n"), ts_str(t_diskname));
    cli_printf_query(_(
                "   Block Size: #/stat/nkn/disk/%s/block_size#\n"), ts_str(t_diskname));
#endif /* 0 */

bail:
    safe_free(node_name);
    ts_free(&t_free_blocks);
    ts_free(&t_block_size);
    ts_free(&t_firmware_version);
    ts_free(&t_disk_state_check);
    return err;

} /* end of cli_nvsd_print_disk_info () */

/*---------------------------------------------------------------------*/
static int 
cli_nvsd_print_disk_info_in_oneline(const char* t_diskname, 
				const char* t_diskid, 
				uint64_t* p_free_mbytes)
{
    int err = 0;
    char *node_name = NULL;
    tstring *t_free_blocks = NULL;
    tstring *t_block_size = NULL;
    tstring *t_disk_state_check= NULL;
    uint64_t	t_free_bytes, t_free_mbytes ;

    /* Now get the block_size */
    node_name = smprintf ("/stat/nkn/disk/%s/block_size", t_diskname);

    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_block_size, node_name, NULL);
    bail_error(err);
    safe_free(node_name);
    bail_null(t_block_size);

    /* Now get the free_blocks */
    node_name = smprintf ("/stat/nkn/disk/%s/free_blocks", t_diskname);

    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_free_blocks, node_name, NULL);
    bail_error(err);
    bail_null(t_free_blocks);

    /* Now calculate */
    t_free_bytes = (uint64_t)atoi(ts_str(t_block_size)) * (uint64_t)atoi(ts_str(t_free_blocks));
    t_free_mbytes = t_free_bytes /(1024 * 1024);

    /* Return the free Mbytes */
    if (p_free_mbytes)
	*p_free_mbytes = t_free_mbytes;

    /* Print the output */
    if ((lc_is_suffix("*@", t_diskid, false)) &&
	 !(lc_is_prefix("vd", t_diskid, false)))
    {
        cli_printf_query(_(
                    "#w:8~j:right~/nkn/nvsd/diskcache/monitor/disk_id/%s/diskname#*"), t_diskid);

	/* Also log  an entry in syslog that there is an unknown type */
	lc_log_basic(LOG_NOTICE, "Warning: disk %s is of UNKNOWN type", t_diskid);
    }
    else
    {
        cli_printf_query(_(
                    "#w:8~j:right~/nkn/nvsd/diskcache/monitor/disk_id/%s/diskname#"), t_diskid);
    }
    cli_printf_query(_(
                "#w:14~j:right~/nkn/nvsd/diskcache/monitor/disk_id/%s/disktype#"), t_diskid);
    cli_printf_query(_(
                "#w:10~j:right~/nkn/nvsd/diskcache/monitor/disk_id/%s/tier#"), t_diskid);
    cli_printf_query(_(
                "#w:8~j:right~/nkn/nvsd/diskcache/config/disk_id/%s/activated#"), t_diskid);
    cli_printf_query(_(
                "#w:8~j:right~/nkn/nvsd/diskcache/config/disk_id/%s/cache_enabled#"), t_diskid);

    node_name = smprintf ("/nkn/nvsd/diskcache/monitor/disk_id/%s/diskstate", t_diskid);
    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_disk_state_check, node_name, NULL);
    bail_error(err);
    safe_free(node_name);
    bail_null(t_disk_state_check);

    if (ts_length(t_disk_state_check) > 0 && ts_equal_str(t_disk_state_check, "cache running", false))
	cli_printf_query(_(
                "%8ld MiB"), t_free_mbytes);
    else
    {
	cli_printf(_("%12s"), "    -   - ");
	/*The disk cache is not running so snd 0 bytes */
	*p_free_mbytes = 0;
    }
    cli_printf_query(_(
                "  #/nkn/nvsd/diskcache/monitor/disk_id/%s/diskstate#"), t_diskid);
    cli_printf(_("\n"));

bail:
    safe_free(node_name);
    ts_free(&t_free_blocks);
    ts_free(&t_block_size);
    ts_free(&t_disk_state_check);
    return err;

} /* end of cli_nvsd_print_disk_info_in_oneline () */


/*---------------------------------------------------------------------*/
static int 
cli_nvsd_diskcache_format(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 ret_err = 0;
    int system_retval = 0;
    bn_request *req = NULL;
    tstring *ret_msg = NULL;
    char *node_name = NULL;
    const char *c_diskname = NULL;
    tstring *t_pre_format_str = NULL;
    bn_binding_array *ret_bindings = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);
    UNUSED_ARGUMENT(cmd);

    c_diskname = tstr_array_get_str_quick(params, 0);
    bail_null(c_diskname);

    err = bn_action_request_msg_create (&req,
    			"/nkn/nvsd/diskcache/actions/pre_format");
    bail_error(err);
    err = bn_action_request_msg_append_new_binding
    	(req, 0, "diskname", bt_string, c_diskname, NULL);
    bail_error(err);

    if (cmd->cc_magic == cc_disk_format_sb)
    {
    	err = bn_action_request_msg_append_new_binding
        	(req, 0, "blocks", bt_string, "small-blocks", NULL);
    	bail_error(err);
    }else{
	err = bn_action_request_msg_append_new_binding
                (req, 0, "blocks", bt_string, "noblocks", NULL);
        bail_error(err);
    }

    /* Send the action message to get the returne code first */
    err = mdc_send_mgmt_msg (cli_mcc, req, false, &ret_bindings, &ret_err, NULL);
    bail_error(err);

#if 0
    /* Now get the string that could be command or error string */
    if (cmd->cc_magic == cc_disk_format_sb){
        	node_name = smprintf ("/nkn/nvsd/diskcache/monitor/diskname/%s/pre_format_sb", c_diskname);
    } else {
		node_name = smprintf ("/nkn/nvsd/diskcache/monitor/diskname/%s/pre_format", c_diskname);
    }

    /* Now get the pre_format string */
    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_pre_format_str, node_name, NULL);
    bail_error(err);
    safe_free (node_name);
#endif
    err = bn_binding_array_get_value_tstr_by_name(ret_bindings, "cmd_str",
	    NULL, &t_pre_format_str);
    bail_error(err);

    //cli_printf("\nCMD-STR: %d, %s\n", ret_err, ts_str(t_pre_format_str));
    if (ret_err ) {
	//cli_printf_error(_("command failed, try again after sometime"));
	goto bail;
    }
    /* If ret_err is 0 then we have a format command in t_pre_format_str
     * else it means format cannot be done and error string was sent
     * in the action ret_msg parameter that will get printed  */
    //if (ret_err) goto bail;
    /* ret_err is 0 hence execute the format command in ret_msg */
    system_retval = system (ts_str(t_pre_format_str));

    /* Send the post-format action to nvsd only if the command was successful */
    if (!system_retval) // 0 means succes else there was error
    {
	ts_free(&ret_msg);
	err = mdc_send_action_with_bindings_str_va(cli_mcc, &ret_err, &ret_msg,
		"/nkn/nvsd/diskcache/actions/post_format", 1,
		"diskname", bt_string, c_diskname);
	bail_error(err);
    }
    else
	cli_printf_error ("\tfailed to format disk : %s\n", c_diskname);

bail:
    ts_free(&ret_msg);
    ts_free(&t_pre_format_str);
    return err;

} /* end of cli_nvsd_diskcache_format () */


int 
cli_nvsd_disk_metadata_upload(void *data,
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
		(remote_url, cli_disk_metadata_upload_finish,
		cmd, cmd_line, params, NULL);
	bail_error(err);
    }
    else {
    	err = cli_disk_metadata_upload_finish(NULL, cpr_ok, cmd, cmd_line, params,
		NULL, NULL);
	bail_error(err);
    }

bail:
    return(err);

}

/* Send a req to an action node which runs a script to get meta-data.
The meta data path is got from the action node and use it to trigger an 
action to upload the file*/
static int
cli_disk_metadata_upload_finish(const char *password, clish_password_result result,
				const cli_command *cmd, const tstr_array *cmd_line,
				const tstr_array *params, void *data,
				tbool *ret_continue)
{
	int err = 0;
	const char *remote_url = NULL;

	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(ret_continue);

	if(result != cpr_ok) {
		goto bail;
	}

	remote_url = tstr_array_get_str_quick(params, 0);
	bail_null(remote_url);

	if(password) {
		err = mdc_send_action_with_bindings_str_va(cli_mcc,
				NULL, NULL,
				"/nkn/debug/generate/disk_metadata", 2,
				"remote_url", bt_string, remote_url,
				"password", bt_string, password);
		bail_error(err);
	} else {
		err = mdc_send_action_with_bindings_str_va(cli_mcc,
				NULL, NULL,
				"/nkn/debug/generate/disk_metadata", 1,
				"remote_url", bt_string, remote_url);
		bail_error(err);
	}

bail:
	return err;
}

/* -------------------------------------------------------------------- */
static int 
cli_nvsd_diskcache_status_revmap_cb (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err = 0;
    const char *diskid = NULL;
    char *cmd_str = NULL;
    char *node_name = NULL;
    tstring *t_diskname = NULL;
    tstring *t_cur_ramcache = NULL;
    tstring *t_config_ramcache = NULL;
    tbool enabled = false;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(cmd);

    /* Get the disk id */
    diskid= tstr_array_get_str_quick(name_parts, 5);
    bail_null(diskid);

    /* Now get the disk_name */
    node_name = smprintf ("/nkn/nvsd/diskcache/monitor/disk_id/%s/diskname", diskid);

    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_diskname, node_name, NULL);
    bail_error(err);

    err = lc_str_to_bool(value, &enabled);
    bail_error(err);

    if (t_diskname) {
	/* Build the command */
	cmd_str = smprintf("media-cache disk %s status %s", ts_str(t_diskname),
		enabled ? "active" : "inactive");

	err = tstr_array_append_str_takeover(ret_reply->crr_cmds, &cmd_str);
	bail_error(err);
    }

    err = tstr_array_append_str(ret_reply->crr_nodes, name);
    bail_error(err);

bail:
    safe_free(node_name);
    safe_free(cmd_str); /* Only in error case */
    ts_free(&t_cur_ramcache);
    ts_free(&t_config_ramcache);
    ts_free(&t_diskname);
    return(err);
}

/* -------------------------------------------------------------------- */
static int 
cli_nvsd_ramcache_status_revmap_cb (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err = 0;
    char *cmd_str = NULL;
    tstring *t_cur_ramcache = NULL;
    tstring *t_config_ramcache = NULL;
    tstring *t_cur_ramcache_atrr_count = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

    /* Add the RAM cache info first */
    /* Get the configured RAM cache */
    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_config_ramcache,
			"/nkn/nvsd/buffermgr/config/max_cachesize", NULL);
    bail_error(err);

    /* Build the command */
    cmd_str = smprintf("#  ram-cache cache-size-MB %s\n",
			ts_str(t_config_ramcache));
    bail_null(cmd_str);

    /* Add to the revmap */
    err = tstr_array_append_str_takeover(ret_reply->crr_cmds, &cmd_str);
    bail_error(err);

    /* Get the current RAM cache */
    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_cur_ramcache,
			"/nkn/nvsd/buffermgr/monitor/cachesize", NULL);
    bail_error(err);

    /* Build the command */
    cmd_str = smprintf("#  Current RAM cache size : %s", ts_str(t_cur_ramcache));
    bail_null(cmd_str);

    /* Add to the revmap */
    err = tstr_array_append_str_takeover(ret_reply->crr_cmds, &cmd_str);
    bail_error(err);

    /* Get the current RAM cache */
    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
			&t_cur_ramcache_atrr_count,
			"/nkn/nvsd/buffermgr/monitor/attributecount", NULL);
    bail_error(err);

    /* Build the command */
    cmd_str = smprintf("#  Current RAM Cache small attribute count: %s", ts_str(t_cur_ramcache_atrr_count));
    bail_null(cmd_str);

    /* Add to the revmap */
    err = tstr_array_append_str_takeover(ret_reply->crr_cmds, &cmd_str);
    bail_error(err);

bail:
    safe_free(cmd_str); /* Only in error case */
    ts_free(&t_cur_ramcache);
    ts_free(&t_config_ramcache);
    return(err);
}

#if 0
static int
cli_nvsd_comp_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    const char *diskid = NULL;
    char *cmd_str = NULL;
    char *node_name = NULL;
//    char *comp_node = NULL;
    tstring *t_diskname = NULL;
    tstring *t_compvalue = NULL;

    /* Get the disk id */
    diskid= tstr_array_get_str_quick(name_parts, 5);
    bail_null(diskid);

    /* Now get the disk_name */
    node_name = smprintf ("/nkn/nvsd/diskcache/monitor/disk_id/%s/diskname", diskid);

    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
                        &t_diskname, node_name, NULL);
    bail_error(err);

   /*comp_node = smprintf("/nkn/nvsd/diskcache/config/disk_id/%s/metadata_comp_per",ts_str(t_diskname));

    err = mdc_get_binding_tstr (cli_mcc, NULL, NULL, NULL,
                        &t_compvalue, comp_node, NULL);

    cmd_str = smprintf("media-cache disk %s metadata compression %s", ts_str(t_diskname),
                       ts_str(t_compvalue));*/

    err = tstr_array_append_str(ret_reply->crr_nodes, name);
    bail_error(err);
    err = tstr_array_append_str_takeover(ret_reply->crr_cmds, &cmd_str);
    bail_error(err);

bail:
    safe_free(node_name);
    safe_free(comp_node);
    safe_free(cmd_str); /* Only in error case */
    return(err);
}
#endif

int cli_nvsd_diskcontroller(void *data, const cli_command *cmd, const tstr_array *cmd_line, const tstr_array *params)
{
    int err = 0;
    int32 status = 0;
    tstring *ret_output = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    switch(cmd->cc_magic)
    {
        case cid_3ware_controller:
            err = lc_launch_quick_status(&status, &ret_output, true, 2, "/opt/nkn/bin/tw_cli", "show");
	    bail_error(err);
	    err = cli_printf(" %s\n", ts_str(ret_output));
        bail_error(err);
    }
bail :
    return(err);
}

static int
cli_nvsd_diskcache_sas_inwm_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *t_sas_watermark_high = NULL;
    tstring *t_sas_watermark_low = NULL;
    tstring *rev_cmd = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(name_parts);
    
    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/watermark/internal/sas/high",
            NULL, &t_sas_watermark_high);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/watermark/internal/sas/low",
            NULL, &t_sas_watermark_low);
    bail_error(err);

    err = ts_new_str(&rev_cmd, "media-cache internal-eviction sas watermark");
    bail_error(err);

    err = ts_append_sprintf(rev_cmd, " %s %s", ts_str(t_sas_watermark_high), ts_str(t_sas_watermark_low));
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/watermark/internal/sas/high");
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/watermark/internal/sas/low");
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

bail:
	ts_free(&t_sas_watermark_high);
	ts_free(&t_sas_watermark_low);
	ts_free(&rev_cmd);
    return err;
}


static int
cli_nvsd_diskcache_sata_inwm_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *t_sata_watermark_high = NULL;
    tstring *t_sata_watermark_low = NULL;
    tstring *rev_cmd = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(name_parts);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/watermark/internal/sata/high",
            NULL, &t_sata_watermark_high);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/watermark/internal/sata/low",
            NULL, &t_sata_watermark_low);
    bail_error(err);

    err = ts_new_str(&rev_cmd, "media-cache internal-eviction sata watermark");
    bail_error(err);

    err = ts_append_sprintf(rev_cmd, " %s %s", ts_str(t_sata_watermark_high), ts_str(t_sata_watermark_low));
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/watermark/internal/sata/high");
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/watermark/internal/sata/low");
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

bail:
	ts_free(&t_sata_watermark_high);
	ts_free(&t_sata_watermark_low);
	ts_free(&rev_cmd);
    return err;
}

static int
cli_nvsd_diskcache_ssd_inwm_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *t_ssd_watermark_high = NULL;
    tstring *t_ssd_watermark_low = NULL;
    tstring *rev_cmd = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(name_parts);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/watermark/internal/ssd/high",
            NULL, &t_ssd_watermark_high);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/watermark/internal/ssd/low",
            NULL, &t_ssd_watermark_low);
    bail_error(err);

    err = ts_new_str(&rev_cmd, "media-cache internal-eviction ssd watermark");
    bail_error(err);

    err = ts_append_sprintf(rev_cmd, " %s %s", ts_str(t_ssd_watermark_high), ts_str(t_ssd_watermark_low));
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/watermark/internal/ssd/high");
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/watermark/internal/ssd/low");
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

bail:
	ts_free(&t_ssd_watermark_high);
	ts_free(&t_ssd_watermark_low);
	ts_free(&rev_cmd);
    return err;
}

static int
cli_nvsd_diskcache_dict_inwm_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *t_watermark_high = NULL;
    tstring *t_watermark_low = NULL;
    tstring *rev_cmd = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(name_parts);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/watermark/internal/dict/high",
            NULL, &t_watermark_high);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/watermark/internal/dict/low",
            NULL, &t_watermark_low);
    bail_error(err);

    err = ts_new_str(&rev_cmd, "media-cache internal-eviction dictionary watermark");
    bail_error(err);

    err = ts_append_sprintf(rev_cmd, " %s %s", ts_str(t_watermark_high), ts_str(t_watermark_low));
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/watermark/internal/dict/high");
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/watermark/internal/dict/low");
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

bail:
	ts_free(&t_watermark_high);
	ts_free(&t_watermark_low);
	ts_free(&rev_cmd);
    return err;
}


static int
cli_nvsd_diskcache_sas_exwm_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *t_sas_watermark_high = NULL;
    tstring *t_sas_watermark_low = NULL;
    tstring *rev_cmd = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(name_parts);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/watermark/external/sas/high",
            NULL, &t_sas_watermark_high);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/watermark/external/sas/low",
            NULL, &t_sas_watermark_low);
    bail_error(err);

    err = ts_new_str(&rev_cmd, "media-cache external-eviction sas watermark");
    bail_error(err);

    err = ts_append_sprintf(rev_cmd, " %s %s", ts_str(t_sas_watermark_high), ts_str(t_sas_watermark_low));
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/watermark/external/sas/high");
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/watermark/external/sas/low");
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

bail:
	ts_free(&t_sas_watermark_high);
	ts_free(&t_sas_watermark_low);
	ts_free(&rev_cmd);
    return err;
}


static int
cli_nvsd_diskcache_sata_exwm_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *t_sata_watermark_high = NULL;
    tstring *t_sata_watermark_low = NULL;
    tstring *rev_cmd = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(name_parts);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/watermark/external/sata/high",
            NULL, &t_sata_watermark_high);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/watermark/external/sata/low",
            NULL, &t_sata_watermark_low);
    bail_error(err);

    err = ts_new_str(&rev_cmd, "media-cache external-eviction sata watermark");
    bail_error(err);

    err = ts_append_sprintf(rev_cmd, " %s %s", ts_str(t_sata_watermark_high), ts_str(t_sata_watermark_low));
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/watermark/external/sata/high");
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/watermark/external/sata/low");
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

bail:
	ts_free(&t_sata_watermark_high);
	ts_free(&t_sata_watermark_low);
	ts_free(&rev_cmd);
    return err;
}

static int
cli_nvsd_diskcache_ssd_exwm_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *t_ssd_watermark_high = NULL;
    tstring *t_ssd_watermark_low = NULL;
    tstring *rev_cmd = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(name_parts);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/watermark/external/ssd/high",
            NULL, &t_ssd_watermark_high);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/watermark/external/ssd/low",
            NULL, &t_ssd_watermark_low);
    bail_error(err);

    err = ts_new_str(&rev_cmd, "media-cache external-eviction ssd watermark");
    bail_error(err);

    err = ts_append_sprintf(rev_cmd, " %s %s", ts_str(t_ssd_watermark_high), ts_str(t_ssd_watermark_low));
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/watermark/external/ssd/high");
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/watermark/external/ssd/low");
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

bail:
	ts_free(&t_ssd_watermark_high);
	ts_free(&t_ssd_watermark_low);
	ts_free(&rev_cmd);
    return err;
}

int
cli_nvsd_diskcache_evict_wm_external(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err=0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    cli_printf_query(_("Tier\tHi\tLo\nSSD\t#/nkn/nvsd/diskcache/config/watermark/external/ssd/high#\t"
    		"#/nkn/nvsd/diskcache/config/watermark/external/ssd/low#\nSAS\t"
    		"#/nkn/nvsd/diskcache/config/watermark/external/sas/high#\t"
    		"#/nkn/nvsd/diskcache/config/watermark/external/sas/low#\nSATA\t"
    		"#/nkn/nvsd/diskcache/config/watermark/external/sata/high#\t"
    		"#/nkn/nvsd/diskcache/config/watermark/external/sata/low#\n"));

    return err;
}

static int
cli_nvsd_set_time_based_int_evict_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{

    const char *ns_name = NULL;
    const char *header = NULL;
    tstring *t_header = NULL;
    int err = 0;
    bn_request *req = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    header = tstr_array_get_str_quick(params, 1);
    bail_null(header);
   
    err = ts_new_sprintf(&t_header, "%s",header);
    bail_error_null(err,t_header);

    err = ts_trim_whitespace(t_header);
    bail_error(err);

    if(strncmp(ts_str(t_header),"",1)==0) {
        cli_printf_error("error: validity-begin-header should not be empty");
        goto bail; 
    }
    
    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
            bt_string, 0, header, NULL, "/nkn/nvsd/origin_fetch/config/%s/object/validity/start_header_name", ns_name);
    bail_error(err);

    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);
bail:
    ts_free(&t_header);
    return err;



}

int
cli_nvsd_diskcache_sata_timed_evict_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *t_lwm = NULL;
    tstring *t_evict_time = NULL;
    tstring *rev_cmd = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(name_parts);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/internal/watermark/sata/evict_time",
            NULL, &t_evict_time);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/internal/watermark/sata/time_low_pct",
            NULL, &t_lwm);
    bail_error(err);

    err = ts_new_str(&rev_cmd, "media-cache internal-eviction sata start-time");
    bail_error(err);

    err = ts_append_sprintf(rev_cmd, " %s watermark %s", ts_str(t_evict_time), ts_str(t_lwm));
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/internal/watermark/sata/evict_time");
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/internal/watermark/sata/time_low_pct");
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

bail:
        ts_free(&t_lwm);
        ts_free(&t_evict_time);
        ts_free(&rev_cmd);
    return err;

}

int
cli_nvsd_diskcache_sas_timed_evict_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *t_lwm = NULL;
    tstring *t_evict_time = NULL;
    tstring *rev_cmd = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(name_parts);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/internal/watermark/sas/evict_time",
            NULL, &t_evict_time);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/internal/watermark/sas/time_low_pct",
            NULL, &t_lwm);
    bail_error(err);

    err = ts_new_str(&rev_cmd, "media-cache internal-eviction sas start-time");
    bail_error(err);

    err = ts_append_sprintf(rev_cmd, " %s watermark %s", ts_str(t_evict_time), ts_str(t_lwm));
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/internal/watermark/sas/evict_time");
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/internal/watermark/sas/time_low_pct");
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

bail:
        ts_free(&t_lwm);
        ts_free(&t_evict_time);
        ts_free(&rev_cmd);
    return err;

}

int
cli_nvsd_diskcache_ssd_timed_evict_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *t_lwm = NULL;
    tstring *t_evict_time = NULL;
    tstring *rev_cmd = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(name_parts);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/internal/watermark/ssd/evict_time",
            NULL, &t_evict_time);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/diskcache/config/internal/watermark/ssd/time_low_pct",
            NULL, &t_lwm);
    bail_error(err);

    err = ts_new_str(&rev_cmd, "media-cache internal-eviction ssd start-time");
    bail_error(err);

    err = ts_append_sprintf(rev_cmd, " %s watermark %s", ts_str(t_evict_time), ts_str(t_lwm));
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/internal/watermark/ssd/evict_time");
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/diskcache/config/internal/watermark/ssd/time_low_pct");
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

bail:
        ts_free(&t_lwm);
        ts_free(&t_evict_time);
        ts_free(&rev_cmd);
    return err;

}


/*---------------------------------------------------------------------*/
/* End of cli_nvsd_diskcache_cmds.c */
