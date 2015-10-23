#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=netsnmpaux

#
# This library is comprised of code which in the original NET-SNMP
# distribution was built into three libraries:
#   - libnetsnmpagent
#   - libnetsnmphelpers
#   - libnetsnmpmibs
#
# The three sections of source files below contain the files
# used in the building of each of these three original libraries. 
#
# Much of this code seems like it should only be needed for the agent,
# but in the original NET-SNMP build, snmptrapd was also linked with
# these three libraries, so that is preserved here.
#

vpath %.c ../../net-snmp/agent
vpath %.c ../../net-snmp/agent/helpers/
vpath %.c ../../net-snmp/agent/mibgroup/

vpath %.c ../../net-snmp/agent/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/snmp-notification-mib/snmpNotifyFilterTable/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/udp-mib/data_access/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/udp-mib/udpEndpointTable/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/host/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/ip-mib/ipv6ScopeZoneIndexTable/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/ip-mib/ipv4InterfaceTable/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/ip-mib/data_access/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/ip-mib/ipSystemStatsTable/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/ip-mib/ipv6InterfaceTable/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/ip-mib/ipIfStatsTable/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/ip-mib/ipDefaultRouterTable/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/ip-mib/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/ip-mib/inetNetToMediaTable/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/ip-mib/ipAddressTable/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/target/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/if-mib/data_access/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/if-mib/ifXTable/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/if-mib/ifTable/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/hardware/cpu/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/hardware/fsys/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/hardware/memory/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/utilities/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/notification/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/tcp-mib/data_access/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/tcp-mib/tcpListenerTable/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/tcp-mib/tcpConnectionTable/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/util_funcs/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/ip-forward-mib/ipCidrRouteTable/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/ip-forward-mib/data_access/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/ip-forward-mib/inetCidrRouteTable/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/notification-log-mib/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/mibII/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/ucd-snmp/BIN_DIST
vpath %.c ../../net-snmp/agent/mibgroup/snmpv3/BIN_DIST

SRCS=   \
	agent_handler.c \
	agent_index.c \
	agent_read_config.c \
	agent_registry.c \
	agent_trap.c \
	auto_nlist.c \
	kernel.c \
	mibgroup/mibII/vacm_conf.c \
	mibgroup/utilities/execute.c \
	mibgroup/utilities/iquery.c \
	mib_modules.c \
	snmp_agent.c \
	snmp_vars.c \
	agent_sysORTable.c \
	\
	all_helpers.c \
	baby_steps.c \
	bulk_to_next.c \
	cache_handler.c \
	debug_handler.c \
	instance.c \
	mode_end_call.c \
	multiplexer.c \
	null.c \
	old_api.c \
	read_only.c \
	row_merge.c \
	scalar.c \
	scalar_group.c \
	serialize.c \
	stash_cache.c \
	stash_to_next.c \
	table_array.c \
	table.c \
	table_container.c \
	table_data.c \
	table_dataset.c \
	table_iterator.c \
	table_row.c \
	table_tdata.c \
	watcher.c \
	\
	util_funcs.c \
	\

ifeq (${PROD_TARGET_OS},LINUX)
SRCS+=\
	hardware/cpu/cpu.c \
	hardware/cpu/cpu_linux.c \
	hardware/fsys/fsys_mntent.c \
	hardware/fsys/hw_fsys.c \
	hardware/memory/hw_mem.c \
	hardware/memory/memory_linux.c \
	header_complex.c \
	host/hr_device.c \
	host/hr_disk.c \
	host/hrh_filesys.c \
	host/hrh_storage.c \
	host/hr_network.c \
	host/hr_other.c \
	host/hr_partition.c \
	host/hr_print.c \
	host/hr_proc.c \
	host/hr_swrun.c \
	host/hr_system.c \
	\
	if-mib/data_access/interface.c \
	if-mib/data_access/interface_ioctl.c \
	if-mib/data_access/interface_linux.c \
	if-mib/ifTable/ifTable.c \
	if-mib/ifTable/ifTable_data_access.c \
	if-mib/ifTable/ifTable_interface.c \
	if-mib/ifXTable/ifXTable.c \
	if-mib/ifXTable/ifXTable_data_access.c \
	if-mib/ifXTable/ifXTable_interface.c \
	ip-forward-mib/data_access/route_common.c \
	ip-forward-mib/data_access/route_ioctl.c \
	ip-forward-mib/data_access/route_linux.c \
	ip-forward-mib/inetCidrRouteTable/inetCidrRouteTable.c \
	ip-forward-mib/inetCidrRouteTable/inetCidrRouteTable_data_access.c \
	ip-forward-mib/inetCidrRouteTable/inetCidrRouteTable_interface.c \
	ip-forward-mib/ipCidrRouteTable/ipCidrRouteTable.c \
	ip-forward-mib/ipCidrRouteTable/ipCidrRouteTable_data_access.c \
	ip-forward-mib/ipCidrRouteTable/ipCidrRouteTable_interface.c \
	ip-mib/data_access/arp_common.c \
	ip-mib/data_access/arp_linux.c \
	ip-mib/data_access/ipaddress_common.c \
	ip-mib/data_access/ipaddress_ioctl.c \
	ip-mib/data_access/ipaddress_linux.c \
	ip-mib/data_access/scalars_linux.c \
	ip-mib/data_access/systemstats_common.c \
	ip-mib/data_access/systemstats_linux.c \
	ip-mib/inetNetToMediaTable/inetNetToMediaTable.c \
	ip-mib/inetNetToMediaTable/inetNetToMediaTable_data_access.c \
	ip-mib/inetNetToMediaTable/inetNetToMediaTable_interface.c \
	ip-mib/ipAddressTable/ipAddressTable.c \
	ip-mib/ipAddressTable/ipAddressTable_data_access.c \
	ip-mib/ipAddressTable/ipAddressTable_interface.c \
	ip-mib/ipIfStatsTable/ipIfStatsTable.c \
	ip-mib/ipIfStatsTable/ipIfStatsTable_data_access.c \
	ip-mib/ipIfStatsTable/ipIfStatsTable_data_get.c \
	ip-mib/ipIfStatsTable/ipIfStatsTable_interface.c \
	ip-mib/ip_scalars.c \
	ip-mib/ipSystemStatsTable/ipSystemStatsTable.c \
	ip-mib/ipSystemStatsTable/ipSystemStatsTable_data_access.c \
	ip-mib/ipSystemStatsTable/ipSystemStatsTable_interface.c \
	ip-mib/ipv4InterfaceTable/ipv4InterfaceTable.c \
	ip-mib/ipv4InterfaceTable/ipv4InterfaceTable_data_access.c \
	ip-mib/ipv4InterfaceTable/ipv4InterfaceTable_interface.c \
	ip-mib/ipDefaultRouterTable/ipDefaultRouterTable.c \
	ip-mib/ipDefaultRouterTable/ipDefaultRouterTable_data_access.c \
	ip-mib/ipDefaultRouterTable/ipDefaultRouterTable_data_get.c \
	ip-mib/ipDefaultRouterTable/ipDefaultRouterTable_interface.c \
	ip-mib/data_access/defaultrouter_common.c \
	ip-mib/data_access/defaultrouter_linux.c \
	mibII/at.c \
	mibII/icmp.c \
	mibII/ipAddr.c \
	mibII/ip.c \
	mibII/kernel_linux.c \
	mibII/route_write.c \
	mibII/setSerialNo.c \
	mibII/snmp_mib_5_5.c \
	mibII/sysORTable.c \
	mibII/system_mib.c \
	mibII/tcp.c \
	mibII/tcpTable.c \
	mibII/udp.c \
	mibII/udpTable.c \
	mibII/updates.c \
	mibII/vacm_context.c \
	mibII/vacm_vars.c \
	mibII/var_route.c \
	notification-log-mib/notification_log.c \
	notification/snmpNotifyFilterProfileTable.c \
	notification/snmpNotifyTable.c \
	snmp-notification-mib/snmpNotifyFilterTable/snmpNotifyFilterTable.c \
	snmp-notification-mib/snmpNotifyFilterTable/snmpNotifyFilterTable_data_access.c \
	snmp-notification-mib/snmpNotifyFilterTable/snmpNotifyFilterTable_interface.c \
	target/snmpTargetAddrEntry.c \
	target/snmpTargetParamsEntry.c \
	target/target.c \
	target/target_counters_5_5.c \
	tcp-mib/data_access/tcpConn_common.c \
	tcp-mib/data_access/tcpConn_linux.c \
	tcp-mib/tcpConnectionTable/tcpConnectionTable.c \
	tcp-mib/tcpConnectionTable/tcpConnectionTable_data_access.c \
	tcp-mib/tcpConnectionTable/tcpConnectionTable_interface.c \
	tcp-mib/tcpListenerTable/tcpListenerTable.c \
	tcp-mib/tcpListenerTable/tcpListenerTable_data_access.c \
	tcp-mib/tcpListenerTable/tcpListenerTable_interface.c \
	udp-mib/data_access/udp_endpoint_common.c \
	udp-mib/data_access/udp_endpoint_linux.c \
	udp-mib/udpEndpointTable/udpEndpointTable.c \
	udp-mib/udpEndpointTable/udpEndpointTable_data_access.c \
	udp-mib/udpEndpointTable/udpEndpointTable_interface.c \
	util_funcs/get_pid_from_inode.c \
	util_funcs/header_generic.c \
	util_funcs/header_simple_table.c \
	utilities/override.c \
	utilities/snmp_get_statistic.c \
	snmpv3/snmpEngine.c \
	snmpv3/snmpMPDStats_5_5.c \
	snmpv3/usmConf.c \
	snmpv3/usmStats_5_5.c \
	snmpv3/usmUser.c \

endif

ifneq (${PROD_FEATURE_UCD_SNMP_MIB},0)
SRCS+=\
	ucd-snmp/memory.c \
	ucd-snmp/vmstat.c \
	ucd-snmp/proc.c \
	ucd-snmp/versioninfo.c \
	ucd-snmp/pass.c \
	ucd-snmp/pass_persist.c \
	ucd-snmp/disk.c \
	ucd-snmp/extensible.c \
	ucd-snmp/errormib.c \
	ucd-snmp/file.c \
	ucd-snmp/dlmod.c \
	ucd-snmp/proxy.c \
	ucd-snmp/logmatch.c \
	ucd-snmp/loadave.c \

endif

ifneq (${PROD_FEATURE_SNMP_AGENTX},0)
SRCS+=\
	mibgroup/agentx/master.c \
	mibgroup/agentx/subagent.c \
	mibgroup/agentx/protocol.c \
	mibgroup/agentx/client.c \
	mibgroup/agentx/master_admin.c \
	mibgroup/agentx/agentx_config.c \

endif

LIB_BUILD_DIR=${ILIB_DIR}/bin/snmp/lib/libnetsnmpaux

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/include \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/include/net-snmp/agent \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/agent \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/agent/mibgroup \
	-I${PROD_TREE_ROOT}/src/bin/snmp/net-snmp/snmplib \
	-I${PROD_TREE_ROOT}/src/bin/snmp/tms \

DEFINES= \
	-D_GNU_SOURCE \


ifneq (${PROD_FEATURE_IPV6},0)
	SRCS+= \
		ip-mib/ipv6InterfaceTable/ipv6InterfaceTable.c \
		ip-mib/ipv6InterfaceTable/ipv6InterfaceTable_data_access.c \
		ip-mib/ipv6InterfaceTable/ipv6InterfaceTable_interface.c \
		ip-mib/data_access/ipv6scopezone_common.c \
		ip-mib/data_access/ipv6scopezone_linux.c \
		ip-mib/ipv6ScopeZoneIndexTable/ipv6ScopeZoneIndexTable.c \
		ip-mib/ipv6ScopeZoneIndexTable/ipv6ScopeZoneIndexTable_data_access.c \
		ip-mib/ipv6ScopeZoneIndexTable/ipv6ScopeZoneIndexTable_interface.c \

endif

ifdef STATIC_MODULES
    DEFINES+=-DSTATIC_MODULES
endif

WARNINGS_OVERRIDE=1
WARNINGS= \
	-Wall \
	-Wstrict-prototypes \
	-Wno-unused \
	-Wno-format \

# XXXX/EMT: can any of these be removed?
WARNINGS+= \
	-Wno-deprecated-declarations \
	-Wno-implicit \
	-Wno-int-to-pointer-cast \
	-Wno-pointer-to-int-cast \


ifeq ($(call CC_VERSION_ATLEAST,40000,1,0),1)
	WARNINGS+= \
		-Wno-pointer-sign \

endif

ifeq ($(call CC_VERSION_ATLEAST,40200,1,0),1)
	WARNINGS+= \
		-Wno-address \

endif


ifeq (${PROD_TARGET_OS},SUNOS)
    CFLAGS+= -std=gnu89
    DEFINES+= -Dsolaris2
    INCLUDES+= -I/usr/local/ssl/include
endif

ifeq (${PROD_TARGET_OS},LINUX)
    DEFINES+= -Dlinux
    SRCS+= mibII/kernel_linux.c
endif

ifeq (${PROD_TARGET_OS},FREEBSD)
	DEFINES+= -Dfreebsd6 -DNETSNMP_NO_INLINE
endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk
