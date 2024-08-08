#!/bin/sh

#check RA parse running


    RA_ID=`ps -ef |grep "./build/router" | grep -v grep | sort -r -k 8 | awk '{print $2}' | xargs`

if [ "$RA_ID" = "" ]; then
    echo "router is not existed!"
    exit
else
    echo "Stopping RA parse [ $RA_ID ]......"
    `kill -9 $RA_ID`
fi


