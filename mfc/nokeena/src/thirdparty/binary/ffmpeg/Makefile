# Makefile to put thirdparty binary files into the MFC package.  These binary files
# are NOT built from source each time MFC is built.
#
# Note: If thirdparty files are added or removed from the MFC package, and
#   if the files are GPL or have some other license that needs to have the
#   source made available, update nokeena/src/release/freedist/Makefile.inc

# License types:
#   ffmpeg                    GPL/LGPL  (and uses libx264 which is GPL)


include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
  ${SYS_INSTALL_DIR_opt_nkn} \
  ${SYS_INSTALL_DIR_opt_nkn_bin} \
  ${SYS_INSTALL_DIR_opt_nkn_bin_ffpresets} \
  ${SYS_INSTALL_FILE_ffmpeg} \
  ${SYS_INSTALL_FILE_ffpresets_baseline} \
  ${SYS_INSTALL_FILE_ffpresets_faster} \
  ${SYS_INSTALL_FILE_ffpresets_faster_firstpass} \
  ${SYS_INSTALL_FILE_ffpresets_fast} \
  ${SYS_INSTALL_FILE_ffpresets_fast_firstpass} \
  ${SYS_INSTALL_FILE_ffpresets_ipod320} \
  ${SYS_INSTALL_FILE_ffpresets_ipod640} \
  ${SYS_INSTALL_FILE_ffpresets_lossless_fast} \
  ${SYS_INSTALL_FILE_ffpresets_lossless_max} \
  ${SYS_INSTALL_FILE_ffpresets_lossless_medium} \
  ${SYS_INSTALL_FILE_ffpresets_lossless_slower} \
  ${SYS_INSTALL_FILE_ffpresets_lossless_slow} \
  ${SYS_INSTALL_FILE_ffpresets_lossless_ultrafast} \
  ${SYS_INSTALL_FILE_ffpresets_main} \
  ${SYS_INSTALL_FILE_ffpresets_medium} \
  ${SYS_INSTALL_FILE_ffpresets_medium_firstpass} \
  ${SYS_INSTALL_FILE_ffpresets_placebo} \
  ${SYS_INSTALL_FILE_ffpresets_placebo_firstpass} \
  ${SYS_INSTALL_FILE_ffpresets_slower} \
  ${SYS_INSTALL_FILE_ffpresets_slower_firstpass} \
  ${SYS_INSTALL_FILE_ffpresets_slow} \
  ${SYS_INSTALL_FILE_ffpresets_slow_firstpass} \
  ${SYS_INSTALL_FILE_ffpresets_superfast} \
  ${SYS_INSTALL_FILE_ffpresets_superfast_firstpass} \
  ${SYS_INSTALL_FILE_ffpresets_ultrafast} \
  ${SYS_INSTALL_FILE_ffpresets_ultrafast_firstpass} \
  ${SYS_INSTALL_FILE_ffpresets_veryfast} \
  ${SYS_INSTALL_FILE_ffpresets_veryfast_firstpass} \
  ${SYS_INSTALL_FILE_ffpresets_veryslow} \
  ${SYS_INSTALL_FILE_ffpresets_veryslow_firstpass} \

CLEANFILES= \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffmpeg \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-baseline.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-faster.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-faster_firstpass.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-fast.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-fast_firstpass.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-ipod320.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-ipod640.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-lossless_fast.ffpreset \ \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-lossless_max.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-lossless_medium.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-lossless_slower.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-lossless_slow.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-lossless_ultrafast.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-main.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-medium.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-medium_firstpass.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-placebo.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-placebo_firstpass.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-slower.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-slower_firstpass.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-slow.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-slow_firstpass.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-superfast.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-superfast_firstpass.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-ultrafast.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-ultrafast_firstpass.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-veryfast.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-veryfast_firstpass.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-veryslow.ffpreset \
  ${FULL_INSTALL_DIR}/opt/nkn/bin/ffpresets/libx264-veryslow_firstpass.ffpreset \


# Directory creation specs:
SYS_INSTALL_DIR_opt_nkn="dir 0755 root root /opt/nkn"
SYS_INSTALL_DIR_opt_nkn_bin="dir 0755 root root /opt/nkn/bin"
SYS_INSTALL_DIR_opt_nkn_bin_ffpresets="dir 0755 root root /opt/nkn/bin/ffpresets"


# File installation specs:

SYS_INSTALL_FILE_ffmpeg="file   ffmpeg    0755 root root /opt/nkn/bin/ffmpeg"

SYS_INSTALL_FILE_ffpresets_baseline="file ffpresets/libx264-baseline.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-baseline.ffpreset"
SYS_INSTALL_FILE_ffpresets_faster="file ffpresets/libx264-faster.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-faster.ffpreset"
SYS_INSTALL_FILE_ffpresets_faster_firstpass="file ffpresets/libx264-faster_firstpass.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-faster_firstpass.ffpreset"
SYS_INSTALL_FILE_ffpresets_fast="file ffpresets/libx264-fast.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-fast.ffpreset"
SYS_INSTALL_FILE_ffpresets_fast_firstpass="file ffpresets/libx264-fast_firstpass.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-fast_firstpass.ffpreset"
SYS_INSTALL_FILE_ffpresets_ipod320="file ffpresets/libx264-ipod320.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-ipod320.ffpreset"
SYS_INSTALL_FILE_ffpresets_ipod640="file ffpresets/libx264-ipod640.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-ipod640.ffpreset"
SYS_INSTALL_FILE_ffpresets_lossless_fast="file ffpresets/libx264-lossless_fast.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-lossless_fast.ffpreset"
SYS_INSTALL_FILE_ffpresets_lossless_max="file ffpresets/libx264-lossless_max.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-lossless_max.ffpreset"
SYS_INSTALL_FILE_ffpresets_lossless_medium="file ffpresets/libx264-lossless_medium.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-lossless_medium.ffpreset"
SYS_INSTALL_FILE_ffpresets_lossless_slower="file ffpresets/libx264-lossless_slower.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-lossless_slower.ffpreset"
SYS_INSTALL_FILE_ffpresets_lossless_slow="file ffpresets/libx264-lossless_slow.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-lossless_slow.ffpreset"
SYS_INSTALL_FILE_ffpresets_lossless_ultrafast="file ffpresets/libx264-lossless_ultrafast.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-lossless_ultrafast.ffpreset"
SYS_INSTALL_FILE_ffpresets_main="file ffpresets/libx264-main.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-main.ffpreset"
SYS_INSTALL_FILE_ffpresets_medium="file ffpresets/libx264-medium.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-medium.ffpreset"
SYS_INSTALL_FILE_ffpresets_medium_firstpass="file ffpresets/libx264-medium_firstpass.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-medium_firstpass.ffpreset"
SYS_INSTALL_FILE_ffpresets_placebo="file ffpresets/libx264-placebo.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-placebo.ffpreset"
SYS_INSTALL_FILE_ffpresets_placebo_firstpass="file ffpresets/libx264-placebo_firstpass.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-placebo_firstpass.ffpreset"
SYS_INSTALL_FILE_ffpresets_slower="file ffpresets/libx264-slower.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-slower.ffpreset"
SYS_INSTALL_FILE_ffpresets_slower_firstpass="file ffpresets/libx264-slower_firstpass.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-slower_firstpass.ffpreset"
SYS_INSTALL_FILE_ffpresets_slow="file ffpresets/libx264-slow.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-slow.ffpreset"
SYS_INSTALL_FILE_ffpresets_slow_firstpass="file ffpresets/libx264-slow_firstpass.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-slow_firstpass.ffpreset"
SYS_INSTALL_FILE_ffpresets_superfast="file ffpresets/libx264-superfast.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-superfast.ffpreset"
SYS_INSTALL_FILE_ffpresets_superfast_firstpass="file ffpresets/libx264-superfast_firstpass.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-superfast_firstpass.ffpreset"
SYS_INSTALL_FILE_ffpresets_ultrafast="file ffpresets/libx264-ultrafast.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-ultrafast.ffpreset"
SYS_INSTALL_FILE_ffpresets_ultrafast_firstpass="file ffpresets/libx264-ultrafast_firstpass.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-ultrafast_firstpass.ffpreset"
SYS_INSTALL_FILE_ffpresets_veryfast="file ffpresets/libx264-veryfast.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-veryfast.ffpreset"
SYS_INSTALL_FILE_ffpresets_veryfast_firstpass="file ffpresets/libx264-veryfast_firstpass.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-veryfast_firstpass.ffpreset"
SYS_INSTALL_FILE_ffpresets_veryslow="file ffpresets/libx264-veryslow.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-veryslow.ffpreset"
SYS_INSTALL_FILE_ffpresets_veryslow_firstpass="file ffpresets/libx264-veryslow_firstpass.ffpreset 0444 root root /opt/nkn/bin/ffpresets/libx264-veryslow_firstpass.ffpreset"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install
