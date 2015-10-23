:
MY_DIR=`dirname ${0}`
VERBOSE=verbose
VERBOSE=quiet
if [ -f build_required.inc ] ; then
  THIS=build_required.inc
else
  echo No build_required.inc in the current dir.
  exit 1
fi
FROM=${THIS}.orig
TO=${THIS}.new
[ ! -f ${FROM} ] && cp -a ${THIS} ${FROM}

[ ! -d tmpdir ] && mkdir tmpdir
cat ${FROM} | grep '\.' | grep -v "(" | grep -v =  | grep -v '#' \
 | awk '{print $1}' | sort | uniq \
 | sed "/\$/s//.rpm/" \
 > tmpdir/rpms.full.list
wc -l tmpdir/rpms.full.list


# Now generate the files
#   ok.list
#   special.list
#   change.list
#   change-notfound.list
sh ${MY_DIR}/FIND_RPM_UPDATES.sh tmpdir `cat tmpdir/rpms.full.list`
wc -l tmpdir/change.list tmpdir/change-notfound.list tmpdir/ok.list tmpdir/special.list

# Now create an updated verison of ${THIS}
sh ${MY_DIR}/UPDATE_build_req.sh tmpdir ${FROM} ${TO} tmpdir/change.list tmpdir/change-notfound.list

ls -l tmpdir/${TO}
