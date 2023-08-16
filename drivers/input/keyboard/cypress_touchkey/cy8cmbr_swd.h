#ifndef _CY8CMBR_SWD_H
#define _CY8CMBR_SWD_H


int cy8cmbr_swd_program(struct device *dev,
	int (*power)(struct device *dev2, bool on),
	int swdio_gpio,
	int swdck_gpio,
	const u8* fw_img,		// NULL means hex firmware download
	size_t fw_img_size);

#endif
