# Filename: lib/nvsd/parser/ts2mfu_test/src/Makefile

WARNINGS_EXTRA_OVERRIDE = 1
include ${PROD_TREE_ROOT}/src/mk/common.mk

topdir = ../../../../..
includedir = -I$(topdir)/include\
	-I$(topdir)/include/mfp\
	-I/usr/include/glib-2.0\
	-I/usr/lib64/glib-2.0/include\
	-I/usr/include/libxml2\
	-fPIC\
	-I$(topdir)/lib/mediacodecs/common\
	-I$(topdir)/lib/mfp/latm_parser/

outputdir = $(topdir)/lib/nvsd/parser/ts2mfu_test/bin
output =  ts2mfu

#main_new_ts2mfu.c
SRCS = \
	main_new_ts2mfu.c \
	$(topdir)/lib/nvsd/parser/nkn_vpe_bitio.c \
	$(topdir)/lib/nvsd/parser/nkn_vpe_h264_parser.c \
	$(topdir)/lib/nvsd/parser/nkn_vpe_mp4_parser.c \
	$(topdir)/lib/nvsd/parser/rtp_mpeg4_es.c \
	$(topdir)/lib/nvsd/parser/rtp_m4a_read.c \
	$(topdir)/lib/nvsd/parser/nkn_vpe_mfp_ts2mfu.c \
	$(topdir)/lib/nvsd/parser/nkn_vpe_mfu_parser.c \
	$(topdir)/lib/nvsd/parser/nkn_vpe_mfu_parse.c \
	$(topdir)/lib/nvsd/parser/nkn_vpe_pattern_search.c \
	$(topdir)/lib/mfp/aligner/mfp_video_aligner.c \
	$(topdir)/bin/mfp/common/mfp_video_header.c \
	$(topdir)/bin/mfp/nkn_vpe_mfu_aac.c \
	$(topdir)/bin/mfp/nkn_vpe_ts_video.c \
	$(topdir)/lib/nvsd/parser/nkn_vpe_utils.c \
	$(topdir)/lib/nvsd/memalloc/nkn_memalloc.c \
	$(topdir)/lib/nvsd/parser/nkn_vpe_mp4_seek_api.c\
	$(topdir)/lib/nvsd/parser/nkn_vpe_mp4_seek_parser.c\
    $(topdir)/lib/mfp/latm_parser/ts_parser.c\
	

CFLAGS += -D_GNU_SOURCE
CFLAGS += -DCHECK -DUNIT_TEST -DTS2MFU_TEST -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -DINC_ALIGNER
#CFLAGS += -D_USE_INTERFACE_ -D_SUMALATHA_OPT_ 
CFLAGS += -U_USE_NKN_VFS -g `xml2-config --cflags`
CFLAGS += ${BUILD_EXTRA_CFLAGS}

LDFLAGS = -L/usr/lib64 -lm -lglib-2.0 `xml2-config --libs` $(PROD_OUTPUT_ROOT)/product-nokeena-x86_64/build/image/custlib/mfp/libmfp_latm_parser.a
 

CC = gcc

driver: $(SRCS)
	$(CC) $(SRCS) $(CFLAGS) $(includedir) $(LDFLAGS) $(LDADD) -o $(outputdir)/$(output) 

mp4feature:
	$(CC) -c $(CFLAGS) $(mp4parser_src) $(includedir)
etags:
	etags $(srcdir)/*.c $(etagsdir)
clean:
	rm -f *.o $(outputdir)/$(output)

#$(topdir)/bin/mfp/live_pub/mfu2iphone.c \
#
#$(topdir)/bin/mfp/nkn_vpe_mfp_ts.c \
