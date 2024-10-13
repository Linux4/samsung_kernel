/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_UTILS_H__
#define __KUNIT_MOCK_UTILS_H__

#include "../utils.h"

#ifdef SLSI_UTILS_H__
#undef strtoint
#endif

#define wake_lock_init(args...)			((void) 0)
#define strtoint(args...)			kunit_mock_strtoint(args)


static int kunit_mock_strtoint(const char *s, int *res)
{
	int num = 0;
	bool is_minus = 0;

	if (s == NULL)
		return -1;
	if (s && s[0] == '-') {
		is_minus = 1;
		s++;
	}
	while (*s != '\0') {
		num = num * 10 + *s - '0';
		s++;
	}
	*res = is_minus ? num * -1 : num;
	return *res;
}
#endif
