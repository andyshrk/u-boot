#!/bin/sh
DIR=$(dirname $0)
./${DIR}/out/tools/mkimage -n rk3288 -T rksd -d ./${DIR}/out/spl/u-boot-spl.bin  u-boot.img
cat ./${DIR}/out/u-boot.bin >> u-boot.img 
