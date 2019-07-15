#!/bin/bash
 
#make clean
#make
./tsd -p 3010 -h $(ifconfig enp0s3 | grep 'inet addr:' | cut -d: -f2 | awk '{print $1}')