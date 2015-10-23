# This script is run from firstboot.sh to perform customer-specific 
# upgrades to the /var partition.  If you add a new rule here, make
# sure to increment the version number in 
# customer/<product>/src/base_os/common/image_files/var_version_<customer>.sh.
# NOTE that you use your customer name (defined in customer.mk as
# PROD_CUSTOMER) rather than the product name to form the filename.
#
# This script takes one parameter, a string in the form
# "<old_version>_<new_version>".  The new version number must
# be exactly one higher than the old version; thus if multiple 
# upgrades are required, the script must be called multiple times.
# For example, to do an upgrade from version 2 to version 4, the
# script is called twice, once with "2_3" and once with "3_4".
# For an example, see src/base_os/common/script_files/var_upgrade.sh

versions=$1

case "$versions" in
    1_2)
          logger "Image upgrade $versions"
          ls -l > /var/tmp/ls.out
          df -h > /var/tmp/df.out
          ;;
    *)
          logger "Image upgrade failed: could not find action for version upgrade $versions"
          error=1
          ;;
esac
