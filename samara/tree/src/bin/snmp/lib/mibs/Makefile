#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

#
# See src/mk/files_adv.mk for comments explaining the different types
# of directives available.
#

# XXX: /usr/share isn't specific to us, and should probably be
# created by image_files.  But it isn't installed until after us.
# Ideally there should probably be a separate Makefile that runs before
# everyone and creates all the directories, but we'll leave that for
# another day.

INSTALL_DIRECTIVES= \
	${SYS_INSTALL_DIR_usr_share} \
	${SYS_INSTALL_DIR_usr_share_snmp} \
	${SYS_INSTALL_DIR_usr_share_snmp_mibs} \
	${SYS_INSTALL_DIR_usr_share_snmp_conf} \
	\
	${SYS_INSTALL_FILE_etherlike_mib} \
	${SYS_INSTALL_FILE_host_resources_mib} \
	${SYS_INSTALL_FILE_host_resources_types} \
	${SYS_INSTALL_FILE_iana_address_family_numbers_mib} \
	${SYS_INSTALL_FILE_iana_language_mib} \
	${SYS_INSTALL_FILE_ianaiftype_mib} \
	${SYS_INSTALL_FILE_iana_rtproto_mib} \
	${SYS_INSTALL_FILE_if_inverted_stack_mib} \
	${SYS_INSTALL_FILE_if_mib} \
	${SYS_INSTALL_FILE_inet_address_mib} \
	${SYS_INSTALL_FILE_ip_forward_mib} \
	${SYS_INSTALL_FILE_ip_mib} \
	${SYS_INSTALL_FILE_notification_log_mib} \
	${SYS_INSTALL_FILE_rfc_1215} \
	${SYS_INSTALL_FILE_rfc1155_smi} \
	${SYS_INSTALL_FILE_rfc1213_mib} \
	${SYS_INSTALL_FILE_snmp_community_mib} \
	${SYS_INSTALL_FILE_snmp_framework_mib} \
	${SYS_INSTALL_FILE_snmp_mpd_mib} \
	${SYS_INSTALL_FILE_snmp_notification_mib} \
	${SYS_INSTALL_FILE_snmp_proxy_mib} \
	${SYS_INSTALL_FILE_snmp_target_mib} \
	${SYS_INSTALL_FILE_snmp_user_based_sm_mib} \
	${SYS_INSTALL_FILE_snmp_usm_aes_mib} \
	${SYS_INSTALL_FILE_snmp_view_based_acm_mib} \
	${SYS_INSTALL_FILE_snmpv2_conf} \
	${SYS_INSTALL_FILE_snmpv2_mib} \
	${SYS_INSTALL_FILE_snmpv2_smi} \
	${SYS_INSTALL_FILE_snmpv2_tc} \
	${SYS_INSTALL_FILE_snmpv2_tm} \
	${SYS_INSTALL_FILE_tcp_mib} \
	${SYS_INSTALL_FILE_udp_mib} \

ifneq (${PROD_FEATURE_IPV6},0)
	INSTALL_DIRECTIVES+=\
	${SYS_INSTALL_FILE_snmptrapd_conf_ipv6} \

else
	INSTALL_DIRECTIVES+=\
	${SYS_INSTALL_FILE_snmptrapd_conf} \

endif

ifneq (${PROD_FEATURE_UCD_SNMP_MIB},0)
	INSTALL_DIRECTIVES+=\
	${SYS_INSTALL_FILE_ucd_snmp_mib} \
	${SYS_INSTALL_FILE_ucd_demo_mib} \
	${SYS_INSTALL_FILE_ucd_dlmod_mib} \

#	${SYS_INSTALL_FILE_net_snmp_extend_mib} \

	SYS_INSTALL_FILE_ucd_snmp_mib="file ../../net-snmp/mibs/UCD-SNMP-MIB.txt 0644 root root /usr/share/snmp/mibs/UCD-SNMP-MIB.txt"
	SYS_INSTALL_FILE_ucd_demo_mib="file ../../net-snmp/mibs/UCD-DEMO-MIB.txt 0644 root root /usr/share/snmp/mibs/UCD-DEMO-MIB.txt"
	SYS_INSTALL_FILE_net_snmp_extend_mib="file ../../net-snmp/mibs/NET-SNMP-EXTEND-MIB.txt 0644 root root /usr/share/snmp/mibs/NET-SNMP-EXTEND-MIB.txt"
	SYS_INSTALL_FILE_ucd_dlmod_mib="file ../../net-snmp/mibs/UCD-DLMOD-MIB.txt 0644 root root /usr/share/snmp/mibs/UCD-DLMOD-MIB.txt"
endif

SYS_INSTALL_DIR_usr_share="dir 0755 root root /usr/share"
SYS_INSTALL_DIR_usr_share_snmp="dir 0755 root root /usr/share/snmp"
SYS_INSTALL_DIR_usr_share_snmp_mibs="dir 0755 root root /usr/share/snmp/mibs"
SYS_INSTALL_DIR_usr_share_snmp_conf="dir 0755 root root /usr/share/snmp/conf"

# This is sort of a stowaway in this file.
SYS_INSTALL_FILE_snmptrapd_conf="file snmptrapd.conf 0644 root root /usr/share/snmp/conf/snmptrapd.conf"
SYS_INSTALL_FILE_snmptrapd_conf_ipv6="file snmptrapd.conf.ipv6 0644 root root /usr/share/snmp/conf/snmptrapd.conf"

SYS_INSTALL_FILE_rfc1213_mib="file ../../net-snmp/mibs/RFC1213-MIB.txt 0644 root root /usr/share/snmp/mibs/RFC1213-MIB.txt"
SYS_INSTALL_FILE_rfc1155_smi="file ../../net-snmp/mibs/RFC1155-SMI.txt 0644 root root /usr/share/snmp/mibs/RFC1155-SMI.txt"
SYS_INSTALL_FILE_snmpv2_mib="file ../../net-snmp/mibs/SNMPv2-MIB.txt 0644 root root /usr/share/snmp/mibs/SNMPv2-MIB.txt"
SYS_INSTALL_FILE_snmpv2_smi="file ../../net-snmp/mibs/SNMPv2-SMI.txt 0644 root root /usr/share/snmp/mibs/SNMPv2-SMI.txt"

SYS_INSTALL_FILE_etherlike_mib="file ../../net-snmp/mibs/EtherLike-MIB.txt 0644 root root /usr/share/snmp/mibs/EtherLike-MIB.txt"
SYS_INSTALL_FILE_host_resources_mib="file ../../net-snmp/mibs/HOST-RESOURCES-MIB.txt 0644 root root /usr/share/snmp/mibs/HOST-RESOURCES-MIB.txt"
SYS_INSTALL_FILE_host_resources_types="file ../../net-snmp/mibs/HOST-RESOURCES-TYPES.txt 0644 root root /usr/share/snmp/mibs/HOST-RESOURCES-TYPES.txt"
SYS_INSTALL_FILE_iana_address_family_numbers_mib="file ../../net-snmp/mibs/IANA-ADDRESS-FAMILY-NUMBERS-MIB.txt 0644 root root /usr/share/snmp/mibs/IANA-ADDRESS-FAMILY-NUMBERS-MIB.txt"
SYS_INSTALL_FILE_iana_language_mib="file ../../net-snmp/mibs/IANA-LANGUAGE-MIB.txt 0644 root root /usr/share/snmp/mibs/IANA-LANGUAGE-MIB.txt"
SYS_INSTALL_FILE_ianaiftype_mib="file ../../net-snmp/mibs/IANAifType-MIB.txt 0644 root root /usr/share/snmp/mibs/IANAifType-MIB.txt"
SYS_INSTALL_FILE_iana_rtproto_mib="file ../../net-snmp/mibs/IANA-RTPROTO-MIB.txt 0644 root root /usr/share/snmp/mibs/IANA-RTPROTO-MIB.txt"
SYS_INSTALL_FILE_if_inverted_stack_mib="file ../../net-snmp/mibs/IF-INVERTED-STACK-MIB.txt 0644 root root /usr/share/snmp/mibs/IF-INVERTED-STACK-MIB.txt"
SYS_INSTALL_FILE_if_mib="file ../../net-snmp/mibs/IF-MIB.txt 0644 root root /usr/share/snmp/mibs/IF-MIB.txt"
SYS_INSTALL_FILE_inet_address_mib="file ../../net-snmp/mibs/INET-ADDRESS-MIB.txt 0644 root root /usr/share/snmp/mibs/INET-ADDRESS-MIB.txt"
SYS_INSTALL_FILE_ip_forward_mib="file ../../net-snmp/mibs/IP-FORWARD-MIB.txt 0644 root root /usr/share/snmp/mibs/IP-FORWARD-MIB.txt"
SYS_INSTALL_FILE_ip_mib="file ../../net-snmp/mibs/IP-MIB.txt 0644 root root /usr/share/snmp/mibs/IP-MIB.txt"
SYS_INSTALL_FILE_notification_log_mib="file ../../net-snmp/mibs/NOTIFICATION-LOG-MIB.txt 0644 root root /usr/share/snmp/mibs/NOTIFICATION-LOG-MIB.txt"
SYS_INSTALL_FILE_rfc_1215="file ../../net-snmp/mibs/RFC-1215.txt 0644 root root /usr/share/snmp/mibs/RFC-1215.txt"
SYS_INSTALL_FILE_rfc1155_smi="file ../../net-snmp/mibs/RFC1155-SMI.txt 0644 root root /usr/share/snmp/mibs/RFC1155-SMI.txt"
SYS_INSTALL_FILE_rfc1213_mib="file ../../net-snmp/mibs/RFC1213-MIB.txt 0644 root root /usr/share/snmp/mibs/RFC1213-MIB.txt"
SYS_INSTALL_FILE_snmp_community_mib="file ../../net-snmp/mibs/SNMP-COMMUNITY-MIB.txt 0644 root root /usr/share/snmp/mibs/SNMP-COMMUNITY-MIB.txt"
SYS_INSTALL_FILE_snmp_framework_mib="file ../../net-snmp/mibs/SNMP-FRAMEWORK-MIB.txt 0644 root root /usr/share/snmp/mibs/SNMP-FRAMEWORK-MIB.txt"
SYS_INSTALL_FILE_snmp_mpd_mib="file ../../net-snmp/mibs/SNMP-MPD-MIB.txt 0644 root root /usr/share/snmp/mibs/SNMP-MPD-MIB.txt"
SYS_INSTALL_FILE_snmp_notification_mib="file ../../net-snmp/mibs/SNMP-NOTIFICATION-MIB.txt 0644 root root /usr/share/snmp/mibs/SNMP-NOTIFICATION-MIB.txt"
SYS_INSTALL_FILE_snmp_proxy_mib="file ../../net-snmp/mibs/SNMP-PROXY-MIB.txt 0644 root root /usr/share/snmp/mibs/SNMP-PROXY-MIB.txt"
SYS_INSTALL_FILE_snmp_target_mib="file ../../net-snmp/mibs/SNMP-TARGET-MIB.txt 0644 root root /usr/share/snmp/mibs/SNMP-TARGET-MIB.txt"
SYS_INSTALL_FILE_snmp_user_based_sm_mib="file ../../net-snmp/mibs/SNMP-USER-BASED-SM-MIB.txt 0644 root root /usr/share/snmp/mibs/SNMP-USER-BASED-SM-MIB.txt"
SYS_INSTALL_FILE_snmp_usm_aes_mib="file ../../net-snmp/mibs/SNMP-USM-AES-MIB.txt 0644 root root /usr/share/snmp/mibs/SNMP-USM-AES-MIB.txt"
SYS_INSTALL_FILE_snmp_view_based_acm_mib="file ../../net-snmp/mibs/SNMP-VIEW-BASED-ACM-MIB.txt 0644 root root /usr/share/snmp/mibs/SNMP-VIEW-BASED-ACM-MIB.txt"
SYS_INSTALL_FILE_snmpv2_conf="file ../../net-snmp/mibs/SNMPv2-CONF.txt 0644 root root /usr/share/snmp/mibs/SNMPv2-CONF.txt"
SYS_INSTALL_FILE_snmpv2_mib="file ../../net-snmp/mibs/SNMPv2-MIB.txt 0644 root root /usr/share/snmp/mibs/SNMPv2-MIB.txt"
SYS_INSTALL_FILE_snmpv2_smi="file ../../net-snmp/mibs/SNMPv2-SMI.txt 0644 root root /usr/share/snmp/mibs/SNMPv2-SMI.txt"
SYS_INSTALL_FILE_snmpv2_tc="file ../../net-snmp/mibs/SNMPv2-TC.txt 0644 root root /usr/share/snmp/mibs/SNMPv2-TC.txt"
SYS_INSTALL_FILE_snmpv2_tm="file ../../net-snmp/mibs/SNMPv2-TM.txt 0644 root root /usr/share/snmp/mibs/SNMPv2-TM.txt"
SYS_INSTALL_FILE_tcp_mib="file ../../net-snmp/mibs/TCP-MIB.txt 0644 root root /usr/share/snmp/mibs/TCP-MIB.txt"
SYS_INSTALL_FILE_udp_mib="file ../../net-snmp/mibs/UDP-MIB.txt 0644 root root /usr/share/snmp/mibs/UDP-MIB.txt"


# The following are not installed on purpose, as they are not currently used.
SYS_INSTALL_FILE_agentx_mib="file ../../net-snmp/mibs/AGENTX-MIB.txt 0644 root root /usr/share/snmp/mibs/AGENTX-MIB.txt"
SYS_INSTALL_FILE_hcnum_tc="file ../../net-snmp/mibs/HCNUM-TC.txt 0644 root root /usr/share/snmp/mibs/HCNUM-TC.txt"
SYS_INSTALL_FILE_ipv6_icmp_mib="file ../../net-snmp/mibs/IPV6-ICMP-MIB.txt 0644 root root /usr/share/snmp/mibs/IPV6-ICMP-MIB.txt"
SYS_INSTALL_FILE_ipv6_mib="file ../../net-snmp/mibs/IPV6-MIB.txt 0644 root root /usr/share/snmp/mibs/IPV6-MIB.txt"
SYS_INSTALL_FILE_ipv6_tc="file ../../net-snmp/mibs/IPV6-TC.txt 0644 root root /usr/share/snmp/mibs/IPV6-TC.txt"
SYS_INSTALL_FILE_ipv6_tcp_mib="file ../../net-snmp/mibs/IPV6-TCP-MIB.txt 0644 root root /usr/share/snmp/mibs/IPV6-TCP-MIB.txt"
SYS_INSTALL_FILE_ipv6_udp_mib="file ../../net-snmp/mibs/IPV6-UDP-MIB.txt 0644 root root /usr/share/snmp/mibs/IPV6-UDP-MIB.txt"
SYS_INSTALL_FILE_net_snmp_agent_mib="file ../../net-snmp/mibs/NET-SNMP-AGENT-MIB.txt 0644 root root /usr/share/snmp/mibs/NET-SNMP-AGENT-MIB.txt"
SYS_INSTALL_FILE_net_snmp_examples_mib="file ../../net-snmp/mibs/NET-SNMP-EXAMPLES-MIB.txt 0644 root root /usr/share/snmp/mibs/NET-SNMP-EXAMPLES-MIB.txt"
SYS_INSTALL_FILE_net_snmp_mib="file ../../net-snmp/mibs/NET-SNMP-MIB.txt 0644 root root /usr/share/snmp/mibs/NET-SNMP-MIB.txt"
SYS_INSTALL_FILE_net_snmp_monitor_mib="file ../../net-snmp/mibs/NET-SNMP-MONITOR-MIB.txt 0644 root root /usr/share/snmp/mibs/NET-SNMP-MONITOR-MIB.txt"
SYS_INSTALL_FILE_net_snmp_system_mib="file ../../net-snmp/mibs/NET-SNMP-SYSTEM-MIB.txt 0644 root root /usr/share/snmp/mibs/NET-SNMP-SYSTEM-MIB.txt"
SYS_INSTALL_FILE_net_snmp_tc="file ../../net-snmp/mibs/NET-SNMP-TC.txt 0644 root root /usr/share/snmp/mibs/NET-SNMP-TC.txt"
SYS_INSTALL_FILE_network_services_mib="file ../../net-snmp/mibs/NETWORK-SERVICES-MIB.txt 0644 root root /usr/share/snmp/mibs/NETWORK-SERVICES-MIB.txt"
SYS_INSTALL_FILE_lm_sensors_mib="file ../../net-snmp/mibs/LM-SENSORS-MIB.txt 0644 root root /usr/share/snmp/mibs/LM-SENSORS-MIB.txt"
SYS_INSTALL_FILE_mta_mib="file ../../net-snmp/mibs/MTA-MIB.txt 0644 root root /usr/share/snmp/mibs/MTA-MIB.txt"
SYS_INSTALL_FILE_smux_mib="file ../../net-snmp/mibs/SMUX-MIB.txt 0644 root root /usr/share/snmp/mibs/SMUX-MIB.txt"
SYS_INSTALL_FILE_tunnel_mib="file ../../net-snmp/mibs/TUNNEL-MIB.txt 0644 root root /usr/share/snmp/mibs/TUNNEL-MIB.txt"


include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

