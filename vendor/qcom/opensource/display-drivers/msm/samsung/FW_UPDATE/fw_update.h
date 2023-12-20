/*
*
 * Source file for Display FW Update Driver
 *
 * Copyright (c) 2023 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

/*
 * FW_UPDATE interface
 *
 * Author: Samsung display driver team
 * Company:  Samsung Electronics
 */

 #ifndef _FW_UPDATE_H_
 #define _FW_UPDATE_H_

/* Align & Paylod Size for Embedded mode Transfer */
#define MAX_PAYLOAD_SIZE 200

/* Align & Paylod Size for Non-Embedded mode(Mass) Command Transfer */
#define MASS_CMD_ALIGN 256
#define MAX_PAYLOAD_SIZE_MASS 0xFFFF0 /* 983,024 */



#endif
