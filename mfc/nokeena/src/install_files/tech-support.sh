#! /bin/bash
# 
# Tech -support script to grab logs from varius modules
#
# Nokeena Networks Inc, 
# (C) All rights reserved, 2008-2009
#
#


# This file must exist
if [ -f /config/nkn/tech-support.cfg ] ; then
    . /config/nkn/tech-support.cfg
else
    exit 1
fi

log_count=${#support[@]}

tech_supp_checkconfig() {
    local i a n name

    for (( i=0; i< $log_count; i++ ))
    do
        name=${support[$i]}
        a="support_${name}[@]"
        a=( "${!a}" )
        n=${#a[@]}

        if [ $n -le 1 ]
        then 
            #echo "too few arguments for support_$name (${a[*]}"
            return 1
        fi
    done
}


tech_supp_process() {
    local i a n name

    #STAGE_DIR=/home/dhruva/samara/trunk/nokeena/src/install_files
    # make sure our sysdump directory is available
    if [[ ! -d "${STAGE_DIR}/module-logs" ]]
    then
        mkdir "${STAGE_DIR}/module-logs"
    fi

    for (( i=0; i< $log_count; i++ ))
    do
        name=${support[$i]}
        a="support_${name}[@]"
        a=( "${!a}" )


        local file script args
        arg_count=${#a[@]}
        for (( j=0; j<$arg_count; j++ ))
        do
            # get $file
            file1=`echo "${a[$j]}" | grep "^file=*"`
            if [ ! -z "$file1" ]
	    then
                file=${a[$j]#file=}
            fi

            # get the script to run
            script1=`echo "${a[$j]}" | grep "^script=*"`
            if [ ! -z "$script1" ]
            then 
                script=${a[$j]#script=}
            fi

            # Grab any additional args to pass to the script
            args1=`echo "${a[$j]}" | grep "^args=*"`
            if [ ! -z "$args1" ]
            then 
                args=${a[$j]#args=}
            fi
        done
        if [[ -f $script ]]
        then
            $script $args 
            ret=$?
            if [[ "$ret" -eq "0" ]]
            then
                mv $file ${STAGE_DIR}/module-logs
            fi
        fi
    done
}


#checkconfig || exit 1
#process || exit 1
