#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=zile
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=/usr/bin
PROG_STRIP=1

vpath %.c ../../src/src

SRCS=	\
	basic.c \
	bind.c \
	buffer.c \
	file.c \
	fontlock.c \
	fontlock_c.c \
	fontlock_cpp.c \
	funcs.c \
	glue.c \
	help.c \
	keys.c \
	killring.c \
	line.c \
	macro.c \
	main.c \
	minibuf.c \
	pathbuffer.c \
	rc.c \
	redisplay.c \
	registers.c \
	search.c \
	undo.c \
	variables.c \
	window.c \

CFLAGS=
INCLUDES= -I../../src/lib -I../../src

DEFINES= \
	-D_GNU_SOURCE

WARNINGS_OVERRIDE=1

WARNINGS= \
        -Wall \
        -W \
        -Wshadow \
        -Wstrict-prototypes \
        -Wmissing-prototypes \
        -Wmissing-declarations \
        -Wredundant-decls \
        -Wpointer-arith \
        -Wnested-externs \
        -Wredundant-decls \
        -Wundef \
        -Wreturn-type \
        -Wno-unused \
	-Wno-strict-prototypes \
	-Wno-missing-prototypes \
	-Wno-missing-declarations \
	-Wno-shadow \

LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/common/zile/lib/libzile/libzile.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/common/zile/lib/libterm_ncurses/libterm_ncurses.a \
	-lncurses \

include ${PROD_TREE_ROOT}/src/mk/prog.mk
