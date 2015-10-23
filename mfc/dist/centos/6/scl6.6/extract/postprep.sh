:
PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH
echo +++++++++++++ =========== +++++++++++++++++++
echo +++++++++++++ postprep.sh +++++++++++++++++++
echo +++++++++++++ =========== +++++++++++++++++++
echo ${0} ${@}
USE_KERNEL_NAME=${1}
USE_LINUX_NAME=${2}
if [ ! -z "${USE_KERNEL_NAME}" ]; then
  if [ -z "${USE_LINUX_NAME}}" ]; then
    USE_LINUX_NAME=${USE_KERNEL_NAME}
  fi
fi

# Rename the kernel directory, only if forced
if [ ! -z "${USE_KERNEL_NAME}" ]; then
    kfn=`ls -d kernel-* | head -1`
    new_kfn=`echo kernel-${USE_KERNEL_NAME}`
    if [ -z "${kfn}" -o -z "${new_kfn}" ]; then
        echo ERROR: kfn or new_kfn is empty
        exit 1
    fi
    echo mv "${kfn}" "${new_kfn}"
    mv "${kfn}" "${new_kfn}"
    RV=${?}
    if [ ${RV} -ne 0 ] ; then
        echo ERROR: mv failed ${RV}
        exit 1
    fi
fi 

# Rename the linux directory inside it
cd kernel-2.6.*
rm -rf vanilla vanilla-*
lfn=`ls -d linux-* | head -1`
if [ -z "${USE_LINUX_NAME}}" ]; then
    new_lfn=`echo ${lfn} | sed 's/\(linux-.*\)\.[^.]*$/\1/'`
else
    new_lfn=`echo linux-${USE_LINUX_NAME}}`
fi
if [ -z "${lfn}" -o -z "${new_lfn}" ]; then
    exit 1
fi
echo mv "${lfn}" "${new_lfn}" 
mv "${lfn}" "${new_lfn}" || exit 1
RV=${?}
if [ ${RV} -ne 0 ] ; then
    echo ERROR: mv failed ${RV}
    exit 1
fi
echo postprep success
pwd
ls -l
exit 0
