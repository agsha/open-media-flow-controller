:
MY_DIR=`dirname ${0}`
VERBOSE=verbose
VERBOSE=quiet
if [ -f source_pkgs.inc ] ; then
  THIS=source_pkgs.inc
else
  echo No source_pkgs.inc in the current dir.
  exit 1
fi
FROM=${THIS}.orig
TO=${THIS}.new
[ ! -f ${FROM} ] && cp -a ${THIS} ${FROM}

[ ! -d tmpdir ] && mkdir tmpdir
cat ${FROM} | grep '\.' | grep -v "(" | grep -v =  | grep -v '#' \
 | grep -v ^include \
 | awk '{print $1}' | sort | uniq \
 > tmpdir/rpms.full.list
wc -l tmpdir/rpms.full.list


# Now generate the files
#   special.list
#   change.list
#   change-notfound.list
sh ${MY_DIR}/FIND_SRPMS.sh tmpdir `cat tmpdir/rpms.full.list`
wc -l tmpdir/change.list tmpdir/change-notfound.list tmpdir/special.list

# Now create an updated verison of ${THIS}
sh ${MY_DIR}/UPDATE_source_pkgs.sh tmpdir ${FROM} ${TO} tmpdir/change.list tmpdir/change-notfound.list

ls -l tmpdir/${TO}
