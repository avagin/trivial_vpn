#!/bin/sh

set -e
touch A
touch B
unshare -n mount --bind /proc/self/ns/net A
unshare -n mount --bind /proc/self/ns/net B
./tvpn 127.0.0.1 32000 32001 A vpc0 &
./tvpn 127.0.0.1 32001 32000 B vpc0 &
nsenter --net=A ip link set up dev lo
nsenter --net=A ip link set up dev vpc0
nsenter --net=A ip addr add 192.168.5.1/24 dev vpc0

nsenter --net=B ip link set up dev lo
nsenter --net=B ip link set up dev vpc0
nsenter --net=B ip addr add 192.168.5.2/24 dev vpc0

nsenter --net=B ip a
nsenter --net=A ip a
nsenter --net=B ping -c 10 192.168.5.1
