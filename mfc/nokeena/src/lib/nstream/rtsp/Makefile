# File: lib/nstream/rtsp/Makefile

WARNINGS_EXTRA_OVERRIDE = 1
include ${PROD_TREE_ROOT}/src/mk/common.mk

LINT=splint

LIB=librtspmsg.a

all: lib

lib: $(LIB)

SRCS= rtsp.c\
	rtsp_table.c\
	rtsp_validate.c\
	rtsp_parse.c\
	rtsp_compose.c

OBJS = $(patsubst %.c,%.o,$(SRCS))

librtspmsg.a: $(OBJS)
	$(AR) -ruv  $@ $^

INCLUDE = -I include/

CFLAGS += -ggdb -g3
CFLAGS += -D_GNU_SOURCE
CFLAGS += $(INCLUDE)
CFLAGS += ${BUILD_EXTRA_CFLAGS}

CC = gcc

lint: $(SRCS) 
	$(LINT) $^ -globstate -unrecog -boolops -predboolint -predboolothers $(INCLUDE)


clean : 
	rm -f $(LIB)
	rm  -f $(OBJS)
