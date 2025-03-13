#!/bin/sh

./vm.sh -s -n 2 -p 9994 >/dev/null &
sleep 2
./vm.sh -n 1 -p 9994 >/dev/null &
sleep 2
./vm.sh -n 1 -p 9995 >/dev/null &
