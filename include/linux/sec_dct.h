/*
 * sec_dct.h -- Samsung Display Clock tunning
 */



#ifndef __SEC_DCT_H
#define __SEC_DCT_H

#include <linux/types.h>
#include <linux/fb.h>
#include <linux/printk.h>

#define MAX_DCT_NAME_LENGTH	50
#define MAX_DCT_DATA_LENGHT	4096

#define DCT_LOG_PRINT	0
#if DCT_LOG_PRINT
#define DCT_LOG(fmt, ...)	printk(KERN_INFO fmt, ##__VA_ARGS__)
#else
#define DCT_LOG(fmt, ...)
#endif

#define DCT_INTERFACE_VER	"1.0"

// =======================================================
// If you want to change value_type, dct_tinfo(sec_dct.c) should be changed , too.
// The order of value is important.
enum value_ctype {
	CTYPE_UCHAR = 0,
	CTYPE_USHORT,
	CTYPE_UINT,
	CTYPE_ULONG,
	CTYPE_ULONGLONG,
	CTYPE_MAX
};

enum value_type {
	DCT_TYPE_U8 = 0,
	DCT_TYPE_U16,
	DCT_TYPE_U32,
	DCT_TYPE_U64,
	DCT_TYPE_MAX
};
// =======================================================

enum state_type {
	DCT_STATE_NODATA = 0,
	DCT_STATE_DATA,
	DCT_STATE_APPLYED,
	DCT_STATE_ERROR_KERNEL,
	DCT_STATE_ERROR_DATA,
	DCT_STATE_MAX
};

struct sec_dct_typeinfo_t
{
	char name[MAX_DCT_NAME_LENGTH];
	char *format;
	int size;
};

struct sec_dct_ctypeinfo_t
{
	//char name[MAX_NAME_LENGTH];
	char format[10];
	int size;
};

struct sec_dct_datainfo_t {
	char name[MAX_DCT_NAME_LENGTH];
	int type;
	void *data;
	int size;	
};


struct sec_dct_info_t
{
	void *data;
	int state;
	bool tunned;
	bool enabled;
	char *log;
	int (*set_allData)(char *);
	void (*applyData)(struct fb_info *);
	void (*finish_applyData)(void);
	void (*get_libname)(char *);
};

int dct_setData(char *name, int type, void *data, int size);
void dct_applyData(struct fb_info *info);
void dct_finish_applyData(void);

#endif
