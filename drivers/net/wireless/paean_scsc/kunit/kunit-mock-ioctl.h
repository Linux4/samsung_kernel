/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_IOCTL_H__
#define __KUNIT_MOCK_IOCTL_H__

#include "../ioctl.h"

#define slsi_ioctl(args...)			kunit_mock_slsi_ioctl(args)
#define slsi_get_private_command_args(args...)	kunit_mock_slsi_get_private_command_args(args)
#define slsi_get_sta_info(args...)		kunit_mock_slsi_get_sta_info(args)


static int kunit_mock_slsi_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	return 0;
}

static struct slsi_ioctl_args *kunit_mock_slsi_get_private_command_args(char *buffer, int buf_len, int max_arg_count)
{
	return NULL;
}

static int kunit_mock_slsi_get_sta_info(struct net_device *dev, char *command, int buf_len)
{
	return 0;
}
#endif
