# File : Makefile Author : Patrick Mishel R
# Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
# All rights reserved.
#

LINT=splint

all: rtspc


SRCS= rtspc.c\
	  rtspc_session.c\
	  rtspc_request.c\
	  rtspc_response.c

LD=gcc

OBJS = $(patsubst %.c,%.o,$(SRCS))

NSTREAM_LIB_PATH = ../../../lib/nstream
RTSP_LIB = $(NSTREAM_LIB_PATH)/rtsp
RTSP_LIB_INCLUDE = $(NSTREAM_LIB_PATH)/rtsp/include

INCLUDE = -I $(RTSP_LIB_INCLUDE) -I ../include
LIB_PATH_OPT = -L $(RTSP_LIB)

rtspc: $(OBJS) 
	$(LD) -o $@ $(OBJS) $(LIB_PATH_OPT) -lc -lrtspmsg

CFLAGS = -Wall -ggdb -g3 $(INCLUDE)
CFLAGS += -D_GNU_SOURCE

CC = gcc

lint: $(SRCS) 
	$(LINT) $^ -globstate -unrecog -boolops -predboolint -predboolothers $(INCLUDE)

clean : 
	rm -f $(LIB)
	rm  -f $(OBJS)
	rm -f rtspc
