#!/bin/sh

set -o verbose
mkdir -p output
cd snes9x/sdl
./buildhtml.sh
cd ../..
