# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

ifdef STATIC_MODULES
    LIB=wiz_generic_mods
else
    SO_CLASS=wizard
endif

SRCS=                              \


INCLUDES=\
        -I${PROD_TREE_ROOT}/src/bin/wizard     \

ifdef STATIC_MODULES
    include ${PROD_TREE_ROOT}/src/mk/lib.mk
else
    include ${PROD_TREE_ROOT}/src/mk/so.mk
endif
