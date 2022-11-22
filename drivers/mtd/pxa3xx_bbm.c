/*
 * Bad Block Management support for PXA3XX.
 * Copyright (C) 2009 Marvell International Ltd.
 *     Lei Wen <leiwen@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <plat/pxa3xx_bbm.h>
#include <linux/module.h>
#include <linux/slab.h>

#define NEW_BBM_RELOC_PERCENTAGE	(2)
#define MAX_SUPPRTED_PARTNUM		(3)

static int erase_success;
static int should_reloc = 1;
static loff_t pxa3xx_reseved_sz(struct mtd_info *mtd);
static void pxa3xx_bbm_callback(struct erase_info *instr)
{
	if (instr->fail_addr == MTD_FAIL_ADDR_UNKNOWN)
		erase_success = 1;
	else
		erase_success = 0;
}

static void dump_reloc_table(struct reloc_item *item, int entry_num)
{
	int i;

	if (entry_num == 0) {
		printk(KERN_INFO "The reloc table is empty now\n");
		return;
	}

	for (i = 0; i < entry_num; i++)
		printk(KERN_INFO "block: %8d is relocated to block: %d\n",
				item[i].from, item[i].to);
}

static void dump_fact_bads(struct pxa3xx_bbt *fbbt)
{
	uint32_t *fact_bad = (uint32_t *)&fbbt->fact_bad;
	int i;

	if (fbbt->entry_num == 0) {
		printk(KERN_INFO "There is no factory bad block!!\n");
		return;
	}

	for (i = 0; i < fbbt->entry_num; i++)
		printk(KERN_INFO "block %d is bad.\n", fact_bad[i]);
}

static void dump_part_info(struct mtd_info *mtd)
{
	struct pxa3xx_bbm *bbm = (struct pxa3xx_bbm *)mtd->bbm;
	struct pxa3xx_new_bbm *new_bbm = (struct pxa3xx_new_bbm *)bbm->data_buf;
	struct pxa3xx_part *part = new_bbm->part;
	struct pxa3xx_partinfo *partinfo;
	struct pxa3xx_bbt *rbbt;
	struct reloc_item *item;
	char tmp[9];
	int i;
	uint32_t swap_temp;

	printk(KERN_INFO "\nThere are totally %d parts", part->part_num);
	for (i = 0; i < part->part_num; i++) {
		printk(KERN_INFO "\n===The part %d info:===\n", i);
		partinfo = &new_bbm->partinfo[i];
		if (partinfo->type == PART_LOGI)
			printk(KERN_INFO "This part is Logi\n");
		else
			printk(KERN_INFO "This part is Phys\n");
		if (partinfo->usage && partinfo->usage != 0xffffffff) {
			memcpy(tmp, &partinfo->usage, 4);
			tmp[4] = '\0';
			printk(KERN_INFO "Part name %s\n", tmp);
		}
		if (partinfo->identifier
		    && partinfo->identifier != 0xffffffff) {
			memcpy(tmp, &partinfo->identifier, 4);
			tmp[4] = '\0';
			printk(KERN_INFO "identifier %s\n", tmp);
		}
		printk(KERN_INFO "Attr %16x\n", partinfo->attrs);
		printk(KERN_INFO "This part start from %llx to %llx\n",
				partinfo->start_addr, partinfo->end_addr);
		printk(KERN_INFO "Reserved pool start from %llx, size %llx\n",
				partinfo->rp_start, partinfo->rp_size);
		if (partinfo->rp_algo == RP_UPWD)
			printk(KERN_INFO "Reserved pool grow upwards\n");
		else
			printk(KERN_INFO "Reserved pool grow downwards\n");

		swap_temp = partinfo->rbbt_type;
		swab32s(&swap_temp);
		memcpy(tmp, &swap_temp, 4);
		tmp[4] = '\0';
		printk(KERN_INFO "\nRBBT type %s\n", tmp);
		printk(KERN_INFO "RBBT start at %llx, its back at %llx\n",
			partinfo->rbbt_start, partinfo->rbbt_start_back);
		rbbt = &new_bbm->rbbt[i];
		printk(KERN_INFO "RBBT could max reloc %d blocks\n",
				new_bbm->max_reloc_entry[i]);
		printk(KERN_INFO "Current slot is at 0x%llx\n",
			new_bbm->rbbt_offset[i] << mtd->writesize_shift);
		item = (struct reloc_item *)rbbt->reloc;
		dump_reloc_table(item, new_bbm->rbbt->entry_num);
	}
}

static void pxa3xx_uninit_reloc_tb(struct mtd_info *mtd)
{
	struct pxa3xx_bbm *bbm = (struct pxa3xx_bbm *)mtd->bbm;
	struct pxa3xx_legacy_bbm *legacy_bbm;
	struct pxa3xx_new_bbm *new_bbm;

	if (bbm) {
		switch (bbm->bbm_type) {
		case BBM_LEGACY:
			legacy_bbm = (struct pxa3xx_legacy_bbm *)bbm->data_buf;
			kfree(legacy_bbm->table);
			break;

		case BBM_NEW:
			new_bbm = (struct pxa3xx_new_bbm *)bbm->data_buf;
			kfree(new_bbm->rbbt);
			kfree(new_bbm->fbbt);
			kfree(new_bbm->part);
		default:
			break;
		}

		kfree(bbm->data_buf);
		kfree(bbm);
		mtd->bbm = NULL;
	}
}

/*
 * Found the block belong to which partition
 */
static int find_part(struct mtd_info *mtd, uint64_t offset)
{
	struct pxa3xx_bbm *bbm = (struct pxa3xx_bbm *)mtd->bbm;
	struct pxa3xx_new_bbm *new_bbm = (struct pxa3xx_new_bbm *)bbm->data_buf;
	struct pxa3xx_part *part = new_bbm->part;
	struct pxa3xx_partinfo *partinfo;
	int i, found_part = -EINVAL;

	for (i = 0; i < part->part_num; i++) {
		partinfo = &(new_bbm->partinfo[i]);
		if (offset < partinfo->start_addr)
			break;

		if (offset < partinfo->end_addr) {
			found_part = i;
			break;
		}
	}

	return found_part;
}

/*
 * start_page and end_page should be in one block boundary
 * direction: 1 for positive page grow order, 0 for the reversed order
 * indicator should be meaningful bit order stand for BBT
 */
int page_search(struct mtd_info *mtd, int start_page, int end_page,
	int direction, unsigned int indicator, void *buf, unsigned int mask)
{
	int found_page = -EINVAL, cur_page, ret;
	unsigned int header;
	size_t retlen;

	cur_page = (direction == ORDER_POSITIVE) ? end_page : start_page;
	while (start_page <= end_page) {
		ret = mtd_read(mtd, (loff_t)cur_page * mtd->writesize,
				mtd->writesize, &retlen, buf);
		header = *(unsigned int *)buf & mask;
		if (!ret && header == indicator) {
			found_page = cur_page;
			break;
		}

		if (direction == ORDER_POSITIVE) {
			cur_page--;
			if (cur_page < start_page)
				break;
		} else {
			cur_page++;
			if (cur_page > end_page)
				break;
		}
	}

	return found_page;
}

/* add the relocation entry into the relocation table
 * It's valid on MOBM V3.
 * If the relocated block is bad, an new entry will be added into the
 * bottom of the relocation table.
 */
static int sync_pxa3xx_bbt(struct mtd_info *mtd, loff_t ofs)
{
	struct pxa3xx_bbm *bbm = mtd->bbm;
	struct pxa3xx_legacy_bbm *legacy_bbm = NULL;
	struct pxa3xx_new_bbm *new_bbm;
	struct pxa3xx_partinfo *partinfo;
	struct pxa3xx_bbt *bbt = NULL;
	struct reloc_item *item;
	struct erase_info instr;
	int reloc_block, entry_num = -1;
	char *rel_dist;
	int i, block, _rel, max_reloc_entry, reloc_boundary, total, part;

	printk(KERN_INFO "ready to put %llx into the bbt\n", ofs);
	if (bbm->bbm_type == BBM_LEGACY) {
		legacy_bbm = (struct pxa3xx_legacy_bbm *)bbm->data_buf;
		item = legacy_bbm->reloc;
		reloc_boundary = mtd_div_by_eb(mtd->size, mtd)
			- legacy_bbm->max_reloc_entry;
		max_reloc_entry = legacy_bbm->max_reloc_entry;
		total = legacy_bbm->table->total;
	} else {
		new_bbm = (struct pxa3xx_new_bbm *)bbm->data_buf;
		part = find_part(mtd, ofs);
		if (part < 0)
			return -EINVAL;
		new_bbm->update_indicator |= 1 << part;
		max_reloc_entry = new_bbm->max_reloc_entry[part];
		bbt = &new_bbm->rbbt[part];
		partinfo = &new_bbm->partinfo[part];
		item = (struct reloc_item *)&bbt->reloc;
		reloc_boundary = mtd_div_by_eb(partinfo->rp_start, mtd);
		total = bbt->entry_num;
	}

	block = (int)(ofs >> mtd->erasesize_shift);
	if (total >= max_reloc_entry) {
		printk(KERN_WARNING "Relocation table currently have %d\n"
			"Exceed max num %d, cannot relocate block %d!!\n",
			total, max_reloc_entry, block);
		return -ENOSPC;
	}

	if (block >= reloc_boundary)
		return -EINVAL;

	/* identify whether the block has been relocated */
	for (i = total - 1; i >= 0; i--) {
		if (block == item[i].from)
			entry_num = i;
	}

	rel_dist = bbm->rel_dist;
	if (!rel_dist) {
		rel_dist = kzalloc(max_reloc_entry, GFP_KERNEL);
		/* need to save this */
		bbm->rel_dist = rel_dist;
	} else
		memset(rel_dist, 0, max_reloc_entry);
	/* find the available block with the largest num in reservered area */
	for (i = 0; i < total; i++) {
		_rel = (item[i].to != 65535) ? item[i].to : item[i].from;
		if (_rel - reloc_boundary < 0) {
			printk(KERN_ERR "BBT is corruppted! block %d"
					" --> block %d invalid !",
					item[i].from, item[i].to);
			BUG();
		}
		rel_dist[_rel - reloc_boundary] = 1;
	}

	while (1) {
		/* Make sure that reloc_block is pointing to a valid block */
		for (reloc_block = max_reloc_entry - 1;
				reloc_block >= 0; reloc_block--) {
			if (rel_dist[reloc_block] == 0)
				break;
		}

		if (reloc_block < 0) {
			if (entry_num >= 0) {
				item[entry_num].from = item[entry_num].to;
				item[entry_num].to = 65535;
			}
			return -ENOSPC;
		}

		reloc_block = reloc_block + reloc_boundary;
		memset(&instr, 0, sizeof(struct erase_info));
		instr.mtd = mtd;
		instr.addr = (uint64_t)reloc_block << mtd->erasesize_shift;
		instr.len = mtd->erasesize;
		instr.callback = pxa3xx_bbm_callback;

		should_reloc = 0;
		mtd_erase(mtd, &instr);
		should_reloc = 1;
		if (erase_success)
			break;
		else {
			/* skip it if the reloc_block is also a
			 * bad block
			 */
			if (instr.fail_addr == instr.addr) {
				item[total].from = reloc_block;

				item[total].to = 65535;
				total++;
				rel_dist[reloc_block - reloc_boundary] = 1;;
				continue;
			} else
				return -EINVAL;
		}
	}

	/*
	 * Create the relocated block information in the table
	 * when the block is relocated before, blob should modify
	 * the original entry to new relocated block and the old
	 * relocated block point to 65535. If not the situation,
	 * create a new entry
	 */
	if (entry_num != -1) {
		item[total].from = item[entry_num].to;
		item[total].to = 65535;
		total++;
		item[entry_num].to = reloc_block;
	} else {
		item[total].from = block;
		item[total].to = reloc_block;
		total++;
	}

	if (bbm->bbm_type == BBM_LEGACY)
		legacy_bbm->table->total = total;
	else
		bbt->entry_num = total;

	return 0;
}

/* Write the relocation table back to device, if there's room. */
int pxa3xx_update_bbt(struct mtd_info *mtd, loff_t offs)
{
	struct pxa3xx_bbm *bbm = (struct pxa3xx_bbm *)mtd->bbm;
	struct pxa3xx_legacy_bbm *legacy_bbm;
	struct pxa3xx_new_bbm *new_bbm;
	size_t retlen;
	loff_t offset = 0;
	void *buf;
	int ret = 1, part = 0, pages;

	while (1) {
		switch (bbm->bbm_type) {
		case BBM_LEGACY:
			if (!ret) {
				printk(KERN_INFO "update legacy bbt"
						" at %llx\n", offset);
				return 0;
			}

			pages = mtd->erasesize >> mtd->writesize_shift;
			legacy_bbm = (struct pxa3xx_legacy_bbm *)bbm->data_buf;
			if (legacy_bbm->current_slot <= PXA_BEGIN_SLOT
				|| legacy_bbm->current_slot > pages)
				goto ERR_EXIT;

			/* should write to the next slot */
			legacy_bbm->current_slot--;
			buf = legacy_bbm->table;
			offset = legacy_bbm->current_slot
				<< mtd->writesize_shift;
			break;

		case BBM_NEW:
			new_bbm = (struct pxa3xx_new_bbm *)bbm->data_buf;
			if (!ret) {
				printk(KERN_INFO "update new bbm bbt"
						" at %llx\n", offset);
				new_bbm->update_indicator &= ~(1 << part);
			}
			for (; part < MAX_SUPPRTED_PARTNUM; part++)
				if (new_bbm->update_indicator & (1 << part))
					break;

			if (part >= MAX_SUPPRTED_PARTNUM)
				return 0;

			offset = (new_bbm->rbbt_offset[part] + 1)
				<< mtd->writesize_shift;
			if (!(unsigned int)(offset & mtd->erasesize_mask))
				goto ERR_EXIT;

			new_bbm->rbbt_offset[part]++;
			buf = new_bbm->rbbt;
			break;

		default:
			return 0;
		}

		ret = mtd_write(mtd, offset, mtd->writesize, &retlen, buf);
	}

ERR_EXIT:
	printk(KERN_ERR "Can't write relocation table to device any more.\n");
	return -EINVAL;
}
EXPORT_SYMBOL(pxa3xx_update_bbt);

/* Find the relocated block of the bad one.
 * If it's a good block, return 0. Otherwise, return a relocated one.
 * idx points to the next relocation entry
 * If the relocated block is bad, an new entry will be added into the
 * bottom of the relocation table.
 */
static loff_t pxa3xx_search_reloc_tb(struct mtd_info *mtd, loff_t ofs)
{
	struct pxa3xx_bbm *bbm = (struct pxa3xx_bbm *)mtd->bbm;
	struct pxa3xx_legacy_bbm *legacy_bbm;
	struct pxa3xx_new_bbm *new_bbm;
	struct reloc_item *item;
	int i, block, max_allow_relocated, entry_num, part;

	if (!bbm || (bbm->is_init == BBT_NOINIT))
		return ofs;

	block = ofs >> mtd->erasesize_shift;
	switch (bbm->bbm_type) {
	case BBM_LEGACY:
		legacy_bbm = (struct pxa3xx_legacy_bbm *)bbm->data_buf;
		if (legacy_bbm->current_slot < 0)
			return ofs;

		max_allow_relocated = mtd_div_by_eb(mtd->size, mtd)
			- legacy_bbm->max_reloc_entry;
		item = legacy_bbm->reloc;
		entry_num = legacy_bbm->table->total;
		break;

	case BBM_NEW:
		new_bbm = (struct pxa3xx_new_bbm *)bbm->data_buf;
		part = find_part(mtd, ofs);
		if (part < 0)
			return ofs;
		item = (struct reloc_item *)&new_bbm->rbbt[part].reloc;
		entry_num = new_bbm->rbbt[part].entry_num;
		max_allow_relocated =
		mtd_div_by_eb(new_bbm->partinfo[part].end_addr, mtd);
		break;

	default:
		return ofs;
	}

	if (block >= max_allow_relocated || entry_num == 0)
		return ofs;

	ofs -= block * mtd->erasesize;
	for (i =0; i < entry_num; i ++)
		if (block == item[i].from)
			block = item[i].to;

	ofs += block * mtd->erasesize;

	return ofs;
}

static int pxa3xx_init_bbm(struct mtd_info *mtd, int bbm_type)
{
	struct pxa3xx_bbm *bbm = mtd->bbm;
	struct pxa3xx_legacy_bbm *legacy_bbm;
	struct pxa3xx_new_bbm *new_bbm;
	int size, ret, entrys, max_relcs;

	if (bbm_type != BBM_NEW && bbm_type != BBM_LEGACY)
		return -EFAULT;

	bbm = kzalloc(sizeof(struct pxa3xx_bbm), GFP_KERNEL);
	if (!bbm)
		return -ENOMEM;

	bbm->search = pxa3xx_search_reloc_tb;
	bbm->uninit = pxa3xx_uninit_reloc_tb;
	bbm->reserved_sz = pxa3xx_reseved_sz;
	mtd->bbm = bbm;
	size = (bbm_type == BBM_NEW) ? sizeof(struct pxa3xx_new_bbm) :
					sizeof(struct pxa3xx_legacy_bbm);
	bbm->is_init = BBT_NOINIT;
	bbm->no_sync = 0;
	bbm->data_buf = kzalloc(size, GFP_KERNEL);
	if (!bbm->data_buf) {
		ret = -ENOMEM;
		goto ERR_EXIT;
	}

	if (bbm_type == BBM_NEW) {
		bbm->bbm_type = BBM_NEW;
		new_bbm = (struct pxa3xx_new_bbm *)bbm->data_buf;
		new_bbm->main_block = -1;
		new_bbm->back_block = -1;
		new_bbm->fbbt =	kzalloc(mtd->writesize, GFP_KERNEL);
		new_bbm->part =	kzalloc(mtd->writesize, GFP_KERNEL);
		new_bbm->rbbt =
		   kzalloc(mtd->writesize*MAX_SUPPRTED_PARTNUM, GFP_KERNEL);
		new_bbm->rbbt_offset =
		   kzalloc(sizeof(loff_t)*MAX_SUPPRTED_PARTNUM, GFP_KERNEL);
		new_bbm->max_reloc_entry =
		   kzalloc(sizeof(int)*MAX_SUPPRTED_PARTNUM, GFP_KERNEL);
		if (!new_bbm->rbbt
				|| !new_bbm->rbbt_offset
				|| !new_bbm->max_reloc_entry
				|| !new_bbm->fbbt
				|| !new_bbm->part) {
			kfree(bbm->data_buf);
			ret = -ENOMEM;
			goto ERR_EXIT;
		}

		new_bbm->partinfo =
			(struct pxa3xx_partinfo *)&new_bbm->part[1];
		memset(new_bbm->fbbt, 0xff, mtd->writesize);
		memset(new_bbm->part, 0xff, mtd->writesize);
	} else {
		bbm->bbm_type = BBM_LEGACY;
		legacy_bbm = (struct pxa3xx_legacy_bbm *)bbm->data_buf;
		entrys = mtd_div_by_eb(mtd->size, mtd);
		entrys = (entrys / 100) * 2;
		max_relcs = (mtd->writesize - sizeof(struct reloc_table))
			/ sizeof(struct reloc_item);

		legacy_bbm->max_reloc_entry = (entrys < max_relcs) ?
					entrys : max_relcs;
		legacy_bbm = (struct pxa3xx_legacy_bbm *)bbm->data_buf;
		legacy_bbm->table = kzalloc(mtd->writesize, GFP_KERNEL);
		if (!legacy_bbm->table) {
			kfree(bbm->data_buf);
			ret = -ENOMEM;
			goto ERR_EXIT;
		}

		memset(legacy_bbm->table, 0xff, mtd->writesize);
		legacy_bbm->reloc = (struct reloc_item *)&legacy_bbm->table[1];
		legacy_bbm->current_slot = -1;
	}

	return 0;

ERR_EXIT:
	kfree(bbm);
	mtd->bbm = NULL;
	return ret;
}

static int legacy_bbm_scan(struct mtd_info *mtd)
{
	struct pxa3xx_bbm *bbm = (struct pxa3xx_bbm *)mtd->bbm;
	struct pxa3xx_legacy_bbm *legacy_bbm;
	struct reloc_table *table;
	unsigned int write_shift;

	if (is_power_of_2(mtd->writesize))
		write_shift = ffs(mtd->writesize) - 1;
	else
		write_shift = 0;
	legacy_bbm = (struct pxa3xx_legacy_bbm *)bbm->data_buf;
	table = legacy_bbm->table;
	legacy_bbm->current_slot = page_search(mtd, PXA_BEGIN_SLOT,
			(mtd->erasesize >> write_shift) - 1,
			ORDER_REVERSE, PXA_RELOC_HEADER, table, BBM_HALF_MASK);

	printk(KERN_INFO "Max capacity of BBM is %d blocks!!\n",
			legacy_bbm->max_reloc_entry);
	if (legacy_bbm->current_slot >= 0) {
		printk(KERN_INFO "relocation table at page:%d\n",
				legacy_bbm->current_slot);
		dump_reloc_table(legacy_bbm->reloc, table->total);

		return 0;
	}

	/* There should be a valid relocation table slot at least. */
	printk(KERN_ERR "NO VALID reloc table can be recognized\n");
	printk(KERN_ERR "CAUTION: It may cause unpredicated error\n");
	printk(KERN_ERR "Please re-initialize the flash.\n");
	kfree(bbm->data_buf);

	return -EINVAL;
}

#define FOUND_FBBT 0x1
#define FOUND_PART 0x2
#define BBM_NOCOPY 0x1
static int scan_fbbt_part(struct mtd_info *mtd, int block, void *buf, int flag)
{
	/*
	 * NTIM header at least occupy by one page,
	 * so search the FBBT or part from second page,
	 * and this search should be ended at the fifth page
	 */
	struct pxa3xx_bbm *bbm = (struct pxa3xx_bbm *)mtd->bbm;
	struct pxa3xx_new_bbm *new_bbm = (struct pxa3xx_new_bbm *)bbm->data_buf;
	struct pxa3xx_part *part;
	struct pxa3xx_partinfo *partinfo;
	int page, ret, part_num, found = 0, i, max_reloc_entry, rp_num;
	int start_page, end_page;
	loff_t offset;
	size_t retlen;

	max_reloc_entry = (mtd->writesize - 40) / sizeof(struct reloc_item);
	for (page = 1; page < 5; page++) {
		if (found == (FOUND_PART | FOUND_FBBT))
			break;

		offset = ((uint64_t)block << mtd->erasesize_shift)
			+ (page << mtd->writesize_shift);
		ret = mtd_read(mtd, offset, mtd->writesize, &retlen, buf);

		/* found FBBT */
		if (!ret && *(unsigned int *)buf == PXA_NEW_BBM_HEADER) {
			if (flag == BBM_NOCOPY)
				return 1;

			found |= FOUND_FBBT;
			memcpy(new_bbm->fbbt, buf, retlen);
		}

		/* found partition table */
		if (!ret && *(unsigned int *)buf == PXA_PART_IDET_1) {
			if (*((unsigned int *)buf + 1) != PXA_PART_IDET_2)
				continue;

			if (flag == BBM_NOCOPY)
				return 1;

			found |= FOUND_PART;
			memcpy(new_bbm->part, buf, retlen);
			part = new_bbm->part;
			part_num = part->part_num;

			for (i = 0; i < part_num; i++) {
				partinfo = &new_bbm->partinfo[i];
				start_page =
				     mtd_div_by_ws(partinfo->rbbt_start, mtd);
				end_page = start_page - 1 +
				     (mtd->erasesize >> mtd->writesize_shift);
				new_bbm->rbbt_offset[i] =
					page_search(mtd, start_page, end_page,
					ORDER_POSITIVE, PXA_NEW_BBM_HEADER,
					&new_bbm->rbbt[i], BBM_FULL_MASK);
				rp_num = mtd_div_by_eb(partinfo->rp_size, mtd);
				new_bbm->max_reloc_entry[i] =
					(max_reloc_entry < rp_num) ?
					max_reloc_entry : rp_num;
			}
		}
	}

	return found == (FOUND_PART | FOUND_FBBT);
}

static int new_bbm_scan(struct mtd_info *mtd)
{
	struct pxa3xx_bbm *bbm = (struct pxa3xx_bbm *)mtd->bbm;
	struct pxa3xx_new_bbm *new_bbm;
	int block, ret, flag;
	void *buf;

	new_bbm = (struct pxa3xx_new_bbm *)bbm->data_buf;
	buf = kzalloc(mtd->writesize + mtd->oobsize, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	flag = 0;
	for (block = 0; block < 10; block++) {
		ret = scan_fbbt_part(mtd, block, buf, flag);
		if (ret) {
			flag = BBM_NOCOPY;
			if (new_bbm->main_block == -1)
				new_bbm->main_block = block;
			else if (new_bbm->back_block == -1) {
				new_bbm->back_block = block;
				break;
			}
		}
	}
	kfree(buf);

	if (new_bbm->main_block == -1 && new_bbm->back_block == -1) {
		printk(KERN_ERR "New BBM initilization failed!!!!!!\n");
		return -EINVAL;
	}

	printk(KERN_INFO "Found main block at %d, back at %d\n",
			new_bbm->main_block, new_bbm->back_block);
	new_bbm->update_indicator = 0;
	printk(KERN_INFO "Factory marked bad blocks:\n");
	dump_fact_bads(new_bbm->fbbt);
	dump_part_info(mtd);
	return 0;
}

int pxa3xx_scan_bbt(struct mtd_info *mtd)
{
	struct pxa3xx_bbm *bbm;
	size_t retlen;
	int ret, bbm_type;
	void *buf;

	if (!mtd->bbm) {
		/*
		 * Legacy BBM only use the block 0, while new BBM scheme use
		 * the block 0 to block 9. So first read the last page in
		 * block 0 to see if it is legacy BBM, or change the
		 * scan strategy to new BBM scheme.
		 */
		buf = kzalloc(mtd->writesize, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;

		ret = mtd_read(mtd, mtd->erasesize - mtd->writesize,
				mtd->writesize, &retlen, buf);

		/* This flash chip is using legacy BBM */
		if (!ret && *(unsigned short *)buf == PXA_RELOC_HEADER)
			bbm_type = BBM_LEGACY;
		else
			bbm_type = BBM_NEW;

		kfree(buf);
		ret = pxa3xx_init_bbm(mtd, bbm_type);
		if (ret)
			return ret;
		bbm = (struct pxa3xx_bbm *)mtd->bbm;
	} else {
		bbm = (struct pxa3xx_bbm *)mtd->bbm;
		bbm_type = bbm->bbm_type;
	}

	if (bbm->is_init != BBT_NOINIT)
		return 0;

	if (bbm_type == BBM_LEGACY)
		ret = legacy_bbm_scan(mtd);
	else
		ret = new_bbm_scan(mtd);

	if (!ret)
		bbm->is_init = BBT_INITED;
	else {
		printk(KERN_ERR "BBM NOT Initialized, "
				"Please re-init the flash!!!\n\n");
		bbm->is_init = BBT_NOINIT;
	}

	return ret;
}
EXPORT_SYMBOL(pxa3xx_scan_bbt);

static int checkbad(struct mtd_info *mtd, loff_t ofs)
{
	struct mtd_oob_ops ops;
	uint32_t bad_mark;

	ops.ooboffs	= 0;
	ops.ooblen	= 2;
	ops.len		= 2;
	ops.datbuf	= NULL;
	ops.oobbuf	= (uint8_t *)&bad_mark;
	ops.mode	= MTD_OPS_PLACE_OOB;

	mtd_read_oob(mtd, ofs, &ops);
	if ((bad_mark & 0xFF) != 0xFF)
		return 1;
	else
		return 0;
}

static int boot_part_bad(struct mtd_info *mtd, loff_t ofs)
{
	struct pxa3xx_bbm *bbm = (struct pxa3xx_bbm *)mtd->bbm;
	struct pxa3xx_new_bbm *new_bbm = (struct pxa3xx_new_bbm *)bbm->data_buf;
	struct pxa3xx_bbt *fbbt = new_bbm->fbbt;
	int block = ofs >> mtd->erasesize_shift, i;
	uint32_t *fact_bad = (uint32_t *)&fbbt->fact_bad;

	for (i = 0; i < fbbt->entry_num; i++)
		if (fact_bad[i] == block)
			return 1;

	return 0;
}

int pxa3xx_block_bad(struct mtd_info *mtd, loff_t ofs, int allowbbt)
{
	struct pxa3xx_bbm *bbm;
	struct pxa3xx_legacy_bbm *legacy_bbm;
	struct pxa3xx_new_bbm *new_bbm;
	struct reloc_table *table;
	int part;

	bbm = (struct pxa3xx_bbm *)mtd->bbm;
	if (bbm && (bbm->is_init != BBT_NOINIT)) {
		if (bbm->is_init == BBT_FORCE_NOINIT)
			return 0;

		bbm = (struct pxa3xx_bbm *)mtd->bbm;
		switch (bbm->bbm_type) {
		case BBM_LEGACY:
			legacy_bbm = (struct pxa3xx_legacy_bbm *)bbm->data_buf;
			table = legacy_bbm->table;
			/*
			 * If relocation table is not yet full, then any block
			 * in the flash should be good
			 */
			if (legacy_bbm->current_slot >= PXA_BEGIN_SLOT
				&& table->total <= legacy_bbm->max_reloc_entry)
				return 0;
			else
				return 1;
			break;

		case BBM_NEW:
			new_bbm = (struct pxa3xx_new_bbm *)bbm->data_buf;
			part = find_part(mtd, ofs);
			if (part >= 0) {
				if (new_bbm->rbbt[part].entry_num
					< new_bbm->max_reloc_entry[part])
					return 0;
				else
					return 1;
			}
		default:
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL(pxa3xx_block_bad);

int pxa3xx_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct pxa3xx_bbm *bbm = mtd->bbm;
	int ret;

	if (!should_reloc)
		return 0;

	if (bbm) {
		if (bbm->bbm_type != BBM_LEGACY && bbm->bbm_type != BBM_NEW) {
			printk(KERN_WARNING "There is no way"
					" to mark bad at %llx", ofs);
			return 0;
		}

		if (bbm->is_init == BBT_NOINIT) {
			printk(KERN_WARNING "You should scan bbm first!!\n");
			return 0;
		}

		ret = sync_pxa3xx_bbt(mtd, ofs);
		if (!ret && !bbm->no_sync)
			ret = pxa3xx_update_bbt(mtd, 0);

		return ret;
	} else {
		printk(KERN_ERR "Unable to mark bad block at %llx\n", ofs);
		return -EFAULT;
	}
}
EXPORT_SYMBOL(pxa3xx_block_markbad);

static int recover_legacy_bbm(struct mtd_info *mtd, int backup)
{
	struct pxa3xx_bbm *bbm = (struct pxa3xx_bbm *)mtd->bbm;
	struct pxa3xx_legacy_bbm *legacy_bbm;
	struct reloc_table *table;
	struct erase_info instr = {
		.callback	= NULL,
	};
	int backup_size, ret = 0;
	loff_t ofs;
	void *buf;
	size_t retlen;

	backup_size = mtd->writesize * PXA_BEGIN_SLOT;
	bbm->is_init = BBT_FORCE_NOINIT;
	legacy_bbm = (struct pxa3xx_legacy_bbm *)bbm->data_buf;
	legacy_bbm->current_slot = mtd->erasesize >> mtd->writesize_shift;
	table = legacy_bbm->table;
	table->header = PXA_RELOC_HEADER;
	table->total = 0;

	if (backup) {
		buf = kzalloc(backup_size, GFP_KERNEL);
		if (!buf) {
			printk(KERN_ERR "MEM alloc failed!!\n");
			return -ENOMEM;
		}
		printk(KERN_INFO "Ready to read..");
		mtd_read(mtd, 0, backup_size, &retlen, buf);
	}

	instr.addr = 0;
	instr.len = mtd->erasesize;
	instr.callback = pxa3xx_bbm_callback;
	printk(KERN_INFO "erasing..");

	should_reloc = 0;
	mtd_erase(mtd, &instr);
	should_reloc = 1;
	if (!erase_success) {
		printk(KERN_ERR "erase block 0 failed!!!\n");
		return -EFAULT;
	}

	if (backup) {
		printk(KERN_INFO "write back..");
		mtd_write(mtd, 0, backup_size, &retlen, buf);
		kfree(buf);
	}

	printk(KERN_INFO "collect bad info..");
	for (ofs = mtd->erasesize; ofs < mtd->size; ofs += mtd->erasesize)
		if (checkbad(mtd, ofs)) {
			printk(KERN_INFO "\nmark %llx as bad in bbt\n", ofs);
			sync_pxa3xx_bbt(mtd, ofs);
		}

	if (!bbm->no_sync) {
		printk(KERN_INFO "update bbt..");
		ret = pxa3xx_update_bbt(mtd, 0);
	}
	printk(KERN_INFO "done\n");

	return ret;
}

static int update_fbbt(struct pxa3xx_bbt *fbbt, int block)
{
	uint32_t *fact_bad = (uint32_t *)&fbbt->fact_bad;
	int i;

	for (i = 0; i < fbbt->entry_num; i++)
		if (fact_bad[i] == block)
			return 0;

	fact_bad[i] = block;
	fbbt->entry_num++;

	return fbbt->entry_num;
}

/*
 * recover_new_bbm only try to rebuild the fbbt and use the
 * default partition table to build the pt
 */
static int recover_new_bbm(struct mtd_info *mtd, struct reloc_item * item,
		int num, int reserve_last_page)
{
	struct pxa3xx_bbm *bbm = (struct pxa3xx_bbm *)mtd->bbm;
	struct pxa3xx_new_bbm *new_bbm = (struct pxa3xx_new_bbm *)bbm->data_buf;
	struct pxa3xx_bbt *fbbt = new_bbm->fbbt;
	struct pxa3xx_part *part = new_bbm->part;
	struct erase_info instr = {
		.callback	= NULL,
	};
	int boot_block, block, total_block, reserved_block, ret;
	int rbbt, rbbt_back, max_reloc_entry, len, failed = 0;
	loff_t ofs;
	size_t retlen;
	u_char *backup_buf = NULL;

	/*
	 * This should be the most init state
	 * should try to find two good blocks without the fbbt's help
	 * then build up a new fbbt
	 */
	backup_buf = kmalloc(mtd->erasesize, GFP_KERNEL);
	if (!backup_buf) {
		printk(KERN_ERR "Fail to allocate recovery memory!!\n");
		return -ENOMEM;
	}
	bbm->is_init = BBT_FORCE_NOINIT;
	if (new_bbm->main_block == -1) {
		memset(new_bbm->rbbt, 0xff, mtd->writesize);
		new_bbm->rbbt->ident = PXA_NEW_BBM_HEADER;
		new_bbm->rbbt->type = BBT_TYPE_RUNT;
		if (item != NULL && num > 0) {
			memcpy(&(new_bbm->rbbt->reloc), (void *)item,
					sizeof(struct reloc_item)*num);
			new_bbm->rbbt->entry_num = num;
		} else
			new_bbm->rbbt->entry_num = 0;
		max_reloc_entry = (mtd->writesize - sizeof(struct pxa3xx_bbt))
				/ sizeof(struct reloc_item) + 1;

		fbbt->ident = PXA_NEW_BBM_HEADER;
		fbbt->type = BBT_TYPE_FACT;
		fbbt->entry_num = 0;
		instr.len = mtd->erasesize;
		instr.callback = pxa3xx_bbm_callback;
		printk(KERN_INFO "Rebuild new bbm as init state..\n");
		for (boot_block = 0;
		     boot_block < BOOT_PRAT_MAX; boot_block++) {
			if (failed) {
				ofs = (uint64_t)(boot_block - 1)
				      << mtd->erasesize_shift;
				new_bbm->main_block = -1;
				update_fbbt(fbbt, boot_block - 1);
				failed = 0;
			}
			instr.addr = (uint64_t)boot_block
					<< mtd->erasesize_shift;
			ret = mtd_read(mtd, instr.addr, mtd->erasesize,
					&retlen, backup_buf);
			if (ret) {
				printk(KERN_ERR "Cannot backup block %d!!\n",
						boot_block);
				failed = 1;
				continue;
			}
			if (!reserve_last_page)
				memset(backup_buf + mtd->erasesize -
					mtd->writesize, 0xff, mtd->writesize);

			should_reloc = 0;
			mtd_erase(mtd, &instr);
			should_reloc = 1;
			if (!erase_success) {
				printk(KERN_ERR "erase %llx failed!!\n",
						instr.addr);
				failed = 1;
				continue;
			} else {
				ret = mtd_write(mtd, instr.addr,
					mtd->writesize * PXA_BEGIN_SLOT,
						&retlen, backup_buf);
				if (ret) {
					printk(KERN_ERR "restore backup two page failed!!\n");
					failed = 1;
					continue;
				}
				new_bbm->main_block = boot_block;
			}

			printk(KERN_INFO "Get main block at %d\n",
					new_bbm->main_block);
			part->identifier = (uint64_t)PXA_PART_IDET_2 << 32
						| PXA_PART_IDET_1;

			/*
			 * calculate part range under defaut setting of only
			 * one part, The first BOOT_PRAT_MAX block should be
			 * used as boot partition and next two block should
			 * be reversed as run time bbt
			 */
			part->part_num = 1;
			new_bbm->partinfo->type = PART_LOGI;
			total_block = mtd_div_by_eb(mtd->size, mtd);
			rbbt = rbbt_back = -1;
			instr.callback = pxa3xx_bbm_callback;
			instr.len = mtd->erasesize;
			for (block = BOOT_PRAT_MAX;
					block < total_block; block++) {
				instr.addr = (uint64_t)block
						<< mtd->erasesize_shift;
				should_reloc = 0;
				mtd_erase(mtd, &instr);
				should_reloc = 1;
				if (!erase_success) {
					printk(KERN_ERR "Erase %llx failed\n",
						       instr.addr);
					sync_pxa3xx_bbt(mtd, instr.addr);
					update_fbbt(fbbt, block);
				} else {
					ofs = (uint64_t)block
						<< mtd->erasesize_shift;
					if (rbbt == -1) {
						ret = mtd_write(mtd, ofs,
						      mtd->writesize, &retlen,
						      (void *)new_bbm->rbbt);
						if (ret)
							continue;
						rbbt = block;
					} else if (rbbt_back == -1) {
						ret = mtd_write(mtd, ofs,
						      mtd->writesize, &retlen,
						      (void *)new_bbm->rbbt);
						if (ret)
							continue;
						rbbt_back = block++;
						break;
					}
				}
			}

			printk(KERN_INFO "\nGet RBBT at block %d,"
					" its back at %d\n", rbbt, rbbt_back);
			reserved_block = ((total_block - block) / 100)
				* NEW_BBM_RELOC_PERCENTAGE;
			new_bbm->partinfo->start_addr = (uint64_t)block
				<< mtd->erasesize_shift;
			new_bbm->partinfo->end_addr =
				((uint64_t)(total_block - reserved_block)
				 << mtd->erasesize_shift) - 1;
			new_bbm->partinfo->rp_start =
				(uint64_t)(total_block - reserved_block)
				<< mtd->erasesize_shift;
			new_bbm->partinfo->rp_size = (uint64_t)reserved_block
				<< mtd->erasesize_shift;
			new_bbm->partinfo->rp_algo = RP_UPWD;
			new_bbm->partinfo->rbbt_type = PXA_NEW_BBM_HEADER;
			new_bbm->partinfo->rbbt_start = (uint64_t)rbbt
				<< mtd->erasesize_shift;
			new_bbm->partinfo->rbbt_start_back =
				(uint64_t)rbbt_back << mtd->erasesize_shift;

			new_bbm->rbbt_offset[0] =
			    mtd_div_by_ws(new_bbm->partinfo->rbbt_start, mtd);
			new_bbm->max_reloc_entry[0] =
				(max_reloc_entry < reserved_block) ?
				max_reloc_entry : reserved_block;

			ofs = (PXA_BEGIN_SLOT << mtd->writesize_shift)
					+ ((uint64_t)new_bbm->main_block
						<< mtd->erasesize_shift);

			printk(KERN_INFO "\nBegin to write main block..\n");
			ret = mtd_write(mtd, ofs, mtd->writesize, &retlen,
					(void *)fbbt);
			if (ret) {
				printk(KERN_ERR "Write fbbt failed at %llx\n",
						ofs);
				failed = 1;
				continue;
			}

			ofs = ((PXA_BEGIN_SLOT + 1) << mtd->writesize_shift)
				+ ((uint64_t)new_bbm->main_block
						<< mtd->erasesize_shift);
			ret = mtd_write(mtd, ofs, mtd->writesize, &retlen,
					(void *)part);
			if (ret) {
				printk(KERN_ERR "Write part failed at %llx\n",
						ofs);
				failed = 1;
				continue;
			}

			ofs = ((PXA_BEGIN_SLOT + 2) << mtd->writesize_shift)
				+ ((uint64_t)new_bbm->main_block
						<< mtd->erasesize_shift);
			len = mtd->erasesize -
				(mtd->writesize*(PXA_BEGIN_SLOT + 2));
			ret = mtd_write(mtd, ofs, len, &retlen, backup_buf
				+ (mtd->writesize * (PXA_BEGIN_SLOT + 2)));
			if (ret) {
				printk(KERN_ERR "restore obm part failed!!\n");
				failed = 1;
			} else
				break;
		}

		if (boot_block >= BOOT_PRAT_MAX) {
			new_bbm->main_block = -1;
			printk(KERN_ERR "There is no good blocks in first %d"
					" blocks!\n You should use another"
					" flash now!!\n", BOOT_PRAT_MAX);
			return -EFAULT;
		}
	}

	/*
	 * try to find a good block with fbbt's help
	 * and back the main block to back block
	 */
	if (new_bbm->back_block == -1) {
		ofs = (uint64_t)new_bbm->main_block << mtd->erasesize_shift;
		ret = mtd_read(mtd, ofs, mtd->erasesize, &retlen, backup_buf);
		if (ret) {
			printk(KERN_ERR "Cannot load main boot block!!\n");
			return -EFAULT;
		}

		instr.callback = pxa3xx_bbm_callback;
		instr.len = mtd->erasesize;
		instr.addr = 0;
		for (block = 0; block < BOOT_PRAT_MAX; block ++,
				instr.addr += mtd->erasesize) {
			if (block == new_bbm->main_block
					|| boot_part_bad(mtd, instr.addr))
				continue;

			ret = mtd_erase(mtd, &instr);
			if (!ret) {
				printk(KERN_INFO "Got backup block at "
						"block %d\n", block);
				printk(KERN_INFO "\nBegin to write backup "
						"block..\n");
				ret = mtd_write(mtd, instr.addr,
					mtd->erasesize, &retlen, backup_buf);
				if (ret) {
					printk("Failed to backup to %llx\n",
							instr.addr);
					continue;
				}

				new_bbm->back_block = block;
				break;
			}
		}

		if (new_bbm->back_block == -1)
			printk(KERN_WARNING "Unable to recover backup boot "
					"block!!\n");
	}

	kfree(backup_buf);

	printk(KERN_INFO "done!!\n");
	return 0;
}

/*
 * bbm_type:
 * BBM_NONE:	recover the bbm according to original setting
 * BBM_LEGACY:	recover bbm as legacy bbm
 * BBM_NEW:	recover bbm as new bbm
 */
int pxa3xx_bbm_recovery(struct mtd_info *mtd, int bbm_type,
		struct reloc_item *item, int num, int reserve_last_page)
{
	struct pxa3xx_bbm *bbm = mtd->bbm;
	int ret;

	if (bbm && bbm->bbm_type != bbm_type) {
		pxa3xx_uninit_reloc_tb(mtd);
		bbm = mtd->bbm;
	}

	if (!bbm) {
		ret = pxa3xx_init_bbm(mtd, bbm_type);
		if (ret) {
			printk(KERN_ERR "Init failed!!!\n");
			return -EFAULT;
		}
	}

	if (bbm_type == BBM_NONE)
		bbm_type = bbm->bbm_type;

	switch (bbm_type) {
	case BBM_LEGACY:
		printk(KERN_INFO "Ready to recover bbm as legacy!\n");
		ret = recover_legacy_bbm(mtd, 1);
		break;

	case BBM_NEW:
		printk(KERN_INFO "Ready to recover bbm as new!\n");
		ret = recover_new_bbm(mtd, item, num, reserve_last_page);
		break;

	case BBM_NONE:
	default:
		printk(KERN_ERR "Cannot fulfill recovery bbm task!!!\n");
		ret = -EFAULT;
	}

	return ret;
}

static loff_t pxa3xx_reseved_sz(struct mtd_info *mtd)
{
	struct pxa3xx_bbm *bbm = mtd->bbm;
	struct pxa3xx_legacy_bbm *legacy_bbm;
	loff_t boundary_offset, reseved = 0;
	int reloc_boundary;

	if (bbm->bbm_type == BBM_LEGACY) {
		legacy_bbm = (struct pxa3xx_legacy_bbm *)bbm->data_buf;
		reloc_boundary = mtd_div_by_eb(mtd->size, mtd)
			- legacy_bbm->max_reloc_entry;
		boundary_offset = (uint64_t)reloc_boundary * mtd->erasesize;

		reseved = mtd->size - boundary_offset;
		mtd->size = boundary_offset;
	} else {
		printk(KERN_ERR "need implement later!!\n");
		BUG();
	}

	return reseved;
}
