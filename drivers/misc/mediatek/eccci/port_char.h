/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


extern int rawbulk_push_upstream_buffer(int transfer_id, const void *buffer,
		unsigned int length);

int scan_image_list(int md_id, char fmt[], int out_img_list[], int img_list_size);

#ifndef CONFIG_MTK_ECCCI_C2K
#ifdef CONFIG_MTK_SVLTE_SUPPORT
extern void c2k_reset_modem(void);
#endif
#endif
