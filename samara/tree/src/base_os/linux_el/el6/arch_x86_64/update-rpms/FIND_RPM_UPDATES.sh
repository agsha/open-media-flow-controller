:
VERBOSE=verbose
VERBOSE=quiet
TO_DIR=tmpdir
if [ ! -z ${1} ] ; then
  if [ -d ${1} ] ; then
    TO_DIR=${1}
    shift
  fi
fi
if [ ! -z ${1} ] ; then
  rm -f ${TO_DIR}/rpms.full.list
  for i in ${*}
  do basename ${i} >> ${TO_DIR}/rpms.full.list
  done
fi

rm -f ${TO_DIR}/rpms.x86_64.list ; touch ${TO_DIR}/rpms.x86_64.list
rm -f ${TO_DIR}/rpms.i686.list ; touch ${TO_DIR}/rpms.i686.list
rm -f ${TO_DIR}/rpms.noarch.list ; touch ${TO_DIR}/rpms.noarch.list
rm -f ${TO_DIR}/rpms.other.list ; touch ${TO_DIR}/rpms.other.list

for ITEM in `cat ${TO_DIR}/rpms.full.list`
do
  case ${ITEM} in
  *.x86_64.rpm) TYPE=x86_64 ;;
  *.i686.rpm)   TYPE=i686 ;;
  *.noarch.rpm) TYPE=noarch ;;
  *)            TYPE=other ;;
  esac
  case ${ITEM} in
  compat-libstdc++-33-*)
    echo ${ITEM} | sed "/compat-libstdc++-33-[0-9].*/s//compat-libstdc++-33/" >> ${TO_DIR}/rpms.${TYPE}.list
    ;;
  compat-libstdc++-296-*)
    echo ${ITEM} | sed "/compat-libstdc++-296-[0-9].*/s//compat-libstdc++-296/" >> ${TO_DIR}/rpms.${TYPE}.list
    ;;
  java-*-openjdk-devel-*)
    echo ${ITEM} | sed "/\(java-.*-openjdk-devel\)-[0-9].*/s//\1/" >> ${TO_DIR}/rpms.${TYPE}.list
    ;;
  java-*-openjdk-*)
    echo ${ITEM} | sed "/\(java-.*-openjdk\)-[0-9].*/s//\1/" >> ${TO_DIR}/rpms.${TYPE}.list
    ;;
  *)
    echo ${ITEM} | sed "/-[0-9].*/s///" >> ${TO_DIR}/rpms.${TYPE}.list
    ;;
  esac
done
wc -l ${TO_DIR}/rpms.x86_64.list ${TO_DIR}/rpms.i686.list ${TO_DIR}/rpms.noarch.list ${TO_DIR}/rpms.other.list

rpm -qa > ${TO_DIR}/installed-rpms.list

DIST_SCL_6=/volume/ssd-linux-storage01/repo/ssdlinux/scl6/release/6.6.R1.0
DIST_SCL_6_I386=${DIST_SCL_6}/i386/rpm
DIST_SCL_6_X86_64=${DIST_SCL_6}/x86_64/rpm
DIST_SCL_6_SRPMS=${DIST_SCL_6}/srpm
DIST_SCL_6_UPDATE1=${DIST_SCL_6}/SRs/6.6.R1.1
DIST_SCL_6_UPDATE1_I386=${DIST_SCL_6_UPDATE1}/i386/rpm
DIST_SCL_6_UPDATE1_X86_64=${DIST_SCL_6_UPDATE1}/x86_64/rpm
DIST_SCL_6_UPDATE1_SRPMS=${DIST_SCL_6_UPDATE1}/srpm
DIST_SCL_6_UPDATE2=${DIST_SCL_6}/SRs/6.6.R1.2
DIST_SCL_6_UPDATE2_I386=${DIST_SCL_6_UPDATE2}/i386/rpm
DIST_SCL_6_UPDATE2_X86_64=${DIST_SCL_6_UPDATE2}/x86_64/rpm
DIST_SCL_6_UPDATE2_SRPMS=${DIST_SCL_6_UPDATE2}/srpm
DIST_SCL_6_UPDATE3=${DIST_SCL_6}/SRs/6.6.R1.3
DIST_SCL_6_UPDATE3_I386=${DIST_SCL_6_UPDATE3}/i386/rpm
DIST_SCL_6_UPDATE3_X86_64=${DIST_SCL_6_UPDATE3}/x86_64/rpm
DIST_SCL_6_UPDATE3_SRPMS=${DIST_SCL_6_UPDATE3}/srpm
for L in DIST_SCL_6_I386 DIST_SCL_6_X86_64 DIST_SCL_6_UPDATE1_I386 DIST_SCL_6_UPDATE1_X86_64 DIST_SCL_6_UPDATE2_I386 DIST_SCL_6_UPDATE2_X86_64 DIST_SCL_6_UPDATE3_I386 DIST_SCL_6_UPDATE3_X86_64
do
  eval LOC=\$${L}
  [ ! -d ${LOC} ] && continue
  find ${LOC} -type f -name '*.rpm' > ${TO_DIR}/rpms.${L}.list
done

# Find where the exact RPM file is located in the repo.
Find_RPM_File()
{
  THIS=${1}
  export V=n
  export SPECIAL=not
  case ${THIS} in 
  gdb-gdbserver-*) SPECIAL=special ;;
  gdb-*)           SPECIAL=special ;;
  ipsec-tools-*)   SPECIAL=special ;;
  openswan-*)      SPECIAL=special ;;
  esac
  if [ ${SPECIAL} = "special" ] ; then
    echo ${1} exact >> ${TO_DIR}/special.list
    continue
  fi
  [ ${V} = "v" ] && echo Find_RPM_File ${THIS}
  LOCATION=notfound
  for L in DIST_SCL_6_I386 DIST_SCL_6_X86_64 DIST_SCL_6_UPDATE1_I386 DIST_SCL_6_UPDATE1_X86_64 DIST_SCL_6_UPDATE2_I386 DIST_SCL_6_UPDATE2_X86_64 DIST_SCL_6_UPDATE3_I386 DIST_SCL_6_UPDATE3_X86_64
  do
    eval LOC=\$${L}
    [ ${V} = "v" ] && echo check ${TO_DIR}/rpms.${L}.list
    FULLPATH=`grep /${THIS}.rpm ${TO_DIR}/rpms.${L}.list`
    [ -z "${FULLPATH}" ] && continue
    [ ${V} = "v" ] && echo Found ${FULLPATH} 
    LOCATION=${L}
    break
  done
}

# Search the rpm files for the newest instance of a base rpm name
Search_RPMs()
{
  THIS=${1}
  TYPE=${2}
  export V=n
  export SPECIAL=not
  case ${THIS} in 
  gdb-gdbserver) SPECIAL=special ;;
  gdb)           SPECIAL=special ;;
  ipsec-tools)   SPECIAL=special ;;
  openswan)      SPECIAL=special ;;
  esac
  [ ${V} = "v" ] && echo Search_RPMs ${THIS} ${TYPE}
  [ ${V} = "v" ] && set -x
  if [ ${SPECIAL} = "special" ] ; then
    echo ${1} ${2} >> ${TO_DIR}/special.list
    continue
  fi

  LOCATION=notfound
  for L in DIST_SCL_6_UPDATE3_X86_64 DIST_SCL_6_UPDATE3_I386 DIST_SCL_6_UPDATE2_X86_64 DIST_SCL_6_UPDATE2_I386 DIST_SCL_6_UPDATE1_X86_64 DIST_SCL_6_UPDATE1_I386 DIST_SCL_6_X86_64 DIST_SCL_6_I386
  do
    eval LOC=\$${L}
    [ ${V} = "v" ] && echo check ${TO_DIR}/rpms.${L}.list
    [ ! -f ${TO_DIR}/rpms.${L}.list ] && continue
    if [ ${TYPE} = "other" ] ; then
      FULLPATH=`grep "/${THIS}-[0-9]" ${TO_DIR}/rpms.${L}.list`
    else
      FULLPATH=`grep "/${THIS}-[0-9]" ${TO_DIR}/rpms.${L}.list | grep .${TYPE}.rpm`
    fi
    [ -z "${FULLPATH}" ] && continue
    echo ${FULLPATH} | grep " " > /dev/null
    if [ $? -eq 0 ] ; then
      echo WARNING: Found two of ${THIS} ${TYPE} in ${TO_DIR}/rpms.${L}.list
    fi
    [ ${V} = "v" ] && echo Found ${FULLPATH} 
    LOCATION=${L}
    break
  done
  case ${THIS} in 
  compat-libstdc++*) set nolist ;;
  esac
  [ ${V} = "v" ] && set +x
}

rm -f ${TO_DIR}/ok.list ;              touch ${TO_DIR}/ok.list
rm -f ${TO_DIR}/change.list ;          touch ${TO_DIR}/change.list
rm -f ${TO_DIR}/change-notfound.list ; touch ${TO_DIR}/change-notfound.list
rm -f ${TO_DIR}/special.list ;         touch ${TO_DIR}/special.list
for TYPE in x86_64 i686 noarch other
do
  for RPM in `cat ${TO_DIR}/rpms.${TYPE}.list`
  do
    [ -z "${RPM}" ] && continue
    if [ ${TYPE} = "other" ] ; then
      INST=`grep "^${RPM}-[0-9]" ${TO_DIR}/installed-rpms.list`
      FULL=`grep "^${RPM}-[0-9]" ${TO_DIR}/rpms.full.list`
    else
      INST=`grep "^${RPM}-[0-9]" ${TO_DIR}/installed-rpms.list | grep .${TYPE}`
      FULL=`grep "^${RPM}-[0-9]" ${TO_DIR}/rpms.full.list | grep .${TYPE}`
    fi
    [ ${VERBOSE} = "verbose" ] && \
      echo == RPM=${RPM} F=${FULL} T=${TYPE}
    if [ -z "${FULL}" ] ; then
      echo Error, Did not find FULL: ${RPM} ${TYPE} in ${TO_DIR}/rpms.full.list
      continue
    fi
    [ ${VERBOSE} = "verbose" ] && \
      echo ==== I=${INST}
    if [ -z "${INST}" ] ; then
      #echo ${RPM} ${TYPE} ${FULL} >> ${TO_DIR}/notinstalled.list
      Search_RPMs ${RPM} ${TYPE}
      IN=notinstalled
    elif [ "${FULL}" != "${INST}" ] ; then
      Find_RPM_File ${INST}
      IN=installed
    else
      echo ${RPM} ${TYPE} ${FULL} >> ${TO_DIR}/ok.list
      coninue
    fi
    if [ ${LOCATION} = "notfound" ] ; then
      echo ${RPM} ${TYPE} ${FULL} ${IN} DIST_UNKNOWN /unknown/${RPM}-0.UNKNOWN.${TYPE}.rpm >> ${TO_DIR}/change-notfound.list
    else
      if [ -z "${FULLPATH}" ] ; then
        echo Error, FULLPATH is null
        echo ${IN} ${RPM} ${TYPE} ${FULL}
        exit 1
      fi
      echo ${RPM} ${TYPE} ${FULL} ${IN} ${LOCATION} ${FULLPATH} >> ${TO_DIR}/change.list
    fi
  done
done
[ ${VERBOSE} = "verbose" ] && \
  echo ===
