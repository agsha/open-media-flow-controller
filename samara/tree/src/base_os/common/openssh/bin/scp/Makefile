#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=scp
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=/usr/bin
PROG_STRIP=1

vpath %.c ../../src

SRCS=	\
	progressmeter.c \
	scp.c \
	tms-ssh-utils.c \

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/base_os/common/openssh/src \
	-I${PROD_TREE_ROOT}/src/base_os/common/openssh/src/openbsd-compat

DEFINES_OVERRIDE=1
DEFINES= \
	-DSSHDIR=\"/etc/ssh\" \
	-D_PATH_SSH_PROGRAM=\"/usr/bin/ssh\" \
	-D_PATH_SSH_ASKPASS_DEFAULT=\"/usr/libexec/openssh/ssh-askpass\" \
	-D_PATH_SFTP_SERVER=\"/usr/libexec/openssh/sftp-server\" \
	-D_PATH_SSH_KEY_SIGN=\"/usr/libexec/openssh/ssh-keysign\" \
	-D_PATH_SSH_PIDDIR=\"/var/run\" \
	-D_PATH_PRIVSEP_CHROOT_DIR=\"/var/empty/sshd\" \
	-DSSH_RAND_HELPER=\"/usr/libexec/openssh/ssh-rand-helper\" \
	-DHAVE_CONFIG_H

WARNINGS_OVERRIDE=1
WARNINGS= \
	-Wall \
	-Wno-unused \

LDADD=  \
	-lcrypto \
	-lcrypt \
	-lz \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/common/openssh/lib/libssh/libssh.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/common/openssh/lib/libopenbsd-compat/libopenbsd-compat.a \

include ${PROD_TREE_ROOT}/src/mk/prog.mk
