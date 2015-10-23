#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

NAME=cli_mods

ifdef STATIC_MODULES
    LIB=${NAME}
    LIB_BUILD_DIR=${ILIB_DIR}/bin/cli/modules
else
    SO_CLASS=cli
endif

SRCS=                              \
	cli_cert_cmds.c            \
	cli_cli_cmds.c             \
	cli_config_cmds.c          \
	cli_diag_cmds.c            \
	cli_email_cmds.c           \
	cli_file_cmds.c            \
	cli_general_cmds.c         \
	cli_image_cmds.c           \
	cli_interface_cmds.c       \
	cli_ip_cmds.c              \
	cli_license_cmds.c         \
	cli_logging_cmds.c         \
	cli_pm_cmds.c              \
	cli_privmode_cmds.c        \
	cli_route_cmds.c           \
	cli_secret_cmds.c          \
	cli_shell_cmds.c           \
	cli_snmp_cmds.c            \
	cli_ssh_cmds.c             \
	cli_stats_cmds.c           \
	cli_text_config_cmds.c     \
	cli_time_cmds.c            \
	cli_username_cmds.c        \
	cli_web_cmds.c             \

ifneq (${PROD_FEATURE_FRONT_PANEL},0)
	SRCS+=                      \
		cli_fp_cmds.c       \

endif

ifneq (${PROD_FEATURE_SCHED},0)
	SRCS+=                      \
		cli_sched_cmds.c    \

endif

ifneq (${PROD_FEATURE_AAA},0)
	SRCS+=                      \
		cli_aaa_cmds.c      \

endif

ifneq (${PROD_FEATURE_RADIUS},0)
	SRCS+=                      \
		cli_radius_cmds.c   \

endif

ifneq (${PROD_FEATURE_TACACS},0)
	SRCS+=                      \
		cli_tacacs_cmds.c   \

endif

ifneq (${PROD_FEATURE_LDAP},0)
	SRCS+=                      \
		cli_ldap_cmds.c     \

endif

ifneq (${PROD_FEATURE_XINETD},0)
	SRCS+=                     \
		cli_xinetd_cmds.c  \

endif

ifneq (${PROD_FEATURE_CMC_SERVER},0)
	SRCS+=cli_cmc_server_cmds.c
endif

ifneq (${PROD_FEATURE_CMC_CLIENT},0)
	SRCS+=cli_cmc_client_cmds.c
endif

ifneq (${PROD_FEATURE_CMC_CLIENT}${PROD_FEATURE_CMC_SERVER},00)
	SRCS+=cli_cmc_common_cmds.c
endif

ifneq (${PROD_FEATURE_BRIDGING},0)
	SRCS+=                      \
		cli_bridge_cmds.c        \

endif

ifneq (${PROD_FEATURE_BONDING},0)
	SRCS+=                      \
		cli_bonding_cmds.c        \

endif

ifneq (${PROD_FEATURE_VLAN},0)
	SRCS+=                      \
		cli_vlan_cmds.c        \

endif


ifneq (${PROD_FEATURE_CLUSTER},0)
	SRCS+=                      \
		cli_cluster_cmds.c    \

endif

ifneq (${PROD_FEATURE_CRYPTO},0)
	SRCS+=                      \
		cli_crypto_cmds.c        \

endif

ifneq (${PROD_FEATURE_RESTRICT_CMDS},0)
	SRCS+=                      \
		cli_restrict_cmds.c \

endif

ifneq (${PROD_FEATURE_IPTABLES},0)
	SRCS+=                       \
		cli_iptables_cmds.c  \

endif

ifneq (${PROD_FEATURE_XML_GW},0)
	SRCS+=                       \
		cli_xg_cmds.c        \

endif

ifneq (${PROD_FEATURE_VIRT},0)
	SRCS+=                     \
	cli_virt_cmds.c            \

endif

CFLAGS= -I${PROD_TREE_ROOT}/src/lib/libtecla

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
