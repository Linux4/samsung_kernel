// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2022 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#ifndef __SEC_OF_H__
#define __SEC_OF_H__

#include <linux/kernel.h>
#include <linux/of.h>

#ifdef CONFIG_OF
static inline int sec_of_parse_reg_prop(struct device_node *np,
		phys_addr_t *base, phys_addr_t *size)
{
	const __be32 *cell;
	int address_cells;
	int size_cells;

	cell = of_get_property(np, "reg", NULL);
	if (IS_ERR_OR_NULL(cell))
		return -EINVAL;

	/* NOTE: 'of_n_addr_cells' and 'of_n_size_cells' always
	 * search from the parent node except 'root'.
	 * To use '#address-cells' and '#size-cells' of the current node,
	 * these 'if' statements are rquired.
	 */
	if (of_property_read_u32(np, "#address-cells", &address_cells))
		address_cells = of_n_addr_cells(np);

	if (of_property_read_u32(np, "#size-cells", &size_cells))
		size_cells = of_n_size_cells(np);

	*base = (phys_addr_t)of_read_number(cell, address_cells);
	*size = (phys_addr_t)of_read_number(cell + address_cells, size_cells);

	return 0;
}
#else
static inline int sec_of_parse_reg_prop(struct device_node *np, phys_addr_t *base, phys_addr_t *size) { return -ENODEV; }
#endif

#endif /* __SEC_OF_H__ */
