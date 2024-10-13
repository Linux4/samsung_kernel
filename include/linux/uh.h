#ifndef __UH_H__
#define __UH_H__

#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <linux/const.h>

/* For uH Command */
#define APP_INIT	0
#define APP_RKP		1
#define APP_KDP		2
#define APP_CFP		3
#define APP_HDM		6

#define UH_PREFIX		UL(0xc300c000)
#define UH_APPID(APP_ID)	((UL(APP_ID) & UL(0xFF)) | UH_PREFIX)

enum __UH_APP_ID {
	UH_INIT = UH_APPID((int)APP_INIT),
	UH_APP_RKP = UH_APPID((int)APP_RKP),
	UH_APP_KDP = UH_APPID((int)APP_KDP),
	UH_APP_CFP = UH_APPID((int)APP_CFP),
	UH_APP_HDM = UH_APPID((int)APP_HDM),
};

/* For uH Memory */
#define UH_LOG_START	0xB0200000
#define UH_LOG_SIZE		0x40000

/* For test */
struct test_case {
	int (* fn)(void); //test case func
	char * describe;
};

unsigned long _uh_call(u64 app_id, u64 command, u64 arg0, u64 arg1, u64 arg2, u64 arg3);
inline static void uh_call(u64 app_id, u64 command, u64 arg0, u64 arg1, u64 arg2, u64 arg3)
{
	_uh_call(app_id, command, arg0, arg1, arg2, arg3);
}

#endif //__ASSEMBLY__
#endif //__UH_H__
