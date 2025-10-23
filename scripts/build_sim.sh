#!/bin/bash

set -euo pipefail

COMPILER="g++"
SOURCE_APP="../user/cli/main.cpp"
CFLAGS="-Wall"
CLI="simtemp"
OUTPUT="../build/sim"
KO_FILE="../kernel/nxp_simtemp.ko"

LKM_MAKEFILE_DIR="../kernel"

#Create the .ko file
make -C "$LKM_MAKEFILE_DIR" sim_enabled

echo "Kernel module created successfully"

#Compile the CLI
"$COMPILER" -D SIM "$CFLAGS" "$SOURCE_APP" -o "$CLI"

echo "CLI created successfully"

mkdir -p "$OUTPUT" 

mv "$CLI" "$OUTPUT/"

mv "$KO_FILE" "$OUTPUT/"

echo "Output folder is ready"

make -C "$LKM_MAKEFILE_DIR" clean

echo "Clean done"
