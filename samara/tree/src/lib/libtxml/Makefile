#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=txml


SRCS= \
	tx_main.c \
	tx_errors.c \

INCLUDES+= -I/usr/include/libxml2

ifneq (${PROD_TARGET_OS},LINUX)
	INCLUDES+= -I/usr/local/include/libxml2
endif

LDADD= \
	-lxml2 \

# Needed for libxml2
WARNINGS+= -Wno-missing-format-attribute

include ${PROD_TREE_ROOT}/src/mk/lib.mk
