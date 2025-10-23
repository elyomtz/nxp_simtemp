#!/bin/bash

set -euo pipefail

if ! cd ../build/demo; then
	echo "Execute build_demo.sh first" >&2
	exit 1
fi	

./simtemp --help
sleep 1
./simtemp load
sleep 1
./simtemp sampling 1000
sleep 1
./simtemp ltemp 5000
sleep 1
sudo ./simtemp run
sleep 1
./simtemp stats
sleep 1 
./simtemp unload
