:
MY_DIR=`dirname ${0}`
[ ! -d tmpdir ] && mkdir tmpdir
THIS=pkg_bin_check.txt
FROM=pkg_bin_check.txt.orig
TO=pkg_bin_check.txt.new
[ ! -f ${FROM} ] && cp -a ${THIS} ${FROM}

cat ${FROM} \
  | cut -f2 -d/ \
  | sort | uniq \
  > tmpdir/rpms.full.list

# Now generate the files
#   ok.list
#   special.list
#   change.list
#   change-notfound.list
sh ${MY_DIR}/FIND_RPM_UPDATES.sh tmpdir `cat tmpdir/rpms.full.list`
wc -l tmpdir/change.list tmpdir/change-notfound.list tmpdir/ok.list tmpdir/special.list

# Now create an updated verison of the Makefile
sh ${MY_DIR}/UPDATE_pkg_bin_check.sh ${TO} tmpdir/change.list

ls -l ${TO}
wc -l ${TO}
