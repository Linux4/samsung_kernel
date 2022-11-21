#ifndef __FASTUH_H__
#define __FASTUH_H__

#ifndef __ASSEMBLY__

/* For fastuH Command */
#define	APP_INIT	0
#define APP_RKP		1
#define APP_KDP		2
#define APP_HDM		6

#define FASTUH_APP_INIT		FASTUH_APPID(APP_INIT)
#define FASTUH_APP_RKP		FASTUH_APPID(APP_RKP)
#define FASTUH_APP_KDP		FASTUH_APPID(APP_KDP)
#define FASTUH_APP_HDM		FASTUH_APPID(APP_HDM)

#define FASTUH_PREFIX  UL(0xc300c000)
#define FASTUH_APPID(APP_ID)  ((UL(APP_ID) & UL(0xFF)) | FASTUH_PREFIX)

/* For uH Memory */
#define UH_NUM_MEM		0x00

#define FASTUH_LOG_START	0xB0200000
#define FASTUH_LOG_SIZE		0x40000

int fastuh_call(u64 app_id, u64 command, u64 arg0, u64 arg1, u64 arg2, u64 arg3);

struct test_case_struct {
	int (* fn)(void); //test case func
	char * describe;
};

#endif //__ASSEMBLY__
#endif //__FASTUH_H__
