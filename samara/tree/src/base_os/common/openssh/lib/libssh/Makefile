#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=ssh

vpath %.c ../../src

SRCS=   \
	acss.c \
	atomicio.c \
	authfd.c \
	authfile.c \
	bufaux.c \
	buffer.c \
	canohost.c \
	channels.c \
	cipher-3des1.c \
	cipher-acss.c \
	cipher-aes.c \
	cipher-bf1.c \
	cipher-ctr.c \
	cipher.c \
	cleanup.c \
	compat.c \
	compress.c \
	crc32.c \
	deattack.c \
	dh.c \
	dispatch.c \
	dns.c \
	entropy.c \
	fatal.c \
	gss-genr.c \
	hostfile.c \
	kex.c \
	kexdh.c \
	kexdhc.c \
	kexgex.c \
	kexgexc.c \
	key.c \
	log.c \
	mac.c \
	match.c \
	misc.c \
	moduli.c \
	mpaux.c \
	msg.c \
	nchan.c \
	packet.c \
	progressmeter.c \
	readpass.c \
	rijndael.c \
	rsa.c \
	scard-opensc.c \
	scard.c \
	ssh-dss.c \
	ssh-rsa.c \
	tildexpand.c \
	ttymodes.c \
	uuencode.c \
	xmalloc.c \


LIB_BUILD_DIR=${ILIB_DIR}/base_os/common/openssh/lib/libssh

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/base_os/common/openssh/src

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
	-Werror \
	-Wall \
	-Wpointer-arith \
	-Wno-uninitialized \


ifeq ($(call CC_VERSION_ATLEAST,40000,1,0),1)
	WARNINGS+= \
		-Wno-pointer-sign \

endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk
