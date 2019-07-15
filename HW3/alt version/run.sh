#!/bin/bash
 
make clean
make
gnome-terminal -e "./tsd -p 3010 -r Z -h localhost"
sleep 2
gnome-terminal -e "./tsd -p 3020 -r S -h localhost"
gnome-terminal -e "./tsd -p 3030 -r S -h localhost"
gnome-terminal -e "./tsd -p 3040 -r S -h localhost"

#./tsd -p 3010 -r Z
#./tsd -p 3020 -r S
#./tsd -p 3030 -r S
#./tsd -p 3040 -r S

echo "Server is ready to connect"

gnome-terminal -e "./tsc -u u1"
gnome-terminal -e "./tsc -u u2"

