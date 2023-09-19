/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_INI_CONFIG_H__
#define __KUNIT_MOCK_INI_CONFIG_H__

#define slsi_process_ini_config_file(args...)	kunit_mock_slsi_process_ini_config_file(args)


static void kunit_mock_slsi_process_ini_config_file(struct slsi_dev *sdev)
{
	return;
}

#endif
