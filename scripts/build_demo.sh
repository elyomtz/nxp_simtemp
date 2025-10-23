#!/bin/bash

set -euo pipefail

COMPILER="g++"
SOURCE_APP="../user/cli/main.cpp"
CFLAGS="-Wall"
CLI="simtemp"
OUTPUT="../build/demo"
KO_FILE="../kernel/nxp_simtemp.ko"

LKM_MAKEFILE_DIR="../kernel"

#Create the .ko file
make -C "$LKM_MAKEFILE_DIR" sim_enabled

#Compile the CLI
"$COMPILER" -D DEMO "$CFLAGS" "$SOURCE_APP" -o "$CLI"

mkdir -p "$OUTPUT" 

mv "$CLI" "$OUTPUT/"

mv "$KO_FILE" "$OUTPUT/"

make -C "$LKM_MAKEFILE_DIR" clean

echo "Output folder is ready to be used for demo script"
