
/*
 * This is starting point of JUNIPER-SNMP-MIB snmp
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

//#include "jmfc_scalars.h"
#include "jmfcNamespaceTable.h"
#include "jmfcNamespaceHttpClientTable.h"
#include "jmfcNamespaceHttpServerTable.h"
#include "jmfcNamespaceRtspClientTable.h"
#include "jmfcNamespaceRtspServerTable.h"
//#include "jmfcDiskCacheTable.h"
#include "jmfcInterfaceTable.h"

void
init_jmfc_snmp(void)
{
    //init_jmfc_scalars();
    //init_jmfcDiskCacheTable();
    //init_jmfcNamespaceTable();
    //init_jmfcNamespaceHttpClientTable();
    //init_jmfcNamespaceHttpServerTable();
    //init_jmfcNamespaceRtspClientTable();
    //init_jmfcNamespaceRtspServerTable();
    init_jmfcInterfaceTable();

    return;
}
