#!/bin/sh
DIR=$(dirname $0)
LOADER=rk3288_bootloader_v1.00.06.bin
upgrade_tool db ${DIR}/$LOADER
upgrade_tool wl 0x40 u-boot.img
upgrade_tool RD 
