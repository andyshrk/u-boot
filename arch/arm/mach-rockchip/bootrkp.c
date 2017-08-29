/*
 * (C) Copyright 2017 Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <config.h>
#include <linux/list.h>
#include <malloc.h>
#include <mmc.h>
#include "resource_img.h"

#define RK_BLK_OFFSET			0x2000
#define TAG_KERNEL			0x4C4E524B
#define MAX_PARAM_SIZE			(1024 * 64)
#define PART_NAME_SIZE			32

#define PART_MISC			"misc"
#define PART_KERNEL			"kernel"
#define PART_BOOT			"boot"
#define PART_RECOVERY			"recovery"
#define PART_RESOURCE			"resource"

#define DTB_FILE			"rk-kernel.dtb"

#define BOOTLOADER_MESSAGE_OFFSET_IN_MISC	(16 * 1024)
#define BOOTLOADER_MESSAGE_BLK_OFFSET		(BOOTLOADER_MESSAGE_OFFSET_IN_MISC >> 9)

enum android_boot_mode {
	ANDROID_BOOT_MODE_NORMAL = 0,

	/* "recovery" mode is triggered by the "reboot recovery" command or
	 * equivalent adb/fastboot command. It can also be triggered by writing
	 * "boot-recovery" in the BCB message. This mode should boot the
	 * recovery kernel.
	 */
	ANDROID_BOOT_MODE_RECOVERY,

	/* "bootloader" mode is triggered by the "reboot bootloader" command or
	 * equivalent adb/fastboot command. It can also be triggered by writing
	 * "bootonce-bootloader" in the BCB message. This mode should boot into
	 * fastboot.
	 */
	ANDROID_BOOT_MODE_BOOTLOADER,
};

struct bootloader_message {
	char command[32];
	char status[32];
	char recovery[768];

	// The 'recovery' field used to be 1024 bytes.  It has only ever
	// been used to store the recovery command line, so 768 bytes
	// should be plenty.  We carve off the last 256 bytes to store the
	// stage string (for multistage packages) and possible future
	// expansion.
	char stage[32];
	char slot_suffix[32];
	char reserved[192];
};

struct rockchip_param {
	u32 tag;
	u32 length;
	char params[1];
	u32 crc;
};

struct blk_part {
	char name[PART_NAME_SIZE];
	unsigned long from;
	unsigned long size;
	struct list_head node;
};

struct rockchip_image {
	uint32_t tag;
	uint32_t size;
	int8_t image[1];
	uint32_t crc;
};

static LIST_HEAD(parts_head);

const char *cmdline;

static int parse_rkparts(char *param)
{
	struct blk_part *part;
	const char *pcmdline = strstr(param, "CMDLINE:");
	const char *blkdev_parts = strstr(pcmdline, "mtdparts");
	const char *blkdev_def = strchr(blkdev_parts, ':') + 1;
	char *next = (char *)blkdev_def;
	char *pend;
	int len;
	unsigned long size, from;

	cmdline = pcmdline;

	debug("%s", cmdline);

	while (*next) {
		if (*next == '-') {
			size = (~0UL);
			next++;
		} else {
			size = simple_strtoul(next, &next, 16);
		}
		next++;
		from = simple_strtoul(next, &next, 16);
		next++;
		pend =  strchr(next, ')');
		if (!pend)
			break;
		len = min_t(int, pend - next, PART_NAME_SIZE);
		part = malloc(sizeof(*part));
		if (!part) {
			printf("out of memory\n");
			break;
		}
		part->from = from + RK_BLK_OFFSET;
		part->size = size;
		strncpy(part->name, next, len);
		part->name[len] = '\0';
		next = strchr(next, ',');
		next++;
		list_add_tail(&part->node, &parts_head);
		debug("0x%lx@0x%lx(%s)\n", part->size, part->from, part->name);
	}

	return 0;
}

struct blk_part *get_rockchip_blk_part(const char *name)
{
	struct blk_part *part;
	struct list_head *node;

	list_for_each(node, &parts_head) {
		part = list_entry(node, struct blk_part, node);
		if (!strcmp(part->name, name))
			return part;
	}

	return NULL;
}

static struct mmc *init_mmc_device(int dev, bool force_init)
{
	struct mmc *mmc;

	mmc = find_mmc_device(dev);
	if (!mmc) {
		printf("no mmc device at slot %x\n", dev);
		return NULL;
	}

	if (force_init)
		mmc->has_init = 0;
	if (mmc_init(mmc))
		return NULL;
	return mmc;
}

static int read_mmc(struct mmc *mmc, void *buffer, u32 blk, u32 cnt)
{
	u32 n;
	ulong start = (ulong)buffer;

	printf("\nMMC read: block # 0x%x, count 0x%x  to %p... ", blk, cnt, buffer);

	n = blk_dread(mmc_get_blk_desc(mmc), blk, cnt, buffer);
	/* flush cache after read */
	invalidate_dcache_range(start, start + cnt * 512);
	printf("%d blocks read: %s\n", n, (n == cnt) ? "OK" : "ERROR");

	return (n == cnt) ? 0 : -EIO;
}

static int read_boot_mode_from_misc(struct mmc *mmc,
					      struct blk_part *misc)
{
	struct bootloader_message *bmsg;
	int size = DIV_ROUND_UP(sizeof(struct bootloader_message),
				RK_BLK_SIZE) * RK_BLK_SIZE;
	int ret = 0;

	bmsg = memalign(ARCH_DMA_MINALIGN, size);
	ret = read_mmc(mmc, bmsg, misc->from + BOOTLOADER_MESSAGE_BLK_OFFSET,
		       size >> 9);
	if (ret < 0)
		goto out;
	if(!strcmp(bmsg->command, "boot-recovery")) {
		printf("bootmode: recovery");
		ret = ANDROID_BOOT_MODE_RECOVERY;
	} else {
		printf("bootmode: normal");
		ret = ANDROID_BOOT_MODE_NORMAL;
	}

out:
	free(bmsg);
	return ret;
}

/* none-OTA packaged kernel.img & boot.img
 * return the image size on success, and a
 * negative value on error.
 */
static int read_rockchip_image(struct mmc *mmc,
			       struct blk_part *part,
			       void *dst)
{
	struct rockchip_image *img = memalign(ARCH_DMA_MINALIGN, RK_BLK_SIZE);
	int header_len = 8;
	int cnt;
	int ret;

	/* read first block with header imformation */
	ret = read_mmc(mmc, img, part->from, 1);
	if (ret < 0)
		goto err;
	if (img->tag != TAG_KERNEL) {
		printf("invalid image tag");
		goto err;
	}

	memcpy(dst, img->image, RK_BLK_SIZE - header_len);
	/*
	 * read the rest blks
	 * total size  = image size + 8 bytes header + 4 bytes crc32
	 */
	cnt = DIV_ROUND_UP(img->size + 8 + 4, RK_BLK_SIZE);
	ret = read_mmc(mmc, dst + RK_BLK_SIZE - header_len,
			  part->from + 1, cnt - 1);
	if (!ret)
		ret = img->size;
err:
	free(img);
	return ret;
}

static int read_file_from_resource_image(struct mmc *mmc,
					 struct blk_part *part,
					 void *addr,
					 const char *name)
{
	struct resource_img_hdr *hdr = NULL;
	struct resource_file *file;
	void *content = NULL;
	int ret = 0;
	int blks;

	file = get_resource_file_info(NULL, NULL, name);
	if (!file) {
		hdr = memalign(ARCH_DMA_MINALIGN, RK_BLK_SIZE);
		ret = read_mmc(mmc, hdr, part->from, 1);
		if (ret < 0)
			goto out;
		ret = resource_image_check_header(hdr);
		if (ret < 0)
			goto out;
		content = memalign(ARCH_DMA_MINALIGN,
				   hdr->e_blks * hdr->e_nums * RK_BLK_SIZE);
		if (!content) {
			printf("alloc memory for content failed\n");
			goto out;
		}
		ret = read_mmc(mmc, content, part->from + hdr->c_offset,
			       hdr->e_blks * hdr->e_nums);
		if (ret < 0)
			goto err;
		file = get_resource_file_info(hdr, content, name);
		if (!file) {
			printf("failed to found file %s\n", name);
			ret = -ENOENT;
			goto err;
		}
	}

	blks = DIV_ROUND_UP(file->f_size, RK_BLK_SIZE);
	ret = read_mmc(mmc, addr, part->from + file->f_offset, blks);
	if (!ret)
		ret = file->f_size;
err:
	free(content);
out:
	free(hdr);

	return ret;
}

static int do_bootrkp(cmd_tbl_t *cmdtp, int flag, int argc,
		      char * const argv[])
{
	struct rockchip_param *param;
	struct blk_part *boot;
	struct blk_part *resource;
	struct blk_part *kernel;
	struct blk_part *misc;
	ulong mmcdev = env_get_ulong("mmcdev", 10, 0);
	ulong fdt_addr_r = env_get_ulong("fdt_addr_r", 16, 0);
	ulong ramdisk_addr_r = env_get_ulong("ramdisk_addr_r", 16, 0);
	ulong kernel_addr_r = env_get_ulong("kernel_addr_r", 16, 0x480000);
	struct mmc *mmc = init_mmc_device(mmcdev, false);
	ulong param_offset = RK_BLK_OFFSET;
	int boot_mode;
	int ramdisk_size;
	int kernel_size;
	int fdt_size;
	int ret = 0;
	char cmdbuf[64];

	param = memalign(ARCH_DMA_MINALIGN, MAX_PARAM_SIZE);
	mmc = init_mmc_device(mmcdev, false);
	if (!mmc)
		return CMD_RET_FAILURE;
	read_mmc(mmc, param, param_offset, MAX_PARAM_SIZE >> 9);

	parse_rkparts(param->params);

	misc = get_rockchip_blk_part(PART_MISC);
	boot_mode = read_boot_mode_from_misc(mmc, misc);
	if (boot_mode == ANDROID_BOOT_MODE_RECOVERY)
		boot = get_rockchip_blk_part(PART_RECOVERY);
	else
		boot = get_rockchip_blk_part(PART_BOOT);
	kernel = get_rockchip_blk_part(PART_KERNEL);
	resource = get_rockchip_blk_part(PART_RESOURCE);

	kernel_size = read_rockchip_image(mmc, kernel, (void *)kernel_addr_r);
	if (kernel_size < 0) {
		ret = CMD_RET_FAILURE;
		goto out;
	}
	ramdisk_size = read_rockchip_image(mmc, boot, (void *)ramdisk_addr_r);
	if (ramdisk_size < 0) {
		ret = CMD_RET_FAILURE;
		goto out;
	}
	fdt_size = read_file_from_resource_image(mmc, resource, (void *)fdt_addr_r,
						 DTB_FILE);
	if (fdt_size < 0) {
		ret = CMD_RET_FAILURE;
		goto out;
	}
	env_set("bootargs", cmdline);
	printf("kernel_size:0x%ulx ramdisk_size:0x%x\n", kernel_size, ramdisk_size);
	sprintf(cmdbuf, "booti 0x%lx 0x%lx:0x%x 0x%lx",
		kernel_addr_r, ramdisk_addr_r, ramdisk_size, fdt_addr_r);
	run_command(cmdbuf, 0);
out:
	free(param);
	return ret;
}

U_BOOT_CMD(
	bootrkp,  CONFIG_SYS_MAXARGS,     1,      do_bootrkp,
	"boot Linux Image image from rockchip partition storage",
	""
);
