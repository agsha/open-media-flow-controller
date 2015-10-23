/*
 *
 * Filename:  web_config_form_nsconfig.c
 * Date:      2009/12/11
 * Author:    Varsha R
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#include <string.h>
#include "common.h"
#include "dso.h"
#include "web.h"
#include "web_config_form.h"
#include "web_internal.h"
#include "md_client.h"

/*-----------------------------------------------------------------------------
 * CONSTANTS
 */
enum {
HTTP=1,
NFS=2,
};
static const char wcf_action_name[] = "config-form-nsconfig";
static const char wcf_namespace_prefix[] = "/nkn/nvsd/namespace";
static const char wcf_namespace_active[] = "/nkn/nvsd/namespace/%s/status/active";
static const char wcf_virtual_player[] = "/nkn/nvsd/namespace/%s/virtual_player";
/*
 * Origin server related variables
 */
static const char wcf_field_originserver[] = "f_originserver";
static const char wcf_id_oshttphost[] = "oshttphost";  
static const char wcf_id_oshttpservermap[] = "oshttpservermap";  
static const char wcf_id_oshttpabsurl[] = "oshttpabsurl";  
static const char wcf_id_oshttpfollowheader[] = "oshttpfollowheader";  
static const char wcf_id_osnfshost[] = "osnfshost";  
static const char wcf_id_osnfsservermap[] = "osnfsservermap";  

static const char wcf_field_namespace[] = "f_namespace";
static const char wcf_field_oshttphost[] = "f_httphostname";  
static const char wcf_field_oshttpport[] = "f_httpport";  
static const char wcf_field_oshttpservermap[] = "f_httpservermap";  
static const char wcf_field_oshttpfollowheader[] = "f_httpfollowheader";  
static const char wcf_field_osnfshost[] = "f_nfshost";  
static const char wcf_field_osnfsport[] = "f_nfsport";  
static const char wcf_field_osnfsservermap[] = "f_nfsservermap";  
/*
 * Match details related variables
 */
static const char wcf_field_matchdetails[] = "f_matchtype";
static const char wcf_id_uriname[] = "uriname";
static const char wcf_id_uriregex[] = "uriregex";
static const char wcf_id_headernamevalue[] = "headernamevalue";
static const char wcf_id_headerregex[] = "headerregex";
static const char wcf_id_querynamevalue[] = "querynamevalue";
static const char wcf_id_queryregex[] = "queryregex";
static const char wcf_id_virtualip[] = "virtualip";
static const char wcf_field_uriname[] = "f_uriname";
static const char wcf_field_uriregex[] = "f_uriregex";
static const char wcf_field_headername[] = "f_headername";
static const char wcf_field_headervalue[] = "f_headervalue";
static const char wcf_field_headerregex[] = "f_headerregex";
static const char wcf_field_queryname[] = "f_queryname";
static const char wcf_field_queryvalue[] = "f_queryvalue";
static const char wcf_field_queryregex[] = "f_queryregex";
static const char wcf_field_virtualip[] = "f_virtualip";
static const char wcf_field_virtualport[] = "f_virtualport";
static const char wcf_field_precedence[] = "f_precedence";

static const char wcf_field_rtspmatchdetails[] = "f_rtsp_matchtype";
static const char wcf_id_rtspuriname[] = "rtsp_uriname";
static const char wcf_id_rtspuriregex[] = "rtsp_uriregex";
static const char wcf_id_rtspvirtualip[] = "rtsp_virtualip";
static const char wcf_field_rtspuriname[] = "f_rtsp_uriname";
static const char wcf_field_rtspuriregex[] = "f_rtsp_uriregex";
static const char wcf_field_rtspvirtualip[] = "f_rtsp_virtualip";
static const char wcf_field_rtspvirtualport[] = "f_rtsp_virtualport";
static const char wcf_field_rtspprecedence[] = "f_rtsp_precedence";
/*
 * Domain configuration
 */
static const char wcf_field_domain[] = "f_domainname";
static const char wcf_id_domainhost[] = "host";
static const char wcf_id_domainregex[] = "regex";
static const char wcf_field_domainhost[] = "f_host";
static const char wcf_field_domainregex[] = "f_regex";

static const char wcf_btn_add[] = "add";
static const char wcf_btn_matchadd[] = "matchadd";
static const char wcf_btn_rtspadd[] = "rtspadd";
static const char wcf_field_virtual_player[] = "f_virtual_player";
/*-----------------------------------------------------------------------------
 * PROTOTYPES
 */

static int wcf_handle_add(void);
static int wcf_handle_matchadd(void);
static int wcf_handle_rtspadd(void);
static web_action_result wcf_nsconfig_form_handler(web_context *ctx,
                                                void *param);

int web_config_form_nsconfig_init(const lc_dso_info *info, void *data);

/*-----------------------------------------------------------------------------
 * IMPLEMENTATION
 */

static int
wcf_handle_add(void)
{
    const bn_response *resp = NULL;
    bn_binding *binding = NULL;
    bn_request *req = NULL;
    const tstring *om_type_str = NULL;
    const tstring *namespace_value = NULL;
    const tstring *uri_value = NULL;
    const tstring *domain_value = NULL;
    const tstring *http_value = NULL;
    const tstring *nfs_value = NULL;
    const tstring *originserver_value = NULL;
    const tstring *httporiginserver_value = NULL;
    const tstring *nfsoriginserver_value = NULL;
    const tstring *virtual_player_value = NULL;
    tstring *msg = NULL;
    tstring *namespace_fixed = NULL;
    tstring *uri_fixed = NULL;
    char *node_name = NULL;
    char *gid_str = NULL;
    uint32 gid = 0;
    uint32 ret_err = 0;
    int err = 0;
    char *uri_value_esc;
    uint32 om_type = 0;
    const tstring *action_value = NULL;
    const tstring *domain_type = NULL;
    char *bn_origin_server = NULL;
    char *bn_origin_server_port = NULL;
    char *bn_domain_host = NULL;
    char *bn_domain_regex = NULL;
    const tstring *hostname = NULL;
    const tstring *port = NULL;
    const tstring *domain = NULL;
    /*! Reading Namespace field */
    err = web_get_request_param_str(g_web_data, wcf_field_namespace, ps_post_data,
                                    &namespace_value);
    bail_error(err);
    if (namespace_value == NULL || ts_num_chars(namespace_value) <= 0) {
        err = ts_new_str(&msg, _("No namespace specified for the new entry."));
        bail_error(err);
        ret_err = 1;
        goto check_error;
    }
    err = web_get_request_param_str(g_web_data,wcf_field_originserver, ps_post_data,
		    &action_value);
    bail_error_null(err,action_value);
    if( (ts_equal_str(action_value, wcf_id_oshttphost,true))){
	bn_origin_server= smprintf("/nkn/nvsd/namespace/%s/origin-server/http/host/name",ts_str(namespace_value));    
	bail_null(bn_origin_server);
	err = web_get_request_param_str(g_web_data, wcf_field_oshttphost, ps_post_data,
			&hostname);
        bail_error(err);
        if (hostname == NULL || ts_num_chars(hostname) <= 0) {
        	err = ts_new_str(&msg, _("Please enter valid Hostname."));
       		bail_error(err);
	        ret_err = 1;
        	goto check_error;
        }	
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_string,ts_str(hostname),
		        bn_origin_server);
	bail_error(err);
	if(ret_err) {
		goto check_error;	
	}
	bn_origin_server_port = smprintf("/nkn/nvsd/namespace/%s/origin-server/http/host/port",ts_str(namespace_value));    
	bail_null(bn_origin_server_port);
	err = web_get_request_param_str(g_web_data, wcf_field_oshttpport, ps_post_data,
			&port);
        bail_error_null(err,port);
	err = mdc_set_binding(web_mcc, NULL,&msg,
			bsso_modify,bt_uint16,ts_str(port),
		        bn_origin_server_port);
	bail_error(err);

    }else if ( ts_equal_str(action_value, wcf_id_oshttpservermap,true)) {
	bn_origin_server= smprintf("/nkn/nvsd/namespace/%s/origin-server/http/server-map",ts_str(namespace_value));    
	bail_null(bn_origin_server);
	err = web_get_request_param_str(g_web_data, wcf_field_oshttpservermap, ps_post_data,
			&hostname);
        bail_error(err);
        if (hostname == NULL || ts_num_chars(hostname) <= 0) {
        	err = ts_new_str(&msg, _("Please enter valid Server-map."));
       		bail_error(err);
	        ret_err = 1;
        	goto check_error;
        }	
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_string,ts_str(hostname),
		        bn_origin_server);
	bail_error(err);
	if(ret_err) {
		goto check_error;	
	}

    }else if ( ts_equal_str(action_value, wcf_id_oshttpfollowheader,true)) {
	bn_origin_server= smprintf("/nkn/nvsd/namespace/%s/origin-server/http/follow/header",ts_str(namespace_value));    
	bail_null(bn_origin_server);
	err = web_get_request_param_str(g_web_data, wcf_field_oshttpfollowheader, ps_post_data,
			&hostname);
        bail_error(err);
        if (hostname == NULL || ts_num_chars(hostname) <= 0) {
        	err = ts_new_str(&msg, _("Please enter valid Follow-header."));
       		bail_error(err);
	        ret_err = 1;
        	goto check_error;
        }	
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_string,ts_str(hostname),
		        bn_origin_server);
	bail_error(err);
	if(ret_err) {
		goto check_error;	
	}

    }else if ( ts_equal_str(action_value, wcf_id_oshttpabsurl,true)) {
	bn_origin_server= smprintf("/nkn/nvsd/namespace/%s/origin-server/http/absolute-url",ts_str(namespace_value));    
	bail_null(bn_origin_server);
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_bool,"true",
		        bn_origin_server);
	bail_error(err);
	if(ret_err) {
		goto check_error;	
	}
    }else if ( ts_equal_str(action_value, wcf_id_osnfshost,true)) {
	bn_origin_server= smprintf("/nkn/nvsd/namespace/%s/origin-server/nfs/host",ts_str(namespace_value));    
	bail_null(bn_origin_server);
	err = web_get_request_param_str(g_web_data, wcf_field_osnfshost, ps_post_data,
			&hostname);
        bail_error(err);
        if (hostname == NULL || ts_num_chars(hostname) <= 0) {
        	err = ts_new_str(&msg, _("Please enter valid NFS hostname."));
       		bail_error(err);
	        ret_err = 1;
        	goto check_error;
        }	
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_string,ts_str(hostname),
		        bn_origin_server);
	bail_error(err);
	if(ret_err) {
		goto check_error;	
	}
	bn_origin_server_port = smprintf("/nkn/nvsd/namespace/%s/origin-server/nfs/port",ts_str(namespace_value));    
	bail_null(bn_origin_server_port);
	err = web_get_request_param_str(g_web_data, wcf_field_osnfsport, ps_post_data,
			&port);
        bail_error_null(err,port);
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_uint16,ts_str(port),
		        bn_origin_server_port);
	bail_error(err);
    }else if ( ts_equal_str(action_value, wcf_id_osnfsservermap,true)) {
	bn_origin_server= smprintf("/nkn/nvsd/namespace/%s/origin-server/nfs/server-map",ts_str(namespace_value));    
	bail_null(bn_origin_server);
	err = web_get_request_param_str(g_web_data, wcf_field_osnfsservermap, ps_post_data,
			&hostname);
        bail_error(err);
        if (hostname == NULL || ts_num_chars(hostname) <= 0) {
        	err = ts_new_str(&msg, _("Please enter valid NFS Server-map."));
       		bail_error(err);
	        ret_err = 1;
        	goto check_error;
        }	
        bail_error_null(err,hostname);
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_string,ts_str(hostname),
		        bn_origin_server);
	bail_error(err);
	if(ret_err) {
		goto check_error;	
	}
    }

    err = web_get_request_param_str(g_web_data,wcf_field_domain, ps_post_data,
		    &domain_type);
    bail_error_null(err,domain_type);

    bn_domain_host= smprintf("/nkn/nvsd/namespace/%s/domain/host",ts_str(namespace_value));    
    bail_null(bn_domain_host);

    bn_domain_regex= smprintf("/nkn/nvsd/namespace/%s/domain/regex",ts_str(namespace_value));    
    bail_null(bn_domain_regex);

    if((ts_equal_str(domain_type, wcf_id_domainhost, true))) {
	err = web_get_request_param_str(g_web_data, wcf_field_domainhost, ps_post_data,
			&domain_value);
        bail_error(err);
	if (ts_num_chars(domain_value) > 0) {
		err = mdc_set_binding(web_mcc, &ret_err, &msg,
				bsso_modify,bt_string,ts_str(domain_value),
			        bn_domain_host);
		bail_error(err);
		if(ret_err) {
			goto check_error;	
		}		
	} else {
		err = mdc_set_binding(web_mcc, &ret_err, &msg,
				bsso_modify,bt_string,"*",
			        bn_domain_host);
		bail_error(err);
		if(ret_err) {
			goto check_error;	
		}	
	}
		err = mdc_set_binding(web_mcc, &ret_err, &msg,
				bsso_modify,bt_regex,"",
		        	bn_domain_regex);
		bail_error(err);
		if(ret_err) {
			goto check_error;	
		}	
    }else if((ts_equal_str(domain_type, wcf_id_domainregex, true))) {
	err = web_get_request_param_str(g_web_data, wcf_field_domainregex, ps_post_data,
			&domain_value);
        bail_error(err);
	if (ts_num_chars(domain_value)> 0) {
		err = mdc_set_binding(web_mcc, &ret_err, &msg,
				bsso_modify,bt_regex,ts_str(domain_value),
		        	bn_domain_regex);
		bail_error(err);
		if(ret_err) {
			goto check_error;	
		}	
	}
		err = mdc_set_binding(web_mcc, &ret_err, &msg,
				bsso_modify,bt_string,"*",
			        bn_domain_host);
		bail_error(err);
		if(ret_err) {
			goto check_error;	
		}	
    }

 check_error:
    err = web_set_msg_result(g_web_data, ret_err,msg);
    bail_error(err);

    if (!ret_err) {
        err = ts_new_str(&msg, _("Origin Server details configured."));
        bail_error(err);
	err = web_set_msg_result(g_web_data,ret_err,msg);
        bail_error(err);
        err = web_clear_post_data(g_web_data);
        bail_error(err);
    }

 bail:
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&namespace_fixed);
    ts_free(&uri_fixed);
    ts_free(&msg);
    safe_free(gid_str);
    safe_free(node_name);
    safe_free(bn_origin_server);
    safe_free(bn_origin_server_port);
    return(err);
}

static int
wcf_handle_matchadd(void)
{
    const bn_response *resp = NULL;
    bn_binding *binding = NULL;
    bn_request *req = NULL;
    const tstring *namespace_value = NULL;
    tstring *uri_fixed = NULL;
    tstring *domain_fixed = NULL;
    tstring *msg = NULL;
    char *node_name = NULL;
    char *gid_str = NULL;
    uint32 ret_err = 0;
    int err = 0;
    const tstring *match_setting = NULL;
    char *bn_match_detail = NULL;
    char *bn_match_precedence = NULL;
    const tstring *matchdata = NULL;
    const tstring *precedence = NULL;
    /*! Reading Namespace field */
    err = web_get_request_param_str(g_web_data, wcf_field_namespace, ps_post_data,
                                    &namespace_value);
    bail_error(err);
    if (namespace_value == NULL || ts_num_chars(namespace_value) <= 0) {
        err = ts_new_str(&msg, _("No namespace specified for the new entry."));
        bail_error(err);
        ret_err = 1;
        goto check_error;
    }
    err = web_get_request_param_str(g_web_data,wcf_field_matchdetails, ps_post_data,
		    &match_setting);
    bail_error_null(err,match_setting);

    if((ts_equal_str(match_setting, wcf_id_uriname, true))) {
	bn_match_detail = smprintf("/nkn/nvsd/namespace/%s/match/uri/uri_name",ts_str(namespace_value));
	bail_null(bn_match_detail);
	err = web_get_request_param_str(g_web_data, wcf_field_uriname , ps_post_data,
		&matchdata);
	bail_error(err);
    	if (matchdata == NULL || ts_num_chars(matchdata) <= 0) {
        	err = ts_new_str(&msg, _("No Uri name specified for the new entry."));
	        bail_error(err);
        	ret_err = 1;
	        goto check_error;
        }	
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_uri,ts_str(matchdata),
			bn_match_detail);
	bail_error(err);
	if(ret_err) {
		goto check_error;
	}
    }else if((ts_equal_str(match_setting, wcf_id_uriregex, true))) {
	bn_match_detail = smprintf("/nkn/nvsd/namespace/%s/match/uri/regex",ts_str(namespace_value));
	bail_null(bn_match_detail);
	err = web_get_request_param_str(g_web_data, wcf_field_uriregex , ps_post_data,
		&matchdata);
	bail_error(err);
    	if (matchdata == NULL || ts_num_chars(matchdata) <= 0) {
        	err = ts_new_str(&msg, _("No Uri Regex specified for the new entry."));
	        bail_error(err);
        	ret_err = 1;
	        goto check_error;
        }	
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_regex,ts_str(matchdata),
			bn_match_detail);
	bail_error(err);
	if(ret_err) {
		goto check_error;
	}

    }else if((ts_equal_str(match_setting, wcf_id_headernamevalue, true))) {
	bn_match_detail = smprintf("/nkn/nvsd/namespace/%s/match/header/name",ts_str(namespace_value));
	bail_null(bn_match_detail);
	err = web_get_request_param_str(g_web_data, wcf_field_headername , ps_post_data,
		&matchdata);
	bail_error(err);
    	if (matchdata == NULL || ts_num_chars(matchdata) <= 0) {
        	err = ts_new_str(&msg, _("No Header Name specified for the new entry."));
	        bail_error(err);
        	ret_err = 1;
	        goto check_error;
        }	
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_string,ts_str(matchdata),
			bn_match_detail);
	bail_error(err);
	if(ret_err) {
		goto check_error;
	}
	bn_match_detail = smprintf("/nkn/nvsd/namespace/%s/match/header/value",ts_str(namespace_value));
	bail_null(bn_match_detail);
	err = web_get_request_param_str(g_web_data, wcf_field_headervalue , ps_post_data,
		&matchdata);
	bail_error_null(err, matchdata);
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_string,ts_str(matchdata),
			bn_match_detail);
	bail_error(err);
	if(ret_err) {
		goto check_error;
	}
	
    }else if((ts_equal_str(match_setting, wcf_id_headerregex, true))) {
	bn_match_detail = smprintf("/nkn/nvsd/namespace/%s/match/header/regex",ts_str(namespace_value));
	bail_null(bn_match_detail);
	err = web_get_request_param_str(g_web_data, wcf_field_headerregex , ps_post_data,
		&matchdata);
	bail_error(err);
    	if (matchdata == NULL || ts_num_chars(matchdata) <= 0) {
        	err = ts_new_str(&msg, _("No Header Regex specified for the new entry."));
	        bail_error(err);
        	ret_err = 1;
	        goto check_error;
        }	
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_regex,ts_str(matchdata),
			bn_match_detail);
	bail_error(err);
	if(ret_err) {
		goto check_error;
	}
    }else if((ts_equal_str(match_setting, wcf_id_querynamevalue, true))) {
	bn_match_detail = smprintf("/nkn/nvsd/namespace/%s/match/query-string/name",ts_str(namespace_value));
	bail_null(bn_match_detail);
	err = web_get_request_param_str(g_web_data, wcf_field_queryname, ps_post_data,
		&matchdata);
	bail_error(err);
    	if (matchdata == NULL || ts_num_chars(matchdata) <= 0) {
        	err = ts_new_str(&msg, _("No Query string Name specified for the new entry."));
	        bail_error(err);
        	ret_err = 1;
	        goto check_error;
        }	
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_string,ts_str(matchdata),
			bn_match_detail);
	bail_error(err);
	if(ret_err) {
		goto check_error;
	}
	bn_match_detail = smprintf("/nkn/nvsd/namespace/%s/match/query-string/value",ts_str(namespace_value));
	bail_null(bn_match_detail);
	err = web_get_request_param_str(g_web_data, wcf_field_queryvalue, ps_post_data,
		&matchdata);
	bail_error_null(err, matchdata);
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_string,ts_str(matchdata),
			bn_match_detail);
	bail_error(err);
	if(ret_err) {
		goto check_error;
	}
    }else if((ts_equal_str(match_setting, wcf_id_queryregex, true))) {
	bn_match_detail = smprintf("/nkn/nvsd/namespace/%s/match/query-string/regex",ts_str(namespace_value));
	bail_null(bn_match_detail);
	err = web_get_request_param_str(g_web_data, wcf_field_queryregex, ps_post_data,
		&matchdata);
	bail_error(err);
    	if (matchdata == NULL || ts_num_chars(matchdata) <= 0) {
        	err = ts_new_str(&msg, _("No Query string regex specified for the new entry."));
	        bail_error(err);
        	ret_err = 1;
	        goto check_error;
        }	
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_regex,ts_str(matchdata),
			bn_match_detail);
	bail_error(err);
	if(ret_err) {
		goto check_error;
	}
    }else if((ts_equal_str(match_setting, wcf_id_virtualip, true))) {
	bn_match_detail = smprintf("/nkn/nvsd/namespace/%s/match/virtual-host/ip",ts_str(namespace_value));
	bail_null(bn_match_detail);
	err = web_get_request_param_str(g_web_data, wcf_field_virtualip, ps_post_data,
		&matchdata);
	bail_error(err);
    	if (matchdata == NULL || ts_num_chars(matchdata) <= 0) {
        	err = ts_new_str(&msg, _("No Virtual Host IP specified for the new entry."));
	        bail_error(err);
        	ret_err = 1;
	        goto check_error;
        }	
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_ipv4addr,ts_str(matchdata),
			bn_match_detail);
	bail_error(err);
	if(ret_err) {
		goto check_error;
	}
	bn_match_detail = smprintf("/nkn/nvsd/namespace/%s/match/virtual-host/port",ts_str(namespace_value));
	bail_null(bn_match_detail);
	err = web_get_request_param_str(g_web_data, wcf_field_virtualport, ps_post_data,
		&matchdata);
	bail_error_null(err, matchdata);
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_uint16,ts_str(matchdata),
			bn_match_detail);
	bail_error(err);
	if(ret_err) {
		goto check_error;
	}
    }
    bn_match_precedence = smprintf("/nkn/nvsd/namespace/%s/match/precedence", ts_str(namespace_value));
    bail_null(bn_match_precedence);
    err = web_get_request_param_str(g_web_data, wcf_field_precedence, ps_post_data,
		&precedence);
    bail_error_null(err, precedence);
    err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_uint32,ts_str(precedence),
			bn_match_precedence);
    bail_error(err);
    if(ret_err) {
	goto check_error;
    }

 check_error:
    err = web_set_msg_result(g_web_data, ret_err,msg);
    bail_error(err);

    if (!ret_err) {
        err = ts_new_str(&msg, _("Match details configured."));
        bail_error(err);
	err = web_set_msg_result(g_web_data,ret_err,msg);
        bail_error(err);
        err = web_clear_post_data(g_web_data);
        bail_error(err);
    }

 bail:
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&msg);
    safe_free(node_name);
    safe_free(bn_match_detail);
    return(err);
}
static web_action_result
wcf_nsconfig_form_handler(web_context *ctx, void *param)
{
    int err = 0;

    if (web_request_param_exists_str(g_web_data, wcf_btn_add, ps_post_data)) {
        err = wcf_handle_add();
        bail_error(err);
    }else if (web_request_param_exists_str(g_web_data, wcf_btn_matchadd, ps_post_data)){
	err = wcf_handle_matchadd();
	bail_error(err);
    }else if (web_request_param_exists_str(g_web_data, wcf_btn_rtspadd, ps_post_data)){
	err = wcf_handle_rtspadd();
	bail_error(err);
    }else {
        err = web_process_form_buttons(g_web_data);
        bail_error(err);
    }

 bail:
    if (err) {
        return(war_error);
    } else {
        return(war_ok);
    }
}

int
web_config_form_nsconfig_init(const lc_dso_info *info, void *data)
{
    int err = 0;

    /*
     * the context contains useful information including a pointer
     * to the TCL interpreter so that modules can register their own
     * TCL commands.
     */

    /*
     * register all commands that are handled by this module.
     */
    err = web_register_action_str(g_rh_data, wcf_action_name, NULL,
                                  wcf_nsconfig_form_handler);
    bail_error(err);

 bail:
    return(err);
}

static int
wcf_handle_rtspadd(void)
{
    const bn_response *resp = NULL;
    bn_binding *binding = NULL;
    bn_request *req = NULL;
    const tstring *namespace_value = NULL;
    tstring *uri_fixed = NULL;
    tstring *domain_fixed = NULL;
    tstring *msg = NULL;
    char *node_name = NULL;
    char *gid_str = NULL;
    uint32 ret_err = 0;
    int err = 0;
    const tstring *match_setting = NULL;
    char *bn_match_detail = NULL;
    char *bn_match_precedence = NULL;
    const tstring *matchdata = NULL;
    const tstring *precedence = NULL;
#if 0
    /*! Reading Namespace field */
    err = web_get_request_param_str(g_web_data, wcf_field_namespace, ps_post_data,
                                    &namespace_value);
    bail_error(err);
    if (namespace_value == NULL || ts_num_chars(namespace_value) <= 0) {
        err = ts_new_str(&msg, _("No namespace specified for the new entry."));
        bail_error(err);
        ret_err = 1;
        goto check_error;
    }
    err = web_get_request_param_str(g_web_data,wcf_field_rtspmatchdetails, ps_post_data,
		    &match_setting);
    bail_error_null(err,match_setting);

    if((ts_equal_str(match_setting, wcf_id_rtspuriname, true))) {
	bn_match_detail = smprintf("/nkn/nvsd/namespace/%s/rtsp/match/uri/uri_name",ts_str(namespace_value));
	bail_null(bn_match_detail);
	err = web_get_request_param_str(g_web_data, wcf_field_rtspuriname , ps_post_data,
		&matchdata);
	bail_error(err);
    	if (matchdata == NULL || ts_num_chars(matchdata) <= 0) {
        	err = ts_new_str(&msg, _("No Uri name specified for the new entry."));
	        bail_error(err);
        	ret_err = 1;
	        goto check_error;
        }	
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_uri,ts_str(matchdata),
			bn_match_detail);
	bail_error(err);
	if(ret_err) {
		goto check_error;
	}
    }else if((ts_equal_str(match_setting, wcf_id_rtspuriregex, true))) {
	bn_match_detail = smprintf("/nkn/nvsd/namespace/%s/rtsp/match/uri/regex",ts_str(namespace_value));
	bail_null(bn_match_detail);
	err = web_get_request_param_str(g_web_data, wcf_field_rtspuriregex , ps_post_data,
		&matchdata);
	bail_error(err);
    	if (matchdata == NULL || ts_num_chars(matchdata) <= 0) {
        	err = ts_new_str(&msg, _("No Uri Regex specified for the new entry."));
	        bail_error(err);
        	ret_err = 1;
	        goto check_error;
        }	
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_regex,ts_str(matchdata),
			bn_match_detail);
	bail_error(err);
	if(ret_err) {
		goto check_error;
	}
    }else if((ts_equal_str(match_setting, wcf_id_rtspvirtualip, true))) {
	bn_match_detail = smprintf("/nkn/nvsd/namespace/%s/rtsp/match/virtual-host/ip",ts_str(namespace_value));
	bail_null(bn_match_detail);
	err = web_get_request_param_str(g_web_data, wcf_field_rtspvirtualip, ps_post_data,
		&matchdata);
	bail_error(err);
    	if (matchdata == NULL || ts_num_chars(matchdata) <= 0) {
        	err = ts_new_str(&msg, _("No Virtual Host IP specified for the new entry."));
	        bail_error(err);
        	ret_err = 1;
	        goto check_error;
        }	
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_ipv4addr,ts_str(matchdata),
			bn_match_detail);
	bail_error(err);
	if(ret_err) {
		goto check_error;
	}
	bn_match_detail = smprintf("/nkn/nvsd/namespace/%s/rtsp/match/virtual-host/port",ts_str(namespace_value));
	bail_null(bn_match_detail);
	err = web_get_request_param_str(g_web_data, wcf_field_rtspvirtualport, ps_post_data,
		&matchdata);
	bail_error_null(err,matchdata);
	err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_uint16,ts_str(matchdata),
			bn_match_detail);
	bail_error(err);
    }
    bn_match_precedence = smprintf("/nkn/nvsd/namespace/%s/rtsp/match/precedence", ts_str(namespace_value));
    bail_null(bn_match_precedence);
    err = web_get_request_param_str(g_web_data, wcf_field_rtspprecedence, ps_post_data,
		&precedence);
    bail_error_null(err, precedence);
    err = mdc_set_binding(web_mcc, &ret_err, &msg,
			bsso_modify,bt_uint32,ts_str(precedence),
			bn_match_precedence);
    bail_error(err);
    if(ret_err) {
	goto check_error;
    }

 check_error:
    err = web_set_msg_result(g_web_data, ret_err,msg);
    bail_error(err);

    if (!ret_err) {
        err = ts_new_str(&msg, _(" RTSP Match details configured."));
        bail_error(err);
	err = web_set_msg_result(g_web_data,ret_err,msg);
        bail_error(err);
        err = web_clear_post_data(g_web_data);
        bail_error(err);
    }
#endif

 bail:
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&msg);
    safe_free(node_name);
    safe_free(bn_match_detail);
    return(err);
}
