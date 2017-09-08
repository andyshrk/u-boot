#!/bin/sh
DIR=$(dirname $0)
LOADER=PX5MiniLoaderAll_V2.54.bin
upgrade_tool UL ${DIR}/$LOADER
upgrade_tool di uboot u-boot.img
upgrade_tool RD 
