#!/bin/bash

clear

echo "Getting available ports: "
PORT=$(./port-for-user.pl)
PORT2=$(($PORT+1))

echo "Setting up proxy on $PORT"
./proxy $PORT &

# echo "Setting up tiny server on $PORT2"
# ./tiny/tiny $PORT2 &
# echo ""

echo "Curl request to localhost:$PORT2:"
curl -v --proxy http://localhost:$PORT http://www.beej.us/guide/bgnet/html/single/bgnet.html
echo ""

# echo "Killing proxy and tiny server"
# pgrep -f tiny | $(awk '{print "kill " $1}')
pgrep -f proxy | $(awk '{print "kill " $1}')

