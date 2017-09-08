#!/bin/sh
DIR=$(dirname $0)
LOADER=RV1108_usb_boot_V1.22_ddr400.bin
rkdeveloptool db ${DIR}/$LOADER
sleep 3
rkdeveloptool wl 0x40 u-boot.img
rkdeveloptool RD
