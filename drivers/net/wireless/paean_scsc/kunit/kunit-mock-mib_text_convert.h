/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_MIB_TEXT_CONVERT_H__
#define __KUNIT_MOCK_MIB_TEXT_CONVERT_H__

#include "../mib_text_convert.h"

#define CsrWifiMibConvertText(args...)	kunit_mock_CsrWifiMibConvertText(args)


static bool kunit_mock_CsrWifiMibConvertText(const char *mibText, struct slsi_mib_data *mibDataSet,
					     struct slsi_mib_data *mibDataGet)
{
	return false;
}
#endif
