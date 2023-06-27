/*
 *
 * Copyright (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _UIO_HANTRO_H_
#define _UIO_HANTRO_H_

#define IOP_MAGIC	'h'

#define HANTRO_CMD_POWER_ON		_IO(IOP_MAGIC, 0)
#define HANTRO_CMD_POWER_OFF		_IO(IOP_MAGIC, 1)
#define HANTRO_CMD_CLK_ON		_IO(IOP_MAGIC, 2)
#define HANTRO_CMD_CLK_OFF		_IO(IOP_MAGIC, 3)
#define HANTRO_CMD_LOCK			_IO(IOP_MAGIC, 4)
#define HANTRO_CMD_UNLOCK		_IO(IOP_MAGIC, 5)
#define HANTRO_CMD_GET_INS_ID		_IO(IOP_MAGIC, 6)

#endif
