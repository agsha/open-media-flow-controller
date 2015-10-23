/*
 *
 * src/bin/snmp/net-snmp/include/net-snmp/net-snmp-config.h
 *
 *
 */

#if defined(PROD_TARGET_OS_LINUX)
    #include "net-snmp-config.h.linux"
#elif defined(PROD_TARGET_OS_SUNOS)
    #include "net-snmp-config.h.solaris"
#elif defined(PROD_TARGET_OS_FREEBSD)
    #include "net-snmp-config.h.freebsd"
#endif
