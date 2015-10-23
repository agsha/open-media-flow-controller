/*
 *
 * src/bin/snmp/net-snmp/include/net-snmp/library/snmpv3-security-includes.h
 *
 *
 */

#if defined(PROD_TARGET_OS_LINUX)
    #include "snmpv3-security-includes.h.linux"
#elif defined(PROD_TARGET_OS_SUNOS)
    #include "snmpv3-security-includes.h.solaris"
#elif defined(PROD_TARGET_OS_FREEBSD)
    #include "snmpv3-security-includes.h.freebsd"
#endif
