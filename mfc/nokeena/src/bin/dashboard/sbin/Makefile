#  Makefile
#
include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=sbin
PROG=nkn_dashboard

SRCS= \
  dashboard_datas.cc \
  dashboard_graphs.cc \
  nkn_dashboard.cc \

# The "-Wshadow" options causes warnings from ../include/*.h,
# so we have to specify the -W options we want ourselves.
WARNINGS_OVERRIDE = 1
MY_W_DEFINES= \
  -Wall \
  -W \
  -Wredundant-decls \
  -Wpointer-arith \
  -Wno-unused \
  -Wformat \
  -Wformat-security \
  -Wmissing-format-attribute \
  -Winline \

# Compiling with g++, so use CXX_FLAGS
CXXFLAGS= $(MY_W_DEFINES) -I../../../thirdparty/lib/chartdirector/include -I../../../include ${BUILD_EXTRA_CFLAGS}
LDFLAGS = ../../../thirdparty/lib/chartdirector/libchartdir.so.5.0.0 
LDFLAGS+= -lelf -lpthread -lcprops

LDADD = ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
        ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_mt/libtc_mt.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/memalloc/libnvsd_memalloc.so.1.0.0 \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncnt_client/libnkncnt_client.a\
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncounters/libnkn_counters.a \
	${BUILD_EXTRA_LDADD}

ifeq (${BUILD_GCOV}, 1)
        CXXFLAGS += -fprofile-arcs -ftest-coverage
endif


include ${PROD_TREE_ROOT}/src/mk/prog.mk
