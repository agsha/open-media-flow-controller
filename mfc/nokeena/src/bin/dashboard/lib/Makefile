# Makefile
# Install dashboard related files to /var/nkn/dashboard and /lib
# Note: Other dashboard files are those that are installed in the browser's
# content and cgi-bin directories.  The source to those are under
# src/bin/web/content/dashboard and src/bin/web/cgi-bin/dashboard
#
# Filename:  $Source: ~/samara/trunk/nokeena/src/install_files/dashboard
# 
# (C) Copyright 2009 Nokeena Networks, Inc.  
# All rights reserved.
#
# The lib and font files are from "ChartDirector" by Advanced Softwasre Engineering,
# http://www.advsofteng.com
# We have purcahsed a "ChartDirector Redistribute License":
#      License for royalty free redistribution of ChartDirector runtime on
#      Windows, Linux or FreeBSD. Provides the right to embed ChartDirector
#      runtime in a product or system that is not for ChartDirector software
#      development, and to redistribute that product or system to unlimited
#      number of computers without further royalties to ASE. 


include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
  ${DIR_var_nkn} \
  ${DIR_var_nkn_dashboard} \
  ${DIR_var_nkn_dashboard_sample} \
  ${DIR_lib_fonts} \
  ${FILE_T_1} \
  ${FILE_T_2} \
  ${FILE_T_3} \
  ${FILE_T_4} \
  ${FILE_T_5} \
  ${FILE_T_6} \
  ${FILE_T_7} \
  ${FILE_LIC_01} \
  ${FILE_LIB_01} \
  ${SYMLINK_LIB_01} \
  ${SYMLINK_LIB_02} \
  ${FILE_FONT_01} \
  ${FILE_FONT_02} \
  ${FILE_FONT_03} \
  ${FILE_FONT_04} \
  ${FILE_FONT_05} \
  ${FILE_FONT_06} \
  ${FILE_FONT_07} \
  ${FILE_FONT_08} \
  ${FILE_FONT_09} \
  ${FILE_FONT_10} \
  ${FILE_FONT_11} \
  ${FILE_FONT_12} \
  ${FILE_FONT_13} \
  ${FILE_FONT_14} \
  ${FILE_FONT_15} \
  ${FILE_FONT_16} \
  ${FILE_FONT_17} \
  ${FILE_FONT_18} \
  ${FILE_FONT_19} \
  ${FILE_FONT_20} \
  ${FILE_FONT_21} \
  ${FILE_FONT_22} \
  ${FILE_FONT_23} \
  ${FILE_FONT_24} \
  ${FILE_FONT_25} \
  ${FILE_FONT_26} \
  ${FILE_FONT_27} \
  ${FILE_FONT_28} \
  ${FILE_FONT_29} \

CLEANFILES= \
  ${FULL_INSTALL_DIR}/var/nkn/dashboard/cookievalue.txt \
  ${FULL_INSTALL_DIR}/var/nkn/dashboard/sample/data_bw.txt \
  ${FULL_INSTALL_DIR}/var/nkn/dashboard/sample/data_cpu.txt \
  ${FULL_INSTALL_DIR}/var/nkn/dashboard/sample/data_disk_usage.txt \
  ${FULL_INSTALL_DIR}/var/nkn/dashboard/sample/data_open_conn.txt \
  ${FULL_INSTALL_DIR}/var/nkn/dashboard/sample/data_source.txt \
  ${FULL_INSTALL_DIR}/var/nkn/dashboard/sample/data_throughput.txt \
  ${FULL_INSTALL_DIR}/lib/chartdir.lic \
  ${FULL_INSTALL_DIR}/lib/libchartdir.so \
  ${FULL_INSTALL_DIR}/lib/libchartdir.so.5.0 \
  ${FULL_INSTALL_DIR}/lib/libchartdir.so.5.0.0 \
  ${FULL_INSTALL_DIR}/lib/fonts/helvB.pfc \
  ${FULL_INSTALL_DIR}/lib/fonts/helvB08.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/helvBI.pfc \
  ${FULL_INSTALL_DIR}/lib/fonts/helvBI08.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/helvI.pfc \
  ${FULL_INSTALL_DIR}/lib/fonts/helvI08.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/helvI10.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/helvI12.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/helvI14.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/helvR.pfc \
  ${FULL_INSTALL_DIR}/lib/fonts/helvR08.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/helvR10.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/helvR12.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/helvR14.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/script.pfc \
  ${FULL_INSTALL_DIR}/lib/fonts/timB.pfc \
  ${FULL_INSTALL_DIR}/lib/fonts/timB08.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/timBI.pfc \
  ${FULL_INSTALL_DIR}/lib/fonts/timBI08.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/timI.pfc \
  ${FULL_INSTALL_DIR}/lib/fonts/timI08.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/timI10.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/timI12.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/timI14.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/timR.pfc \
  ${FULL_INSTALL_DIR}/lib/fonts/timR08.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/timR10.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/timR12.pcf \
  ${FULL_INSTALL_DIR}/lib/fonts/timR14.pcf \

DIR_var_nkn="dir                  0755 root root /var/nkn"
DIR_var_nkn_dashboard="dir        0777 root root /var/nkn/dashboard"
DIR_var_nkn_dashboard_sample="dir 0755 root root /var/nkn/dashboard/sample"
DIR_lib_fonts="dir                0755 root root /lib/fonts"

# var-nkn-dashboard
FILE_T_1="file var-nkn-dashboard/cookievalue.txt            0666 root root /var/nkn/dashboard/cookievalue.txt"
FILE_T_2="file var-nkn-dashboard/sample/data_bw.txt         0444 root root /var/nkn/dashboard/sample/data_bw.txt"
FILE_T_3="file var-nkn-dashboard/sample/data_cpu.txt        0444 root root /var/nkn/dashboard/sample/data_cpu.txt"
FILE_T_4="file var-nkn-dashboard/sample/data_disk_usage.txt 0444 root root /var/nkn/dashboard/sample/data_disk_usage.txt"
FILE_T_5="file var-nkn-dashboard/sample/data_open_conn.txt  0444 root root /var/nkn/dashboard/sample/data_open_conn.txt"
FILE_T_6="file var-nkn-dashboard/sample/data_source.txt     0444 root root /var/nkn/dashboard/sample/data_source.txt"
FILE_T_7="file var-nkn-dashboard/sample/data_throughput.txt 0444 root root /var/nkn/dashboard/sample/data_throughput.txt"

FILE_LIC_01="file chartdir.lic         0444 root root /lib/chartdir.lic"

FILE_LIB_01="file ../../../thirdparty/lib/chartdirector/libchartdir.so.5.0.0 0444 root root /lib/libchartdir.so.5.0.0"
SYMLINK_LIB_01="symlink /lib libchartdir.so.5.0.0 libchartdir.so"
SYMLINK_LIB_02="symlink /lib libchartdir.so.5.0.0 libchartdir.so.5.0"

FILE_FONT_01="file ../../../thirdparty/lib/chartdirector/fonts/helvB.pfc     0444 root root /lib/fonts/helvB.pfc"
FILE_FONT_02="file ../../../thirdparty/lib/chartdirector/fonts/helvB08.pcf   0444 root root /lib/fonts/helvB08.pcf"
FILE_FONT_03="file ../../../thirdparty/lib/chartdirector/fonts/helvBI.pfc    0444 root root /lib/fonts/helvBI.pfc"
FILE_FONT_04="file ../../../thirdparty/lib/chartdirector/fonts/helvBI08.pcf  0444 root root /lib/fonts/helvBI08.pcf"
FILE_FONT_05="file ../../../thirdparty/lib/chartdirector/fonts/helvI.pfc     0444 root root /lib/fonts/helvI.pfc"
FILE_FONT_06="file ../../../thirdparty/lib/chartdirector/fonts/helvI08.pcf   0444 root root /lib/fonts/helvI08.pcf"
FILE_FONT_07="file ../../../thirdparty/lib/chartdirector/fonts/helvI10.pcf   0444 root root /lib/fonts/helvI10.pcf"
FILE_FONT_08="file ../../../thirdparty/lib/chartdirector/fonts/helvI12.pcf   0444 root root /lib/fonts/helvI12.pcf"
FILE_FONT_09="file ../../../thirdparty/lib/chartdirector/fonts/helvI14.pcf   0444 root root /lib/fonts/helvI14.pcf"
FILE_FONT_10="file ../../../thirdparty/lib/chartdirector/fonts/helvR.pfc     0444 root root /lib/fonts/helvR.pfc"
FILE_FONT_11="file ../../../thirdparty/lib/chartdirector/fonts/helvR08.pcf   0444 root root /lib/fonts/helvR08.pcf"
FILE_FONT_12="file ../../../thirdparty/lib/chartdirector/fonts/helvR10.pcf   0444 root root /lib/fonts/helvR10.pcf"
FILE_FONT_13="file ../../../thirdparty/lib/chartdirector/fonts/helvR12.pcf   0444 root root /lib/fonts/helvR12.pcf"
FILE_FONT_14="file ../../../thirdparty/lib/chartdirector/fonts/helvR14.pcf   0444 root root /lib/fonts/helvR14.pcf"
FILE_FONT_15="file ../../../thirdparty/lib/chartdirector/fonts/script.pfc    0444 root root /lib/fonts/script.pfc"
FILE_FONT_16="file ../../../thirdparty/lib/chartdirector/fonts/timB.pfc      0444 root root /lib/fonts/timB.pfc"
FILE_FONT_17="file ../../../thirdparty/lib/chartdirector/fonts/timB08.pcf    0444 root root /lib/fonts/timB08.pcf"
FILE_FONT_18="file ../../../thirdparty/lib/chartdirector/fonts/timBI.pfc     0444 root root /lib/fonts/timBI.pfc"
FILE_FONT_19="file ../../../thirdparty/lib/chartdirector/fonts/timBI08.pcf   0444 root root /lib/fonts/timBI08.pcf"
FILE_FONT_20="file ../../../thirdparty/lib/chartdirector/fonts/timI.pfc      0444 root root /lib/fonts/timI.pfc"
FILE_FONT_21="file ../../../thirdparty/lib/chartdirector/fonts/timI08.pcf    0444 root root /lib/fonts/timI08.pcf"
FILE_FONT_22="file ../../../thirdparty/lib/chartdirector/fonts/timI10.pcf    0444 root root /lib/fonts/timI10.pcf"
FILE_FONT_23="file ../../../thirdparty/lib/chartdirector/fonts/timI12.pcf    0444 root root /lib/fonts/timI12.pcf"
FILE_FONT_24="file ../../../thirdparty/lib/chartdirector/fonts/timI14.pcf    0444 root root /lib/fonts/timI14.pcf"
FILE_FONT_25="file ../../../thirdparty/lib/chartdirector/fonts/timR.pfc      0444 root root /lib/fonts/timR.pfc"
FILE_FONT_26="file ../../../thirdparty/lib/chartdirector/fonts/timR08.pcf    0444 root root /lib/fonts/timR08.pcf"
FILE_FONT_27="file ../../../thirdparty/lib/chartdirector/fonts/timR10.pcf    0444 root root /lib/fonts/timR10.pcf"
FILE_FONT_28="file ../../../thirdparty/lib/chartdirector/fonts/timR12.pcf    0444 root root /lib/fonts/timR12.pcf"
FILE_FONT_29="file ../../../thirdparty/lib/chartdirector/fonts/timR14.pcf    0444 root root /lib/fonts/timR14.pcf"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install
