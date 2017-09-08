#!/bin/sh
DIR=$(dirname $0)
LOADER=rk3368_loader_v2.01.260.bin
upgrade_tool UL ${DIR}/$LOADER
upgrade_tool di uboot u-boot.img
upgrade_tool RD 
