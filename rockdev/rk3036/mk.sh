#!/bin/sh
DIR=$(dirname $0)
LOADER=u-boot.img

SPL_BIN=${DIR}/out/spl/u-boot-spl.bin
UBOOT_BIN=${DIR}/out/u-boot-dtb.bin


# add spl bin(with signature)
${DIR}/out/tools/mkimage -n rk3036 -T rksd -d ${SPL_BIN} ${LOADER}
# add loader
cat ${UBOOT_BIN} >> ${LOADER}

echo Done...
