// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>
#include "../mib.h"

/* unit test function definition*/


static void test_CsrIsSpace(struct kunit *test)
{
	u8 c = '\t';

	KUNIT_EXPECT_TRUE(test, CsrIsSpace(c));
}

static void test_CsrStrDup(struct kunit *test)
{
	char *string = "0x12ab";
	char *result;

	result = CsrStrDup(string);
	kfree(result);
	CsrStrDup(NULL);
}

static void test_CsrStrNICmp(struct kunit *test)
{
	char *string1 = "0x12ab";
	char *string2 = "0x11ab";

	CsrStrNICmp(string1, string2, strlen(string1));
	CsrStrNICmp(string2, string1, strlen(string2));
}

static void test_CsrHexStrToUint8(struct kunit *test)
{
	char *string = "0x1234";
	u8 returnValue = 0;

	KUNIT_EXPECT_TRUE(test, CsrHexStrToUint8(string, &returnValue));

	string = "zxcv";
	KUNIT_EXPECT_FALSE(test, CsrHexStrToUint32(string, &returnValue));
}

static void test_CsrHexStrToUint16(struct kunit *test)
{
	char *string = "0x1234";
	u16 returnValue = 0;

	KUNIT_EXPECT_TRUE(test, CsrHexStrToUint16(string, &returnValue));

	string = "zxcv";
	KUNIT_EXPECT_FALSE(test, CsrHexStrToUint32(string, &returnValue));
}

static void test_CsrHexStrToUint32(struct kunit *test)
{
	char *string = "0x1234";
	u32 returnValue = 0;

	KUNIT_EXPECT_TRUE(test, CsrHexStrToUint32(string, &returnValue));

	string = "zxcv";
	KUNIT_EXPECT_FALSE(test, CsrHexStrToUint32(string, &returnValue));
}

static void test_CsrWifiMibConvertStrToUint16(struct kunit *test)
{
	char *string = "1234";
	u16 returnValue = 0;

	KUNIT_EXPECT_TRUE(test, CsrWifiMibConvertStrToUint16(string, &returnValue));

	string = "zxcv";
	KUNIT_EXPECT_FALSE(test, CsrWifiMibConvertStrToUint16(string, &returnValue));

	string = "abcd";
	KUNIT_EXPECT_FALSE(test, CsrWifiMibConvertStrToUint16(string, &returnValue));
}

static void test_CsrWifiMibConvertStrToUint32(struct kunit *test)
{
	char *string = "1234";
	u32 returnValue = 0;

	KUNIT_EXPECT_TRUE(test, CsrWifiMibConvertStrToUint32(string, &returnValue));

	string = "zxcv";
	KUNIT_EXPECT_FALSE(test, CsrWifiMibConvertStrToUint32(string, &returnValue));

	string = "abcd";
	KUNIT_EXPECT_FALSE(test, CsrWifiMibConvertStrToUint16(string, &returnValue));
}

static void test_CsrWifiMibConvertTextParseLine(struct kunit *test)
{
	struct slsi_mib_data *mibDataSet = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);
	struct slsi_mib_data *mibDataGet = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);
	char *string = "#1234";

	CsrWifiMibConvertTextParseLine(string, mibDataSet, mibDataGet);

	if (mibDataSet->data) {
		kfree(mibDataSet->data);
		mibDataSet->data = NULL;
	}
	if (mibDataGet->data) {
		kfree(mibDataGet->data);
		mibDataGet->data = NULL;
	}

	string = "\t1234";
	CsrWifiMibConvertTextParseLine(string, mibDataSet, mibDataGet);
	if (mibDataSet->data) {
		kfree(mibDataSet->data);
		mibDataSet->data = NULL;
	}
	if (mibDataGet->data) {
		kfree(mibDataGet->data);
		mibDataGet->data = NULL;
	}

	string = "...abcd";
	CsrWifiMibConvertTextParseLine(string, mibDataSet, mibDataGet);

	if (mibDataSet->data) {
		kfree(mibDataSet->data);
		mibDataSet->data = NULL;
	}
	if (mibDataGet->data) {
		kfree(mibDataGet->data);
		mibDataGet->data = NULL;
	}

	string = "....1234";
	CsrWifiMibConvertTextParseLine(string, mibDataSet, mibDataGet);

	if (mibDataSet->data) {
		kfree(mibDataSet->data);
		mibDataSet->data = NULL;
	}
	if (mibDataGet->data) {
		kfree(mibDataGet->data);
		mibDataGet->data = NULL;
	}

	string = "\"1234";
	CsrWifiMibConvertTextParseLine(string, mibDataSet, mibDataGet);

	if (mibDataSet->data) {
		kfree(mibDataSet->data);
		mibDataSet->data = NULL;
	}
	if (mibDataGet->data) {
		kfree(mibDataGet->data);
		mibDataGet->data = NULL;
	}

	string = "1=true";
	CsrWifiMibConvertTextParseLine(string, mibDataSet, mibDataGet);

	if (mibDataSet->data) {
		kfree(mibDataSet->data);
		mibDataSet->data = NULL;
	}
	if (mibDataGet->data) {
		kfree(mibDataGet->data);
		mibDataGet->data = NULL;
	}

	string = "1=false";
	CsrWifiMibConvertTextParseLine(string, mibDataSet, mibDataGet);

	if (mibDataSet->data) {
		kfree(mibDataSet->data);
		mibDataSet->data = NULL;
	}
	if (mibDataGet->data) {
		kfree(mibDataGet->data);
		mibDataGet->data = NULL;
	}

	string = "1=\"1234";
	CsrWifiMibConvertTextParseLine(string, mibDataSet, mibDataGet);

	if (mibDataSet->data) {
		kfree(mibDataSet->data);
		mibDataSet->data = NULL;
	}
	if (mibDataGet->data) {
		kfree(mibDataGet->data);
		mibDataGet->data = NULL;
	}

	string = "1=[zxcv";
	CsrWifiMibConvertTextParseLine(string, mibDataSet, mibDataGet);
	if (mibDataSet->data) {
		kfree(mibDataSet->data);
		mibDataSet->data = NULL;
	}
	if (mibDataGet->data) {
		kfree(mibDataGet->data);
		mibDataGet->data = NULL;
	}

	string = "1=-abcd";
	CsrWifiMibConvertTextParseLine(string, mibDataSet, mibDataGet);
	if (mibDataSet->data) {
		kfree(mibDataSet->data);
		mibDataSet->data = NULL;
	}
	if (mibDataGet->data) {
		kfree(mibDataGet->data);
		mibDataGet->data = NULL;
	}

	string = "1=-1234";
	CsrWifiMibConvertTextParseLine(string, mibDataSet, mibDataGet);
	if (mibDataSet->data) {
		kfree(mibDataSet->data);
		mibDataSet->data = NULL;
	}
	if (mibDataGet->data) {
		kfree(mibDataGet->data);
		mibDataGet->data = NULL;
	}

	string = "1=abcd";
	CsrWifiMibConvertTextParseLine(string, mibDataSet, mibDataGet);
	if (mibDataSet->data) {
		kfree(mibDataSet->data);
		mibDataSet->data = NULL;
	}
	if (mibDataGet->data) {
		kfree(mibDataGet->data);
		mibDataGet->data = NULL;
	}

	string = "1=1234";
	CsrWifiMibConvertTextParseLine(string, mibDataSet, mibDataGet);
	if (mibDataSet->data) {
		kfree(mibDataSet->data);
		mibDataSet->data = NULL;
	}
	if (mibDataGet->data) {
		kfree(mibDataGet->data);
		mibDataGet->data = NULL;
	}
}

static void test_CsrWifiMibConvertTextAppend(struct kunit *test)
{
	char *string = "\nabcd";
	struct slsi_mib_data *mibDataSet = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);
	struct slsi_mib_data *mibDataGet = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);

	KUNIT_EXPECT_FALSE(test, CsrWifiMibConvertTextAppend(string, mibDataSet, mibDataGet));

	if (mibDataSet->data) {
		kfree(mibDataSet->data);
		mibDataSet->data = NULL;
	}
	if (mibDataGet->data) {
		kfree(mibDataGet->data);
		mibDataGet->data = NULL;
	}
}

static void test_CsrWifiMibConvertText(struct kunit *test)
{
	char *string = "\n\0";
	struct slsi_mib_data *mibDataSet = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);
	struct slsi_mib_data *mibDataGet = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);

	KUNIT_EXPECT_TRUE(test, CsrWifiMibConvertText(string, mibDataSet, mibDataGet));

	if (mibDataSet->data) {
		kfree(mibDataSet->data);
		mibDataSet->data = NULL;
	}
	if (mibDataGet->data) {
		kfree(mibDataGet->data);
		mibDataGet->data = NULL;
	}
}


/* Test fictures */
static int mib_text_convert_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void mib_text_convert_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case mib_text_convert_test_cases[] = {
	KUNIT_CASE(test_CsrIsSpace),
	KUNIT_CASE(test_CsrStrDup),
	KUNIT_CASE(test_CsrStrNICmp),
	KUNIT_CASE(test_CsrHexStrToUint8),
	KUNIT_CASE(test_CsrHexStrToUint16),
	KUNIT_CASE(test_CsrHexStrToUint32),
	KUNIT_CASE(test_CsrWifiMibConvertStrToUint16),
	KUNIT_CASE(test_CsrWifiMibConvertStrToUint32),
	KUNIT_CASE(test_CsrWifiMibConvertTextParseLine),
	KUNIT_CASE(test_CsrWifiMibConvertTextAppend),
	KUNIT_CASE(test_CsrWifiMibConvertText),
	{}
};

static struct kunit_suite mib_text_convert_test_suite[] = {
	{
		.name = "mib_text_convert-test",
		.test_cases = mib_text_convert_test_cases,
		.init = mib_text_convert_test_init,
		.exit = mib_text_convert_test_exit,
	}
};

kunit_test_suites(mib_text_convert_test_suite);
