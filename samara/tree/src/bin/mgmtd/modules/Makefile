#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

NAME=md_mods

ifdef STATIC_MODULES
    LIB=${NAME}
    LIB_BUILD_DIR=${ILIB_DIR}/bin/mgmtd/modules
else
    SO_CLASS=md
endif

SRCS=                       \
	md_aaa.c            \
	md_addr_state.c     \
	md_arp.c            \
	md_cert.c           \
	md_cli.c            \
	md_debug.c          \
	md_email_config.c   \
	md_email_events.c   \
	md_file.c           \
	md_if_config.c      \
	md_if_state.c       \
	md_image.c          \
	md_logging.c        \
	md_misc_upgrades.c  \
	md_neighbors.c      \
	md_net_general.c    \
	md_ntp.c            \
	md_passwd.c         \
	md_pm.c	            \
	md_progress.c       \
	md_resolver.c       \
	md_route_state.c    \
	md_routes.c         \
	md_smart.c          \
	md_snmp.c           \
	md_ssh.c            \
	md_static_hosts.c   \
	md_stats.c          \
	md_syslog.c         \
	md_system.c         \
	md_text_config.c    \
	md_time.c           \
	md_web.c            \
	md_wsmd.c           \

ifneq (${PROD_FEATURE_WIZARD},0)
	SRCS+=                      \
		md_wizard.c         \

endif

ifneq (${PROD_FEATURE_RADIUS},0)
	SRCS+=                      \
		md_radius.c         \

endif

ifneq (${PROD_FEATURE_TACACS},0)
	SRCS+=                      \
		md_tacacs.c         \

endif

ifneq (${PROD_FEATURE_LDAP},0)
	SRCS+=                      \
		md_ldap.c           \

endif

ifneq (${PROD_FEATURE_I18N_SUPPORT},0)
	SRCS+=                      \
		md_locale.c         \

endif

ifneq (${PROD_FEATURE_FRONT_PANEL},0)
	SRCS+=                      \
		md_fp.c             \

endif

ifneq (${PROD_FEATURE_SCHED},0)
	SRCS+=                      \
		md_sched.c          \

endif

ifneq (${PROD_FEATURE_GRAPHING},0)
	SRCS+=                      \
		md_tms_graphs.c     \

endif

ifneq (${PROD_FEATURE_XINETD},0)
	SRCS+=              \
		md_xinetd.c \

endif

ifneq (${PROD_FEATURE_CMC_SERVER},0)
	SRCS+=md_cmc_server.c
endif

ifneq (${PROD_FEATURE_CMC_CLIENT},0)
	SRCS+=md_cmc_client.c
endif

ifneq (${PROD_FEATURE_CMC_CLIENT}${PROD_FEATURE_CMC_SERVER},00)
	SRCS+=                      \
		md_cmc_common.c     \

endif

ifneq (${PROD_FEATURE_CMC_CLIENT}${PROD_FEATURE_CMC_SERVER}${PROD_FEATURE_REMOTE_GCL},000)
	SRCS+=                      \
		md_rgp.c            \

endif

ifneq (${PROD_FEATURE_BRIDGING},0)
	SRCS+=                      \
		md_bridge.c         \

endif

ifneq (${PROD_FEATURE_BONDING},0)
	SRCS+=                      \
		md_bonding.c         \

endif

ifneq (${PROD_FEATURE_VLAN},0)
	SRCS+=                    \
		md_vlan.c         \

endif

ifneq (${PROD_FEATURE_CLUSTER},0)
	SRCS+=                      \
		md_cluster.c     \

endif

ifneq (${PROD_FEATURE_CRYPTO},0)
	SRCS+=                      \
		md_crypto.c         \

endif

ifneq (${PROD_FEATURE_IPTABLES},0)
	SRCS+=                      \
		md_iptables.c       \

endif

ifneq (${PROD_FEATURE_XML_GW},0)
	SRCS+=                      \
		md_xg.c             \

endif

ifneq (${PROD_FEATURE_VIRT},0)
	SRCS+=                      \
		md_virt.c       \

endif

INCLUDES= -I.. -I${PROD_TREE_ROOT}/src/lib/libevent \
	-I${PROD_TREE_ROOT}/src/bin/pm \
	-I${PROD_TREE_ROOT}/src/lib/libgd \

CFLAGS+=

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${NAME}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

ifdef STATIC_MODULES
    include ${PROD_TREE_ROOT}/src/mk/lib.mk
else
    include ${PROD_TREE_ROOT}/src/mk/so.mk
endif
