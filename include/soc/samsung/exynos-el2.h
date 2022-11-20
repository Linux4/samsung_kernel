/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - EL2 module
 * Author: Junho Choi <junhosj.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_EL2_H
#define __EXYNOS_EL2_H

/* EL2 crash buffer size for each CPU */
#define EL2_CRASH_BUFFER_SIZE				(0x1000)

/* Retry count for allocate EL2 crash buffer */
#define MAX_RETRY_COUNT					(8)

#ifndef __ASSEMBLY__

#endif	/* __ASSEMBLY__ */

#endif	/* __EXYNOS_EL2_H */
