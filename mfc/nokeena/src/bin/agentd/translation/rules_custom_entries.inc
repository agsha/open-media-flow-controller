    TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-fetchactionscache-agecontent-type*age*")
    TRANSL_CUST(ns_del_proto_cache_age_content_type, "/nkn/nvsd/origin_fetch/config/%s/cache-age/content-type")
        TRANSL_END_CUST
    TRANSL_ENTRY_END

    TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-fetchactionscache-agecontent-type*")
        TRANSL_CUST(agentd_custom_entry_ignore, "/nkn/nvsd/origin_fetch/config/%s/cache-age/content-type")
        TRANSL_END_CUST
    TRANSL_ENTRY_END

    TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-fetchactionscache-agecontent-type-anyage*")
        TRANSL_CUST(ns_del_proto_cache_age_content_type, "/nkn/nvsd/origin_fetch/config/%s/cache-age/content-type")
        TRANSL_END_CUST
    TRANSL_ENTRY_END

    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionscache-agecontent-type-anyage*")
        TRANSL_CUST(ns_del_proto_cache_age_rm_content_type, "/nkn/nvsd/origin_fetch/config/%s/cache-age/content-type")
        TRANSL_END_CUST
    TRANSL_ENTRY_END


    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionscache-agecontent-type*")
        TRANSL_CUST(ns_del_proto_cache_age_rm_content_type, "/nkn/nvsd/origin_fetch/config/%s/cache-age/content-type")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionscache-agecontent-type*age*")
        TRANSL_CUST(agentd_custom_entry_ignore, "/nkn/nvsd/origin_fetch/config/%s/cache-age/content-type")
        TRANSL_END_CUST
    TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*client-responseadd-header*value*")
    TRANSL_CUST(ns_del_proto_client_rsp_add_hdr, "/nkn/nvsd/namespace/%s/client-response/header/add")
    TRANSL_END_CUST
TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*client-responseadd-header*")
	TRANSL_CUST(agentd_custom_entry_ignore, "/nkn/nvsd/namespace/%s/client-response/header/add")
	TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-requestactionsheaderadd-header*value*")
	/* Reuse the custom function defined for client-response add header */
        TRANSL_CUST(ns_del_proto_client_rsp_add_hdr, "/nkn/nvsd/namespace/%s/origin-request/header/add")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-requestactionsheaderadd-header*")
        TRANSL_CUST(agentd_custom_entry_ignore, "/nkn/nvsd/namespace/%s/origin-request/header/add")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(" ")
        TRANSL_CUST(agentd_custom_entry_ignore, "")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY("")
        TRANSL_CUST(agentd_custom_entry_ignore, "")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY("mfc-cluster*servicehttpinstance**")
        TRANSL_CUST(agentd_custom_entry_ignore, "")
        TRANSL_END_CUST
    TRANSL_ENTRY_END

    TRANSL_ENTRY("deletemfc-cluster*servicehttpinstance*client-responsedelete-header*")
        TRANSL_CUST(ns_del_proto_rm_header, "/nkn/nvsd/namespace/%s/client-response/header/delete")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-requestactionsheaderadd-header*value*")
	/* Reuse the custom function defined for client-response add header */
        TRANSL_CUST(ns_del_proto_rm_header, "/nkn/nvsd/namespace/%s/origin-request/header/add")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-requestactionsheaderadd-header*")
        TRANSL_CUST(agentd_custom_entry_ignore, "")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-responseadd-header*value*")
    TRANSL_CUST(ns_del_proto_rm_header, "/nkn/nvsd/namespace/%s/client-response/header/add")
    TRANSL_END_CUST
TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-responseadd-header*")
	TRANSL_CUST(agentd_custom_entry_ignore, "/nkn/nvsd/namespace/%s/client-response/header/add")
	TRANSL_END_CUST
    TRANSL_ENTRY_END

    /*Thilak */
    TRANSL_ENTRY(PREPEND_TRANS_STR"namespace*origin-serverhttpserver-map*")
	TRANSL_CUST(ns_org_http_ser_map, "/nkn/nvsd/namespace/%s/origin-server/http/server-map")
	TRANSL_END_CUST
    TRANSL_ENTRY_END
//    {"namespace*origin-serverhttpserver-map*",{CUST_ENTRY,{ns_org_http_ser_map,"/nkn/nvsd/namespace/%s/origin-server/http/server-map"} }},
     TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*client-responsedelete-header*")
     	TRANSL_CUST(ns_del_proto_client_rsp_del_hdr, "/nkn/nvsd/namespace/%s/client-response/header/delete")
	TRANSL_END_CUST
     TRANSL_ENTRY_END
//    {"namespace*delivery-protocolhttpclient-responsedelete-header*",{CUST_ENTRY,{ns_del_proto_client_rsp_del_hdr,"/nkn/nvsd/namespace/%s/client-response/header/delete"} }},
//
     TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*pre-stageftp-user*")
     TRANSL_CUST(agentd_custom_entry_ignore,"/nkn/nvsd/namespace/%s/prestage/user/%s")
     TRANSL_END_CUST
     TRANSL_ENTRY_END

     TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*pre-stageftp-user*password*")
     TRANSL_CUST(ns_prestage_ftp_password,"/nkn/nvsd/namespace/%s/prestage/user/%s/password")
     TRANSL_END_CUST
     TRANSL_ENTRY_END

    TRANSL_ENTRY( PREPEND_TRANS_STR)
	TRANSL_CUST(agentd_custom_entry_ignore, "")
	TRANSL_END_CUST
    TRANSL_ENTRY_END
    /*Will we get the complete delete of mfc-cluster*/
    TRANSL_ENTRY( PREPEND_TRANS_STR"member-node*")
	TRANSL_CUST(agentd_custom_entry_ignore, "")
	TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR)
	TRANSL_CUST(agentd_custom_entry_ignore, "")
	TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*")
	TRANSL_CUST(agentd_custom_entry_ignore, "")
	TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"policy-engine*") /* TODO - Handle this */
	TRANSL_CUST(agentd_custom_entry_ignore, "")
	TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"policy-engine*") /* TODO - Handle this */
	TRANSL_CUST(agentd_custom_entry_ignore, "")
	TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*cache-inherit*")
	TRANSL_CUST(agentd_custom_entry_ignore, "")
	TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*cache-inherit*")
	TRANSL_CUST(agentd_custom_entry_ignore, "")
	TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*ip-address*")
	TRANSL_CUST(agentd_interface_ip_addr, "/net/interface/config/%s/addr/ipv4/static/1/ip")
	TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*ip-address*")
	TRANSL_CUST(agentd_interface_ip_addr, "/net/interface/config/%s/addr/ipv4/static/1/ip")
	TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*alias*")
        TRANSL_CUST(agentd_custom_entry_ignore, "")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*alias*")
        TRANSL_CUST(agentd_custom_entry_ignore, "")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*alias*ip-address*")
	TRANSL_CUST(agentd_interface_ip_addr, "/net/interface/config/%s/addr/ipv4/static/1/ip")
	TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*alias*ip-address*")
	TRANSL_CUST(agentd_interface_ip_addr, "/net/interface/config/%s/addr/ipv4/static/1/ip")
	TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typebladeinterface*alias*")
        TRANSL_CUST(agentd_del_alias, "")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standaloneinterface*alias*")
        TRANSL_CUST(agentd_del_alias, "")
        TRANSL_END_CUST
    TRANSL_ENTRY_END

/* Translation entries for bonding interface configuration and deletion */
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*bonded-interface*")
        TRANSL_CUST(agentd_bond_interface_params, "interface")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*bonded-interface*")
        TRANSL_CUST(agentd_bond_interface_params, "interface")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typebladeinterface*bonded-interface*")
        TRANSL_CUST(agentd_delete_bonded_interface, "/net/bonding/config/")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standaloneinterface*bonded-interface*")
        TRANSL_CUST(agentd_delete_bonded_interface, "/net/bonding/config/")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*type*")
        TRANSL_CUST(agentd_custom_entry_ignore, "")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*type*")
        TRANSL_CUST(agentd_custom_entry_ignore, "")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typebladeinterface*type*")
        TRANSL_CUST(agentd_custom_entry_ignore, "")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standaloneinterface*type*")
        TRANSL_CUST(agentd_custom_entry_ignore, "")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*mode*")
        TRANSL_CUST(agentd_bond_interface_params, "mode")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*link-mon-time*")
        TRANSL_CUST(agentd_bond_interface_params, "link-mon-time")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*up-delay-time*")
        TRANSL_CUST(agentd_bond_interface_params, "up-delay-time")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*down-delay-time*")
        TRANSL_CUST(agentd_bond_interface_params, "down-delay-time")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*mode*")
        TRANSL_CUST(agentd_bond_interface_params, "mode")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*link-mon-time*")
        TRANSL_CUST(agentd_bond_interface_params, "link-mon-time")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*up-delay-time*")
        TRANSL_CUST(agentd_bond_interface_params, "up-delay-time")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*down-delay-time*")
        TRANSL_CUST(agentd_bond_interface_params, "down-delay-time")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typebladeinterface*mode*")
        TRANSL_CUST(agentd_custom_entry_ignore, "") /* Mode is mandatory for bond configuration */
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typebladeinterface*link-mon-time*")
        TRANSL_CUST(agentd_delete_bond_interface_params, "/net/bonding/config/%d/link-mon-time")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typebladeinterface*up-delay-time*")
        TRANSL_CUST(agentd_delete_bond_interface_params, "/net/bonding/config/%d/up-delay-time")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typebladeinterface*down-delay-time*")
        TRANSL_CUST(agentd_delete_bond_interface_params, "/net/bonding/config/%d/down-delay-time")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standaloneinterface*mode*")
        TRANSL_CUST(agentd_custom_entry_ignore, "") /* Mode is mandatory for bond configuration */
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standaloneinterface*link-mon-time*")
        TRANSL_CUST(agentd_delete_bond_interface_params, "/net/bonding/config/%d/link-mon-time")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standaloneinterface*up-delay-time*")
        TRANSL_CUST(agentd_delete_bond_interface_params, "/net/bonding/config/%d/up-delay-time")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standaloneinterface*down-delay-time*")
        TRANSL_CUST(agentd_delete_bond_interface_params, "/net/bonding/config/%d/down-delay-time")
        TRANSL_END_CUST
    TRANSL_ENTRY_END
    
