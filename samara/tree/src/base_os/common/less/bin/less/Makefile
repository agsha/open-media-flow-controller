#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=less
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=/usr/bin
PROG_STRIP=1

vpath %.c ../../src

SRCS=	\
	brac.c \
	ch.c \
	charset.c \
	cmdbuf.c \
	command.c \
	cvt.c \
	decode.c \
	edit.c \
	filename.c \
	forwback.c \
	help.c \
	ifile.c \
	input.c \
	jump.c \
	line.c \
	linenum.c \
	lsystem.c \
	main.c \
	mark.c \
	optfunc.c \
	option.c \
	opttbl.c \
	os.c \
	output.c \
	pattern.c \
	position.c \
	prompt.c \
	screen.c \
	search.c \
	signal.c \
	tags.c \
	ttyin.c \
	version.c


INCLUDES= -I../../src

ifeq (${PROD_TARGET_PLATFORM_FULL},LINUX_EL_EL6)
	LDADD+= \
		-ltinfo
else ifeq (${PROD_TARGET_ARCH_FAMILY},PPC)
	LDADD+= \
		-lncurses
else
	LDADD+= \
		-lncursesw
endif

DEFINES= -D_BSD_SOURCE -DSYSDIR=\"/usr/local/etc\"

WARNINGS_OVERRIDE=1

ifeq ($(call CC_VERSION_ATLEAST,30404,1,0),1)
	WARNINGS+= \
		-Wno-deprecated-declarations \

endif

include ${PROD_TREE_ROOT}/src/mk/prog.mk
