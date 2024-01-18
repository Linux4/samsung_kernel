/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Display Profiler Mask Image Generator
 * Author: Kimyung Lee <kernel.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include "maskgen.h"

inline bool append_enc_data(struct mprint_props *p, unsigned char type, int size) {
	struct mprint_packet *curr = &(p->pkts[p->pkts_pos]);
	struct mprint_config *c = p->conf;

	if (c->resol_x < p->cursor_x) {
		if (c->debug)
			pr_warn("%s: overflow x, skip %d\n", __func__, size);
		p->cursor_x += size;
		return true;
	}

	p->cursor_x += size;

	if (p->cursor_x > c->resol_x) {
		if (c->debug)
			pr_warn("%s: overflow x, skip %d, size set to %d\n", __func__,
				p->cursor_x - c->resol_x, size - (p->cursor_x - c->resol_x));
		size = size - (p->cursor_x - c->resol_x);
	}

	if (type == curr->type) {
		if ((curr->size + size) < BGP_DATA_MAX) {
			curr->size += size;
			return true;
		}
	}

	if (p->pkts_pos + 1 >= p->pkts_max) {
		if (c->debug)
			pr_warn("%s: overflow pkts, %d %d\n", __func__, p->cursor_y, p->cursor_x);
		return false;
	}

	p->pkts_pos += 1;
	curr = &(p->pkts[p->pkts_pos]);
	curr->type = type;
	curr->size = size;
	return true;

}

int char_to_mask_img(struct mprint_props *p, char *str) {
	struct mprint_config *c;
	int i, row, col, scale, scale_y, strsize;
	unsigned char slice;


	if (!p) {

		pr_err("%s: invalid props\n", __func__);
		return -EINVAL;
	}

	if (!str) {

		pr_err("%s: invalid string\n", __func__);
		return -EINVAL;
	}

	strsize = strlen(str);

	if (strsize < 1) {
		pr_err("%s: strlen is %d\n", __func__, strsize);
		return -EINVAL;
	}
	
	c = p->conf;
	scale = c->scale;
	if (scale < 0) {
		pr_warn("%s: invalid scale %d, set force to 1\n", __func__, scale);
		scale = 1;
	}

	p->cursor_x = p->cursor_y = 0;
	p->pkts_pos = p->pkts_size = 0;
	p->pkts[0].type = MPRINT_PACKET_TYPE_TRANS;
	p->pkts[0].size = 0;

/*
	//y padding
	for (; p->cursor_y < c->padd_y; p->cursor_y++) {
		p->cursor_x = 0;
		if (!append_enc_data(p, MPRINT_PACKET_TYPE_TRANS, c->resol_x))
			goto err_limit_exceed;
	}
*/

	//print slice(row) of font
	for (row = 0; row < MASK_CHAR_HEIGHT; row++) {
		for (scale_y = 0; scale_y < c->scale; scale_y++) {
			p->cursor_x = 0;
			p->cursor_y++;

			//y skip (for reduce data size)
			if ((c->skip_y > 0) && (p->cursor_y % c->skip_y == 0)) {
				//skip line every skip_y
				if (!append_enc_data(p, MPRINT_PACKET_TYPE_TRANS, c->resol_x))
					goto err_limit_exceed;
				continue;
			}
			
			//x padding
			if (c->padd_x > 0) {
				//add padding x
				if (!append_enc_data(p, MPRINT_PACKET_TYPE_TRANS, c->padd_x))
					goto err_limit_exceed;
			}

			//iterate string
			for (i = 0; i < strsize; i++) {
				slice = MASK_CHAR_MAP[(int)str[i]][row];
				for (col = 0; col < MASK_CHAR_WIDTH; col++) {
					if (!append_enc_data(p,
						((slice >> ((MASK_CHAR_WIDTH - 1) - col)) & 0x1) ? MPRINT_PACKET_TYPE_BLACK : MPRINT_PACKET_TYPE_TRANS,
						1 * c->scale))
						goto err_limit_exceed;
				}
				if (c->spacing > 0) {
					if (!append_enc_data(p, MPRINT_PACKET_TYPE_TRANS, c->spacing))
						goto err_limit_exceed;
				}
			}

			//processing remain pixels on width
			if (c->resol_x > p->cursor_x) {
				//add remain x
				if (!append_enc_data(p, MPRINT_PACKET_TYPE_TRANS, c->resol_x - p->cursor_x))
					goto err_limit_exceed;
			}

			if (c->resol_x != p->cursor_x) {
				pr_warn("%s: line %d pxl cnt mismatch! %d\n", __func__, p->cursor_y, p->cursor_x);
			}
			//scale_y end
		}

	}

	//processing remain pixels on height
	for (; p->cursor_y < c->resol_y; p->cursor_y++) {
		p->cursor_x = 0;
		//below text
		if (!append_enc_data(p, MPRINT_PACKET_TYPE_TRANS, c->resol_x))
			goto err_limit_exceed;
	}

	p->pkts_size = p->pkts_pos + 1;

	if (c->debug)
		pr_info("%s: created data packet, %d %d\n", __func__, p->pkts_size, p->cursor_y);

	for (i = 0; i < p->pkts_size; i++) {
		if (p->data_max <= i * 2) {
			pr_err("%s: data output limit exceeded! %d %d\n", __func__, p->pkts_size, p->data_max);
			break;
		}
		p->data[i * 2] = ((p->pkts[i].type << 6) & 0xC0) | ((p->pkts[i].size >> 8) & 0x3F);
		p->data[i * 2 + 1] = p->pkts[i].size & 0xFF;
	}
	p->data_size = i * 2;

	//16byte align
	if ((p->data_size % 16) > 0) {
		i = 16 - (p->data_size % 16);
		memset(p->data + p->data_size, 0x00, i);
		p->data_size += i;
	}

	if (c->debug)
		print_hex_dump(KERN_ERR, "char_to_mask_img ", DUMP_PREFIX_ADDRESS, 16, 1, p->data, p->data_size, false);

	return p->data_size;

err_limit_exceed:
	pr_err("%s: buffer limit exceeded! %d %d\n", __func__, p->cursor_y, p->cursor_x);
	return -ENOMEM;
}

