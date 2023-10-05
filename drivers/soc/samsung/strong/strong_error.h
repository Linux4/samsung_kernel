/*
 * strong_error.h - Samsung Strong Mailbox error return
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __STRONG_ERROR_H__
#define __STRONG_ERROR_H__

/*****************************************************************************/
/* Define error return							     */
/*****************************************************************************/
/* Common */
#define RV_SUCCESS                                      0x0000
#define RV_PASS                                         0x1234
#define RV_FAIL                                         0x7432

/* tx err */
#define RV_MB_ST_ERR_CNT_TIMEOUT			0xC001
#define RV_MB_ST_ERR_TX_LEN_ALIGN			0xC002
#define RV_MB_ST_ERR_REMAIN_LEN_OVERFLOW		0xC003
#define RV_MB_ST_ERR_NOT_IDLE				0xC004
/* rx err */
#define RV_MB_ST_ERR_NOT_REQ				0xC101
#define RV_MB_ST_ERR_INVALID_CMD			0xC102
#define RV_MB_ST_ERR_DATA_LEN_OVERFLOW			0xC103
#define RV_MB_ST_ERR_INVALID_REMAIN_LEN			0xC104
#define RV_MB_ST_ERR_REPEATED_BLOCK			0xC105
#define RV_MB_ST_ERR_RX_LEN_ALIGN			0xC106
#define RV_MB_ST_ERR_BUFFER_FULL			0xC107

/* tx err */
#define RV_MB_AP_ERR_CNT_TIMEOUT			0xC201
#define RV_MB_AP_ERR_TX_LEN_ALIGN			0xC202
#define RV_MB_AP_ERR_REMAIN_LEN_OVERFLOW		0xC203
#define RV_MB_AP_ERR_NOT_IDLE				0xC204
#define RV_MB_AP_ERR_STRONG_POWER_OFF			0xC205
/* rx err */
#define RV_MB_AP_ERR_NOT_REQ				0xC301
#define RV_MB_AP_ERR_INVALID_CMD			0xC302
#define RV_MB_AP_ERR_DATA_LEN_OVERFLOW			0xC303
#define RV_MB_AP_ERR_INVALID_REMAIN_LEN			0xC304
#define RV_MB_AP_ERR_REPEATED_BLOCK			0xC305
#define RV_MB_AP_ERR_RX_LEN_ALIGN			0xC306

/* pending wait err */
#define RV_MB_WAIT1_TIMEOUT				0xC401
#define RV_MB_WAIT2_TIMEOUT				0xC402

#endif /* __STRONG_ERROR_H__ */
