# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=bin
PROG=nv_validator

LEX=/usr/bin/flex
YACC=/usr/bin/bison

BUILD_OUT=${PROD_BUILD_OUTPUT_DIR}/image/bin/nv_validator
LEX_SRCS=nv_parser.l
YACC_SRCS=nv_parser.y
CLEANFILES=${BUILD_OUT}/lex.yy.c ${BUILD_OUT}/${LEX_SRCS} ${BUILD_OUT}/y.tab.c ${BUILD_OUT}/y.tab.h ${BUILD_OUT}/${YACC_SRCS} ${BUILD_OUT}/nv_validator.d

PROG_PRE_BUILD_TARGET=${BUILD_OUT}/lex.yy.c ${BUILD_OUT}/y.tab.c
# lex.yy.c is built from nv_parser.l
# y.tab.c and y.tab.h are built from nv_parser.y
# NOTE: Code in prog.mk that looks for the files listed in SRCS will look for
# lex.yy.c and y.tab.c in the BUILD_OUT dir.  So our rules to create those
# files need to create them there.
# The PROG_PRE_BUILD_TARGET setting makes them get built first.

# These are the source files to create 'nv_validator':
SRCS=           \
 nv_validator.c \
 lex.yy.c       \
 y.tab.c

include ${PROD_TREE_ROOT}/src/mk/prog.mk

${BUILD_OUT}:
	[ ! -d ${BUILD_OUT} ] && mkdir -p ${BUILD_OUT}

${BUILD_OUT}/${LEX_SRCS}: ${BUILD_OUT} ${LEX_SRCS}
	cp ${LEX_SRCS} ${BUILD_OUT}/
	ls -l ${BUILD_OUT}/${LEX_SRCS}

${BUILD_OUT}/${YACC_SRCS}: ${BUILD_OUT} ${YACC_SRCS}
	cp ${YACC_SRCS} ${BUILD_OUT}/
	ls -l ${BUILD_OUT}/${YACC_SRCS}

${BUILD_OUT}/lex.yy.c: ${BUILD_OUT}/${LEX_SRCS}
	# This builds ${BUILD_OUT}/lex.yy.c from nv_parser.l
	cd ${BUILD_OUT} ; $(LEX) $(LEX_SRCS)

${BUILD_OUT}/y.tab.c: ${BUILD_OUT}/${YACC_SRCS}
	# This builds ${BUILD_OUT}/y.tab.c and ${BUILD_OUT}/y.tab.h from nv_parser.y
	cd ${BUILD_OUT} ; $(YACC) -d -y $(YACC_SRCS)

