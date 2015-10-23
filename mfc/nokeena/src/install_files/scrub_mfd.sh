:
# This script is run from /sbin/scrub.sh,
# via scrub_graft_1() and scrub_graft_2()
# which are dotted in from /etc/customer.sh
# (src/base_os/common/script_files/customer.sh)

# scrub.sh is run to clean a system to its default/initial state.
# So in this script we need to clean out all the MFD stuff.

# When called from scrub_graft_1() the command line param is "1",
# When this graft point is called the PM is running and all the
# normal MFD daemons are running.

# When called from scrub_graft_2() the command line param is "2",
# When this graft point is called the PM has already been stopped,
# and our daemons have been stopped as well.

