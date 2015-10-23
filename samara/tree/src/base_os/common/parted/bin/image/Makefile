#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=parted
INSTALL_TREE=image
PROG_STRIP=1
PROG_STRIP_FLAGS=--remove-section=.note --remove-section=.comment
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=sbin

vpath %.c ../../src/parted

LICENSE_REQUIRES_REDIST=1

SRCS=   \
	command.c \
	parted.c \
	strlist.c \
	table.c \
	ui.c \

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/base_os/common/parted/src/include \
	-I${PROD_TREE_ROOT}/src/base_os/common/parted/src \

DEFINES= \
	-DHAVE_CONFIG_H \
	-D_REENTRANT \

# We are already defining it to =64
#	-D_FILE_OFFSET_BITS=no \

CFLAGS=

DEFINES_OVERRIDE=1
WARNINGS_OVERRIDE=1
WARNINGS_ARE_ERRORS=0
WARNINGS= \
	-W \
	-Wall \
	-Wno-unused \
	-Wno-switch \
	-Wno-format \

LDADD=  \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/common/parted/lib/libparted/libparted.a \
	-luuid \

include ${PROD_TREE_ROOT}/src/mk/prog.mk
