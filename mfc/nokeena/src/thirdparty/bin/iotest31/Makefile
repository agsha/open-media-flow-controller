#IOtest makefile
#9/12/99 rbc
#9/23/99 rbc (revised)
#10/19/00  rbc (revised)
#01/22/01  rbc  (revised)


#Which C compiler do you have?
#Don't even think about using SUN's non-ansi "cc"!
#CC = cc
include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=debugbin
PROG=IOtest

INSTALL_TREE?=image
PROG_BUILD_DIR?=${BIN_DIR}/${PROG}
FULL_BUILD_DIR=${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${PROG_BUILD_DIR}
FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/${INSTALL_TREE}/${PROD_INSTALL_PREFIX}/${PROG_INSTALL_DIR}

# Local Macros
BIN_IOTEST = $(PROD_BUILD_OUTPUT_DIR)/image/bin/IOtest/IOtest


#Use the following for Solaris, HPUX, Linux, etc (gcc compiler)

CFLAGS +=  -g -Wall -D_LARGEFILE64_SOURCE=1 -D_GNU_SOURCE -D_NO_ASK_ABOUT_WRITE -D_HAS_O_LARGEFILE


#CFLAGS =  -O2 -Wall -D_LARGEFILE64_SOURCE -D_LOOP_BM -D_OLD_WRITE_BLOCK -D_ALL_SIZES -D_NO_ASK_ABOUT_WRITE -D_DO_BDRS -D_DO_QUICKIE -D_NEW_OUTPUT_FORMAT

#Try the following with HP's ANSI "C" compiler
#CFLAGS =  -Ae +DAportable -D_LARGEFILE64_SOURCE

#Use the following for AIX (gcc)
#CFLAGS = -O2 -Wall -D_LARGE_FILES -D_AIX

#Use the following for SGI (gcc)
#CFLAGS = -O2 -Wall -D_LARGE_FILES -D_SGI

#Use the following for DGUX (gcc) and DEC UNIX (gcc)
#CFLAGS = -O2 -Wall -D_LARGE_FILES


#You shouldn't need to change anything below this line.....
TARGETS = IOtest IOtest-static
SRCS	= IOtest.c exercise.c maint.c benchmks.c
OBJS = $(addprefix ${FULL_BUILD_DIR}/,$(patsubst %.c,%.o,${SRCS}))
DEPS = $(addprefix ${FULL_BUILD_DIR}/,$(patsubst %.c,%.dep,${SRCS}))
IOTEST_OBJS = $(OBJS) $(addprefix ${FULL_BUILD_DIR}/,$(patsubst %.c,%.o,${FTPD_SRCS}))
IOTEST_DEPS = $(addprefix ${FULL_BUILD_DIR}/,$(patsubst %.c,%.dep,${SRCS}))


# Rules
.PHONY: clean clean_self

all: setup bin
	@echo
	@echo "----------IOtest Build Completed----------"
	@echo

setup:
	@echo $(FULL_BUILD_DIR)
	@mkdir -p  $(FULL_BUILD_DIR)

bin: $(BIN_IOTEST)

include:

$(BIN_IOTEST): $(IOTEST_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(IOTEST_OBJS)

${FULL_BUILD_DIR}/%.o : %.c
	$(CC) -MD -c $(CFLAGS) -o $@ $<

install:
	@if [ ! -d ${FULL_INSTALL_DIR} ] ; then echo Create install dir: ${FULL_INSTALL_DIR}; mkdir -p ${FULL_INSTALL_DIR}; fi
	$(CP) $(BIN_IOTEST) ${FULL_INSTALL_DIR}

clean: clean_self

clean_self:
	$(RM) $(IOTEST_OBJS) *.o *.d *.P *.~
	$(RM) $(BIN_IOTEST)


IOtest:		${OBJS}
	$(CC)  $(CFLAGS) -o IOtest ${OBJS}

static:		${OBJS}
	$(CC) -static $(CFLAGS) -o IOtest-static ${OBJS}

debug:		${OBJS}
	$(CC) -DDEBUG $(CFLAGS) -o IOtest ${SRCS}

IOtest.o:	IOtest.c IOtest.h
	$(CC) $(CFLAGS) -c IOtest.c

exercise.o:	exercise.c IOtest.h
	$(CC) $(CFLAGS) -c exercise.c

maint.o:	maint.c IOtest.h
	$(CC) $(CFLAGS) -c maint.c

benchmks.o:	benchmks.c IOtest.h
	$(CC) $(CFLAGS) -c benchmks.c

create:		create.c
	$(CC) $(CFLAGS) -o create create.c

rattle:		rattle.h rattle.c
	$(CC) $(CFLAGS) -o rattle rattle.c

