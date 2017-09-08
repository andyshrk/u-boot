#!/bin/sh
DIR=$(dirname $0)
${DIR}/out/tools/mkimage  -n rv1108 -T rksd -d ${DIR}/ddr.bin spl.bin
cat spl.bin ${DIR}/out/u-boot.bin > u-boot.img
