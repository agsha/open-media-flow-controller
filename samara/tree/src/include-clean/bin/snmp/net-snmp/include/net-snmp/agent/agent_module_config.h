/*
 *
 * src/bin/snmp/net-snmp/include/net-snmp/agent/agent_module_config.h
 *
 *
 */

#if defined(PROD_TARGET_OS_LINUX)
    #include "agent_module_config.h.linux"
#elif defined(PROD_TARGET_OS_SUNOS)
    #include "agent_module_config.h.solaris"
#elif defined(PROD_TARGET_OS_FREEBSD)
    #include "agent_module_config.h.freebsd"
#endif
