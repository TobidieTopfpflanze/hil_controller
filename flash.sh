#!/bin/bash

set -e 

picotool reboot -f -u
sleep 3
echo "Flash"
picotool load "$(ls ./build/*.uf2)" -f -x