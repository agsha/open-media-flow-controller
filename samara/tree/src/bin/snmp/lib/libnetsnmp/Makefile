#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=netsnmp

vpath %.c ../../net-snmp/snmplib
vpath %.c ../../net-snmp/snmplib/BIN_DIST

SRCS=   \
	asn1.c \
	callback.c \
	check_varbind.c \
	cmu_compat.c \
	container_binary_array.c \
	container.c \
	container_iterator.c \
	container_list_ssll.c \
	container_null.c \
	data_list.c \
	default_store.c \
	dir_utils.c \
	fd_event_manager.c \
	file_utils.c \
	int64.c \
	keytools.c \
	large_fd_set.c \
	lcd_time.c \
	md5.c \
	mib.c \
	mt_support.c \
	oid_stash.c \
	parse.c \
	pkcs.c \
	read_config.c \
	scapi.c \
	snmp_alarm.c \
	snmpAliasDomain.c \
	snmp_api.c \
	snmp_auth.c \
	snmp.c \
	snmpCallbackDomain.c \
	snmp_client.c \
	snmp_debug.c \
	snmp_enum.c \
	snmp_logging.c \
	snmp_openssl.c \
	snmp_parse_args.c \
	snmp_secmod.c \
	snmp_service.c \
	snmp-tc.c \
	snmpTCPDomain.c \
	snmp_transport.c \
	snmpUDPDomain.c \
	snmpUnixDomain.c \
	snmpusm.c \
	snmpv3.c \
	snmp_version.c \
	snprintf.c \
	strtok_r.c \
	strtol.c \
	strtoul.c \
	system.c \
	text_utils.c \
	tools.c \
	ucd_compat.c \
	vacm.c \

ifneq (${PROD_FEATURE_IPV6},0)
	SRCS+=           \
		snmpTCPIPv6Domain.c \
		snmpUDPIPv6Domain.c \

endif

LIB_BUILD_DIR=${ILIB_DIR}/bin/snmp/lib/libnetsnmp

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/include

DEFINES= \
	-D_SVID_SOURCE \
	-D_GNU_SOURCE \

WARNINGS_OVERRIDE=1

# XXXX/EMT: try to reinstate some of these?
#WARNINGS= \
#	-Wall \
#	-Wstrict-prototypes \
#	-Wno-unused \
#	-Wno-format \

ifeq ($(call CC_VERSION_ATLEAST,40000,1,0),1)
	WARNINGS+= \
		-Wno-pointer-sign \

endif

ifeq ($(call CC_VERSION_ATLEAST,40200,1,0),1)
	WARNINGS+= \
		-Wno-address \

endif

ifeq (${PROD_TARGET_OS},LINUX)
    DEFINES+= -Dlinux
endif

ifeq (${PROD_TARGET_OS},SUNOS)
    CFLAGS+= -std=gnu89
    DEFINES+= -Dsolaris2
    INCLUDES+= -I/usr/local/ssl/include
endif

ifeq (${PROD_TARGET_OS},FREEBSD)
    CFLAGS+= -std=gnu89 -Dfreebsd6 -DNETSNMP_NO_INLINE
endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk
