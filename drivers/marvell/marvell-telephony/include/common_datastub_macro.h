/*

 *(C) Copyright 2006, 2007 Marvell DSPC Ltd. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 *
 * FILENAME:   common_datastub_macro.h
 * PURPOSE:    data_channel MACRO used by stub
 */

#ifndef __COMMON_DATASTUB_MACRO_H__
#define __COMMON_DATASTUB_MACRO_H__

#define CCIDATASTUB_IOC_MAGIC 246

/* For CSD */
#define CCIDATASTUB_DATASVGHANDLE  _IOW(CCIDATASTUB_IOC_MAGIC, 1, int)
#define CCIDATASTUB_DATAHANDLE  _IOW(CCIDATASTUB_IOC_MAGIC, 2, int)
#define CCIDATASTUB_CS_CHNOK  _IOW(CCIDATASTUB_IOC_MAGIC, 3, int)

/* For PSD */
#define CCIDATASTUB_GCFDATA _IOW(CCIDATASTUB_IOC_MAGIC, 4, int)
#define CCIDATASTUB_TOGGLE_DATA_ENABLE_DISABLE _IOW(CCIDATASTUB_IOC_MAGIC, 5, int)

#endif /* __COMMON_DATASTUB_MACRO_H__ */
