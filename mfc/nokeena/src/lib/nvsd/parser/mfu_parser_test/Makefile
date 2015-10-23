# Filename: lib/nvsd/parser/mfu_parser_test/Makefile

WARNINGS_EXTRA_OVERRIDE = 1
include ${PROD_TREE_ROOT}/src/mk/common.mk

SRCS=                              	\
	mfu_parser_tool.c		\
	../nkn_vpe_mfu_parser.c		\
	

CFLAGS += -D_GNU_SOURCE
CFLAGS += -I/usr/lib64/glib-2.0/include\
-I/usr/include/glib-2.0\
-I${PROD_CUSTOMER_ROOT}/nokeena/src/lib/mediacodecs/common\
-I../../../../include\
-I/usr/lib/glib-2.0/include
CFLAGS += ${BUILD_EXTRA_CFLAGS}

all:	clean build

build:	
	gcc $(CFLAGS)  $(SRCS)  -o mfu_parser

clean: 
	rm -rf mfu_parser
