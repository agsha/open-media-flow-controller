#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

SUBDIRS=\

ifneq (${PROD_FEATURE_DEV_ENV},1)
SUBDIRS+=\
	cron \
	grub \
	syslogd \
	login \
	pam \
        zeroconf \

endif

#
# We build DHCP because we needed it to support DHCPv6 on EL5.
# EL5's binary RPMs did not support DHCPv6 in a way the worked for us
# (it had a separate binary which didn't employ a script to apply its
# changes, so it ended up circumventing mgmtd).
#
# We could have continued to use the binary RPMs on EL5 for DHCPv4,
# and on EL6 for both DHCP versions.  But in order to run the same
# thing everywhere (and to deal with possibly out-of-date RPMs on PPC),
# we simply build it always, and use it always.  We still do have 
# separate dhclient-scripts for DHCPv4 and DHCPv6, but it's all the 
# same binary (passed different flags for v4 vs. v6).
#
ifneq (${PROD_FEATURE_DHCP_CLIENT},0)
	SUBDIRS+=                      \
		dhcp \

endif

ifneq (${PROD_FEATURE_CLUSTER},0)
	SUBDIRS+=                      \
		howl \

endif

# NOTE: These directories must be built and installed as 'root'

include ${PROD_TREE_ROOT}/src/mk/subdir.mk
