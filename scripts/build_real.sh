#!/bin/bash

set -euo pipefail

COMPILER="g++"
SOURCE_APP="../user/cli/main.cpp"
CFLAGS="-Wall"
CLI="simtemp"
OUTPUT="../build/real"
KO_FILE="../kernel/nxp_simtemp.ko"
DT_OUT="../build/real/nxp_simtemp.dtbo"
DT_IN="../kernel/dts/nxp_simtemp.dts"

LKM_MAKEFILE_DIR="../kernel"

mkdir -p "$OUTPUT" 

#Create the .ko file
make -C "$LKM_MAKEFILE_DIR" build

echo "Kernel module created successfully"

#Compile the CLI
"$COMPILER" -D REAL "$CFLAGS" "$SOURCE_APP" -o "$CLI"

echo "CLI created successfully"

#Create the dtbo file
dtc -@ -I dts -O dtb -o "$DT_OUT" "$DT_IN"

echo "dtbo file created successfully"

mv "$CLI" "$OUTPUT/"

mv "$KO_FILE" "$OUTPUT/"

echo "Output folder is ready"

make -C "$LKM_MAKEFILE_DIR" clean

echo "Clean done"
