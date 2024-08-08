#!/bin/sh

#check RA parse running

RA_ID_NEW=`ps -ef |grep "/buile/router" | grep " -N" | grep -v grep | sort -r -k 8 | awk '{print $2}' | xargs`
if [ "$RA_ID_NEW" = "" ]; then
   echo "router is starting now"
   ./build/router
fi




