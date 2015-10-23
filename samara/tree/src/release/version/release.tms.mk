#  Included makefile

# Note: this file is included by common.mk, so do not include anything here.

# This should correspond to a TMS tag, or a TMS tag "-customeremail"
PROD_TMS_SRCS_RELEASE_ID:=samara_release_hickory-juniper

# Short tag for this SRCS_RELEASE_ID to specify overlay ids
PROD_TMS_SRCS_RELEASE_ID_SHORT:=hickory_u0

# There will be one entry in this list per optional bundle
PROD_TMS_SRCS_BUNDLES=

# This should correspond to the date of the TMS tag
PROD_TMS_SRCS_DATE:=yyyy-mm-dd hh:mm:ss

# This is an abitrary TMS version string
PROD_TMS_SRCS_VERSION:=3.6.6

PROD_TMS_SRCS_CUSTOMER:=nokeena

# Include the multi-release overlay files.  Unfortunately we have to include
# them all, as there's no easy way for us to tell which ones might be for
# this release.  The files themselves must be conditionalized.
dotomk:=$(wildcard ${PROD_TREE_ROOT}/src/release/version/overlay.multi.*.mk)
ifneq (${dotomk},)
    -include ${dotomk}
endif

# Include the bundle files, each of which should be appending to
# the make variable PROD_TMS_SRCS_BUNDLES .
dotbmk:=$(wildcard ${PROD_TREE_ROOT}/src/release/version/bundle.*.mk)
ifneq (${dotbmk},)
    -include ${dotbmk}
endif

# Sort the bundle names, and get rid of unwanted spaces
# Note that bundle names are of the form "[lcalphanum_]*"
PROD_TMS_SRCS_BUNDLES:=$(shell echo "${PROD_TMS_SRCS_BUNDLES}" | tr ' ' '\n' | grep -v '^$$' | sort | uniq | tr '\n' ' ' | sed 's/ *$$//')

export PROD_TMS_SRCS_RELEASE_ID
export PROD_TMS_SRCS_RELEASE_ID_SHORT
export PROD_TMS_SRCS_BUNDLES
export PROD_TMS_SRCS_DATE
export PROD_TMS_SRCS_VERSION
export PROD_TMS_SRCS_CUSTOMER
