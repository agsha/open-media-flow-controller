/*
 *
 * Filename:  mdm_db.inc.c
 * Date:      2010/10/26
 * Author:    Manikandan Vengatachalam
 *
 * (C) Copyright 2010 Juniper Networks, Inc.
 * All rights reserved.
 *
 *
 */


#ifndef __MDM_DB_INC_C_
#define __MDM_DB_INC_C_
#endif /* __MDM_DB_INC_C_ */


/* 
 * This file is included by src/bin/mgmtd/mdm_db.c .
*/

/* ========================================================================= */
/* Customer-specific graft point 4: nodes to preserve during a
 * "configuration new <db>"
 * =========================================================================
 */

#if MDM_DB_INC_GRAFT_POINT == 4
"/nkn/nvsd/diskcache/config",
"/net/interface/config/eth0",
"/net/interface/config/eth1",
#endif /* GRAFT_POINT 4 */

/*Preserver the disc configuration for "keep-connect"*/
#if MDM_DB_INC_GRAFT_POINT == 3
"/nkn/nvsd/diskcache/config",
"/net/bonding/config",
#endif /* GRAFT_POINT 3 */

/* ========================================================================= */
/* Customer-specific graft point 1: import-id array declarations 
 * for "keep_basic_reach_dmi"
 * =========================================================================
 */
#if MDM_DB_INC_GRAFT_POINT == 1
static const char *mdi_keep_basic_reach_dmi_source_predelete[] = {
    NULL
};

static const char *mdi_keep_basic_reach_dmi_target_predelete[] = {
/* Add nodes defined in mdi_keep_basic_reach_target_predelete[] */
    "/net/interface",
    "/net/routes",
    "/net/arp",
    "/license",
    "/ssh/server/hostkey",

#if defined (PROD_FEATURE_CMC_CLIENT) || defined (PROD_FEATURE_CMC_SERVER)
    "/cmc/config/available",
    "/cmc/config/rendezvous",
    "/cmc/client/config/available",
    "/cmc/client/config/rendezvous",
    "/auth/passwd/user/cmcrendv",
    "/ssh/client/username/cmcrendv",
    "/ssh/server/username/cmcrendv",
    "/cmc/common/config/auth/ssh-dsa2/identity/client-default",
    "/cmc/common/config/service_name",
#endif

/* Add nodes defined in Customer-specific GRAFT POINT 3: */
    "/nkn/nvsd/diskcache/config",
    "/net/bonding/config",

/* Add nodes defined in Customer-specific GRAFT POINT 4: */
    "/nkn/nvsd/diskcache/config",

/* Added dmi specific nodes */
    "/nkn/nvsd/system/config/mod_dmi",
    "/web",
    "/virt",
    "/pm/process/agentd",
    "/pm/process/ssh_tunnel",

    NULL
};

static const char *mdi_keep_basic_reach_dmi_source_include[] = {
/* Add nodes defined in mdi_keep_basic_reach_source_include[] */
    "/net/interface",
    "/net/routes",
    "/net/arp",
    "/license",
    "/ssh/server/hostkey",

#if defined (PROD_FEATURE_CMC_CLIENT) || defined (PROD_FEATURE_CMC_SERVER)
    "/cmc/config/available",
    "/cmc/config/rendezvous",
    "/cmc/client/config/available",
    "/cmc/client/config/rendezvous",
    "/auth/passwd/user/cmcrendv",
    "/ssh/client/username/cmcrendv",
    "/ssh/server/username/cmcrendv",
    "/cmc/common/config/auth/ssh-dsa2/identity/client-default",
    "/cmc/common/config/service_name",
#endif

/* Add nodes defined in Customer-specific GRAFT POINT 3: */
    "/nkn/nvsd/diskcache/config",
    "/net/bonding/config",

/* Add nodes defined in Customer-specific GRAFT POINT 4: */
    "/nkn/nvsd/diskcache/config",

/* Added dmi specific nodes */
    "/nkn/nvsd/system/config/mod_dmi",
    "/web",
    "/virt",
    "/pm/process/agentd",
    "/pm/process/ssh_tunnel",

    NULL
};

static const char *mdi_keep_basic_reach_dmi_target_postinclude_orig[] = {
    NULL
};

#endif // GRAFT POINT 1

/* ============================================================================= */
/* Customer-specific graft point 2: import-id mapping array for "keep_basic_reach_dmi"
 * =============================================================================
 */
#if MDM_DB_INC_GRAFT_POINT == 2

{"keep_basic_reach_dmi", mdi_keep_basic_reach_dmi_source_predelete,
mdi_keep_basic_reach_dmi_target_predelete,
mdi_keep_basic_reach_dmi_source_include,
mdi_keep_basic_reach_dmi_target_postinclude_orig},

#endif // GRAFT POINT 2
