#ifndef __MCPS_UTILS_H__
#define __MCPS_UTILS_H__

#include "mcps_logger.h"

static inline unsigned int parseUInt(const char *buf, int *error)
{
	unsigned int ret = 0;

	if (buf == NULL) {
		mcps_loge("wrong string : NULL");
		goto fail;
	}

	if (kstrtouint(buf, 0, &ret)) {
		mcps_loge("wrong string : %s", buf);
		goto fail;
	}

	return ret;

fail:
	*error = -EINVAL;

	return ret;
}

#endif