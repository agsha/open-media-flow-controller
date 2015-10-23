/*
 * @file jpsd_mgmt.c
 * @brief
 * jpsd_mgmt.c - definations for jpsd management functions
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include <sys/stat.h>

#include "md_client.h"
#include "mdc_wrapper.h"
#include "bnode.h"
#include "signal_utils.h"
#include "libevent_wrapper.h"
#include "random.h"
#include "dso.h"

#include "nkn_memalloc.h"
#include "nkn_mgmt_defs.h"
#include "jpsd_tdf.h"

#define UNUSED_ARGUMENT(x) (void)x

const char jpsd_tdf_domain_prefix[] = "/nkn/jpsd/tdf/domain";
const char jpsd_tdf_domain_rule_prefix[] = "/nkn/jpsd/tdf/domain-rule";

tbool mgmt_exiting = false;
int update_from_db_done = 0;
const char *program_name = "jpsd";

lew_context *jpsd_mgmt_lew = NULL;
md_client_context *mgmt_mcc = NULL;

static const int signals_handled[] = {
            SIGHUP, SIGINT, SIGPIPE, SIGTERM, SIGUSR1
};
#define num_signals_handled (int)(sizeof(signals_handled) / sizeof(int))

static lew_event *event_signals[24];
static pthread_mutex_t upload_mgmt_log_lock;

static int jpsd_mgmt_initiate_exit(void);
void jpsd_mgmt_thrd_init(void);

static int jpsd_tdf_domain_rule_delete_cfg_handle_change(const bn_binding_array *arr,
				uint32 idx, bn_binding *binding, void *data);
static int jpsd_tdf_domain_rule_cfg_handle_change(const bn_binding_array *arr,
				uint32 idx, bn_binding *binding, void *data);

static int jpsd_tdf_domain_delete_cfg_handle_change(const bn_binding_array *arr,
				uint32 idx, bn_binding *binding, void *data);
static int jpsd_tdf_domain_cfg_handle_change(const bn_binding_array *arr,
				uint32 idx, bn_binding *binding, void *data);

static void catcher(int sig)
{
        switch (sig) {
	case SIGINT:
	case SIGTERM:
		jpsd_mgmt_initiate_exit();
		break;
	case SIGHUP:
	default:
		exit(1);
        }

        return;
}

static int log_basic(const char *fmt, ...)
{
	int n = 0;
	char outstr[1024];
	va_list ap;

	va_start(ap, fmt);
	n = vsprintf(outstr, fmt, ap);
	lc_log_basic(LOG_NOTICE, "%s", outstr);
	va_end(ap);

	return n;
}

static int log_debug(const char *fmt, ...)
{
	int n = 0;
	char outstr[1024];
	va_list ap;

	va_start(ap, fmt);
	n = vsprintf(outstr, fmt, ap);
	lc_log_basic(LOG_INFO, "%s", outstr);
	va_end(ap);

	return n;
}

static int jpsd_deinit(void)
{
	int err = 0;

	err = mdc_wrapper_deinit(&mgmt_mcc);
	bail_error(err);
	mgmt_mcc = NULL;

	err = lew_deinit(&jpsd_mgmt_lew);
	bail_error(err);

	jpsd_mgmt_lew = NULL;

bail:
	return err;
}

static int jpsd_mgmt_main_loop(void)
{
	int err = 0;
	lc_log_debug(LOG_DEBUG, _("jpsd_mgmt_main_loop () - before lew_dispatch"));

	err = lew_dispatch (jpsd_mgmt_lew, NULL, NULL);
	bail_error(err);
bail:
	return err;
}

static int jpsd_mgmt_initiate_exit(void)
{
	int err = 0;
	int i;

	mgmt_exiting = true;

	for (i = 0; i < num_signals_handled; ++i) {
		err = lew_event_delete(jpsd_mgmt_lew, &(event_signals[i]));
		event_signals[i] = NULL;
		bail_error(err);
	}

	err = mdc_wrapper_disconnect(mgmt_mcc, false);
	bail_error(err);

	err = lew_escape_from_dispatch(jpsd_mgmt_lew, true);
	bail_error(err);

bail:
	return err;
}

static int jpsd_mgmt_handle_session_close(int fd,
        fdic_callback_type cbt, void *vsess, void *gsec_arg)
{
        int err = 0;
        gcl_session *gsec_session = vsess;

        if (mgmt_exiting) {
                goto bail;
        }

        lc_log_basic(LOG_ERR, _("Lost connection to mgmtd; exiting"));
        pthread_mutex_lock (&upload_mgmt_log_lock);
        err = jpsd_mgmt_initiate_exit();
        pthread_mutex_unlock (&upload_mgmt_log_lock);
        bail_error(err);

bail:
        return err;
}

static int jpsd_mgmt_module_cfg_handle_change(
			bn_binding_array *old_bindings,
			bn_binding_array *new_bindings)
{
        int err = 0;
        tbool rechecked_licenses = false;

	err = bn_binding_array_foreach(new_bindings, jpsd_tdf_domain_rule_cfg_handle_change,
                            &rechecked_licenses);
        bail_error(err);

	err = bn_binding_array_foreach(old_bindings, jpsd_tdf_domain_rule_delete_cfg_handle_change,
                            &rechecked_licenses);
        bail_error(err);

	err = bn_binding_array_foreach(new_bindings, jpsd_tdf_domain_cfg_handle_change,
                            &rechecked_licenses);
        bail_error(err);

	err = bn_binding_array_foreach(old_bindings, jpsd_tdf_domain_delete_cfg_handle_change,
                            &rechecked_licenses);
        bail_error(err);

bail:
        return err;
}

static int jpsd_config_handle_set_request(
        bn_binding_array *old_bindings,
        bn_binding_array *new_bindings)
{
        int err = 0;
        err = jpsd_mgmt_module_cfg_handle_change(old_bindings, new_bindings);
        bail_error(err);
bail:
        return err;
}

static int jpsd_mgmt_handle_event_request(gcl_session *session,
                    bn_request **inout_request, void *arg)
{
        int err = 0;
        bn_msg_type msg_type = bbmt_none;
        bn_binding_array *old_bindings = NULL, *new_bindings = NULL;
        tstring *event_name = NULL;
        bn_binding_array *bindings = NULL;
        uint32 num_callbacks = 0, i = 0;
        void *data = NULL;

        bail_null(inout_request);
        bail_null(*inout_request);

        err = bn_request_get(*inout_request, &msg_type,
                        NULL, false, &bindings, NULL, NULL);
        bail_error(err);

        bail_require(msg_type == bbmt_event_request);

        err = bn_event_request_msg_get_event_name(*inout_request, &event_name);
        bail_error_null(err, event_name);

        if (ts_equal_str(event_name, mdc_event_dbchange_cleartext, false)) {
                err = mdc_event_config_change_notif_extract(
                                bindings,
                                &old_bindings,
                                &new_bindings,
                                NULL);
                bail_error(err);

                err = jpsd_config_handle_set_request(old_bindings, new_bindings);
                bail_error(err);
        }else {
                lc_log_debug(LOG_DEBUG, I_("Received unexpected event %s"),
                                ts_str(event_name));
        }

bail:
        bn_binding_array_free(&bindings);
        bn_binding_array_free(&old_bindings);
        bn_binding_array_free(&new_bindings);
        ts_free(&event_name);

        return err;
}

static int jpsd_mgmt_event_registeration_init(void)
{
        int err = 0;
        mdc_wrapper_params *mwp = NULL;

        err = mdc_wrapper_params_get_defaults(&mwp);
        bail_error(err);

        mwp->mwp_lew_context = jpsd_mgmt_lew;
        mwp->mwp_gcl_client = gcl_client_ssld;

        mwp->mwp_session_close_func = jpsd_mgmt_handle_session_close;
        mwp->mwp_event_request_func = jpsd_mgmt_handle_event_request;

        err = mdc_wrapper_init(mwp, &mgmt_mcc);
        bail_error(err);

        err = lc_random_seed_nonblocking_once();
        bail_error(err);

        /*! accesslog binding registeration */
        err = mdc_send_action_with_bindings_str_va(
                        mgmt_mcc,
                        NULL,
                        NULL,
                        "/mgmtd/events/actions/interest/add",
                        1,
                        "event_name", bt_name, mdc_event_dbchange_cleartext,
                        "match/1/pattern", bt_string, "/nkn/jpsd/config/**");
        bail_error(err);

bail:
        mdc_wrapper_params_free(&mwp);
        if (err) {
                lc_log_debug(LOG_ERR,
                                _("Unable to connect to the management daemon"));
        }

        return err;
}

static int handle_signal(int signum,
        short ev_type, void *data, lew_context *ctxt)
{
    (void) ev_type;
    (void) data;
    (void) ctxt;

    catcher(signum);
    return 0;
}

static domain_rule_t *find_domain_rule(const char *name)
{
        int i;

        for (i = 0; i < MAX_DOMAIN_RULE; i++) {
                if (g_domain_rule[i].active) {
                        if (strcmp(g_domain_rule[i].name, name) == 0) {
                                return &g_domain_rule[i];
                        }
                }
        }

	return NULL;
}

static domain_rule_t *get_domain_rule(const char *name)
{
	int i;
	domain_rule_t *domain_rule;

        domain_rule = find_domain_rule(name);
        if (domain_rule != NULL) {
                return domain_rule;
        }

        for (i = 0; i < MAX_DOMAIN_RULE; i++) {
                if (g_domain_rule[i].active == 0) {
			strcpy(g_domain_rule[i].name, name);
			g_domain_rule[i].len = strlen(g_domain_rule[i].name);
			g_domain_rule[i].active = 1;
                        return &g_domain_rule[i];
                }
        }

        return NULL;
}

static int jpsd_tdf_domain_rule_delete_cfg_handle_change(const bn_binding_array *arr,
				uint32 idx, bn_binding *binding, void *data)
{
	int err;
        const tstring *name = NULL;
        const char *t_domain_rule = NULL;
        domain_rule_t *domain_rule = NULL;
	tstr_array *name_parts = NULL;
        tbool *rechecked_licenses = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

        if (bn_binding_name_pattern_match(ts_str(name), "/nkn/jpsd/tdf/domain-rule/*")) {
                bn_binding_get_name_parts(binding, &name_parts);
                bail_error_null(err, name_parts);

                t_domain_rule = tstr_array_get_str_quick(name_parts, 4);
                bail_error(err);

                domain_rule = get_domain_rule(t_domain_rule);
                if (domain_rule == NULL) goto bail;
		memset(domain_rule, 0, sizeof(domain_rule_t));
        } else {
                goto bail;
        }

bail:
        tstr_array_free(&name_parts);
        return err;
}

static int jpsd_tdf_domain_rule_cfg_handle_change(const bn_binding_array *arr,
				uint32 idx, bn_binding *binding, void *data)
{
	int err;
        const tstring *name = NULL;
        const char *t_domain_rule = NULL;
        domain_rule_t *domain_rule = NULL;
	tstr_array *name_parts = NULL;
        tbool *rechecked_licenses = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

        if (bn_binding_name_pattern_match(ts_str(name), "/nkn/jpsd/tdf/domain-rule/**")) {
                bn_binding_get_name_parts(binding, &name_parts);
                bail_error_null(err, name_parts);

                t_domain_rule = tstr_array_get_str_quick(name_parts, 4);
                bail_error(err);

                log_debug("Read .../nkn/jpsd/tdf/domain-rule as : \"%s\"", t_domain_rule);
                domain_rule = get_domain_rule(t_domain_rule);
                if (domain_rule == NULL) goto bail;
        } else {
                goto bail;
        }

        if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*", "precedence")) {
                uint32_t precedence;
                err = bn_binding_get_uint32(binding, ba_value,
                                        NULL, &precedence);
                bail_error(err);
                domain_rule->precedence = precedence;
        } else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*", "uplink-vrf")) {
                char *uplink_vrf = NULL;
                err = bn_binding_get_str(binding, ba_value,
                                        bt_string, NULL, &uplink_vrf);
		if (uplink_vrf) {
			if (strcmp(uplink_vrf, "-") == 0) {
				domain_rule->uplink_vrf_len = 0;
			} else {
				strcpy(domain_rule->uplink_vrf, uplink_vrf);
				domain_rule->uplink_vrf_len = strlen(domain_rule->uplink_vrf);
			}
		}
                safe_free(uplink_vrf);
        } else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*", "downlink-vrf")) {
                char *downlink_vrf = NULL;
                err = bn_binding_get_str(binding, ba_value,
                                        bt_string, NULL, &downlink_vrf);
		if (downlink_vrf) {
			if (strcmp(downlink_vrf, "-") == 0) {
				domain_rule->downlink_vrf_len = 0;
			} else {
				strcpy(domain_rule->downlink_vrf, downlink_vrf);
				domain_rule->downlink_vrf_len = strlen(domain_rule->downlink_vrf);
			}
		}
                safe_free(downlink_vrf);
	}

bail:
        tstr_array_free(&name_parts);
        return err;
}

static domain_t *find_domain(const char *name)
{
        int i;

        for (i = 0; i < MAX_DOMAIN; i++) {
                if (g_domain[i].active) {
                        if (strcmp(g_domain[i].name, name) == 0) {
                                return &g_domain[i];
                        }
                }
        }

        return NULL;
}

static domain_t *get_domain(const char *name)
{
        int i = 0;
        int j = 0;
        domain_t *domain;
	int len;

	len = strlen(name);
        if (domain_lookup(name, len, &domain)) {
                return domain;
	}

        for (i = 0; i < MAX_DOMAIN; i++) {
                if (g_domain[i].active == 0) {
			strcpy(g_domain[i].name, name);
			g_domain[i].len = len;
			g_domain[i].active = 1;
                        return &g_domain[i];
                }
        }

        return NULL;
}

static int jpsd_tdf_domain_delete_cfg_handle_change(const bn_binding_array *arr,
				uint32 idx, bn_binding *binding, void *data)
{
	int err;
        const tstring *name = NULL;
        const char *t_domain = NULL;
        domain_t *domain = NULL;
	tstr_array *name_parts = NULL;
        tbool *rechecked_licenses = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

        if (bn_binding_name_pattern_match(ts_str(name), "/nkn/jpsd/tdf/domain/*")) {
		int incarn;
                bn_binding_get_name_parts(binding, &name_parts);
                bail_error_null(err, name_parts);

                t_domain = tstr_array_get_str_quick(name_parts, 4);
                bail_error(err);
                domain = get_domain(t_domain);
		if (domain == NULL) goto bail;
		delete_domain(domain);
		pthread_rwlock_wrlock(&domain->rwlock);
		domain->name[0] = '\0';
		domain->active = 0;
		domain->incarn++;
		memset(&domain->rule, 0, sizeof(domain_rule_t));
		pthread_rwlock_unlock(&domain->rwlock);
        } else {
                goto bail;
        }

bail:
        tstr_array_free(&name_parts);
        return err;
}

static int jpsd_tdf_domain_cfg_handle_change(const bn_binding_array *arr,
				uint32 idx, bn_binding *binding, void *data)
{
	int err;
	int i;
	char *rule = NULL;
        const tstring *name = NULL;
        const char *t_domain = NULL;
        domain_t *domain = NULL;
	tstr_array *name_parts = NULL;
        tbool *rechecked_licenses = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

        if (bn_binding_name_pattern_match(ts_str(name), "/nkn/jpsd/tdf/domain/**")) {
                bn_binding_get_name_parts(binding, &name_parts);
                bail_error_null(err, name_parts);

		if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*", "rule")) {
			t_domain = tstr_array_get_str_quick(name_parts, 4);
			bail_error(err);
			err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &rule);
			if (rule == NULL) goto bail;
			log_debug("Read .../nkn/jpsd/tdf/domain as : \"%s\"", t_domain);
			domain = get_domain(t_domain);
			for (i = 0; i < MAX_DOMAIN_RULE; i++) {
				if (g_domain_rule[i].active &&
					(strcmp(g_domain_rule[i].name, rule) == 0)) {
					memcpy(&domain->rule, &g_domain_rule[i],
								sizeof(domain->rule));
					break;
				}
			}
			safe_free(rule);
			insert_domain(domain);
		} else {
			goto bail;
		}


        }

bail:
        tstr_array_free(&name_parts);
        return err;
}

static int jpsd_tdf_cfg_query(void)
{
	int err = 0;
	tbool rechecked_licenses = false;
	bn_binding_array *bindings = NULL;

	log_debug("jpsd tdf mgmtd query initializing ...");

	err = mdc_get_binding_children_ex(mgmt_mcc,
				NULL,
				NULL,
				true,
				&bindings,
				bqnf_sel_iterate_subtree | bqnf_cleartext,
				jpsd_tdf_domain_rule_prefix,
				NULL, 0, NULL);
	bail_error_null(err, bindings);

	err = bn_binding_array_foreach(bindings, jpsd_tdf_domain_rule_cfg_handle_change,
				&rechecked_licenses);

        bn_binding_array_free(&bindings);
        bail_error(err);

	err = mdc_get_binding_children_ex(mgmt_mcc,
				NULL,
				NULL,
				true,
				&bindings,
				bqnf_sel_iterate_subtree | bqnf_cleartext,
				jpsd_tdf_domain_prefix,
				NULL, 0, NULL);
	bail_error_null(err, bindings);

	err = bn_binding_array_foreach(bindings, jpsd_tdf_domain_cfg_handle_change,
				&rechecked_licenses);

        bn_binding_array_free(&bindings);
        bail_error(err);

bail:
	bn_binding_array_free(&bindings);
        return err;
}

static int jpsd_mgmt_init(int init_value)
{
        int err = 0;
        uint32 i = 0;

        err = lc_log_init(program_name, NULL, LCO_none,
            LC_COMPONENT_ID(LCI_SSLD),
            LOG_DEBUG, LOG_LOCAL4, LCT_SYSLOG);
        bail_error(err);

        err = lew_init(&jpsd_mgmt_lew);
        bail_error(err);

        for (i = 0; i < (int) num_signals_handled; ++i) {
                err = lew_event_reg_signal(jpsd_mgmt_lew, &(event_signals[i]),
				signals_handled[i], handle_signal, NULL);
                bail_error(err);
        }

        err = jpsd_mgmt_event_registeration_init();
        bail_error(err);


	err = jpsd_tdf_cfg_query();
	bail_error(err);

bail:
        if (err) {
                /* Safe to call all the exits */
                jpsd_mgmt_initiate_exit();

                /* Ensure we set the flag back */
                mgmt_exiting = false;
        }

        /* Enable the flag to indicate config init is done */
        update_from_db_done = 1;

        return err;
}

static void jpsd_tdf_domain_init(void)
{
        int i;
	int size;

	for (i = 0; i < MAX_DOMAIN_HASH; i++) {
		TAILQ_INIT(&g_tdf_domain[i]);
	}

	size = sizeof(domain_t) * MAX_DOMAIN_RULE;
	g_domain = nkn_calloc_type(1, size, mod_diameter_t);
	if (g_domain == NULL) assert(0);

        for (i = 0; i < MAX_DOMAIN_RULE; i++) {
		g_domain[i].incarn = 1;
                pthread_rwlock_init(&g_domain[i].rwlock, NULL);
        }

	size = sizeof(domain_rule_t) * MAX_DOMAIN_RULE;
        g_domain_rule = nkn_calloc_type(1, size, mod_diameter_t);
        if (g_domain_rule == NULL) assert(0);

        return;
}

static void *jpsd_mgmt_thrd(void * arg)
{
        int err = 0;
	int i;

        int start = 1;

	jpsd_tdf_domain_init();

        while (1) {
                mgmt_exiting = false;
                printf("mgmt connect: initialized\n");
                if (start == 1) {
                        err = jpsd_mgmt_init(1);
                        bail_error(err);
                } else {
                        while ((err = jpsd_mgmt_init(0)) != 0) {
                                jpsd_deinit();
                                sleep(2);
                        }
                }

                err = jpsd_mgmt_main_loop();
                complain_error(err);

                err = jpsd_deinit();
                complain_error(err);
                printf("mgmt connect: de-initialized\n");
                start = 0;
        }
bail:
        printf("jpsd_mgmt_thrd: exit\n");

        return NULL;
}

void jpsd_mgmt_thrd_init(void)
{
	pthread_t mgmtid;

	pthread_mutex_init(&upload_mgmt_log_lock, NULL);
	pthread_create(&mgmtid, NULL, jpsd_mgmt_thrd, NULL);

	return;
}
