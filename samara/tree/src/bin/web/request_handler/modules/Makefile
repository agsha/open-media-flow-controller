#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

NAME=web_mods

ifdef STATIC_MODULES
    LIB=${NAME}
    LIB_BUILD_DIR=${ILIB_DIR}/bin/web/request_handler/modules
else
    SO_CLASS=web
endif

SRCS=                                \
	web_clear_arp.c                  \
	web_config_form.c                \
	web_config_form_aaa.c            \
	web_config_form_array.c          \
	web_config_form_certs.c          \
	web_config_form_config.c         \
	web_config_form_date.c           \
	web_config_form_dns.c            \
	web_config_form_hosts.c          \
	web_config_form_faults.c         \
	web_config_form_interface.c      \
	web_config_form_licenses.c       \
	web_config_form_list.c           \
	web_config_form_remote_logging.c \
	web_config_form_routes.c         \
	web_config_form_snmp.c           \
	web_config_form_users.c          \
	web_config_form_hostname_dhcp.c  \
	web_config_form_neighbors.c      \
	web_confirm.c                    \
	web_image.c                      \
	web_login.c                      \
	web_logging.c                    \
	web_logout.c		         \
	web_mgmt_action.c                \
	web_reboot.c                     \
	web_report_export.c              \
	web_ssh.c                        \
	web_stats.c                      \
	web_webui.c                      \


ifneq (${PROD_FEATURE_VIRT},0)
	SRCS+=     	                 \
		web_virt.c               \

endif

ifneq (${PROD_FEATURE_CMC_SERVER},0)
	SRCS+=                      \
		web_config_form_profile.c   \
		web_cmc_server.c            \

endif

ifneq (${PROD_FEATURE_CMC_CLIENT},0)
	SRCS+=                      \
		web_cmc_client.c            \

endif


ifneq (${PROD_FEATURE_CLUSTER},0)
	SRCS+=     	                 \
		web_cluster.c            \

endif

ifneq (${PROD_FEATURE_CRYPTO},0)
	SRCS+=     	                 \
        web_crypto.c             \

endif

INCLUDES= -I..

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
