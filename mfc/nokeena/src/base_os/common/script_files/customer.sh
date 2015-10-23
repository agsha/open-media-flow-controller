# -----------------------------------------------------------------------------
# This is used by afail.sh to formulate the subject line of the 
# failure email it sends out.
#
SUBJECT_PREFIX="MFC"


# -----------------------------------------------------------------------------
# This is used by makemail.sh.  It embeds this string in the headers it
# adds to its emails saying the build ID, product type, etc.
# It should be kept in sync with md_email_header_branding in 
# customer.h, which is used for the same thing in other contexts.
#
EMAIL_HEADER_BRANDING="Media-Flow-Controller"


# In Ginkgo, the use of ssl-cert-gen.sh was removed.
# The replacement is a new scheme with settings in include/customer.h.
# See $PROD_TREE_ROOT/customer/generic/src/include/customer.h
#    (Certificates section)
# See $PROD_TREE_ROOT/doc/design/nodes.txt 
#    (/certs/config/global/default/cert_gen)
## -----------------------------------------------------------------------------
## This is used by ssl-cert-gen.sh to generate the SSL certificate
## for use with the Web UI.
##
#SSL_CERT_HEADER="California
#Sunnyvale
#Juniper Networks, Inc.
#Media Flow Controller Platform"


# -----------------------------------------------------------------------------
# Below is a series of functions which serve as graft points to allow
# the customer to add custom code to run during the execution of various
# generic scripts.  A generic script needing a graft point executes
# customer.sh to define the graft functions, and then calls the
# appropriate function if it is defined.
#
# Since sh does not allow any way to test if a function is defined,
# and since calling a nonexistent function produces an error message,
# customer.sh must define a variable named "HAVE_<function name>"
# to have the value "y".  The function will be called iff this value
# is set.
#


# -----------------------------------------------------------------------------
# Graft points for sysdump.sh.  Below are functions which get called from
# various points within that script.
#
# Any output to be inserted into the sysinfo file should be appended to
# the file specified by ${SYSINFO_FILENAME}, for example:
#
#   echo "Hello" >> ${SYSINFO_FILENAME}
#
# You can also add files to the sysdump.  Create any such files in
# ${STAGE_DIR}.  For example:
#
#   cp /etc/fstab ${STAGE_DIR}
#

# .............................................................................
# Graft point #1.  This is called while the sysinfo text is being 
# generated, right after the hostname is emitted.
#
HAVE_SYSDUMP_GRAFT_1=y
sysdump_graft_1()
{
    # First clean out all previous sysdump files
    rm -f ${FINAL_DIR}/sysdump-*.tgz

    ret=`/opt/nkn/bin/nkncnt -f ${STAGE_DIR}/nkncnt.log -t 0`
    ret= `/opt/nkn/bin/nkncnt -P /opt/nkn/sbin/live_mfpd -f ${STAGE_DIR}/live_mfpd_nkncnt.log  -t 0`
    ret= `/opt/nkn/bin/nkncnt -P /opt/nkn/sbin/file_mfpd -f ${STAGE_DIR}/file_mfpd_nkncnt.log  -t 0`
    ret= `/opt/nkn/bin/nkncnt -P /opt/nkn/sbin/ssld -f ${STAGE_DIR}/ssld_nkncnt.log  -t 0`
    ret= `/opt/nkn/bin/nkncnt -P /opt/nkn/sbin/nsmartd -f ${STAGE_DIR}/nsmartd_nkncnt.log  -t 0`

    sleep 2

    if [ "${EXTRA_FILES}" != "" ] ; then
    	bt_dir=`basename ${STAGE_DIR}`

    	full_bt_dir=/var/nkn/backtraces/"${bt_dir}"
    	mkdir -p "${full_bt_dir}"

    	# Copy any specified extra files
    	for ef in ${EXTRA_FILES}; do
       	    cp -p $ef "${full_bt_dir}"
    	done
    fi
    
    # TODO: Enabling the following lines causes CLI to hang
    # Need to REVISIT this later
#    `/opt/tms/bin/mdreq action /nkn/nvsd/am/actions/tier` > ${STAGE_DIR}/analytics.tier
#    `/opt/tms/bin/mdreq action /nkn/nvsd/am/actions/rank` > ${STAGE_DIR}/analytics.rank
}

# .............................................................................
# Graft point #2.  This is called after the header with hostname, version,
# date, and uptime; but before most of the other output.
#
HAVE_SYSDUMP_GRAFT_2=n
# sysdump_graft_2()
# {
# }

# .............................................................................
# Graft point #3.  This is called after all the standard text output has
# been generated, but before the last 'n' lines of the logs are included.
# Set MESSAGES_LOG_LINES to change the number of lines to be included.
#
HAVE_SYSDUMP_GRAFT_3=y
sysdump_graft_3()
{
    do_command_output 'cat /proc/mounts'
    do_command_output 'cat /proc/meminfo'
    do_command_output 'cat /proc/slabinfo'
    do_command_output 'cat /proc/vmstat'
    do_command_output 'cat /proc/zoneinfo'
    do_command_output 'cat /proc/cpuinfo'
    do_command_output 'cat /proc/swaps'
    do_command_output 'cat /proc/version'
    do_command_output 'cat /proc/partitions'
    do_command_output 'cat /proc/diskstats'
    do_command_output 'cat /proc/interrupts'
    do_command_output '/usr/bin/mpstat'
    do_command_output '/usr/bin/top -b -H -n 1'
    do_command_output '/usr/bin/ipcs'
    do_command_output '/bin/ps -eo pid,user,cmd,rss,size,vsize'
    do_command_output '/bin/df /nkn/mnt/dc_*'

    if [ -f /opt/nkn/bin/tech-support.sh ]
    then
        . /opt/nkn/bin/tech-support.sh
        tech_supp_checkconfig
        ret=$?
        if [ "${ret}" -eq "0" ]
        then
            tech_supp_process
        fi
    fi
    cp -R -p /log/install/ "${STAGE_DIR}"
    cp -R -p /log/init/   "${STAGE_DIR}"
    cp -R -p /log/mfglog/ "${STAGE_DIR}"
    cp -R -p /nkn/ns_objects "${STAGE_DIR}"
    /opt/nkn/bin/dmidecode-info.sh > "${STAGE_DIR}"/dmidecode-info.txt
    /opt/nkn/bin/eth-info.sh > "${STAGE_DIR}"/eth-info.txt
    /opt/nkn/bin/nkncmmstatus -i 10 > "${STAGE_DIR}"/cmm-info.txt

    # BZ 4761  - Add accesslogs, errorlog, etc...
    SAVE_OTHERLOG_DIR="${STAGE_DIR}"/nkn
    mkdir -p "${SAVE_OTHERLOG_DIR}"
    cp -R -p /var/log/nkn/* "${SAVE_OTHERLOG_DIR}"

    # BZ 9133 - Collect Adaptec logs in techdump
    SAVE_ADAPTEC_LOG_DIR="${STAGE_DIR}"/Adaptec_log
    mkdir -p "${SAVE_ADAPTEC_LOG_DIR}"
    /opt/nkn/bin/adaptec_log.sh > "${SAVE_ADAPTEC_LOG_DIR}"/adaptec_log.log
    cp -R -p /var/log/nkn/Adaptec_logs/*tgz "${SAVE_ADAPTEC_LOG_DIR}"

    # BZ 8224  - Add the counters samples
    SAVE_NKNCNT_DIR="${STAGE_DIR}"/nkncnt
    mkdir -p "${SAVE_NKNCNT_DIR}"
    cp -R -p /var/log/nkncnt/* "${SAVE_NKNCNT_DIR}"
    
    # BZ 9395  - Add committed PE files to tech-dump
    SAVE_PE_COMMIT_DIR="${STAGE_DIR}"/nkn/policy_engine
    mkdir -p "${SAVE_PE_COMMIT_DIR}"
    cp -R -p /nkn/policy_engine/*.tcl.com "${SAVE_PE_COMMIT_DIR}"
    
    # BZ 9680  - Log analyzer output to tech-dump
    SAVE_LOG_ANALYZER_DIR="${STAGE_DIR}"/nkn/log_report
    mkdir -p "${SAVE_LOG_ANALYZER_DIR}"
    cp -R -p /var/log/report/* "${SAVE_LOG_ANALYZER_DIR}"

    # Get list of namespaces and issue show namespace
    local ns=`/opt/tms/bin/mdreq -v query iterate enter-pass /nkn/nvsd/namespace`
    
    for n in $ns ;
    do
	cli -t "show namespace $n" >> "${STAGE_DIR}"/show_namespace.txt
    done
   
    if [ -d /var/nkn/backtraces ] ; then
        cp -R /var/nkn/backtraces ${STAGE_DIR}
    fi

    # BZ 4762 - take a snapshot of the counters at the end of 
    # sysdump.
    #17-May-2011:Add a check if  snapshot is for a process that doesnt use NVSD shared memory, 
    # in which case, we have to get the dump of the corresponding shared memory.
    ret= `/opt/nkn/bin/nkncnt -P /opt/nkn/sbin/live_mfpd -f ${STAGE_DIR}/live_mfpd_nkncnt.log  -t 0`
    ret= `/opt/nkn/bin/nkncnt -P /opt/nkn/sbin/file_mfpd -f ${STAGE_DIR}/file_mfpd_nkncnt.log  -t 0`
    ret=`/opt/nkn/bin/nkncnt -f ${STAGE_DIR}/nkncnt.log -t 0`
    ret= `/opt/nkn/bin/nkncnt -P /opt/nkn/sbin/ssld -f ${STAGE_DIR}/ssld_nkncnt.log  -t 0`
    ret= `/opt/nkn/bin/nkncnt -P /opt/nkn/sbin/nsmartd -f ${STAGE_DIR}/nsmartd_nkncnt.log  -t 0`
    if [ -d /nkn/tmp ] ; then
        rm -rf "${STAGE_DIR}"/nkn-tmp
        mkdir "${STAGE_DIR}"/nkn-tmp
        cd /nkn/tmp
        echo Files from /nkn/tmp > "${STAGE_DIR}"/nkn-tmp/ABOUT-THIS-DIRECTORY.txt
        date   >> "${STAGE_DIR}"/nkn-tmp/ABOUT-THIS-DIRECTORY.txt
        ls -lR >> "${STAGE_DIR}"/nkn-tmp/ABOUT-THIS-DIRECTORY.txt
        for FN in `find * -type f`
        do
            # Do not copy very large files.  Limit at 2MB.
            SIZE=`ls -l "${FN}" | awk '{ print $5}'`
            [ ${SIZE} -gt 20000000 ] && continue
            DN=`dirname "${FN}"`
            BN=`basename "${FN}"`
            [ ! -d     "${STAGE_DIR}"/nkn-tmp/"${DN}" ] && \
              mkdir -p "${STAGE_DIR}"/nkn-tmp/"${DN}"
            cp -p "${FN}" "${STAGE_DIR}"/nkn-tmp/"${DN}"/
        done
    fi

}

HAVE_SYSDUMP_GRAFT_4=y
sysdump_graft_4()
{
   # delete directories which might contain confidential
   # customer data
   rm -fr "${STAGE_DIR}"/config/nkn/cert
   rm -fr "${STAGE_DIR}"/config/nkn/key
   rm -fr "${STAGE_DIR}"/config/nkn/ca
   rm -fr "${STAGE_DIR}"/config/nkn/csr
}

# -----------------------------------------------------------------------------
# Graft points for firstboot.sh.  Below are functions which get called
# from various points within that script.
#

# .............................................................................
# Graft point #1.  This is called at the very beginning, before the /var
# upgrade is performed
#
HAVE_FIRSTBOOT_GRAFT_1=y
firstboot_graft_1()
{
  # All the files under /opt/nkn/installthis, install under "/".
  # (e.g. /config/nkn/... and /nkn/plugins/...)
  [ ! -d /opt/nkn/installthis ] && return
  cd /opt/nkn/installthis
  for DIR in `find * -type d`
  do
    [ -d /${DIR} ] && continue
    mkdir --mode=755 /${DIR}
    chown admin:root /${DIR}
  done
  for FILE in `find * -type f`
  do
    [ ! -d /${FILE} ] && rm -f /${FILE}
    cp -p ${FILE} /${FILE}
  done
  for FILE in `find * -type l`
  do
    [ ! -d /${FILE} ] && rm -f /${FILE}
    echo ${FILE} | cpio -pdmu / 2> /dev/null
  done

  # Make links to kdump images
  KVER=`uname -r | sed -e "s,smp,,g"`

  # Check if we are running on pacifica
  PACIFICA_HW=no
  cmdline=`cat /proc/cmdline`
  if strstr "${cmdline}" model=pacifica ; then
      PACIFICA_HW=yes
  fi

  # At this point if we are not on pacifica /boot seems to be mounted in RO
  # Check for pacifica to see if we do need to remount /boot
  [ "${PACIFICA_HW}" = "no" ]  && /bin/mount -o remount,rw /boot
  ln -s /boot/vmlinuz-smp /boot/vmlinuz-${KVER}
  ln -s /boot/vmlinuz-smp /boot/vmlinuz-${KVER}-kdump
  ln -s /boot/initrd.img /boot/initrd-${KVER}kdump.img
  [ "${PACIFICA_HW}" = "no" ]  && /bin/mount -o remount,ro /boot
  ln -s /lib/modules/`uname -r` /lib/modules/${KVER}-kdump

  if [ ! -d /coredump/kernel ]
  then
      mkdir -p /coredump/kernel
  fi
}

# .............................................................................
# Graft point #2.  This is called after the /var upgrade is performed,
# and after the grub.conf is regenerated.
#
HAVE_FIRSTBOOT_GRAFT_2=y
firstboot_graft_2()
{
        chkconfig --add portmap
        chkconfig --add eth-reorder
        chkconfig --add firstboot_action
        chkconfig --add fixups
        chkconfig --add delete_unused_drives_from_TMdb
        chkconfig --add fuse
        chkconfig --add initdisks
        chkconfig --add raid_monitor
        chkconfig --add mfd_setup
        chkconfig --add irqbalance
	chkconfig --add rpcbind
	chkconfig --add named
	chkconfig --add ipmi
	chkconfig --add ipmievd
	chkconfig --add kdump
	chkconfig --add nvsdmon
}

# .............................................................................
# Graft point #3.  This is called right before the end of the script,
# but before virtual partitions are synched back to disk and other
# cleanup is performed.
#
HAVE_FIRSTBOOT_GRAFT_3=n
# firstboot_graft_3()
# {
# }


# -----------------------------------------------------------------------------
# Graft points for ${PROD_TREE_ROOT}/src/release/mkver.sh.
#  Below are functions which get called from various points within that script.
#

# .............................................................................
# Graft point #1.  This is called right before the version string is 
# constructed
# This is after we get dotted in after we are dotted in and this line is executed:
#     BUILD_PROD_NAME=${BUILD_PROD_PRODUCT_LC}
#
HAVE_MKVER_GRAFT_1=y
mkver_graft_1()
{
    BUILD_PROD_NAME=mfc
    [ "_${NKN_BUILD_PROD_NAME}" != "_" ]          && BUILD_PROD_NAME="${NKN_BUILD_PROD_NAME}"
    [ "_${NKN_BUILD_PROD_RELEASE}" != "_" ]       && BUILD_PROD_RELEASE="${NKN_BUILD_PROD_RELEASE}"
    [ "_${NKN_BUILD_NUMBER}" != "_" ]             && BUILD_NUMBER="${NKN_BUILD_NUMBER}"
    [ "_${NKN_BUILD_ID}" != "_" ]                 && BUILD_ID="${NKN_BUILD_ID}"
    [ "_${NKN_BUILD_DATE}" != "_" ]               && BUILD_DATE="${NKN_BUILD_DATE}"
    [ "_${NKN_BUILD_HOST_USER}" != "_" ]          && BUILD_HOST_USER="${NKN_BUILD_HOST_USER}"
    [ "_${NKN_BUILD_HOST_USER}" != "_" ]          && BUILD_HOST_REAL_USER="${NKN_BUILD_HOST_USER}"
    [ "_${NKN_BUILD_HOST_USER}" != "_" ]          &&  PROD_HOST_USER="${NKN_BUILD_HOST_USER}"
    [ "_${NKN_BUILD_HOST_MACHINE}" != "_" ]       && BUILD_HOST_MACHINE="${NKN_BUILD_HOST_MACHINE}"
    [ "_${NKN_BUILD_HOST_MACHINE_SHORT}" != "_" ] && BUILD_HOST_MACHINE_SHORT="${NKN_BUILD_HOST_MACHINE_SHORT}"
    [ "_${NKN_BUILD_SCM_INFO}" != "_" ]           && BUILD_SCM_INFO="${NKN_BUILD_SCM_INFO}"
    [ "_${NKN_BUILD_SCM_INFO_SHORT}" != "_" ]     && BUILD_SCM_INFO_SHORT="${NKN_BUILD_SCM_INFO_SHORT}"

}

# .............................................................................
# Graft point #2.  This is called right after the version string is
# constructed:
# BUILD_PROD_VERSION="${BUILD_PROD_NAME} ${BUILD_PROD_RELEASE} ${BUILD_ID} ${BUILD_DATE} ${BUILD_TARGET_ARCH_LC} ${BUILD_HOST_REAL_USER}@${BUILD_HOST_MACHINE_SHORT}:${BUILD_SCM_INFO_SHORT}"
# So if we want BUILD_PROD_VERSION set to something else we can either change the
# settings that go into it, or change it here.
#
# ALSO NOTE: The comment in mkver.sh about adding new variables:
# # Customers can update the variables already set, and add new ones to
# # the BUILD_EXTRA_VARS variable, which will cause them to be included in
# # the build_version.* files.
# These variables MUST be named BUILD_<something>
# Note: mkver.sh is in $PROD_TREE_ROOT/src/release/
HAVE_MKVER_GRAFT_2=y
mkver_graft_2()
{
    [ "_${NKN_BUILD_PROD_VERSION}" != "_" ]  && BUILD_PROD_VERSION="${NKN_BUILD_PROD_VERSION}"
}


# -----------------------------------------------------------------------------
# Graft points for scrub.sh.  Below are functions which get called
# from various points within that script.
#

# .............................................................................
# Graft point #1.  This is called at the very beginning, when everything
# is still running.
#
HAVE_SCRUB_GRAFT_1=y
scrub_graft_1()
{
    /opt/nkn/bin/scrub_mfd.sh 1
}


# .............................................................................
# Graft point #2.  This is called after PM (and thus all of the daemons
# it launches) has been stopped.
#
HAVE_SCRUB_GRAFT_2=y
scrub_graft_2()
{
    /opt/nkn/bin/scrub_mfd.sh 2
}


# -----------------------------------------------------------------------------
# Graft points for resetpw_shell.sh.  Below are functions which get called
# from various points within that script.
#

# .............................................................................
# Graft point #1.  This is called before any actions have been taken, to allow
# default settings to be overridden.  See comment above this graft point in
# resetpw_shell.sh for instructions on how to change certain behaviors.
#
# NOTE: if you assign a hashed password to PW_NEW_PASS, enclose it in 
# SINGLE quotes, e.g.:
#
#    PW_NEW_PASS='$1$cl7wxWpk$n1XpqZpo1NoPpk0zduZiH1'
#
HAVE_RESETPW_SHELL_GRAFT_1=n
# resetpw_shell_graft_1()
# {
# }

# .............................................................................
# Graft point #1.  This is called when we are cleansing "all"
# cleansable nodes.
#
HAVE_CLEANSE_DB_GRAFT_1=y
 cleanse_db_graft_1()
 {
	 CERTS=`${MDDBREQ} -v -q ${DBPATH} query iterate - /nkn/ssld/config/certificate`
		 for CERT in ${CERTS}; do
			 echo "/nkn/ssld/config/certificate/${CERT}/passphrase" >> ${NODES_FILE}
	 done

	 KEYS=`${MDDBREQ} -v -q ${DBPATH} query iterate - /nkn/ssld/config/key`
		 for KEY in ${KEYS}; do
			 echo "/nkn/ssld/config/key/${KEY}/passphrase" >> ${NODES_FILE}
	 done

 }

