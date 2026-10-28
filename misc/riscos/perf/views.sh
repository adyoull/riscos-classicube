#!/bin/bash
# cmpview.sh from five viewpoints: views.sh OUT_A OUT_B
HERE=$(cd "$(dirname "$0")" && pwd)
"$HERE/cmpview.sh" spawn        "$1" "$2"
"$HERE/cmpview.sh" underground  "$1" "$2" CC_POS=64,12,64,0
"$HERE/cmpview.sh" surface-down "$1" "$2" CC_POS=20,45,20,35
"$HERE/cmpview.sh" above-map    "$1" "$2" CC_POS=64,90,64,60
"$HERE/cmpview.sh" cave         "$1" "$2" CC_POS=cave
