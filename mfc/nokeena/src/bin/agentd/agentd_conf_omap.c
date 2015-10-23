/*
 * Filename :   agentd_conf_omap.c
 * Date:        07 Dec 2011
 * Author:      Vijayekkumaran M
 *
 * (C) Copyright 2011, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#include "md_client.h"
#include "mdc_wrapper.h"
#include "bnode.h"
#include "signal_utils.h"
#include "libevent_wrapper.h"
#include "random.h"
#include "dso.h"
#include <ctype.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include "agentd_mgmt.h"
#include "nkn_cfg_params.h"
#include <glib.h>
#include "agentd_conf_omap.h"
#include "agentd_op_cmds_base.h"
#include "xml_utils.h"
#include "agentd_utils.h"
#include "file_utils.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;

extern agentd_array_move_context smap_attachments;

#define HTTP_ERR_CODES_BUF_SIZE (1024*2)

static int agentd_conf_omap_move_smap(const char* smap_name, const char* file_name)
{
    int err = 0;
    /*Move the file to the right location and remove from the present location*/
    char destName[1024];
    snprintf(destName, sizeof(destName), "%s/%s.xml", OMAP_FINAL_LOCATION, smap_name);

    err = lf_rename_file(file_name, destName);
    if (err)
        lc_log_debug(LOG_NOTICE, "errno: %d", errno);

    bail_error(err);

    lc_log_debug(jnpr_log_level, "File has been moved from  %s to %s", file_name, destName);

bail:

    return err;
}

static int agentd_unbind_smap_from_ns(agentd_context_t* context,const char* ns_name,const char* smap_name)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    node_name_t smap_nd = {0};
    node_name_t node_to_delete = {0}, type_node = {0};

    uint32_array *smap_indices = NULL;
    int32 db_rev = 0;
    uint32 num_smaps = 0;

    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;

    bail_null(ns_name);
    bail_null(smap_name);

    lc_log_debug(jnpr_log_level,"unbinding smap:%s from ns:%s",smap_name,ns_name );

    err = bn_binding_array_new(&barr);
    bail_error(err);

    err = bn_binding_new_str_autoinval(&binding, "map-name", ba_value,
            bt_string, 0, smap_name);
    bail_error_null(err, binding);

    err = bn_binding_array_append_takeover(barr, &binding);
    bail_error(err);

    snprintf(smap_nd,sizeof(smap_nd),"/nkn/nvsd/namespace/%s/origin-server/http/server-map",ns_name);

    err = mdc_array_get_matching_indices(agentd_mcc,smap_nd,NULL, barr,
            bn_db_revision_id_none,&db_rev, &smap_indices, NULL, NULL);
    bail_error(err);

    num_smaps = uint32_array_length_quick(smap_indices);
    bail_require_msg( !(num_smaps <= 0),"Could not get smaps from %s..",smap_nd );

    lc_log_debug(jnpr_log_level,"ns:%s has smap:%s at %d",
            ns_name,smap_name,uint32_array_get_quick(smap_indices, 0) );

    snprintf(node_to_delete, sizeof(node_to_delete),"%s/%d",smap_nd,
            uint32_array_get_quick(smap_indices,0) );

    err = agentd_append_req_msg(context, node_to_delete, SUBOP_DELETE, TYPE_STR, "");
    bail_error(err);

    /* Reset origin-server type */
    //snprintf(type_node, sizeof(type_node), "/nkn/nvsd/namespace/%s/origin-server/type", ns_name);
    //err = agentd_append_req_msg(context, type_node, SUBOP_RESET, TYPE_UINT8, "");
    //bail_error(err);

bail:
    uint32_array_free(&smap_indices);
    bn_binding_array_free(&barr);
    return err;
}
static int agentd_bind_smap_to_ns_maybe(agentd_context_t * context, const char* ns_name,const char* smap_name)
{
    int err = 0;
    uint32 code = 0;
    bn_binding_array *bindings = NULL;
    node_name_t smap_nd = {0};
    uint32_array *smap_indices = NULL;
    int32 db_rev = 0;
    uint32 num_smaps = 0;

    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;

    bail_null(ns_name);
    bail_null(smap_name);

    lc_log_debug(jnpr_log_level,"binding smap:%s to ns:%s",smap_name,ns_name );

    err = bn_binding_array_new(&barr);
    bail_error(err);

    err = bn_binding_new_str_autoinval(&binding, "map-name", ba_value,
            bt_string, 0, smap_name);
    bail_error_null(err, binding);

    err = bn_binding_array_append_takeover(barr, &binding);
    bail_error(err);

    snprintf(smap_nd,sizeof(smap_nd),"/nkn/nvsd/namespace/%s/origin-server/http/server-map",ns_name);

    err = mdc_array_get_matching_indices(agentd_mcc,smap_nd,NULL, barr,
            bn_db_revision_id_none,&db_rev, &smap_indices, NULL, NULL);
    bail_error(err);

    num_smaps = uint32_array_length_quick(smap_indices);

    if(num_smaps == 0)
    {
        err = agentd_array_append(agentd_mcc,&smap_attachments,context->mod_bindings, smap_nd, barr,&code,NULL);
        bail_error(err);

        lc_log_debug(jnpr_log_level,"successfully bound smap:%s to ns:%s",
                smap_name,ns_name);

    }
    else
    {
        lc_log_debug(jnpr_log_level,"ns:%s already has smap:%s at %d",
                ns_name,smap_name,uint32_array_get_quick(smap_indices, 0) );
    }

bail:
    bn_binding_array_free(&barr);
    uint32_array_free(&smap_indices);
    return err;
}

static int agentd_conf_omap_set_smap_nodes(agentd_context_t* context, const char* ns_name, const char* om_name)
{
    node_name_t t;
    int err = 0;
    node_name_t baseName = { 0 };
    node_name_t nodeName = { 0 };
    node_name_t smap_name = { 0 };
    char xml_url[1024] = { 0 };

    snprintf(smap_name, sizeof(smap_name), "%s_%s", ns_name, om_name);

    snprintf(baseName, sizeof(baseName), "/nkn/nvsd/server-map/config/%s", smap_name);

    snprintf(xml_url, sizeof(xml_url), "http://127.0.0.2:8080/smap/%s.xml", smap_name);

    snprintf(nodeName, sizeof(nodeName), "%s/map-status", baseName);

    /*XXX- one more bad thing, setting map-status from out-side is wrong */
    lc_log_debug(jnpr_log_level, "Going to create node = %s", nodeName);
    err = agentd_append_req_msg(context, nodeName, SUBOP_MODIFY, TYPE_BOOL, "true");
    bail_error(err);

    snprintf(nodeName, sizeof(nodeName), "%s/file-url", baseName);
    lc_log_debug(jnpr_log_level, "Going to create node = %s", nodeName);
    err = agentd_append_req_msg(context, nodeName, SUBOP_MODIFY, TYPE_STR, xml_url);
    bail_error(err);

    snprintf(nodeName, sizeof(nodeName), "%s/refresh", baseName);
    lc_log_debug(jnpr_log_level, "Going to create node = %s", nodeName);
    err = agentd_append_req_msg(context, nodeName, SUBOP_MODIFY, TYPE_UINT32, "86400");
    bail_error(err);

    err = agentd_bind_smap_to_ns_maybe(context, ns_name,smap_name);
    bail_error(err);
bail:
    return err;
}

static int agentd_get_one_origin_esc_http_err_codes(xmlNodePtr memNodePtr,const char** retVal)
{
    /*The return value is a local static buffer
     * */
    static char buf[HTTP_ERR_CODES_BUF_SIZE]={0};

    int err = 0;
    xmlNodePtr errNodePtr = NULL;
    xmlChar* codeValue = NULL;
    xmlXPathObjectPtr retPathForErrCodes = NULL;

    buf[0]=0;

    xmlNodeSetPtr err_code_setptr = GetMatchingNodeSet(memNodePtr, "http-failure-codes",&retPathForErrCodes);

    if (err_code_setptr == NULL || err_code_setptr->nodeNr == 0)
    {
        lc_log_debug(jnpr_log_level, "Could not find HTTP Error Codes.. ");
        goto bail;
    }

    lc_log_debug(jnpr_log_level, "Found HTTP Error Codes = %d", err_code_setptr->nodeNr);

    for (int i = 0; i < err_code_setptr->nodeNr; i++)
    {
        errNodePtr = err_code_setptr->nodeTab[i];
        bail_null(errNodePtr);

        codeValue = xmlNodeGetContent(errNodePtr);

        if(codeValue==NULL)
            continue;

        lc_log_debug(jnpr_log_level, "Found Error Code = %s", codeValue);

        snprintf(buf+strlen(buf),
                HTTP_ERR_CODES_BUF_SIZE-strlen(buf),
                "%s%s",
                buf[0]?";":"",
                codeValue
                );

        xmlFree(codeValue);codeValue=NULL;
    }

bail:
    xmlXPathFreeObject(retPathForErrCodes);

    if( buf[0]!=0 )
        *retVal = buf;

    lc_log_debug(jnpr_log_level, "Full Err Code Value = %s", buf);

    return err;
}
static int agentd_write_origin_esc_of_one_om(const char* file_name,xmlNodeSetPtr mem_setptr)
{
    int err = 0;

    xmlNodePtr mem_name_ptr, mem_node_ptr, ip_node_ptr, port_node_ptr, heartbeat_node_ptr;
    xmlNodePtr order_node_ptr, err_code_node_ptr;

    xmlChar *mem_name_val_ptr=NULL, *ip_node_val_ptr=NULL;
    xmlChar *port_node_val_ptr=NULL, *heartbeat_node_val_ptr=NULL;
    xmlChar *order_node_val_ptr=NULL;

    FILE* fp = fopen(file_name, "w+");
    bail_require_errno(fp!=NULL,"fopen failed - %s",file_name);

        /* we have a blank file */
    fprintf(fp,
            "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n<!DOCTYPE OriginEscalationMap SYSTEM \"OriginEscalationMap.dtd\">"
                "\n<OriginEscalationMap>\n<Header> <Version>1.0</Version> <Application>MapXML</Application> </Header>\n");

    for (int k = 0; k < mem_setptr->nodeNr; k++)
    {
        const char *cl_entry = "\n<OriginEscalationMapEntry>\n\t<Origin>%s</Origin>\n\t<Port>%s</Port>"
            "\n\t<Options>heartbeatpath=%s,weight=%s,http_response_failure_codes=%s</Options>\n</OriginEscalationMapEntry>";

        mem_node_ptr = mem_setptr->nodeTab[k];
        bail_null(mem_node_ptr);

        mem_name_ptr = find_element_by_name(mem_node_ptr->xmlChildrenNode, "name");
        ip_node_ptr = find_element_by_name(mem_node_ptr->xmlChildrenNode, "ipaddress");
        port_node_ptr = find_element_by_name(mem_node_ptr->xmlChildrenNode, "port");
        heartbeat_node_ptr = find_element_by_name(mem_node_ptr->xmlChildrenNode, "heart-beat-path");
        order_node_ptr = find_element_by_name(mem_node_ptr->xmlChildrenNode, "order");

        bail_require_msg( !(
                mem_name_ptr ==NULL
           || ip_node_ptr==NULL
           || port_node_ptr == NULL
           || heartbeat_node_ptr == NULL
           || order_node_ptr == NULL
           ),"Could not get all member nodes.." );

        const char* http_err_cds= NULL;

        err = agentd_get_one_origin_esc_http_err_codes(mem_node_ptr,&http_err_cds);
        bail_error(err);

        mem_name_val_ptr        = xmlNodeGetContent(mem_name_ptr);
        ip_node_val_ptr         = xmlNodeGetContent(ip_node_ptr);
        port_node_val_ptr       = xmlNodeGetContent(port_node_ptr);
        heartbeat_node_val_ptr  = xmlNodeGetContent(heartbeat_node_ptr);
        order_node_val_ptr      = xmlNodeGetContent(order_node_ptr);

        bail_require_msg( !(
                mem_name_val_ptr ==NULL
           || ip_node_val_ptr==NULL
           || port_node_val_ptr == NULL
           || heartbeat_node_val_ptr == NULL
           || order_node_val_ptr == NULL
           || http_err_cds == NULL
           ),"Could not get orig-esc member node Values.." );


        fprintf(fp, cl_entry,
                ip_node_val_ptr,port_node_val_ptr,
                heartbeat_node_val_ptr,
                order_node_val_ptr,http_err_cds?:"");

        lc_log_debug(jnpr_log_level, "member- %s", mem_name_val_ptr);

        xmlFree(mem_name_val_ptr);
        xmlFree(ip_node_val_ptr);
        xmlFree(port_node_val_ptr);
        xmlFree(heartbeat_node_val_ptr);
        xmlFree(order_node_val_ptr);

        mem_name_val_ptr=NULL;
        ip_node_val_ptr=NULL;
        port_node_val_ptr=NULL;
        heartbeat_node_val_ptr=NULL;
        order_node_val_ptr=NULL;
        http_err_cds = NULL;
    }

    fprintf(fp, "\n</OriginEscalationMap>\n");
    fclose(fp);fp=NULL;

bail:
    if (fp) fclose(fp);
    xmlFree(mem_name_val_ptr);
    xmlFree(ip_node_val_ptr);
    xmlFree(port_node_val_ptr);
    xmlFree(heartbeat_node_val_ptr);
    xmlFree(order_node_val_ptr);

    return err;
}

static int agentd_write_cluster_hash_of_one_om(const char* file_name,xmlNodeSetPtr mem_setptr)
{
    int err = 0;

    xmlNodePtr mem_name_ptr, mem_node_ptr, ip_node_ptr, port_node_ptr, heartbeat_node_ptr;
    xmlChar *mem_name_val_ptr=NULL, *ip_node_val_ptr=NULL;
    xmlChar *port_node_val_ptr=NULL, *heartbeat_node_val_ptr=NULL;

    FILE* fp = fopen(file_name, "w+");
    bail_require_errno(fp!=NULL,"fopen failed - %s",file_name);

        /* we have a blank file */
    fprintf(fp,
            "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?> <!DOCTYPE ClusterMap SYSTEM \"ClusterMap.dtd\">"
                "<ClusterMap> <Header> <Version>1.0</Version> <Application>MapXML</Application> </Header>");

    for (int k = 0; k < mem_setptr->nodeNr; k++)
    {
        const char *cl_entry = "\n<ClusterMapEntry>\n\t<Node>%s</Node>\n\t<IP>%s</IP>\n\t<Port>%s</Port>"
            "\n\t<Options>heartbeatpath=%s</Options>\n</ClusterMapEntry>\n";

        mem_node_ptr = mem_setptr->nodeTab[k];
        bail_null(mem_node_ptr);

        mem_name_ptr = find_element_by_name(mem_node_ptr->xmlChildrenNode, "name");
        ip_node_ptr = find_element_by_name(mem_node_ptr->xmlChildrenNode, "ipaddress");
        port_node_ptr = find_element_by_name(mem_node_ptr->xmlChildrenNode, "port");
        heartbeat_node_ptr = find_element_by_name(mem_node_ptr->xmlChildrenNode, "heart-beat-path");

        bail_require_msg( !(
                mem_name_ptr ==NULL
           || ip_node_ptr==NULL
           || port_node_ptr == NULL
           || heartbeat_node_ptr == NULL
           ),"Could not get all member nodes.." );


        mem_name_val_ptr        = xmlNodeGetContent(mem_name_ptr);
        ip_node_val_ptr         = xmlNodeGetContent(ip_node_ptr);
        port_node_val_ptr       = xmlNodeGetContent(port_node_ptr);
        heartbeat_node_val_ptr  = xmlNodeGetContent(heartbeat_node_ptr);

        bail_require_msg( !(
                mem_name_val_ptr ==NULL
           || ip_node_val_ptr==NULL
           || port_node_val_ptr == NULL
           || heartbeat_node_val_ptr == NULL
           ),"Could not get member node Values.." );


        fprintf(fp, cl_entry, mem_name_val_ptr, ip_node_val_ptr,port_node_val_ptr,heartbeat_node_val_ptr);

        lc_log_debug(jnpr_log_level, "member- %s", mem_name_val_ptr);

        xmlFree(mem_name_val_ptr);
        xmlFree(ip_node_val_ptr);
        xmlFree(port_node_val_ptr);
        xmlFree(heartbeat_node_val_ptr);

        mem_name_val_ptr=NULL;
        ip_node_val_ptr=NULL;
        port_node_val_ptr=NULL;
        heartbeat_node_val_ptr=NULL;
    }

    fprintf(fp, "</ClusterMap>\n");
    fclose(fp); fp = NULL;

bail:
    if (fp) fclose(fp);
    xmlFree(mem_name_val_ptr);
    xmlFree(ip_node_val_ptr);
    xmlFree(port_node_val_ptr);
    xmlFree(heartbeat_node_val_ptr);

    return err;
}

static int agentd_write_one_om(agentd_context_t * context, const unsigned char* ns_name,xmlNodePtr om_nodeptr)
{
    int err = 0;

    int k = 0;
    xmlNodePtr omname_ptr;
    xmlNodeSetPtr mem_setptr;
    const char *base_path = OMAP_CREATION_LOCATION;
    const unsigned char *om_name = NULL;
    char smap_name[256] = { 0 };
    char file_name[512] = { 0 };

    xmlXPathObjectPtr retPathForOmMemset = NULL;

    omname_ptr = find_element_by_name(om_nodeptr->xmlChildrenNode, "name");
    om_name = xmlNodeGetContent(omname_ptr);

    lc_log_debug(jnpr_log_level, "%s : The origin Map Name = %s", ns_name, om_name);

    snprintf(smap_name, sizeof(smap_name), "%s_%s", ns_name, om_name);
    snprintf(file_name, sizeof(file_name), "%s/%s.xml", base_path, smap_name);
    lc_log_debug(jnpr_log_level, "origin-map file name - %s", file_name);

    do
    {
        /*
         * First try to find if this om has consistent-hash/members
         * if not look for origin-escalation/members
         * */

        mem_setptr = GetMatchingNodeSet(om_nodeptr, "consistent-hash/members",&retPathForOmMemset);
        if (mem_setptr != NULL && mem_setptr->nodeNr != 0)
        {
            lc_log_debug(jnpr_log_level, "Found consistent-hash/members count = %d", mem_setptr->nodeNr);

            err = agentd_write_cluster_hash_of_one_om(file_name,mem_setptr);
            bail_error(err);
            break;
        }

        lc_log_debug(jnpr_log_level, "could not find consistent-hash.. checking for orig-esc..");
        mem_setptr = GetMatchingNodeSet(om_nodeptr, "origin-escalation/members",&retPathForOmMemset);

        if (mem_setptr != NULL && mem_setptr->nodeNr != 0)
        {
            lc_log_debug(jnpr_log_level, "Found origin-escalation/members count = %d", mem_setptr->nodeNr);

            err = agentd_write_origin_esc_of_one_om(file_name,mem_setptr);
            bail_error(err);
        }
        else
            goto bail;
    }while(0);

    err = agentd_conf_omap_move_smap(smap_name, file_name);
    bail_error(err);

    /* Add post-commit handler for sending refresh action to the server map*/
    err = agentd_save_post_handler (context, NULL, smap_name, strlen(smap_name), agentd_smap_action_refresh);
    bail_error (err);

bail:
    xmlXPathFreeObject(retPathForOmMemset);
    retPathForOmMemset = NULL;
    xmlFree((xmlChar*)om_name);
    return err;
}

int agentd_regen_smap_xml(agentd_context_t *context)
{
    int err = 0;
    xmlDocPtr doc;
    int i = 0;
    xmlNodeSetPtr xSetPtr = NULL;
    xmlNodePtr cur, ns_nodeptr, nsname_ptr;
    const unsigned char *ns_name = NULL;

    xmlXPathObjectPtr retPathForNS = NULL, retPathForOm=NULL;

    int j = 0;
    xmlNodeSetPtr om_setptr = NULL;

    char file_name[512] = { 0 };

    doc = context->runningConfigDoc;

    if (doc == NULL)
    {
        syslog(LOG_CRIT, "Parsing the origin-map-config.xml failed");
        bail_error(1);
    }

    cur = xmlDocGetRootElement(doc);

    if (cur == NULL || cur->children == NULL)
    {
        syslog(LOG_CRIT, "The root document of origin-map-config.xml is empty\n");
        goto bail;
    }

    xSetPtr = GetMatchingNodeSet(cur, "/mfc-request/data/mfc-cluster/service/http/instance",&retPathForNS);

    if (xSetPtr == NULL || xSetPtr->nodeNr == 0)
    {
        goto bail;
    }

    lc_log_debug(jnpr_log_level, "Need to process %d Namespaces", xSetPtr->nodeNr);

    for (i = 0; i < xSetPtr->nodeNr; i++)
    {

        xmlFree((xmlChar*)ns_name);
        ns_name=NULL;
        xmlXPathFreeObject(retPathForOm);
        retPathForOm = NULL;

        ns_nodeptr = xSetPtr->nodeTab[i];
        nsname_ptr = find_element_by_name(ns_nodeptr->xmlChildrenNode, "name");

        if (nsname_ptr == NULL)
        {
            lc_log_debug(jnpr_log_level, "Could not find name at %d", i);
            continue;
        }

        ns_name = xmlNodeGetContent(nsname_ptr);

        lc_log_debug(jnpr_log_level, "Found namespace %s", ns_name);

        om_setptr = GetMatchingNodeSet(ns_nodeptr, "origin-server/origin-map",&retPathForOm);

        if (om_setptr == NULL || om_setptr->nodeNr == 0)
        {
            lc_log_debug(jnpr_log_level, "No origin map with Namespace %s", ns_name);
            /* we don't have the origin-map */
            continue;
        }

        lc_log_debug(jnpr_log_level, "going to iterate origin maps..");

        for (j = 0; j < om_setptr->nodeNr; j++)
        {
            xmlNodePtr om_nodeptr = om_setptr->nodeTab[j];

            err = agentd_write_one_om(context,ns_name,om_nodeptr);
            bail_error(err);
        }
    }
bail:
    xmlFree((xmlChar*)ns_name);
    ns_name=NULL;

    xmlXPathFreeObject(retPathForNS);
    xmlXPathFreeObject(retPathForOm);

    retPathForNS = NULL;
    retPathForOm = NULL;

    return err;
}

int agentd_origin_map_set(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg,
        char **cli_buff)
{
    int err = 0;
    context->conf_flags |= AF_REGEN_SMAPS_XML;
    lc_log_debug(jnpr_log_level, "custom handling for %s", br_cmd->pattern);
    return err;
}

int agentd_origin_map_set_server_map_nodes(agentd_context_t *context, agentd_binding *br_cmd,
        void *cb_arg, char **cli_buff)
{
    int err = 0;

    const char* ns_name = br_cmd->arg_vals.val_args[1]; //TODO the magic num could be changed to #def
    const char* om_name = br_cmd->arg_vals.val_args[2]; //TODO the magic num could be changed to #def

    bail_null(ns_name);
    bail_null(om_name);

    lc_log_debug(jnpr_log_level,
            "Going to set smap nodes for ns:%s  om_name:%s",
            ns_name, om_name);

    return agentd_conf_omap_set_smap_nodes(context, ns_name, om_name);

bail: return err;
}

static int agentd_origin_map_delete_from_tree(agentd_context_t* context, const char* ns_name, const char* om_name)
{
    node_name_t t;
    int err = 0;

    node_name_t nodeName = { 0 };
    node_name_t smap_name = { 0 };

    snprintf(smap_name, sizeof(smap_name), "%s_%s", ns_name, om_name);

    snprintf(nodeName, sizeof(nodeName), "/nkn/nvsd/server-map/config/%s", smap_name);

    lc_log_debug(jnpr_log_level, "Going to delete node = %s", nodeName);
    err = agentd_append_req_msg(context, nodeName, SUBOP_DELETE, TYPE_STR, "");
    bail_error(err);

    err = agentd_unbind_smap_from_ns(context,ns_name,smap_name);
bail:
    return err;
}

int agentd_origin_map_delete(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg,
        char **cli_buff)
{
    int err = 0;

    err = br_cmd->arg_vals.nargs < 2;
    bail_error(err);

    const char* ns_name = br_cmd->arg_vals.val_args[1]; //TODO the magic num could be changed to #def
    const char* om_name = br_cmd->arg_vals.val_args[2]; //TODO the magic num could be changed to #def

    bail_null(ns_name);
    bail_null(om_name);

    lc_log_debug(jnpr_log_level, "Going to delete smap nodes for ns:%s  om_name:%s", ns_name, om_name);

    /* Add the binding to post handler for deleting the server map file */
    err = agentd_save_post_handler(context, br_cmd, NULL, 0, agentd_delete_smap_xml);
    bail_error (err);

    return agentd_origin_map_delete_from_tree(context, ns_name, om_name);
bail:
    err = -1; /*TODO Need to check if the value is right*/
    return err;
}

int agentd_origin_map_members_deletion(agentd_context_t *context, agentd_binding *br_cmd,
        void *cb_arg, char **cli_buff)
{
    int err = 0;

    lc_log_debug(jnpr_log_level, "custom handling for %s", br_cmd->pattern);

    err = br_cmd->arg_vals.nargs < 3;
    bail_error(err);

    err = 0;

    const char* ns_name = br_cmd->arg_vals.val_args[1]; //TODO the magic num could be changed to #def
    const char* om_name = br_cmd->arg_vals.val_args[2]; //TODO the magic num could be changed to #def
    const char* mem_name = br_cmd->arg_vals.val_args[3]; //TODO the magic num could be changed to #def

    bail_null(ns_name);
    bail_null(om_name);
    bail_null(mem_name);

    lc_log_debug(jnpr_log_level, "Going to delete member nodes for ns:%s  om_name:%s membername = %s", ns_name, om_name,
            mem_name);

    context->conf_flags |= AF_DELETE_SMAP_MEMBERS;

    return err;

bail:
    err = -1; /*TODO Need to check if the value is right*/
    return err;
}

/* Post commit handler for deleting server-map file */
int agentd_delete_smap_xml (void *context, agentd_binding *abinding,
        void *cb_arg)
{
    int err = 0;
    char file_name[1024];

    bail_null(abinding);

    const char* ns_name = abinding->arg_vals.val_args[1];
    const char* om_name = abinding->arg_vals.val_args[2];

    bail_null(ns_name);
    bail_null(om_name);

    snprintf(file_name, sizeof(file_name), "%s/%s_%s.xml", OMAP_FINAL_LOCATION, ns_name, om_name);
    lc_log_debug(jnpr_log_level, "Going to delete server map file: %s", file_name);

    err = lf_remove_file(file_name);
    if (err)
        lc_log_debug(LOG_NOTICE, "errno: %d", errno);

    bail_error(err);

bail:
    return err;
}

/* Post commit handler for refreshing server-map */
int agentd_smap_action_refresh (void *context, agentd_binding *abinding, 
              void *cb_arg)
{
    int err = 0;
    uint32 ret_code = 0;
    tstring * ret_msg = NULL;

    bail_null(cb_arg);
    const char *smap_name = (const char *) cb_arg;

    lc_log_debug(jnpr_log_level, "Sending refresh-force action for server map: %s", smap_name);

    err = mdc_send_action_with_bindings_str_va(
                agentd_mcc, &ret_code, &ret_msg, "/nkn/nvsd/server-map/actions/refresh-force", 1,
                "name", bt_string,smap_name);
    lc_log_debug (jnpr_log_level, "ret_code = %d, ret_msg = %s", ret_code, ts_str(ret_msg));
    if (ret_code != 0) {
        set_ret_code(context, ret_code);
        set_ret_msg(context, ts_str(ret_msg));
        goto bail;
    }

bail:
    return err;
}

