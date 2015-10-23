#!/bin/sh

#
# (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
# (C) Copyright 2002-2012 Tall Maple Systems, Inc.
# All rights reserved.
# The contents of this file are subject to the terms of the MIT License.
# You may not use this file except in compliance with the License.
# See the file License.txt at the top of this source tree.
# You can obtain a copy of the license at https://opensource.org/licenses/MIT
#

# This script is run from firstboot.sh to perform upgrades to the
# /var partition.  If you add a new rule here, make sure to 
# increment the version number in image_files/var_version.sh.
#
# This script takes one parameter, a string in the form
# "<old_version>_<new_version>".  The new version number must
# be exactly one higher than the old version; thus if multiple 
# upgrades are required, the script must be called multiple times.
# For example, to do an upgrade from version 2 to version 4, the
# script is called twice, once with "2_3" and once with "3_4".

versions=$1

MFG_DB_DIR=/config/mfg
MFG_DB_PATH=${MFG_DB_DIR}/mfdb
MDDBREQ=/opt/tms/bin/mddbreq

# --------------------------------------------------
# Returns 0 if the sha1sum (hash) of the given file is in hash list, 1
# if not in hash list, 2 if the file does not exist, or 3 on any error.
# The hash is computed after first "unexpanding" any of the CVS/SVN ids
# that may be present, as customer revision control systems may have
# modified the original values of these.
#
# Note that this function follows symlinks.
#
# NOTE: to create unexpanded sha1sum's of files, see the script
# "make-hashlist.sh" in the 'tools' directory.
#
# arguments:
#  filename "hash1 hash2 "
file_in_hashlist() {
    fn=$1
    shift
    hl="$@"
    if [ ! -f "${fn}" ]; then
        return 2
    fi
    hash_fn=$(cat "${fn}" | sed -e 's,\$\(Author\|Date\|Header\|Id\|Locker\|Log\|Name\|RCSfile\|Revision\|Source\|State\|TallMaple\|LastChangedRevision\|Rev\|LastChangedDate\|LastChangedBy\|HeadURL\|URL\):[^$][^$]*\$\(.*\)$,\$\1\$\2,g' | sha1sum - | awk '{print $1}')
    if [ $? -ne 0 ]; then
        return 3
    fi
    for h in ${hl}; do
        if [ "${hash_fn}" = "${h}" ]; then
            return 0
        fi
    done
    return 1
}

# --------------------------------------------------
# Same as file_in_hashlist() , except return 0 if the file is missing.
file_in_hashlist_or_missing() {
    file_in_hashlist "$@"
    rv=$?
    if [ "${rv}" = "2" ]; then
        return 0
    else
        return $rv
    fi
}

# --------------------------------------------------
case "$versions" in
    1_2)
          logger -p user.info "Fixing admin home directory"
          mkdir -p /var/home/root
          cp -p /etc/skel/.* /var/home/root
          ;;

    2_3)
          # This upgrade was customer-specific and has been moved elsewhere.
          ;;

    3_4)
          # This upgrade was customer-specific and has been moved elsewhere.
          ;;

    4_5)
          # This upgrade was customer-specific and has been moved elsewhere.
          ;;

    5_6)
          # This upgrade was customer-specific and has been moved elsewhere.
          ;;
    6_7)
          # Empty upgrade rule to get from 7 to 11, since previous release
          # was at 10.
          ;;
    7_8)
          # Empty upgrade rule to get from 7 to 11, since previous release
          # was at 10.
          ;;
    8_9)
          # Empty upgrade rule to get from 7 to 11, since previous release
          # was at 10.
          ;;
    9_10)
          # Empty upgrade rule to get from 7 to 11, since previous release
          # was at 10.
          ;;
    10_11)
          # Make sure the /var/opt/tms/stats/reports directory exists.
          mkdir -p /var/opt/tms/stats/reports
          chmod 0755 /var/opt/tms/stats/reports
          chown root.root /var/opt/tms/stats/reports
          ;;

    11_12)
          # For read-only root, many things from /etc moved to a location
          # in /var
          mkdir -p /var/opt/tms/output
          chmod 0755 /var/opt/tms/output
          mkdir -p /var/opt/tms/output/cron.d
          chmod 0755 /var/opt/tms/output/cron.d
          mkdir -p /var/root
          chmod 0755 /var/root
          mkdir -p /var/root/tmp
          chmod 1777 /var/root/tmp
          ln -s /etc/cron_conf.d/sysstat /var/opt/tms/output/cron.d/sysstat
          ;;

    12_13)
          mkdir -p /var/opt/tms/output/web/conf
          chmod 0755 /var/opt/tms/output/web/conf
          mv -f /var/opt/tms/web/conf/server.crt /var/opt/tms/output/web/conf
          mv -f /var/opt/tms/web/conf/server.hostname /var/opt/tms/output/web/conf
          mv -f /var/opt/tms/web/conf/server.key /var/opt/tms/output/web/conf
          ;;

    13_14)
          touch /var/opt/tms/output/ssh_config
          ln -f -s /var/opt/tms/output/ssh_config /etc/ssh/ssh_config 
          touch /var/opt/tms/output/ssh_known_hosts
          ln -f -s /var/opt/tms/output/ssh_known_hosts /etc/ssh/ssh_known_hosts 
          ;;

    14_15)
          mkdir -p /var/opt/tms/output/racoon
          ;;

    15_16)
          ln -f -s /var/opt/tms/output/logrotate /var/opt/tms/output/cron.d/logrotate
          ln -f -s /var/opt/tms/output/vpart_sync /var/opt/tms/output/cron.d/vpart_sync
          mkdir -p /var/racoon
          chmod 755 /var/racoon
          chmod 755 /var/opt/tms/output/racoon
          mkdir -p /var/opt/tms/output/racoon/certs
          chmod 700 /var/opt/tms/output/racoon/certs
          ;;

    16_17)
          mkdir -p /var/opt/tms/cmc/ssh/keys
          mv -f /var/opt/tms/cmc/ssh/id_* /var/opt/tms/cmc/ssh/keys > /dev/null 2>&1
          ;;

    17_18)
          mkdir /var/stmp
          chown 0.0 /var/stmp
          chmod 700 /var/stmp
          ;;

    18_19)
          mkdir -p /var/opt/tms/web/sysimages
          chmod 755 /var/opt/tms/web/sysimages
          ;;

    19_20)
          touch /var/log/systemlog
          chmod 600 /var/log/systemlog
          mkdir /var/log/build_versions
          chmod 700 /var/log/build_versions
          ;;

    20_21)
          mkdir -p /var/opt/tms/web/graphs
          chown 0.0 /var/opt/tms/web/graphs
          chmod 1777 /var/opt/tms/web/graphs
          ;;

    21_22)
          # Re-install the grub, which was updated
          /sbin/aiset.sh -i -r
          ;;

    22_23)
          # The 22-to-23 upgrade rule has been removed.  It previously 
          # added the /var/vtmp directory, which was later decided to move
          # to /vtmp instead.
          ;;

    23_24)
          # If PROD_FEATURE_UCD_SNMP_MIB is enabled, a few new MIBs got
          # added, and NET-SNMP is not smart enough to recalculate the
          # contents of this file -- it actually gives errors until you
          # remove the file to trigger regeneration!  If that feature
          # is not enabled, there is neither benefit nor harm in removing
          # it, as it will get automatically regenerated.
          rm -f /var/net-snmp/_usr_share_snmp_mibs_index
          ;;

    24_25)
          if [ ! -d  /var/opt/tms/text_configs ]; then
              mkdir /var/opt/tms/text_configs
          fi
          chmod 0700 /var/opt/tms/text_configs
          if [ ! -e /config/text ]; then
              ln -s /var/opt/tms/text_configs /config/text
          fi
          ;;

    25_26)

          # Re-make swap for our example layouts STD and SDATA, as
          # now we're mounting swap by label, and older systems
          # (pre-Dogwood Update 4) will not have labels on the swap
          # partitions.
          # 
          # We try to be careful, in case customers have modified STD or
          # SDATA, which is why some of the checks below are present.
          #
          # Customers who move to label'd swaps should follow a similar
          # plan.

          if [ -f /etc/image_layout.sh -a -f /etc/layout_settings.sh ]; then
              . /etc/image_layout.sh
              . /etc/layout_settings.sh
              . /etc/image_layout.sh

              if [ "${IL_LAYOUT}" = "STD" -o "${IL_LAYOUT}" = "SDATA" ]; then
                  part=SWAP
                  eval 'target="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_TARGET}"'
                  eval 'disk_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${target}'_DEV}"'
                  eval 'dev="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_DEV}"'
                  eval 'label="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_LABEL}"'
                  eval 'part_num="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_PART_NUM}"'
                  eval 'part_id="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_PART_ID}"'

                  # Verify that this is a swap partition
                  if [ "${part_id}" = "82" -a ! -z "${disk_dev}" -a ! -z "${part_num}" -a "${label}" = "SWAP_1" ]; then
                      FAILURE=0
                      actual_part_id=`sfdisk -q --print-id "${disk_dev}" "${part_num}" 2> /dev/null` || FAILURE=1
                      if [ ${FAILURE} -eq 0 -a "${actual_part_id}" = "${part_id}" ]; then
                          FAILURE=0
                          swapoff -a
                          mkswap -v1 -L "${label}" ${dev} || FAILURE=1
                          swapon -a -e
                          if [ ${FAILURE} -eq 1 ]; then
                              logger -p users.err "Could not make swap with label ${label} on ${dev}"
                          fi
                      else
                          logger -p user.err "Could not find valid swap partition to label on ${dev}"
                      fi
                  else
                      logger -p user.err "Could not find swap partition to label"
                  fi

              fi
          fi
          
          ;;
    
    26_27)
          cd /var/opt/tms/stats
          FILES=`ls -1`
          for FILE in ${FILES}; do
              # e.g. don't do the 'reports' directory.
              if [ -f ${FILE} ]; then
                  /opt/tms/bin/stats_fix_file -1 ${FILE}
              fi
          done
          ;;

    27_28)
          find /var/opt/tms/sysdumps -type f | xargs -r chmod 0600
          ;;

    28_29)
          # Fix the aftereffects of an old 24-to-25 upgrade (which has
          # since been fixed).
          if [ -L /var/opt/tms/text_configs/text_configs ]; then
              rm -f /var/opt/tms/text_configs/text_configs
          fi
          ;; 

    29_30)
          mkdir -p   /var/cache
          chmod 0755 /var/cache
          mkdir -p   /var/cache/libvirt
          chmod 0700 /var/cache/libvirt
          mkdir -p   /var/lib/misc
          chmod 0755 /var/lib/misc
          mkdir -p   /var/opt/tms/vdisks
          chmod 0755 /var/opt/tms/vdisks
          mkdir -p   /var/opt/tms/isos
          chmod 0755 /var/opt/tms/isos
          ;;

    30_31)
          mkdir -p    /var/opt/tms/output/libvirt
          chmod 0700  /var/opt/tms/output/libvirt
          mkdir -p    /var/opt/tms/output/libvirt/qemu
          chmod 0700  /var/opt/tms/output/libvirt/qemu
          mkdir -p    /var/opt/tms/output/libvirt/qemu/networks
          chmod 0700  /var/opt/tms/output/libvirt/qemu/networks
          mkdir -p    /var/opt/tms/output/libvirt/qemu/networks/autostart
          chmod 0700  /var/opt/tms/output/libvirt/qemu/networks/autostart
          mkdir -p    /var/opt/tms/output/libvirt/qemu/autostart
          chmod 0700  /var/opt/tms/output/libvirt/qemu/autostart
          ;;          

    31_32)
          mkdir -p   /var/opt/tms/vm_save
          chmod 0700 /var/opt/tms/vm_save
          ;;

    32_33)
          mkdir -p   /var/opt/tms/pools
          chmod 0700 /var/opt/tms/pools
          mkdir -p   /var/opt/tms/pools/default
          chmod 0700 /var/opt/tms/pools/default
          mv -f      /var/opt/tms/vdisks/* /var/opt/tms/pools/default > /dev/null 2>&1
          mv -f      /var/opt/tms/isos/*   /var/opt/tms/pools/default > /dev/null 2>&1
          rm -rf     /var/opt/tms/vdisks
          rm -rf     /var/opt/tms/isos
          ;;

    33_34)
          mkdir -p   /var/opt/tms/pools/default/.temp
          chmod 0700 /var/opt/tms/pools/default/.temp
          ;;

    34_35)
          # Our /var skeleton stuff might have made this directory 
          # for us, but we don't want it!  Particularly, we don't want
          # its contents, since we'll be moving over the other dirs.
          rm -rf     /var/opt/tms/virt

          mkdir -p   /var/opt/tms/virt
          chmod 0700 /var/opt/tms/virt
          mv         /var/opt/tms/pools    /var/opt/tms/virt/
          mv         /var/opt/tms/vm_save  /var/opt/tms/virt/
          ;;

    35_36)
          # The 36_37 upgrade rule was originally here.  It was moved
          # down so we could run it again.  See below for the two reasons.
          ;;

    36_37)
          # For 35_36:
          # Since our 34-to-35 rule moved the pool location, any running
          # VMs would have incorrect paths for the attached volumes.
          # Rather than try to fix this up at runtime (if it's even
          # possible), just delete saved VMs; otherwise there will be
          # errors.  Someone who booted at rev 35 would still get these
          # errors, but jumping straight to rev 36 should be fine.
          # 
          # For 36_37:
          # Since the fix for bug 13078, we want to delete any saved VMs
          # that were created since the 35_36 upgrade (presumably only 
          # in TMS engineering), lest they have too many VCPUs and 
          # cause trouble.
          rm -rf     /var/opt/tms/virt/vm_save/*
          ;;

    37_38)
          mkdir -p   /var/opt/tms/virt/vm_save/.temp
          chmod 0700 /var/opt/tms/virt/vm_save/.temp
          ;;

    38_39)
          ${MDDBREQ} ${MFG_DB_PATH} set modify - /mfg/mfdb/virt/pools/pool/default string default
          ;;

    39_40)
          UNAMES=`ls -1 /var/home`
          for UNAME in ${UNAMES}; do
              SSH_DIR=/var/home/${UNAME}/.ssh
              KH_FILE=${SSH_DIR}/known_hosts
              if [ -e ${KH_FILE} ]; then
                  chmod 0644 ${KH_FILE}

                  # To put the file right, we need the owner and group.
                  # We already know the owner, but the group needs to be 
                  # determined.  The .ssh directory itself is a convenient
                  # source for this information, since it presumably has not
                  # been messed with, and should have the right one.
                  GNAME=`stat -c "%G" ${SSH_DIR}`

                  chown ${UNAME}.${GNAME} ${KH_FILE}
              fi
          done
          ;;

    40_41)
          # Cleanup old NET-SNMP index files.  Snmpd will generate these
          # anew when it is launched.

          # Leftover from pre-5.5 versions of NET-SNMP
          rm -rf /var/net-snmp/_usr_share_snmp_mibs_index

          # Maybe leftover (with wrong mode/contents) from running early
          # NET-SNMP 5.5 before merge was done.
          rm -rf /var/net-snmp/mib_indexes
          ;;

    41_42)
          # Remove any "usmUser" and "oldEngineID" lines from snmpd's
          # own saved snmpd.conf (not the one we generate).  Our fix 
          # for bug 13848 may not take hold immediately if there are 
          # old engine IDs leftover from before.
          SNMPD_CONF=/var/net-snmp/snmpd.conf
          SNMPD_CONF_TEMP=/var/net-snmp/snmpd.conf.tmp
          if [ -e ${SNMPD_CONF} ]; then
              cat ${SNMPD_CONF} | egrep -v '^usmUser|^oldEngineID' > ${SNMPD_CONF_TEMP}
              mv -f ${SNMPD_CONF_TEMP} ${SNMPD_CONF}
          fi
          ;;

    42_43)
          # Repeat of 40-to-41 rule.  The upgrades through 41-to-42 were
          # given out in Eucalyptus Update 4 Overlay 11, since the 41-to-42
          # one was needed for that.  The 40-to-41 rule was unnecessary but
          # harmless in that context.  We need 42-to-43 for systems which 
          # took the overlay, since the code change which necessitated 
          # 40-to-41 had not happened yet.  For systems which did not take
          # that overlay, it will again be unnecessary but harmless.  These
          # files will be automatically regenerated wherever appropriate 
          # after this upgrade step.
          rm -rf /var/net-snmp/_usr_share_snmp_mibs_index
          rm -rf /var/net-snmp/mib_indexes
          ;;

    43_44)
          # Fix a few directory permissions that were too permissive
          if [ -d /var/opt/tms/output/libvirt/storage ]; then
              chmod 755 /var/opt/tms/output/libvirt/storage
          fi
          if [ -d /var/opt/tms/output/libvirt/storage/autostart ]; then
              chmod 755 /var/opt/tms/output/libvirt/storage/autostart
          fi
          ;;

    44_45)
          # Fix group and mode on /var/log/messages and all of its 
          # archives, to give readability to the new "diag" group.
          # There is no way to get a user into this group under capabilities;
          # it is solely used for ACLs.
          #
          # We reference the "diag" group by its number 3000, rather
          # than its name, because probably at the point this runs, mdinit
          # has not run yet, and so mgmtd has not had a chance to update
          # /etc/group to define the "diag" group.
          find -L /var/log -type f \( -name 'messages' -o -name 'messages.*.gz' \) | xargs -r chgrp -f 3000
          find -L /var/log -type f \( -name 'messages' -o -name 'messages.*.gz' \) | xargs -r chmod -f 0640
          ;;

    45_46)
          # Same as 44-to-45, but for a few other kinds of files.
          # Group 3000 (diag) is used again here; and we also use the
          # newly added group 3001 (stats) and group 3002 (virt) as well.

          find /var/opt/tms/snapshots -type d | xargs -r chmod -f 0775
          find /var/opt/tms/snapshots -type f | xargs -r chmod -f 0640

          chgrp -f -R 3000 /var/opt/tms/sysdumps
          find /var/opt/tms/sysdumps -type d | xargs -r chmod -f 0770
          find /var/opt/tms/sysdumps -type f | xargs -r chmod -f 0640

          chgrp -f -R 3001 /var/opt/tms/stats
          find /var/opt/tms/stats -type d | xargs -r chmod -f 0750
          find /var/opt/tms/stats -type f | xargs -r chmod -f 0640

          find /var/opt/tms/tcpdumps -type d | xargs -r chmod -f 0770
          # We could chmod the files, but it wouldn't do much good,
          # since tcpdump itself could write new files world-readable.
          # (Unless we set a umask in the CLI, and that would require
          # some thought.)  It doesn't matter anyway, since the directory
          # is not executable.

          # Note that virt_sync_pools.sh takes care of all the storage
          # pools, whether under /var/opt/tms/virt or not.  So this likely
          # has some (harmless) overlap with that.
          #
          # Note the use of '-L' to fix the virtual volumes in storage
          # pools which are located elsewhere and symlinked to from
          # /var/opt/tms/virt/pools.
          chgrp -f -R 3002 /var/opt/tms/virt
          find -L /var/opt/tms/virt -type d | xargs -r chmod -f 0770
          find -L /var/opt/tms/virt -type f | xargs -r chmod -f 0660

          ;;

    46_47)
          # Fix bug 14245.  Libvirtd will remake the contents of this
          # directory when it is launched.
          rm -rf /var/run/libvirt/*
          ;;

    47_48)
          # Obsoleted by 49-to-50 upgrade rule.
          ;;

    48_49)
          # Obsoleted by 49-to-50 upgrade rule.
          ;;

    49_50)
          # Blow away any previous tallynamelog, which may have the wrong
          # database schema, and re-create it.
          #
          # We and the base image now have responsibility for making this
          # file present with the right owner/group/mode, as the PAM module
          # will no longer voluntarily create it.
          rm -rf /var/log/tallynamelog
          touch /var/log/tallynamelog
          chown 0.0 /var/log/tallynamelog
          chmod 0600 /var/log/tallynamelog
          ;;

    50_51)
          # Empty usernames used to be accepted, but are no longer.
          #
          # Note that we need the redirect to /dev/null because if our
          # module or binary have not run, there will be no 'users' 
          # table at all, which produces an error message.  The DELETE
          # command does not seem to have an "IF EXISTS" option.
          /usr/bin/sqlite3 /var/log/tallynamelog "DELETE FROM users WHERE username = ''" > /dev/null 2>&1
          ;;

    51_52)
          mkdir -p /var/opt/tms/output/certs
          chown 0.0 /var/opt/tms/output/certs
          chmod 750 /var/opt/tms/output/certs
          ;;

    52_53)
          mkdir -p /var/opt/tms/output/cacerts
          chown 0.0 /var/opt/tms/output/cacerts
          chmod 750 /var/opt/tms/output/cacerts
          ;;

    53_54)
          # These are shell-only files, for developers, but we've made
          # changes we want developers to get by default, unless they've
          # modified the files we gave them in the first place.

          if file_in_hashlist_or_missing /var/home/root/.bash_profile "edf9dada104c119289be2b4102d7032deb1aeea7" ; then
              cp -pf /etc/skel-root/.bash_profile /var/home/root/
          fi
          if file_in_hashlist_or_missing /var/home/root/.bash_logout "091d38c25c8fb9b6fb272439a33895a6e440cdf0" ; then
              cp -pf /etc/skel-root/.bash_logout /var/home/root/
          fi
          if file_in_hashlist_or_missing /var/home/root/.gdbinit "b77b5006ecdb8c9475cb2996e271057fbb164320" ; then
              cp -pf /etc/skel-root/.gdbinit /var/home/root/
          fi
          ;;

    54_55)
          #
          # /var/opt/tms/images itself will be chmod'd by md_image during
          # initialization.  But we have to fix the image files, since
          # those are only set by the clients at download time.
          #
          # We also have to fix the owner of the directory.
          # To eliminate ordering problems (e.g. maybe /etc/group isn't
          # rewritten yet to define the 'diag' group), we use raw numbers.
          # 48 == apache; 3000 == diag (mgt_diag from md_client.h)
          #
          chown 48.3000 /var/opt/tms/images
          chmod 0644 /var/opt/tms/images/*
          ;;

    55_56)
          if [ ! -e /var/opt/tms/images/.temp ]; then
              mkdir /var/opt/tms/images/.temp
          fi
          chown 0.0 /var/opt/tms/images/.temp
          chmod 0700 /var/opt/tms/images/.temp
          ;;

    56_57)
          # 3000 == mgt_diag (from md_client.h)
          chgrp -f -R 3000 /var/opt/tms/tcpdumps
          ;;

    *)
          logger "Image upgrade failed: could not find action for version upgrade $versions"
          error=1
          ;;
esac
