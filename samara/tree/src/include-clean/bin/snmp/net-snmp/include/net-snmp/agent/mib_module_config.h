/*
 *
 * src/bin/snmp/net-snmp/include/net-snmp/agent/mib_module_config.h
 *
 *
 */

#if defined(PROD_TARGET_OS_LINUX)
    #include "mib_module_config.h.linux"
#elif defined(PROD_TARGET_OS_SUNOS)
    #include "mib_module_config.h.solaris"
#elif defined(PROD_TARGET_OS_FREEBSD)
    #include "mib_module_config.h.freebsd"
#endif
