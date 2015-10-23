/*
 * Filename :   agentd_conf_omap.h
 * Date:        07 Dec 2011
 * Author:      Vijayekkumaran M
 *
 * (C) Copyright 2011, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef AGENTD_CONF_OMAP_H
#define AGENTD_CONF_OMAP_H

int agentd_origin_map_set(agentd_context_t *context, 
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

int agentd_regen_smap_xml(agentd_context_t *context);

int agentd_origin_map_set_server_map_nodes(agentd_context_t *context,
                agentd_binding *binding, void *cb_arg,
                char **cli_buff);
int agentd_origin_map_delete(agentd_context_t *context, 
        agentd_binding *binding, void *cb_arg, char **cli_buff);

int agentd_origin_map_members_deletion(agentd_context_t *context,
                agentd_binding *binding, void *cb_arg,
                char **cli_buff);

int agentd_delete_smap_xml (void *context, agentd_binding *abinding,
        void *cb_arg);

int agentd_smap_action_refresh (void *context, agentd_binding *abinding,
              void *cb_arg);

#define OMAP_CREATION_LOCATION 	"/var/tmp"
#define OMAP_FINAL_LOCATION 	"/nkn/smap"

#endif /*AGENTD_CONF_OMAP_H*/
