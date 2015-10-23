#! /bin/bash
#
#	File : cache_evictd.sh
#
#	Description : This script evicts objects from the disk cache
#
#	Created By : Michael Nishimoto (miken@nokeena.com)
#
#	Created On : 25 February, 2009
#
#	Copyright (c) Nokeena Inc., 2009
#

# source the common evict functions
source /opt/nkn/bin/evictd_common.sh

PATH=$PATH:/sbin


#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE 
#
#--------------------------------------------------------------

get_options



while [ true ]; do
  # 6 = SATA ; evict only the SATA tier from this daemon
  for PROVIDER in 6; do
    evict_provider ${PROVIDER}
  done
  sleep ${EVICT_CYCLE}
done
