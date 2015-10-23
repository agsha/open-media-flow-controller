#!/bin/awk -f

#
# Following script calculates the total memory and then calculate used percent
# age. if percentage is more than 95%, then freeup the page, inodes caches
#

# get total memory
/^MemTotal:/ { total = $2}

# get total free memory
/^MemFree:/  { free = $2 }

# get the threshold from user
/^Threshold:/ {threshold = $2 }

END {
# calculate used %age, if more than 95, free caches

  if (system("test -r /var/run/drop_cache.lck") == 0 ) {
# file exist, make a exit
    exit 0;
  }

  percent=(total- free)*100/total;
  if ( percent >= (100 - threshold) )
  {
# create lock file to avoid running multiple instances
    system("touch /var/run/drop_cache.lck")
#issue sync command to write all direty pages into the disk, this will help
#freeing more buffers.
    system("sync");
    system("sleep 1");
    system("echo 2 > /proc/sys/vm/drop_caches");
    system("sleep 10");
# free the caches
    system("echo 3 > /proc/sys/vm/drop_caches");
    system("logger \"cleared page caches and inodes, dir entry caches\"");
#remove the lock file
    system("rm /var/run/drop_cache.lck")
  }
}

