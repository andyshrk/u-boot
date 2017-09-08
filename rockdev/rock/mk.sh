#!/bin/sh
DIR=$(dirname $0)
cat ${DIR}/out/tpl/u-boot-tpl.bin > tplspl.bin
truncate -s 1020 tplspl.bin
sed -i "/^/{1s/^/RK31/}"  tplspl.bin
cat ${DIR}/out/spl/u-boot-spl.bin > spl.bin
truncate -s %2048 spl.bin
cat spl.bin >> tplspl.bin
cp ${DIR}/out/u-boot.bin .
cp ${DIR}/3188_LPDDR2_300MHz_DDR3_300MHz_20130830.bin .
cp ${DIR}/rk30usbplug.bin .
${DIR}/boot_merger ${DIR}/RK310BMINIALL.ini
rm 3188_LPDDR2_300MHz_DDR3_300MHz_20130830.bin
rm rk30usbplug.bin 
