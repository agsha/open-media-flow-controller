# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=sbin
PROG=vpe_pre_proc

SRCS = \
	nkn_vpe_pre_proc.c\
	nkn_vpe_types.c\
	nkn_vpe_bitstream.c\
	nkn_vpe_file_io.c\
	nkn_vpe_backend_io_plugin.c\
	unit_test.c\
	nkn_vpe_codec_plugin.c\
	nkn_vpe_codec_handlers.c \
	nkn_vpe_mp4_parser.c \
	nkn_vpe_mpeg2ts_parser.c\
	nkn_vpe_mpeg2ps_parser.c\
	nkn_vpe_h264_parser.c	


CFLAGS += -Wall -Werror -Wno-missing-format-attribute -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -fPIC -I/usr/lib64/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I${PROD_TREE_ROOT}/src/lib/libgcl

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

LDFLAGS += -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` -lm -lelf -ldl -lssl -luuid -L/usr/lib64/python2.6/ -lpython2.6

include ${PROD_TREE_ROOT}/src/mk/prog.mk
