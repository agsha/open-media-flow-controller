#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=eetool

SRCS=\
	ee_main.c \
	ee_mtcr.c \

DEFINES= \
	-DENABLE_MST_DEV_I2C  \

SUBMAKEFILES=Makefile.rootflop

include ${PROD_TREE_ROOT}/src/mk/prog.mk
