#!/bin/sh
DIR=$(dirname $0)
rkdeveloptool db ${DIR}/rk3036_loader_v1.07.237.bin
sleep 3
rkdeveloptool wl 64 u-boot.img
rkdeveloptool rd
