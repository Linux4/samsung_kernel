/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SAP_DBG_H__
#define __KUNIT_MOCK_SAP_DBG_H__

#include "../sap_dbg.h"

#define sap_dbg_init(args...)		kunit_mock_sap_dbg_init(args)
#define sap_dbg_deinit(args...)		kunit_mock_sap_dbg_deinit(args)


static int kunit_mock_sap_dbg_init(void)
{
	return 0;
}

static int kunit_mock_sap_dbg_deinit(void)
{
	return 0;
}
#endif
