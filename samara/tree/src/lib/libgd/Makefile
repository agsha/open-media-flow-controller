#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=gd

SRCS= \
	gd.c \
	gd_gd.c \
	gd_gd2.c \
	gd_gif_in.c \
	gd_gif_out.c \
	gd_io.c \
	gd_io_dp.c \
	gd_io_file.c \
	gd_io_ss.c \
	gd_png.c \
	gd_security.c \
	gd_ss.c \
	gd_topal.c \
	gdcache.c \
	gdfontg.c \
	gdfontl.c \
	gdfontmb.c \
	gdfonts.c \
	gdfontt.c \
	gdft.c \
	gdfx.c \
	gdhelpers.c \
	gdkanji.c \
	gdtables.c \

CFLAGS= -I/usr/include/freetype2

ifeq (${PROD_TARGET_OS},FREEBSD)
	CFLAGS+=-I/usr/local/include/freetype2
endif
ifeq (${PROD_TARGET_OS},SUNOS)
	CFLAGS+=-I/usr/local/include/freetype2
endif

DEFINES= \
	-D_SVID_SOURCE \
	-D_GNU_SOURCE \
	-DHAVE_CONFIG_H \

WARNINGS_OVERRIDE=1
WARNINGS= \
	-Wall \
	-Wstrict-prototypes \
	-Wno-unused \

ifeq (${PROD_TARGET_OS},LINUX)
    DEFINES+= -Dlinux
endif

ifeq ($(call CC_VERSION_ATLEAST,40000,1,0),1)
	WARNINGS+= \
		-Wno-pointer-sign \

endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk
