# Makefile

# Put thirdparty binary files into the MFC package.
# These binary files are NOT built from source each time MFC is built.
#
# Note: If thirdparty files are added or removed from the MFC package, and
#   if the files are GPL or have some other license that needs to have the
#   source made available, update nokeena/src/release/freedist/Makefile.inc

# License types:
#   dmidecode          GPL
#   ffmpeg             GPL/LGPL  (and uses libx264 which is GPL)
#   gpac               LGPL
#   hdparm             BSD style
#   lsiutil            ??? Binary distribution OK
#   pf_ring            ???
#   quagga             ???
#   sas2ircu           ??? Binary distribution OK
#   sdparm             BSD style
#   smartctl           GPL (using lastest source instead of older one in Centos)
#   ssh-43p2           ???
#   tcl_84             ???
#   tw_cli             ???
#   virtualenv         ???
#   wget               ???
#   yamdi              BSD style

include ${PROD_TREE_ROOT}/src/mk/common.mk

SUBDIRS= \
  dmidecode \
  ffmpeg \
  gpac \
  hdparm \
  lsiutil \
  pf_ring \
  quagga \
  sas2ircu \
  sdparm \
  smartctl \
  ssh-43p2 \
  tcl_84 \
  tw_cli \
  virtualenv \
  wget \
  yamdi \

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install
