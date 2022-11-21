/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __FPRINT_SECURE_H
#define __FPRINT_SECURE_H

#if defined(CONFIG_SEC_LENTIS_PROJECT) || defined(CONFIG_SEC_TRLTE_PROJECT) \
	|| defined(CONFIG_SEC_KCCAT6_PROJECT) || defined(CONFIG_SEC_TBLTE_PROJECT)
#ifdef CONFIG_SEC_FACTORY
#undef ENABLE_SENSORS_FPRINT_SECURE
#else
#ifdef CONFIG_SENSORS_VFS7XXX
#define ENABLE_SENSORS_FPRINT_SECURE
#else
#define ENABLE_SENSORS_FPRINT_SECURE
#endif
#endif /* CONFIG_SEC_FACTORY */
#endif /* CONFIG_SEC_TRLTE_PROJECT, CONFIG_SEC_LENTIS_PROJECT */

#define FP_SPI_FIRST	55
#define FP_SPI_CS		57
#define FP_SPI_LAST	58

#endif /* __FPRINT_SECURE_H */
