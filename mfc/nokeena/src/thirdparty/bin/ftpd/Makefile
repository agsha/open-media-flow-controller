# Filename: ftpd/Makefile
#  (C) Copyright 2008 Nokeena Networks, Inc.  
#  All rights reserved.
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=bin
PROG=ftpd

INSTALL_TREE?=image
PROG_BUILD_DIR?=${BIN_DIR}/${PROG}
FULL_BUILD_DIR=${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${PROG_BUILD_DIR}
FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/${INSTALL_TREE}/${PROD_INSTALL_PREFIX}/${PROG_INSTALL_DIR}

# Local Macros
BIN_FTPD = $(PROD_BUILD_OUTPUT_DIR)/image/bin/ftpd/pure-ftpd
BIN_FTPWHO = $(PROD_BUILD_OUTPUT_DIR)/image/bin/ftpd/pure-ftpwho

CFLAGS += -fPIC -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include  -I. -DHAVE_CONFIG_H -D_GNU_SOURCE=1 -DCONFDIR=\"/etc\" -DINCLUDE_IO_WRAPPERS=1 -DDEBUG=1
LDFLAGS += -L$(PROJECTLIB) -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` -lm -lcap -lcrypt -lpam -ldl

SRCS = altlog.c \
	   caps.c         \
	   daemons.c     \
	   fakesnprintf.c  \
	   ftpwho-update.c  \
	   log_ldap.c   \
	   log_puredb.c  \
	   quotas.c \
		bsd-getopt_long.c  \
		crypto.c       \
		diraliases.c  \
		log_mysql.c  \
		log_unix.c    \
		parser.c          \
		tls.c \
		bsd-glob.c         \
		crypto-md5.c   \
		dynamic.c     \
		ftp_parser.c    \
		ipv4stack.c      \
		log_pam.c    \
		ls.c          \
		privsep.c         \
		upload-pipe.c \
		bsd-realpath.c     \
		crypto-sha1.c  \
		fakechroot.c  \
		ftpwho-read.c   \
		log_extauth.c    \
		log_pgsql.c  \
		mysnprintf.c  


FTPD_SRCS = ftpd.c             
#			mysnprintf.c \
			fakesnprintf.c \
			ipv4stack.c \
			bsd-getopt_long.c


FTPWHO_SRCS = pure-ftpwho.c
#FTPWHO_OBJS = $(FTPWHO_SRCS:.c=.o)
#FTPWHO_DEPS = $(FTPWHO_SRCS:.c=.dep)
FTPWHO_OBJS = $(addprefix ${FULL_BUILD_DIR}/,$(patsubst %.c,%.o,${FTPWHO_SRCS}))
FTPWHO_DEPS = $(addprefix ${FULL_BUILD_DIR}/,$(patsubst %.c,%.dep,${FTPWHO_SRCS}))


#FTPD_OBJS = $(SRCS:.c=.o) $(FTPD_SRCS:.c=.o)
#FTPD_DEPS = $(SRCS:.c=.dep) $(FTPD_SRCS:.c=.dep)
OBJS = $(addprefix ${FULL_BUILD_DIR}/,$(patsubst %.c,%.o,${SRCS}))
DEPS = $(addprefix ${FULL_BUILD_DIR}/,$(patsubst %.c,%.dep,${SRCS}))
FTPD_OBJS = $(OBJS) $(addprefix ${FULL_BUILD_DIR}/,$(patsubst %.c,%.o,${FTPD_SRCS}))
FTPD_DEPS = $(DEPS) $(addprefix ${FULL_BUILD_DIR}/,$(patsubst %.c,%.dep,${FTPD_SRCS}))

# Rules 
.PHONY: clean clean_self

all: setup bin
	@echo
	@echo "----------Pure-FTPD Build Completed----------" 
	@echo

setup:
	@mkdir -p  $(FULL_BUILD_DIR)

bin: $(BIN_FTPD) $(BIN_FTPWHO)

lib: include $(LIB_FTPD)

include:

$(BIN_FTPD): $(FTPD_OBJS) 
	$(CC) $(LDFLAGS) -o $@ $(FTPD_OBJS) 
#	@cp _install/pure-config.pl $(PROJECTBIN)/.


$(BIN_FTPWHO): $(FTPWHO_OBJS) 
	$(CC) $(LDFLAGS) -o $@ $(FTPWHO_OBJS) 

$(LIB_FTPD): $(FTPD_OBJS) 
	$(AR) rv $@ $(FTPD_OBJS)
	ranlib $@

${FULL_BUILD_DIR}/%.o : %.c
	$(CC) -MD -c $(CFLAGS) -o $@ $<

install:
	@if [ ! -d ${FULL_INSTALL_DIR} ] ; then echo Create install dir: ${FULL_INSTALL_DIR}; mkdir -p ${FULL_INSTALL_DIR}; fi
	$(CP) $(BIN_FTPD) ${FULL_INSTALL_DIR}
	$(CP) $(BIN_FTPWHO) ${FULL_INSTALL_DIR}

clean: clean_self

clean_self: 
	$(RM) $(FTPD_OBJS) $(FTPWHO_OBJS) *.o *.d *.P *.~
	$(RM) $(LIB_FTPD) $(BIN_FTPD) $(BIN_FTPWHO)

# End of Makefile
