#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=login
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=/bin
PROG_STRIP=1

vpath %.c ../../src

SRCS=	\
	login.c \
	setproctitle.c \

DEFINES= \
	-D_GNU_SOURCE \
	-DUSE_TTY_GROUP \
	-DLOCALEDIR=\"/usr/share/locale\"

ifeq (${PROD_TARGET_ARCH_FAMILY},X86)
	DEFINES+=\
		-DHAVE_LIBAUDIT

endif

# NOTE: stack-protector does not appear to work on PPC.
STACK_PROTECTOR=
ifeq (${PROD_TARGET_ARCH_FAMILY},X86)

STACK_PROTECTOR=\
	-fstack-protector --param=ssp-buffer-size=4 \

endif

CFLAGS+=\
	-O2 \
	-g \
	-fsigned-char \
	-fexceptions \
	${STACK_PROTECTOR} \

# XXXX-security: we wish this were above, but it causes unused warnings
#	-Wp,-D_FORTIFY_SOURCE=2 \


WARNINGS_OVERRIDE=1
WARNINGS= \
	-Wall \
	-Wno-shadow \

LDADD=  \
	-lcrypt \
	-lpam \
	-ldl \
	-lpam_misc \


include ${PROD_TREE_ROOT}/src/mk/prog.mk
