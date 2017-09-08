#!/bin/sh
DIR=$(dirname $0)
LOADER=rk3399_loader_v1.08.109.bin
upgrade_tool UL ${DIR}/$LOADER
upgrade_tool di uboot u-boot.img
upgrade_tool RD 
