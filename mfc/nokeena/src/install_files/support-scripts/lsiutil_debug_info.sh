#! /bin/bash
# This script provides information about LSI controllers available on the
# system PR 928801.

LSPCI="lspci"
LSIUTIL="lsiutil"

# Check if the system has LSI controller on it
has_lsi()
{
        TMP=`"${LSPCI}" | grep LSI`
        if [ $? == 0 ]; then
                echo "1"
        else
                echo "0"
        fi
}

# Execute lsiutil command with options on exh controller and send the
# output to a temporary file
execute_lsiutil()
{
	local saved_output="/nkn/tmp/lsiutil_output.txt"
        if [ $( has_lsi ) == "1" ]; then
                NO_CNTRLR=`"${LSIUTIL}" -i -s | grep "MPT Ports* found" | awk '{print $1}'`
                if [ "$NO_CNTRLR" -eq 1 ]; then
                        VAL=`lsiutil -e -p 1 1 16 35 40 42 47 50 51 59 66 67 68 69 70 71 >> ${saved_output}`
                elif [ "$NO_CNTRLR" -gt 1 ]; then
                        CNTR=1
                        while [ $CNTR -le $NO_CNTRLR ]; do
                                echo "Information about Port $CNTR" >> ${saved_output}
                                VAL=`lsiutil -e -p ${CNTR} 1 16 35 40 42 47 50 51 59 66 67 68 69 70 71 >> ${saved_output}`
                                echo "============================================================================" >> ${saved_output}
                                CNTR=$((CNTR+1))
                        done
                else
                        echo "No MPT ports found" >> ${saved_output}
                fi
        fi
}

execute_lsiutil
