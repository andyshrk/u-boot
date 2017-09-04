/*
 * (C) Copyright 2017 Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include "resource_img.h"

static LIST_HEAD(entrys_head);

int resource_image_check_header(const struct resource_img_hdr *hdr)
{
	int ret;
	ret = memcmp(RESOURCE_MAGIC, hdr->magic, RESOURCE_MAGIC_SIZE);
	if (ret) {
		printf("bad resource image magic\n");
		ret = -EINVAL;
	}
	debug("resource image header:\n");
	debug("magic:%s\n", hdr->magic);
	debug("version:%d\n", hdr->version);
	debug("c_version:%d\n", hdr->c_version);
	debug("blks:%d\n", hdr->blks);
	debug("c_offset:%d\n", hdr->c_offset);
	debug("e_blks:%d\n", hdr->e_blks);
	debug("e_num:%d\n", hdr->e_nums);

	return ret;
}

struct resource_file *get_resource_file_info(struct resource_img_hdr *hdr,
					     const void *content,
					     const char *name)
{
	struct resource_entry *entry;
	struct resource_file *file;
	struct list_head *node;
	int e_num;
	int size;

	if (hdr && content) {
		for (e_num = 0; e_num < hdr->e_nums; e_num++) {
			size = e_num * hdr->e_blks * RK_BLK_SIZE;
			entry = (struct resource_entry *)(content + size);
			if (memcmp(entry->tag, ENTRY_TAG, ENTRY_TAG_SIZE))
				continue;
			file = malloc(sizeof(*file));
			if (!file) {
				printf("out of memory\n");
				break;
			}
			strcpy(file->name, entry->name);
			file->f_offset = entry->f_offset;
			file->f_size = entry->f_size;
			list_add_tail(&file->link, &entrys_head);
			debug("entry:%p  %s offset:%d size:%d\n",
			      entry, file->name, file->f_offset, file->f_size);
		}
	}

	list_for_each(node, &entrys_head) {
		file = list_entry(node, struct resource_file, link);
		if (!strcmp(file->name, name))
			return file;
	}

	return NULL;
}
