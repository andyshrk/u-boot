/* SPDX-License-Identifier: GPL-2.0+ */
/*
 */

#ifndef __ROCK5B_RK3588_H
#define __ROCK5B_RK3588_H

#define ROCKCHIP_DEVICE_SETTINGS \
		"stdout=serial,vidconsole\0" \
		"stderr=serial,vidconsole\0"

#include <configs/rk3588_common.h>

#undef CFG_EXTRA_ENV_SETTINGS

/*
 * As a laptop, this is no sdmmc, and
 * want to set usb the highest boot priority
 * for os installation.
 */
#define CFG_EXTRA_ENV_SETTINGS \
	"fdtfile=" CONFIG_DEFAULT_FDT_FILE "\0" \
	"partitions=" PARTS_DEFAULT		\
	ENV_MEM_LAYOUT_SETTINGS			\
	ROCKCHIP_DEVICE_SETTINGS \
	"boot_targets=" "usb mmc0" "\0"

#endif /* __GENBOOK_RK3588_H */
