#:

BASE_RPMS_DIR=/volume/ssd-linux-storage01/repo/ssdlinux/scl6/release/6.6.R1.0/
UPDATES_RPMS_DIR=/volume/ssd-linux-storage01/repo/ssdlinux/scl6/release/6.6.R1.0/SRs/6.6.R1.1
MAKEFILE_BASE_PREFIX='"\t${DIST_EL_EL6_SRPMS}/"'
MAKEFILE_UPDATES_PREFIX='"\t${DIST_EL_EL6_UPDATES_SRPMS}/"'

rm -f /tmp/pl-os.txt /tmp/pl-oss.txt
find ${BASE_RPMS_DIR} -type f -name '*.rpm' | xargs -I'{}' -r -n 1 rpm --queryformat '%{SOURCERPM}\n' -qp '{}' 2>/dev/null | sort | uniq | grep '\.src\.rpm$' | awk '{printf '${MAKEFILE_BASE_PREFIX}' $1 " \\\n" }' > /tmp/pl-os.txt
#find ${UPDATES_RPMS_DIR} -type f -name '*.rpm' | xargs -I'{}' -r -n 1 rpm --queryformat '%{SOURCERPM}\n' -qp '{}' 2>/dev/null | sort | uniq | grep '\.src\.rpm$' | awk '{printf '${MAKEFILE_UPDATES_PREFIX}' $1 " \\\n" }' >> /tmp/pl-os.txt

orig_name=source_pkgs.inc
temp_list=/tmp/source_pkgs.inc.$$

cat /tmp/pl-os.txt | sort -t/ -k2 > ${temp_list}

echo "Output in $temp_list"

##echo "Diff to current: "
##diff ${orig_name} ${temp_list} && rm -f ${temp_list}
