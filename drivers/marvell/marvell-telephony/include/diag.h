/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#ifndef _DIAG_H_
#define _DIAG_H_

#include "linux_driver_types.h"

struct diagheader {
	UINT16 seqNo;
	UINT16 packetlen;
	UINT32 msgType;
};/* don't change the definition. variable definition order sensitive */

struct diagmsgheader {
	struct diagheader diagHeader;
	int procId;
};

#endif
