#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=sshd
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=/usr/sbin
PROG_STRIP=1

vpath %.c ../../src

SRCS=	\
        auth-bsdauth.c \
        auth-chall.c \
        auth-krb5.c \
        auth-options.c \
        auth-pam.c \
        auth-passwd.c \
        auth-rh-rsa.c \
        auth-rhosts.c \
        auth-rsa.c \
        auth-shadow.c \
        auth-sia.c \
        auth-skey.c \
        auth.c \
        auth1.c \
        auth2-chall.c \
        auth2-gss.c \
        auth2-hostbased.c \
        auth2-kbdint.c \
        auth2-none.c \
        auth2-passwd.c \
        auth2-pubkey.c \
        auth2.c \
        groupaccess.c \
        gss-serv-krb5.c \
        gss-serv.c \
        kexdhs.c \
        kexgexs.c \
        loginrec.c \
        md5crypt.c \
        monitor.c \
        monitor_fdpass.c \
        monitor_mm.c \
        monitor_wrap.c \
        servconf.c \
        serverloop.c \
        session.c \
        sshd.c \
        sshlogin.c \
        sshpty.c \
        uidswap.c \


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
	-Wall -Wno-unused \

LDADD=  \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/common/openssh/lib/libssh/libssh.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/common/openssh/lib/libopenbsd-compat/libopenbsd-compat.a \
	-lutil \
	-lz \
	-lcrypto \
	-lcrypt \
	-lpam \

include ${PROD_TREE_ROOT}/src/mk/prog.mk
