#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=term_ncurses

vpath %.c ../../src/src/term_ncurses

SRCS=   \
	ncurses_key.c        \
	ncurses_minibuf.c    \
	ncurses_misc.c       \
	ncurses_redisplay.c  \
	term_ncurses.c       \


LIB_BUILD_DIR=${ILIB_DIR}/base_os/common/zile/lib/libterm_ncurses

INCLUDES= -I../../src/src -I../../src/lib

DEFINES=-D_GNU_SOURCE

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

ifeq ($(call CC_VERSION_ATLEAST,40000,1,0),1)
	WARNINGS+= \
		-Wno-pointer-sign \

endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk
