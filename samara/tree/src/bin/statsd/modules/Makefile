#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

NAME=stats_mods

ifdef STATIC_MODULES
    LIB=${NAME}
    LIB_BUILD_DIR=${ILIB_DIR}/bin/statsd/modules
else
    SO_CLASS=stats
endif

SRCS=                         \
	stm_cpu.c             \
	stm_disk.c            \
	stm_disk_device.c     \
	stm_memory.c          \
	stm_network.c         \

ifneq (${PROD_FEATURE_CMC_SERVER},0)
	SRCS+=                      \
		stm_cmc.c     \

endif

ifneq (${PROD_FEATURE_VIRT},0)
	SRCS+=                      \
		stm_virt.c          \

endif

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/bin/statsd \

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
