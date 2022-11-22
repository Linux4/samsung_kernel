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
#define CCIDATASTUB_CONN_CONF _IOW(CCIDATASTUB_IOC_MAGIC, 2, int)
#define CCIDATASTUB_ACT_CONF _IOW(CCIDATASTUB_IOC_MAGIC, 3, int)
#define CCIDATASTUB_START  _IOW(CCIDATASTUB_IOC_MAGIC, 1, int)
#define CCIDATASTUB_DATAHANDLE  _IOW(CCIDATASTUB_IOC_MAGIC, 4, int)
#define CCIDATASTUB_DATASVGHANDLE  _IOW(CCIDATASTUB_IOC_MAGIC, 5, int)
#define CCIDATASTUB_LINKSTATUS  _IOW(CCIDATASTUB_IOC_MAGIC, 6, int)
#define CCIDATASTUB_CHNOK  _IOW(CCIDATASTUB_IOC_MAGIC, 7, int)
#define CCIDATASTUB_CHOK  _IOW(CCIDATASTUB_IOC_MAGIC, 8, int)
#define CCIDATASTUB_GCFDATA _IOW(CCIDATASTUB_IOC_MAGIC, 9, int)
#define CCIDATASTUB_REMOVE_ALLCH  _IOW(CCIDATASTUB_IOC_MAGIC, 10, int)
#define CCIDATASTUB_CS_CHNOK  _IOW(CCIDATASTUB_IOC_MAGIC, 11, int)
#define CCIDATASTUB_CS_CHOK  _IOW(CCIDATASTUB_IOC_MAGIC, 12, int)
#define CCIDATASTUB_GCFDATA_REMOTE _IOW(CCIDATASTUB_IOC_MAGIC, 13, int)

#define RETURN_ETH_HDR { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00 }

#define CIDATATTY_TTY_MINORS            3

#endif /* __COMMON_DATASTUB_MACRO_H__ */

