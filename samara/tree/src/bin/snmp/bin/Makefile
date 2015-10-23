#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

SUBDIRS=snmpd

#
# The following are not required for full product functionality,
# but may be useful in testing or debugging.  They are listed 
# separately below to make it easier to selectively disable them.
# 
ifneq (${PROD_TARGET_OS},SUNOS)
    #SUBDIRS+=snmpbulkget
    #SUBDIRS+=snmpbulkwalk
    #SUBDIRS+=snmpdelta
    #SUBDIRS+=snmpget
    #SUBDIRS+=snmpgetnext
    #SUBDIRS+=snmpstatus
    #SUBDIRS+=snmptable
    SUBDIRS+=snmptranslate
    SUBDIRS+=snmptrap
    SUBDIRS+=snmptrapd
    SUBDIRS+=snmpwalk
    SUBDIRS+=snmpkeys
endif

ifneq (${PROD_FEATURE_SNMP_SETS},0)
    SUBDIRS+=snmpset
endif

include ${PROD_TREE_ROOT}/src/mk/subdir.mk
