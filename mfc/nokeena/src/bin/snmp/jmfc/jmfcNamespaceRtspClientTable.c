/*
 * Note: this file originally auto-generated by mib2c using
 *       version : 13834 $ of $ 
 *
 * $Id:$
 */
/** \mainpage MFD helper for jmfcNamespaceRtspClientTable
 *
 * \section intro Introduction
 * Introductory text.
 *
 */
/*
 * standard Net-SNMP includes 
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

/*
 * include our parent header 
 */
#include "jmfcNamespaceRtspClientTable.h"

#include <net-snmp/agent/mib_modules.h>

#include "jmfcNamespaceRtspClientTable_interface.h"

oid             jmfcNamespaceRtspClientTable_oid[] =
    { JMFCNAMESPACERTSPCLIENTTABLE_OID };
int             jmfcNamespaceRtspClientTable_oid_size =
OID_LENGTH(jmfcNamespaceRtspClientTable_oid);

jmfcNamespaceRtspClientTable_registration
    jmfcNamespaceRtspClientTable_user_context;

void initialize_table_jmfcNamespaceRtspClientTable(void);
void shutdown_table_jmfcNamespaceRtspClientTable(void);


/**
 * Initializes the jmfcNamespaceRtspClientTable module
 */
void
init_jmfcNamespaceRtspClientTable(void)
{
    DEBUGMSGTL(("verbose:jmfcNamespaceRtspClientTable:init_jmfcNamespaceRtspClientTable","called\n"));

    /*
     * TODO:300:o: Perform jmfcNamespaceRtspClientTable one-time module initialization.
     */
     
    /*
     * here we initialize all the tables we're planning on supporting
     */
    if (should_init("jmfcNamespaceRtspClientTable"))
        initialize_table_jmfcNamespaceRtspClientTable();

} /* init_jmfcNamespaceRtspClientTable */

/**
 * Shut-down the jmfcNamespaceRtspClientTable module (agent is exiting)
 */
void
shutdown_jmfcNamespaceRtspClientTable(void)
{
    if (should_init("jmfcNamespaceRtspClientTable"))
        shutdown_table_jmfcNamespaceRtspClientTable();

}

/**
 * Initialize the table jmfcNamespaceRtspClientTable 
 *    (Define its contents and how it's structured)
 */
void
initialize_table_jmfcNamespaceRtspClientTable(void)
{
    jmfcNamespaceRtspClientTable_registration * user_context;
    u_long flags;

    DEBUGMSGTL(("verbose:jmfcNamespaceRtspClientTable:initialize_table_jmfcNamespaceRtspClientTable","called\n"));

    /*
     * TODO:301:o: Perform jmfcNamespaceRtspClientTable one-time table initialization.
     */

    /*
     * TODO:302:o: |->Initialize jmfcNamespaceRtspClientTable user context
     * if you'd like to pass in a pointer to some data for this
     * table, allocate or set it up here.
     */
    /*
     * a netsnmp_data_list is a simple way to store void pointers. A simple
     * string token is used to add, find or remove pointers.
     */
    user_context =
        netsnmp_create_data_list("jmfcNamespaceRtspClientTable", NULL,
                                 NULL);
    
    /*
     * No support for any flags yet, but in the future you would
     * set any flags here.
     */
    flags = 0;
    
    /*
     * call interface initialization code
     */
    _jmfcNamespaceRtspClientTable_initialize_interface(user_context,
                                                       flags);
} /* initialize_table_jmfcNamespaceRtspClientTable */

/**
 * Shutdown the table jmfcNamespaceRtspClientTable 
 */
void
shutdown_table_jmfcNamespaceRtspClientTable(void)
{
    /*
     * call interface shutdown code
     */
    _jmfcNamespaceRtspClientTable_shutdown_interface
        (&jmfcNamespaceRtspClientTable_user_context);
}

/**
 * pre-request callback
 *
 *
 * @retval MFD_SUCCESS              : success.
 * @retval MFD_ERROR                : other error
 */
int
jmfcNamespaceRtspClientTable_pre_request
    (jmfcNamespaceRtspClientTable_registration * user_context)
{
    DEBUGMSGTL(("verbose:jmfcNamespaceRtspClientTable:jmfcNamespaceRtspClientTable_pre_request","called\n"));

    /*
     * TODO:510:o: Perform jmfcNamespaceRtspClientTable pre-request actions.
     */

    return MFD_SUCCESS;
} /* jmfcNamespaceRtspClientTable_pre_request */

/**
 * post-request callback
 *
 * Note:
 *   New rows have been inserted into the container, and
 *   deleted rows have been removed from the container and
 *   released.
 *
 * @param rc : MFD_SUCCESS if all requests succeeded
 *
 * @retval MFD_SUCCESS : success.
 * @retval MFD_ERROR   : other error (ignored)
 */
int
jmfcNamespaceRtspClientTable_post_request
    (jmfcNamespaceRtspClientTable_registration * user_context, int rc)
{
    DEBUGMSGTL(("verbose:jmfcNamespaceRtspClientTable:jmfcNamespaceRtspClientTable_post_request","called\n"));

    /*
     * TODO:511:o: Perform jmfcNamespaceRtspClientTable post-request actions.
     */

    return MFD_SUCCESS;
} /* jmfcNamespaceRtspClientTable_post_request */


/** @{ */
