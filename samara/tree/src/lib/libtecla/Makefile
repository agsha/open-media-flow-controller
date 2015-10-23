#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=tecla

SRCS= \
	getline.c \
	keytab.c \
	freelist.c \
	strngmem.c \
	hash.c \
	history.c \
	direader.c \
	homedir.c \
	pathutil.c \
	expand.c \
	stringrp.c \
	cplfile.c \
	cplmatch.c \
	pcache.c \
	version.c \

DEFINES= \
	-DHAVE_SELECT=1 \
	-DHAVE_SYSV_PTY=1 \
	-DUSE_TERMINFO=1 \
	-DHAVE_TERM_H=1 \
	-DHAVE_CURSES_H=1 \
	-DHAVE_NCURSES_TERM_H=1 \
	-DHAVE_NCURSES_CURSES_H \

WARNINGS+= -Wno-sign-compare

LDADD= \
	-lncurses \

include ${PROD_TREE_ROOT}/src/mk/lib.mk
