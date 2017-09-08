#!/bin/sh
DIR=$(dirname $0)
${DIR}/loaderimage --pack --uboot ${DIR}/out/u-boot.bin u-boot.img
