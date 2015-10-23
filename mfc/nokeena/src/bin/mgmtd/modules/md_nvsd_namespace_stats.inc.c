/*
 * Copyright (c) 2010, Juniper Networks, Inc.
 * All rights reserved.
 *
 * Author: Dhruva Narasimhan
 */

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/tunnels";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.tunnels");
    node->mrn_description = "Current number of tunneled requests";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/sessions";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.sessions");
    node->mrn_description = "Current Client sessions for HTTP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/client/requests";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.client.requests");
    node->mrn_description = "Total number of requests made by clients for HTTP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/client/responses";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.client.responses");
    node->mrn_description = "Total number of requests made by clients for HTTP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/client/in_bytes";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.client.in_bytes");
    node->mrn_description = "Total number of bytes (including headers) received from clients";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/client/cache_hits";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.client.cache_hits");
    node->mrn_description = "Total number of cache-hits";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/client/cache_miss";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.client.cache_miss");
    node->mrn_description = "Total number of cache-misses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/client/resp_2xx";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.client.resp_2xx");
    node->mrn_description = "Total number of 200 and 206 responses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/client/resp_3xx";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.client.resp_3xx");
    node->mrn_description = "Total number of 3xx responses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/client/resp_4xx";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.client.resp_4xx");
    node->mrn_description = "Total number of 4xx responses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/client/resp_5xx";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.client.resp_5xx");
    node->mrn_description = "Total number of 5xx responses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/client/out_bytes";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.client.out_bytes");
    node->mrn_description = "Total number bytes served";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/client/out_bytes_tunnel";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.client.out_bytes_tunnel");
    node->mrn_description = "Total number bytes served from tunnel";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/client/out_bytes_origin";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.client.out_bytes_origin");
    node->mrn_description = "Total number bytes served from origin servers";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/client/out_bytes_ram";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.client.out_bytes_ram");
    node->mrn_description = "Total number bytes served from RAM";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/client/out_bytes_disk";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.client.out_bytes_disk");
    node->mrn_description = "Total number bytes served from disk";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/client/expired_obj_served";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.client.expired_objs_delivered");
    node->mrn_description = "Total number of expired objects delivered";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /* Define HTTP server-side (origin facing) statistics */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/server/requests";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.server.requests");
    node->mrn_description = "Total number of requests made by ORIGIN-MGR for HTTP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/server/responses";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.server.responses");
    node->mrn_description = "Total number of requests made by OM for HTTP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/server/active_conx";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.server.active_conns");
    node->mrn_description = "Total number active transaction to Origin";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/server/in_bytes";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.server.in_bytes");
    node->mrn_description = "Total number of bytes (including headers) received from origins";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/server/resp_2xx";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.server.resp_2xx");
    node->mrn_description = "Total number of 200 and 206 responses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/server/resp_3xx";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.server.resp_3xx");
    node->mrn_description = "Total number of 3xx responses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/server/resp_4xx";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.server.resp_4xx");
    node->mrn_description = "Total number of 4xx responses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/server/resp_5xx";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.server.resp_5xx");
    node->mrn_description = "Total number of 5xx responses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/http/server/out_bytes";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.http.server.out_bytes");
    node->mrn_description = "Total number bytes served to origin servers";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Define RTSP Counters */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/sessions";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.sessions");
    node->mrn_description = "Current Client sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/client/requests";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.client.requests");
    node->mrn_description = "Total number of requests made by clients for rtsp";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/client/responses";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.client.responses");
    node->mrn_description = "Total number of requests made by clients for rtsp";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/client/in_bytes";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.client.in_bytes");
    node->mrn_description = "Total number of bytes (including headers) received from clients";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/client/cache_hits";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.client.cache_hits");
    node->mrn_description = "Total number of cache-hits";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/client/cache_miss";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.client.cache_miss");
    node->mrn_description = "Total number of cache-misses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/client/resp_2xx";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.client.resp_2xx");
    node->mrn_description = "Total number of 200 and 206 responses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/client/resp_3xx";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.client.resp_3xx");
    node->mrn_description = "Total number of 3xx responses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/client/resp_4xx";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.client.resp_4xx");
    node->mrn_description = "Total number of 4xx responses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/client/resp_5xx";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.client.resp_5xx");
    node->mrn_description = "Total number of 5xx responses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/client/out_bytes";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.client.out_bytes");
    node->mrn_description = "Total number bytes served";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/client/out_bytes_tunnel";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.client.out_bytes_tunnel");
    node->mrn_description = "Total number bytes served from tunnel";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/client/out_bytes_origin";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.client.out_bytes_origin");
    node->mrn_description = "Total number bytes served from origin servers";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/client/out_bytes_ram";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.client.out_bytes_ram");
    node->mrn_description = "Total number bytes served from RAM";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/client/out_bytes_disk";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.client.out_bytes_disk");
    node->mrn_description = "Total number bytes served from disk";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Define RTSP server-side (origin facing) statistics */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/server/requests";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.server.requests");
    node->mrn_description = "Total number of requests made by ORIGIN-MGR for rtsp";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/server/responses";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.server.responses");
    node->mrn_description = "Total number of requests made by OM for rtsp";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/server/in_bytes";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.server.in_bytes");
    node->mrn_description = "Total number of bytes (including headers) received from origins";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/server/resp_2xx";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.server.resp_2xx");
    node->mrn_description = "Total number of 200 and 206 responses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/server/resp_3xx";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.server.resp_3xx");
    node->mrn_description = "Total number of 3xx responses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/server/resp_4xx";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.server.resp_4xx");
    node->mrn_description = "Total number of 4xx responses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/server/resp_5xx";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.server.resp_5xx");
    node->mrn_description = "Total number of 5xx responses";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/rtsp/server/out_bytes";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.rtsp.server.out_bytes");
    node->mrn_description = "Total number bytes served to origin servers";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/tunnelled_requests";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.tunnels");
    node->mrn_description = "Tunnelled requests";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/cache_pin_obj_count";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_namespace_dm2_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.cache.tier.lowest.cache_pin.obj_count");
    node->mrn_description = "Cache Pin Obj Count";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/cache_pin_bytes_used";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_namespace_dm2_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.cache.tier.lowest.cache_pin.bytes_used");
    node->mrn_description = "Cache Pin bytes used";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/url_filter_acp_cnt";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.urlfilter.acp_cnt");
    node->mrn_description = "Filter accept count";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/url_filter_rej_cnt";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.urlfilter.rej_cnt");
    node->mrn_description = "Filter reject count";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/namespace/*/url_filter_bw";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
    node->mrn_mon_get_arg = (void*)("ns.%s.urlfilter.bw");
    node->mrn_description = "Filter bandwidth";
    err = mdm_add_node(module, &node);
    bail_error(err);

