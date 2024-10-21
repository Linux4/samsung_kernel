/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2022 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef MCLOADFORMAT_H
#define MCLOADFORMAT_H

#include <linux/uaccess.h>	/* u32 and friends */
#include "public/mc_user.h"	/* struct mc_uuid_t */

/** MCLF flags */
/** Service has no WSM control interface. */
#define MC_SERVICE_HEADER_FLAGS_NO_CONTROL_INTERFACE	BIT(1)

/**
 * @defgroup MCLF_VER_V25   MCLF Version 25
 * @ingroup MCLF_VER
 *
 * @addtogroup MCLF_VER_V25
 */

/*
 * GP TA identity.
 */
struct identity {
	/** GP TA login type */
	u32	login_type;
	/** GP TA login data */
	union {
		u8		login_data[16];
		gid_t		gid; /* Requested group id */
		struct {
			uid_t	euid;
			uid_t	ruid;
		} uid;
	};
};

/**
 * Version 2.5 MCLF header.
 */
struct mclf_header {
	/** TEE defined */
	u8	tee_defined_1[8];
	/** Service flags */
	u32	flags;
	/** TEE defined */
	u8	tee_defined_2[12];
	/** Loadable service unique identifier (UUID) */
	struct mc_uuid_t	uuid;
	/** TEE defined */
	u8	tee_defined_3[80];
	/** Reserved for future use */
	u8	rfu_padding_2[8];
};

#endif /* MCLOADFORMAT_H */
