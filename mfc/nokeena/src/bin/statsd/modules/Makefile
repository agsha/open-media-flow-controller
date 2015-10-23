# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

NAME=stats_nkn_mods

ifdef STATIC_MODULES
    LIB=stats_generic_mods
else
    SO_CLASS=stats
endif

SRCS= stm_nkn_stats.c \
	stm_nkn_alarms.c \

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/bin/statsd \
	-I${PROD_TREE_ROOT}/src/include/bin/statsd \

ifdef STATIC_MODULES
    include ${PROD_TREE_ROOT}/src/mk/lib.mk
else
    include ${PROD_TREE_ROOT}/src/mk/so.mk
endif
