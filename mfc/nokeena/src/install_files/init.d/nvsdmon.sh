#!/bin/bash
wait=1 # wait time in seconds
maxerr=3
errcnt=$maxerr # start in error to disable bgp on startup (reboot)
vtysh=/usr/local/bin/vtysh
debug=false

timestamp=0

# Script objectives:
# 1.  Insure nvsd is updating counters
# 2.  Insure preread is not occuring
# 3.  Disable/enable BGP neighbors as needed
# 4.  Perform the function with a minimum of overhead per check
# 5.  Provide ~3 second response time on an issue

while true; do
  disks=false
  error=false
  message="No disk tier found" 

  # while loop to check counters from nkncnt
  # note:  If no counters are found, no disk are found, and this is an error
  # if any are in preread, error
  # if the timestamp is not incrementing, error

  while read counter junk value junk; do
	case "$counter" in
	 "SATA.dm2_preread_done" | "SAS.dm2_preread_done" | "SSD.dm2_preread_done")
	        if [ "$debug" = "true" ]; then
	                logger "nvsdmon: $counter $value"
	        fi
		disks=true
		if [ $value = 0 ]; then
			error=true
			message="Preread in progress: $counter"
		fi;;
	 "glob_nkn_cur_ts")
                if [ "$debug" = "true" ]; then
                        logger "nvsdmon: $counter $value"
                fi
		if [ ! $value -gt $timestamp ]; then
			error=true
			message="nvsd timestamp not incrementing: $timestamp"
			break
		fi
		timestamp=$value;;
	esac

  done < <( /opt/nkn/bin/nkncnt -t0 -s dm2_preread_done -s glob_nkn_cur_ts )

  if [ "$disks" = "false" -o "$error" = "true" ]; then
	errcnt=$((errcnt+1))
	# the longer in error (within reason), the longer to stay in error
	if [ $errcnt -gt $(($maxerr*10)) ]; then
        	errcnt=$(($maxerr*10))
  	fi
  else
	if [ $errcnt -gt 0 ]; then
		errcnt=$((errcnt-1))
	fi
  fi

  # get the current bgp status, save it for reference, used for both good/bad cases
  $vtysh -c "show ip bgp summary" |while read ip two as four five six seven eight nine status; do
	neighbor=$(expr "$ip $as $status" : '\(^[0-9].*\)')
	if [ "${neighbor:-0}" != "0" ]; then
	   echo $neighbor
	fi
  done > /tmp/bgpstatus.txt
  if [ "debug" = "true" ]; then
	logger "nvsdmon: errcnt=$errcnt"
  fi

  # if the maxerr count has been reached, disable bgp neighbors (only) if currently enabled
  if [ $errcnt -ge $maxerr ]; then
	# test if already failed--prevents log spamming
	if [ ! -f /tmp/nvsd-fail ]; then
		logger "nvsdmon fail: $message"
		touch /tmp/nvsd-fail # can use this to trigger behavior in other scripts
	fi
        while read ip as status; do
		if [ ! "${status}" = "Idle (Admin)" ]; then
                	logger "nvsdmon: disabling bgp neighbor $ip $as: $message"
			$vtysh -c "conf t" -c "router bgp $as" -c "neighbor $ip shutdown"
		fi
          done < /tmp/bgpstatus.txt
  else
	if [ -f /tmp/nvsd-fail ]; then
		logger "nvsdmon pass"
		rm /tmp/nvsd-fail 2> /dev/null
	fi
        # Things are good (enough) check if all BGP neighbors are enabled, enable if not
	  while read ip as status; do
            if [ "${status}" = "Idle (Admin)" ]; then
                logger "nvsdmon: enabling bgp neighbor $ip $as"
                $vtysh -c "conf t" -c "router bgp $as" -c "no neighbor $ip shutdown"
            fi  
          done < /tmp/bgpstatus.txt
  fi
sleep $wait
done


