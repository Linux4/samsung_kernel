// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/miscdevice.h>

#include "is-interface-ddk.h"
#include "pablo-obte.h"

#define JSON_SIZE			0x00800000

#define LIB_ISP_TUNING_ADDR		(DDK_LIB_ADDR + 0x000000c0)
#define PABLO_SHARE_BASE_OFFSET		0x01FC0000
#define PABLO_SHARE_BASE_SIZE		0x00010000
#define AF_LOG_V5_SIZE			0x00020000 /* 128KB for each stream */

#define UNKNOWN_INSTANCE		0xFFFFFFFF
#define INVALID_ORDER_BAYER		4 /* There are four types of Bayer(0,1,2,3) */

#define BLOCK_MAX			64
#define BLOCK_REGISTER_MAX	640
#define ALL_REGISTER_MAX	(BLOCK_REGISTER_MAX * BLOCK_MAX)

#define BLOCK_MODULE_NAME_MAX		64
#define MAX_STRIPE_NUM		64

#define TUNING_DRV_MAGIC_NUMBER		0x20191218

#define TOTALCOUNT_SSX_ID		8
#define TOTALCOUNT_MODULE		8

#define TUNE_ID_REGISTER_DUMP_BY_OBTE	0x20210317
#define MAXSIZE_BUF_NAME		32
#define MAXSIZE_CONVERSION_TABLE 16

#define PABLO_OBTE_INIT	_IOW('S', 0x01, int)
#define PABLO_OBTE_WRITE	_IOW('S', 0x02, int)
#define PABLO_SET_TUNEID	_IOW('S', 0x03, int)
#define PABLO_OBTE_READ	_IOR('S', 0x04, int)
#define PABLO_SET_ISPID	_IOW('S', 0x05, int)
#define PABLO_SHAREBASE_READ	_IOR('S', 0x06, int)
#define PABLO_DEBUGBASE_READ	_IOR('S', 0x07, int)
#define PABLO_SENSOR_INFO_CHG	_IOW('S', 0x08, int)
#define PABLO_DDK_VERSION	_IOR('S', 0x09, int)
#define PABLO_AFLOG_READ	_IOR('S', 0x0A, int)
#define PABLO_SETFILE_VERSION	_IOR('S', 0x0B, int)
#define PABLO_TUNE_STREAM_TYPE	_IOR('S', 0x0C, int)
#define PABLO_TUNE_MODE_TYPE	_IOR('S', 0x0D, int)
#define PABLO_TUNE_EMULATOR_TYPE	_IOR('S', 0x0E, int)
#define PABLO_TUNE_SENSOR_POSITION	_IOR('S', 0x0F, int)
#define PABLO_TUNE_SET_DUMP_STAT_INFO	_IOW('S', 0x10, int)
#define PABLO_TUNE_GET_DUMP_STAT_INFO	_IOR('S', 0x11, int)
#define PABLO_TUNE_SET_NFD_INFO_PARAM	_IOW('S', 0x12, int)
#define PABLO_OBTE_READ_UPPER_BUFFER	_IOR('S', 0x13, int)
#define PABLO_TUNE_SET_DUMP_REGISTERS_INFO	_IOR('S', 0x14, int)
#define PABLO_GET_DEBUG_SIZE_INFO	_IOR('S', 0x15, int)
#define PABLO_SET_VMALLOC_MEMORY	_IOW('S', 0x16, int)

enum tune_type {
	TUNE_MAIN_STREAM,
	TUNE_REPROCESS_STREAM,
	TUNE_MAX,
};

enum tune_mode_type {
	TUNE_MODE_NORMAL                    = 0,
	TUNE_MODE_STRIPEPROCESS             = 1,
	TUNE_MODE_SUPERNIGHTSHOT_DRC_OFF    = 2,
	TUNE_MODE_SUPERNIGHTSHOT_DRC_ON     = 3,
	TUNE_MODE_CAPTURETNRPROCESS         = 4,
	TUNE_MODE_VIDEOHDR                  = 5,
	TUNE_MODE_ZSL_STRIPEPROCESS         = 6,
	TUNE_MODE_RGB_INPUT_TO_ITP          = 7,
	TUNE_MODE_PREVIEWTNRPROCESS         = 8, /* == TUNE_MODE_VIDEOTNRPROCESS*/
	TUNE_MODE_HF_DECOMPOSITION          = 9,
	TUNE_MODE_VIDEOTNRPROCESS_WITH_HF   = 10,
	TUNE_MODE_VIDEOTNRPROCESS_WITH_ZSL_STRIPEPROCESS   = 11,
	UNKNOWN_TUNE_MODE_TYPE              = 0xFF,
	TUNE_MODE_MAX,
};

enum tune_emulator_type {
	TUNE_EMULATOR_OFF,
	TUNE_EMULATOR_ON,
	TUNE_EMULATOR_MAX,
};

enum json_type {
	JASON_TYPE_READ,
	JASON_TYPE_WRITE,
	JASON_TYPE_MAX,
};

typedef struct lib_tuning_config {
	/*
	 * update_shared_region
	 * 0: do not update on shared region address (shared_addr),
	 * 1: update on shared region address (shared_addr)
	 */
	u32 update_shared_region;
	uintptr_t   shared_addr;
	uintptr_t   af_tuning_addr;
	/* emul_repro_flag for capture tuning emulation composed of preview path */
	u32 emul_repro_flag;
	u32 reserved[4];
} lib_tuning_config_t;

#ifdef CONFIG_PABLO_V8_20_0
#define USE_LEGACY_PABLO_OBTE_INTERFACE	1
#define EN_VRAINFO_MEMCPY           1 /* for nacho vra*/
#else
#define USE_LEGACY_PABLO_OBTE_INTERFACE	0
#define EN_VRAINFO_MEMCPY           0 /* for nacho vra*/
#endif

#if IS_ENABLED(USE_LEGACY_PABLO_OBTE_INTERFACE)
#include "is-param.h"

typedef int (*Wrap_replace_sensor_bayer_order)(u32, u32 *);

typedef struct lib_interface_func_for_tuning {
	/* v0 */
	int (*json_readwrite_ctl)(void *ispObject, u32 instance_id,
				  u32 json_type, u32 tuning_id, ulong addr, u32 *size_bytes);
	int (*set_tuning_config)(lib_tuning_config_t *tuning_config);

	/* v1 for 2020 / Neus TNR internal memory dump */
	int (*get_stat_data)(u32 instance_id, u32 stat_type, ulong *buffers, u32 num_buffer, u32 size_buffer, u32 *size_weight_map);

	/* v2 for emulation bayer oder control */
	int magic_number;
	int (*replace_sensor_bayer_order)(u32 moduleId, u32 *order_ptr);

	/* for olympus :remove obte code in kernel code */
	int (*is_simmian_init_3aa)(void *ispObject, u32 instance_id, bool flag);
	int (*is_simmian_deinit_3aa)(u32 instance_id);
	int (*cb_simmian_RegDump)(u32 instance_id, u32 hw_id, void *param_set); /*(u32 instance_id, u32 tuning_id);*/

} lib_interface_func_for_tuning_t;
#else
typedef struct lib_interface_func_for_tuning {
	int (*json_readwrite_ctl)(void *ispObject, u32 instance_id,
				  u32 json_type, u32 tuning_id, ulong addr, u32 *size_bytes);
	int (*set_tuning_config)(lib_tuning_config_t *tuning_config);

	int (*reserved_func_ptr_1)(void);

	int magic_number; /* must set TUNING_DRV_MAGIC_NUMBER */

	int (*reserved_cb_func_ptr_1)(void);
	int (*reserved_cb_func_ptr_2)(void);
	int (*reserved_cb_func_ptr_3)(void);
	int (*reserved_cb_func_ptr_4)(void);
} lib_interface_func_for_tuning_t;
#endif

typedef struct __module_info {
	u32 id; /* module_id */
	u32 order; /* sensor bayer order */
} module_into_t;

typedef struct __obte_init_param {
	/*
	 * version
	 * old version(supported u32 data): 0 or 1,
	 * new version(supported struct param): 2 or more
	 */
	u32 version;
	/* emul_repro_flag for capture tuning emulation composed of preview path */
	u32 emul_repro_flag;
	u32 module_count;
	module_into_t module_info[TOTALCOUNT_MODULE];
	u32 dumpbuffer_maxsize;
} obte_init_param_t;

typedef struct __dump_ip_info {
	u32 ip_id;
	u32 dump_id;   /* reference to hw_reg_dump_id_type */
	u32 image_num;
	u32 json_size;
	u32 base_address;
	u32 address_range;
	u32 all_register_num;
	u32 *register_value[MAX_STRIPE_NUM];
	u32 register_address[ALL_REGISTER_MAX];
} dump_register_ip_kernel;

typedef struct __pablo_debug_size_info {
	u32 size_of_camera2_node;
	u32 size_of_camera2_shot;
} pablo_debug_size_info_t;

struct base_address_conversion_table_t {
	u32 base_address;
	void __iomem *dump_address;
};

struct stripe_buf_info_t {
	u32 reg_value_buf_num;
	u32 *reg_value[MAX_STRIPE_NUM];
};

static const register_lib_isp get_lib_tuning_func = (register_lib_isp)LIB_ISP_TUNING_ADDR;
static const char pablo_obte_devname[] = "is_simmian";
static lib_interface_func_for_tuning_t *lib_obte_interface;
static obte_init_param_t obte_init_param;
static u32 tune_instance = UNKNOWN_INSTANCE;
static u32 tune_instance_rep = UNKNOWN_INSTANCE;
static unsigned char *_pablo_obte_buf[TOTALCOUNT_BUF_ID];
static char _pablo_obte_buf_name[TOTALCOUNT_BUF_ID][MAXSIZE_BUF_NAME];
static bool obte_status;
static u32 tune_id;
static u32 tune_stream;
static u32 tune_mode;
static u32 tune_emulator;
static u32 tune_sensor_position;
static u32 readsize;
static u32 readsize_upperbuffer;
static pablo_ssx_status_t pablo_ssx_status[TOTALCOUNT_SSX_ID];
static lib_tuning_config_t sharebase;
static spinlock_t slock_json;
static spinlock_t slock_dump;
static bool use_vmalloc_memory; /* use on memory lack */
static bool init_alloc_memory;
static bool init_alloc_cr_dump_memory;
static u32 addr_conv_tbl_num;
static struct base_address_conversion_table_t base_addr_conv_tbl[MAXSIZE_CONVERSION_TABLE];
static struct stripe_buf_info_t stripe_buf_info[HW_REG_DUMP_ID_MAX];
static struct pablo_obte_sensor obte_sensor;
static int dbg_obte_param = 0;

#if IS_ENABLED(EN_VRAINFO_MEMCPY)

struct fd_info_ext {
	struct fd_info fdInfo;
	u32 cropW;  /* 3AA/IPP bayer crop width*/
	u32 cropH;  /* 3AA/IPP bayer crop height*/
} ;

struct fd_info_ext emul_param_fd_data; /* user -> kernel*/
struct fd_info_ext ddk_param_fd_data;  /* kernel -> ddk*/
#endif

module_param_named(debug_obte, dbg_obte_param, int, S_IRUGO | S_IWUSR);

int pablo_obte_get_dbg_param(void)
{
	return dbg_obte_param;
}

/*
 *	true: use DDK
 *	false: use IRTA or other ISP FW
 */
bool pablo_obte_use_ddk(void)
{
	return (obte_init_param.version <= 3) ? true : false;
}

void pablo_obte_simmian_setdata_vra_fd_info(void *pvface_data, void *pvface_in_size)
{
#if IS_ENABLED(EN_VRAINFO_MEMCPY)
	struct fd_info *pface_data = (struct fd_info *)pvface_data;
	struct fd_rectangle *pfd_in_size = (struct fd_rectangle *)pvface_in_size;

	FIMC_BUG_VOID(!pface_data);
	FIMC_BUG_VOID(!pfd_in_size);

	memcpy(pface_data, &emul_param_fd_data.fdInfo, sizeof(struct fd_info));
	pfd_in_size->width = emul_param_fd_data.cropW;
	pfd_in_size->height = emul_param_fd_data.cropH;

	if (memcmp((void *)&emul_param_fd_data, (void *)&ddk_param_fd_data, sizeof(struct fd_info_ext)) != 0) {
		memcpy((void *)&ddk_param_fd_data, (void *)&emul_param_fd_data, sizeof(struct fd_info_ext));
		info_obte("(vra_info) Detected faces from emul.json = %d\n", pface_data->face_num);
		info_obte("--> %d x %d\n", pfd_in_size->width, pfd_in_size->height);
	}
#else
	warn_obte("not support pablo_obte_simmian_setdata_vra_fd_info !!\n");
#endif
}

bool pablo_obte_is_running(void)
{
	return obte_status;
}
EXPORT_SYMBOL_GPL(pablo_obte_is_running);

char *pablo_obte_buf_name(unsigned int id)
{
	if (id >= TOTALCOUNT_BUF_ID) {
		err_obte("unknown buf id (%d), return null\n", id);
		return NULL;
	}

	return &_pablo_obte_buf_name[id][0];
}

void pablo_obte_buf_setaddr(unsigned int id, unsigned char *addr)
{
	if (id >= TOTALCOUNT_BUF_ID)
		err_obte("unknown buf id (%d)\n", id);
	else
		_pablo_obte_buf[id] = addr;
}

unsigned char *pablo_obte_buf_getaddr(unsigned int id)
{
	if (id >= TOTALCOUNT_BUF_ID) {
		err_obte("unknown buf id (%d)\n", id);
		return NULL;
	}

	return _pablo_obte_buf[id];
}

unsigned char *pablo_obte_alloc(unsigned long size)
{
	unsigned char *new_addr;

	if (use_vmalloc_memory)
		new_addr = vzalloc(size);
	else
		new_addr = kzalloc(size, GFP_KERNEL);

	if (!new_addr) {
		err_obte("alloc fail, size (%lu)\n", size);
		return NULL;
	}

	return new_addr;
}

bool pablo_obte_buf_alloc(unsigned int id, char *buf_name, unsigned long size)
{
	unsigned char *new_addr;
	char *new_name = pablo_obte_buf_name(id);

	new_addr = pablo_obte_alloc(size);
	if (!new_addr) {
		err_obte("failed pablo_obte_alloc(%s)\n", buf_name);
		return false;
	}

	pablo_obte_buf_setaddr(id, new_addr);
	if (new_name)
		snprintf(new_name, MAXSIZE_BUF_NAME - 1, "%s", buf_name);
	else
		err_obte("failed new_name(%d/%s)\n", id, buf_name);

	info_obte("alloc('%s')\n", pablo_obte_buf_name(id));

	return true;
}

void pablo_obte_buf_free(unsigned int id)
{
	unsigned char *addr = pablo_obte_buf_getaddr(id);

	if (addr) {
		if (use_vmalloc_memory)
			vfree(addr);
		else
			kfree(addr);

		pablo_obte_buf_setaddr(id, NULL);
		info_obte("free('%s')\n", pablo_obte_buf_name(id));
	}
}

struct stripe_buf_info_t *pablo_obte_get_stripe_buf(u32 dump_id)
{
	if (dump_id >= HW_REG_DUMP_ID_MAX) {
		info_obte("dump_id is abnormal, dump_id: %d\n", dump_id);
		return NULL;
	}

	return &stripe_buf_info[dump_id];
}

void pablo_obte_stripe_value_buf_free(u32 dump_id, u32 reg_value_buf_id)
{
	struct stripe_buf_info_t *temp_stripe_buf;

	if (dump_id >= HW_REG_DUMP_ID_MAX) {
		info_obte("dump_id is abnormal, dump_id: %d\n", dump_id);
		return;
	}

	if (reg_value_buf_id >= MAX_STRIPE_NUM) {
		info_obte("buf id is abnormal(%d)\n", reg_value_buf_id);
		return;
	}

	temp_stripe_buf = pablo_obte_get_stripe_buf(dump_id);

	if (temp_stripe_buf->reg_value[reg_value_buf_id]) {
		if (use_vmalloc_memory)
			vfree(temp_stripe_buf->reg_value[reg_value_buf_id]);
		else
			kfree(temp_stripe_buf->reg_value[reg_value_buf_id]);

		temp_stripe_buf->reg_value[reg_value_buf_id] = NULL;
		if (temp_stripe_buf->reg_value_buf_num > 0)
			temp_stripe_buf->reg_value_buf_num--;
	}
}

void pablo_obte_buf_init(void)
{
	unsigned int id;

	for (id = 0; id < TOTALCOUNT_BUF_ID; id++) {
		_pablo_obte_buf[id] = NULL;
		memset(&_pablo_obte_buf_name[id][0], 0, MAXSIZE_BUF_NAME);
	}

	sharebase.shared_addr = (ulong)0;
	sharebase.af_tuning_addr = (ulong)0;
}

void pablo_obte_buf_free_stripe(void)
{
	int i, j;

	for (i = 0; i < HW_REG_DUMP_ID_MAX; i++)
		for (j = 0; j < MAX_STRIPE_NUM; j++)
			pablo_obte_stripe_value_buf_free(i, j);
}

void pablo_obte_buf_free_all(void)
{
	int i;

	for (i = 0; i < TOTALCOUNT_BUF_ID; i++)
		pablo_obte_buf_free(i);

	pablo_obte_buf_init();
	init_alloc_memory = false;
	init_alloc_cr_dump_memory = false;

	pablo_obte_buf_free_stripe();
}

void pablo_obte_buf_free_cr_dump(void)
{
	int i;

	for (i = BUF_ID_CRDUMP_START; i <= BUF_ID_CRDUMP_END; i++)
		pablo_obte_buf_free(i);

	init_alloc_cr_dump_memory = false;

	pablo_obte_buf_free_stripe();
}

bool pablo_obte_buf_alloc_cr_dump(void)
{
	int buf_id_crdump;

	if (init_alloc_cr_dump_memory) {
		info_obte("already allocated cr dump buffer\n");
		return true;
	}

	for (buf_id_crdump = BUF_ID_CRDUMP_START; buf_id_crdump <= BUF_ID_CRDUMP_END; buf_id_crdump++) {
		if (!pablo_obte_buf_alloc(buf_id_crdump, strBufferType[buf_id_crdump], sizeof(dump_register_ip_kernel)))
			goto error_pablo_obte_buf_alloc_cr_dump;
	}

	info_obte("allocated cr dump buffer\n");
	init_alloc_cr_dump_memory = true;

	return true;

error_pablo_obte_buf_alloc_cr_dump:
	pablo_obte_buf_free_cr_dump();

	return false;
}

bool pablo_obte_buf_alloc_all(void)
{
	int buf_id;

	if (init_alloc_memory) {
		info_obte("already allocated pablo_obte buffer\n");
		return true;
	}

	for (buf_id = BUF_ID_READ; buf_id < BUF_ID_CRDUMP_START; buf_id++) {
		if (!pablo_obte_buf_alloc(buf_id, strBufferType[buf_id], JSON_SIZE / 2))
			goto error_pablo_obte_buf_alloc_all;
	}

	sharebase.shared_addr = (ulong)pablo_obte_buf_getaddr(BUF_ID_SHAREBASE);
	sharebase.af_tuning_addr = (ulong)pablo_obte_buf_getaddr(BUF_ID_AF_TUNING);

	pablo_obte_buf_alloc_cr_dump();

	info_obte("allocated pablo_obte buffer\n");
	init_alloc_memory = true;

	return true;

error_pablo_obte_buf_alloc_all:
	pablo_obte_buf_free_all();

	return false;
}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
static lib_interface_func_for_tuning_t *pablo_kunit_obte_interface;
void pablo_kunit_obte_set_interface(void *itf)
{
	pablo_kunit_obte_interface = itf;
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_obte_set_interface);
#endif

unsigned int replace_sensor_bayer_order(unsigned int moduleId, unsigned int *order_ptr)
{
	unsigned int prev_order = (*order_ptr);
	int loop = obte_init_param.module_count;
	int i;

	if (obte_init_param.version == 2) {
		for (i = 0; i < loop; i++) {
			if (obte_init_param.module_info[i].id == moduleId) {
				*order_ptr = obte_init_param.module_info[i].order;

				info_obte("moduleId: %u, order: %u -> %u\n", moduleId, prev_order, *order_ptr);
				break;
			}
		}
	} else if (obte_init_param.version == 3) { /* enable picasso select bayer order */
		if (obte_init_param.module_info[0].order < INVALID_ORDER_BAYER) {
			*order_ptr = obte_init_param.module_info[0].order;
			info_obte("moduleId: %u, order: %u -> %u\n", moduleId, prev_order, *order_ptr);
		} else
			info_obte("moduleId: %u, Bayer order has not changed : %u\n", moduleId, *order_ptr);
	} else {
		info_obte("moduleId: %u, order: %u\n", moduleId, *order_ptr);
	}

	return prev_order;
}

int __nocfi pablo_obte_get_ddk_tuning_interface(void)
{
	if (pablo_obte_use_ddk()) {
		/* For DDK used APs */
		if (lib_obte_interface) {
			info_obte("already register\n");
			return 0;
		}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
		lib_obte_interface = pablo_kunit_obte_interface;
#else
		get_lib_tuning_func(LIB_FUNC_3AA, (void **)&lib_obte_interface);
#endif

		if (lib_obte_interface == NULL) {
			info_obte("lib_obte_interface is NULL! check ddk interface connection!");
			return -EPERM;
		} else {
			info_obte("register ddk tuning interface\n");
			lib_obte_interface->magic_number = TUNING_DRV_MAGIC_NUMBER;
	#if IS_ENABLED(USE_LEGACY_PABLO_OBTE_INTERFACE)
			lib_obte_interface->replace_sensor_bayer_order = (Wrap_replace_sensor_bayer_order)replace_sensor_bayer_order;
	#else
			lib_obte_interface->reserved_cb_func_ptr_1 = NULL;
			lib_obte_interface->reserved_cb_func_ptr_2 = NULL;
			lib_obte_interface->reserved_cb_func_ptr_3 = NULL;
			lib_obte_interface->reserved_cb_func_ptr_4 = NULL;
	#endif
		}
	} else {
		lib_obte_interface = NULL;	/* For IRTA used APs */
	}

	if (!pablo_obte_buf_alloc_all()) {
		err_obte("failed buffer allocation\n");
		lib_obte_interface = NULL;
		return -ENOMEM;
	}

	return 0;
}

int __nocfi pablo_obte_init_3aa(u32 instance, bool flag)
{
	int ret = 0;

	if (pablo_obte_use_ddk() && !lib_obte_interface)
		ret = pablo_obte_get_ddk_tuning_interface();

	/* reprocessing path */
	if (flag) {
		tune_instance_rep = instance;
		if (tune_instance == tune_instance_rep)
			err_obte("tune_instance == tune_instance_rep (%d)\n", instance);
	} else {
		tune_instance = instance;
	}

	info_obte("[%d] pablo_obte_init_3aa\n", instance);

	return ret;
}
EXPORT_SYMBOL_GPL(pablo_obte_init_3aa);

void pablo_obte_deinit_3aa(u32 instance)
{
	lib_obte_interface = NULL;

	if (instance == tune_instance)
		tune_instance = UNKNOWN_INSTANCE;
	else if (instance == tune_instance_rep)
		tune_instance_rep = UNKNOWN_INSTANCE;
	else
		warn_obte("unknown %d - %d/%d)\n", instance, tune_instance, tune_instance_rep);

	info_obte("[%d] called 3aa deinit\n", instance);
}
EXPORT_SYMBOL_GPL(pablo_obte_deinit_3aa);

void pablo_obte_setcount_ssx_open(u32 id, s32 status)
{
	if (id >= TOTALCOUNT_SSX_ID) {
		err_obte("overflow ssx_id(%u), status(%d)\n", id, status);
		return;
	}

	pablo_ssx_status[id].enable = true;
	pablo_ssx_status[id].status = status;
	pablo_ssx_status[id].count_called++;

	info_obte("%d, %d\n", id, pablo_ssx_status[id].count_called);
}
EXPORT_SYMBOL_GPL(pablo_obte_setcount_ssx_open);

void pablo_obte_setcount_ssx_close(u32 id)
{
	if (id >= TOTALCOUNT_SSX_ID) {
		err_obte("overflow ssx_id(%u)\n", id);
		return;
	}

	pablo_ssx_status[id].enable = false;
	pablo_ssx_status[id].count_called--;
	info_obte("%d, %d\n", id, pablo_ssx_status[id].count_called);
}
EXPORT_SYMBOL_GPL(pablo_obte_setcount_ssx_close);

int pablo_obte_buf_getid(int reg_dump_id)
{
	int buf_id;

	switch (reg_dump_id) {
	case HW_REG_DUMP_ID_CSTAT:
		buf_id = BUF_ID_CRDUMP_CSTAT;
		break;
	case HW_REG_DUMP_ID_BYRP:
		buf_id = BUF_ID_CRDUMP_BYRP;
		break;
	case HW_REG_DUMP_ID_RGBP:
		buf_id = BUF_ID_CRDUMP_RGBP;
		break;
	case HW_REG_DUMP_ID_MCFP0:
		buf_id = BUF_ID_CRDUMP_MCFP0;
		break;
	case HW_REG_DUMP_ID_MCFP1:
		buf_id = BUF_ID_CRDUMP_MCFP1;
		break;
	case HW_REG_DUMP_ID_DNS:
		buf_id = BUF_ID_CRDUMP_DNS;
		break;
	case HW_REG_DUMP_ID_YUVP:
		buf_id = BUF_ID_CRDUMP_YUVP;
		break;
	case HW_REG_DUMP_ID_MCSC:
		buf_id = BUF_ID_CRDUMP_MCSC;
		break;
	case HW_REG_DUMP_ID_LME:
		buf_id = BUF_ID_CRDUMP_LME;
		break;
	case HW_REG_DUMP_ID_DLFE:
		buf_id = BUF_ID_CRDUMP_DLFE;
		break;
	case HW_REG_DUMP_ID_SHRP:
		buf_id = BUF_ID_CRDUMP_SHRP;
		break;
	default:
		err_obte("invalid reg dump id(%d)\n", reg_dump_id);
		buf_id = UNKNOWN_BUF_ID;
		break;
	}

	return buf_id;
}

dump_register_ip_kernel *pablo_obte_get_register_dump_buffer(u32 reg_dump_id)
{
	if (reg_dump_id >= HW_REG_DUMP_ID_MAX) {
		err_obte("invalid reg dump id(%d)\n", reg_dump_id);
		return NULL;
	}

	return (dump_register_ip_kernel *)pablo_obte_buf_getaddr(
		       pablo_obte_buf_getid(reg_dump_id));
}

bool pablo_is_enable_lpzsl_stripe_mode(void)
{
	return (tune_mode  == TUNE_MODE_STRIPEPROCESS) ? true : false;
}

void pablo_obte_set_stripe_buf(u32 dump_id, u32 *new_add)
{
	int j;

	if (!new_add) {
		err_obte("new_add is null, dump_id: %d\n", dump_id);
		return;
	}

	if (dump_id >= HW_REG_DUMP_ID_MAX) {
		err_obte("buf_id is abnormal, dump_id: %d\n", dump_id);
		return;
	}

	for (j = 0; j < MAX_STRIPE_NUM; j++) {
		if (!stripe_buf_info[dump_id].reg_value[j]) {
			stripe_buf_info[dump_id].reg_value[j] = new_add;
			stripe_buf_info[dump_id].reg_value_buf_num++;
			break;
		}
	}

	if (j == MAX_STRIPE_NUM) {
		err_obte("can not find the free reg_value_buf, dump_id: %d, reg_value_buf_num: %d\n",
			 dump_id, stripe_buf_info[dump_id].reg_value_buf_num);
		return;
	}

	info_obte("add the register value buffer, dump_id: %d, reg_value_buf_num: %d\n",
		 dump_id, stripe_buf_info[dump_id].reg_value_buf_num);
}

void pablo_obte_stripe_buf_alloc(u32 dump_id, u32 image_num, u32 size)
{
	u32 alloc_buffer_num;
	u32 current_stripe_value_buf_num;
	struct stripe_buf_info_t *stripe_buf_temp;
	unsigned char *new_addr;
	int i;

	if (dump_id >= HW_REG_DUMP_ID_MAX) {
		err_obte("dump_id is abnormal, dump_id: %d\n", dump_id);
		return;
	}

	alloc_buffer_num = image_num;

	stripe_buf_temp = pablo_obte_get_stripe_buf(dump_id);
	current_stripe_value_buf_num = stripe_buf_temp->reg_value_buf_num;

	if (current_stripe_value_buf_num == image_num) {
		return;
	} else if (image_num > current_stripe_value_buf_num) {
		alloc_buffer_num = image_num - current_stripe_value_buf_num;
	} else if (image_num < current_stripe_value_buf_num) {
		for (i = 1; i <= current_stripe_value_buf_num - image_num; i++)
			pablo_obte_stripe_value_buf_free(dump_id, current_stripe_value_buf_num - i);

		return;
	}

	for (i = 0; i < alloc_buffer_num; i++) {
		new_addr = pablo_obte_alloc(size);

		if (!new_addr) {
			err_obte("alloc stripe buffer fail, dump_id: %d\n", dump_id);
			return;
		}
		pablo_obte_set_stripe_buf(dump_id, (u32 *)new_addr);
	}

	info_obte("pablo_obte_stripe_buf_alloc finished, dump_id: %d, alloc_buffer_num: %d\n",
		 dump_id, alloc_buffer_num);
}

void pablo_obte_read_register(dump_register_ip_kernel *register_ip_info, u32 stripe_idx)
{
	int i;
	u32 reg_val;
	void __iomem *dump_address = NULL;
	struct stripe_buf_info_t *temp_stripe_buf;

	if (!register_ip_info) {
		err_obte("register_ip_info is NULL\n");
		return;
	}

	if (stripe_idx >= MAX_STRIPE_NUM) {
		err_obte("can not support this stripe id, id: %d\n", stripe_idx);
		return;
	}

	for (i = 0; i < addr_conv_tbl_num; i++) {
		if (base_addr_conv_tbl[i].base_address == register_ip_info->base_address) {
			dump_address = base_addr_conv_tbl[i].dump_address;
			break;
		}
	}

	if (dump_address == NULL) {
		err_obte("dump_address is NULL: base addr(0x%x), table num(%d)\n",
			 register_ip_info->base_address, i);
		return;
	}

	temp_stripe_buf = pablo_obte_get_stripe_buf(register_ip_info->dump_id);
	if (!temp_stripe_buf->reg_value[stripe_idx]) {
		warn_obte("value buffer is null, dump id: %d, stripe id: %d, buffer_num: %d\n",
			  register_ip_info->dump_id, stripe_idx, temp_stripe_buf->reg_value_buf_num);
		return;
	}

	spin_lock(&slock_dump);
	memset(temp_stripe_buf->reg_value[stripe_idx], 0, ALL_REGISTER_MAX * 4);

	if (register_ip_info->all_register_num > ALL_REGISTER_MAX) {
		err_obte("reg_num is more than max reg num all_register_num: %d\n",
			 register_ip_info->all_register_num);
		spin_unlock(&slock_dump);
		return;
	}

	for (i = 0; i < register_ip_info->all_register_num; i++) {
		reg_val = readl(dump_address +
				register_ip_info->register_address[i]);
		temp_stripe_buf->reg_value[stripe_idx][i] = reg_val;
	}
	spin_unlock(&slock_dump);

	return;
}

void dump_strip_register_info(u32 instance, u32 reg_dump_id, u32 stripe_idx, u32 stripe_num)
{
	dump_register_ip_kernel *register_ip_info;

	/* preview stream do not need to dump register. */
	if (pablo_is_enable_lpzsl_stripe_mode() && (instance != tune_instance_rep))
		return;

	if (reg_dump_id >= HW_REG_DUMP_ID_MAX) {
		err_obte("reg_dump_id is incorrect reg_dump_id: %d\n", reg_dump_id);
		return;
	}

	register_ip_info = pablo_obte_get_register_dump_buffer(reg_dump_id);
	if (!register_ip_info) {
		err_obte("register_ip_info is NULL, reg_dump_id: %d\n", reg_dump_id);
		return;
	}

	if (stripe_num == 0)
		register_ip_info->image_num = 1;
	else
		register_ip_info->image_num = stripe_num;

	/* Does not dump the right image, OBTE user binary will send command for right image */
	if ((stripe_num > 1) && (stripe_idx < stripe_num - 1))
		pablo_obte_read_register(register_ip_info, stripe_idx);
}

void pablo_obte_regdump(u32 instance_id, u32 hw_id, u32 stripe_idx, u32 stripe_num)
{
	u32 reg_dump_id = HW_REG_DUMP_ID_MAX;

	if (stripe_num > MAX_STRIPE_NUM) {
		err_obte("invalid stripe_num(%d): MAX_STRIPE_NUM(%d), hw_id(%d)\n",
			 stripe_num, MAX_STRIPE_NUM, hw_id);
		return;
	}

	switch (hw_id) {
#ifndef CONFIG_PABLO_V8_20_0
	case DEV_HW_BYRP:
		reg_dump_id = HW_REG_DUMP_ID_BYRP;
		break;
	case DEV_HW_RGBP:
		reg_dump_id = HW_REG_DUMP_ID_RGBP;
		break;
	case DEV_HW_MCFP:
		reg_dump_id = HW_REG_DUMP_ID_DLFE;
		dump_strip_register_info(instance_id, reg_dump_id, stripe_idx, stripe_num);
		reg_dump_id = HW_REG_DUMP_ID_MCFP;
		break;
#endif
	case DEV_HW_YPP:
		reg_dump_id = HW_REG_DUMP_ID_YUVP;
		break;
	case DEV_HW_3AA0:
	case DEV_HW_3AA1:
	case DEV_HW_3AA2:
    case DEV_HW_3AA3:
		reg_dump_id = HW_REG_DUMP_ID_IPP;
		break;
	case DEV_HW_ISP0:
	case DEV_HW_ISP1:
		reg_dump_id = HW_REG_DUMP_ID_ITP;
		dump_strip_register_info(instance_id, reg_dump_id, stripe_idx, stripe_num);
		reg_dump_id = HW_REG_DUMP_ID_MCFP;
		dump_strip_register_info(instance_id, reg_dump_id, stripe_idx, stripe_num);
		reg_dump_id = HW_REG_DUMP_ID_DNS;
		break;
	case DEV_HW_MCSC0:
    case DEV_HW_MCSC1:
		reg_dump_id = HW_REG_DUMP_ID_MCSC;
		break;
#ifndef CONFIG_PABLO_V8_20_0
	case DEV_HW_LME0:
	case DEV_HW_LME1:
		reg_dump_id = HW_REG_DUMP_ID_LME;
		break;
#endif
    case DEV_HW_DLFE:
		reg_dump_id = HW_REG_DUMP_ID_DLFE;
		break;
    case DEV_HW_SHRP:
		reg_dump_id = HW_REG_DUMP_ID_SHRP;
		break;
	default:
		break;
	}

	if (reg_dump_id < HW_REG_DUMP_ID_MAX)
		dump_strip_register_info(instance_id, reg_dump_id, stripe_idx, stripe_num);
}
EXPORT_SYMBOL_GPL(pablo_obte_regdump);

void pablo_obte_initdata_status(void)
{
	u32 id;

	for (id = 0; id < TOTALCOUNT_SSX_ID; id++) {
		pablo_ssx_status[id].enable = false;
		pablo_ssx_status[id].status = 0;
		pablo_ssx_status[id].count_called = 0;
	}
}

void pablo_obte_initdata(void)
{
	pablo_obte_initdata_status();

#if IS_ENABLED(EN_VRAINFO_MEMCPY)
	memset(&emul_param_fd_data, 0, sizeof(struct fd_info_ext));
	memset(&ddk_param_fd_data, 0, sizeof(struct fd_info_ext));
#endif
}

void pablo_obte_set_status(bool enable)
{
	obte_status = enable;
}

int __nocfi pablo_obte_open(struct inode *ip, struct file *fp)
{
	info_obte("pablo_obte_open\n");

	pablo_obte_initdata();
	pablo_obte_set_status(true);

	obte_sensor.configured = false;

	return 0;
}

static int __nocfi pablo_obte_release(struct inode *ip, struct file *fp)
{
	info_obte("pablo_obte_release\n");

	pablo_obte_set_status(false);
	pablo_obte_initdata();

	return 0;
}

ssize_t __nocfi pablo_user_write_json(struct file *file, const char *buf,
				      size_t count, loff_t *ppos)
{
	int ret;
	u32 instance;
	unsigned char *writebuffer;

	pr_debug("pablo_user_write_json\n");

	/*
	 * For DDK used APs: lib_obte_interface is required.
	 * For IRTA used APs: lib_obte_interface is not required. OBTE can interface with IRTA on user space.
	 */
	if (pablo_obte_use_ddk() && !lib_obte_interface) {
		err_obte("lib_obte_interface is NULL\n");
		return -EINVAL;
	}

	writebuffer = pablo_obte_buf_getaddr(BUF_ID_WRITE);

	if (!writebuffer) {
		err_obte("writebuffer is NULL\n");
		return -EINVAL;
	}

	if (count > JSON_SIZE / 2) {
		err_obte("count size is too large (max size:%d, required size:%zu)\n",
			 JSON_SIZE / 2, count);
		return -EINVAL;
	}

	ret = copy_from_user(writebuffer, buf, count);
	if (tune_stream == TUNE_MAIN_STREAM)
		instance = tune_instance;
	else
		instance = tune_instance_rep;

	spin_lock(&slock_json);
	if (lib_obte_interface)
		lib_obte_interface->json_readwrite_ctl(NULL, instance, JASON_TYPE_WRITE, 0,
			(ulong)writebuffer, (u32 *)&count);
	spin_unlock(&slock_json);

	return ret;
}

void pablo_obte_copy_register_to_user(dump_register_ip_kernel *reg_dump_ip_info)
{
	int i, ret;
	struct stripe_buf_info_t *temp_stripe_buf;

	if (!reg_dump_ip_info) {
		err_obte("reg_dump_ip_info is NULL\n");
		return;
	}

	temp_stripe_buf = pablo_obte_get_stripe_buf(reg_dump_ip_info->dump_id);

	spin_lock(&slock_dump);
	for (i = 0; i < reg_dump_ip_info->image_num; i++) {
		if ((reg_dump_ip_info->register_value[i] != NULL) &&
		    (temp_stripe_buf->reg_value[i] != NULL))
			ret = copy_to_user(reg_dump_ip_info->register_value[i],
				     temp_stripe_buf->reg_value[i], ALL_REGISTER_MAX * 4);
		else
			warn_obte("register value buffer is NULL, dump id: %d i: %d\n",
				  reg_dump_ip_info->dump_id, i);
	}
	spin_unlock(&slock_dump);
}

ssize_t __nocfi pablo_user_read_json(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	int ret = 0, i;
	u32 instance, strip_id, dump_id;
	unsigned char *readbuffer;
	dump_register_ip_kernel *reg_dump_temp_info;
	dump_register_ip_kernel *reg_dump_ip_info;

	pr_debug("pablo_user_read_json\n");

	/*
	 *	For DDK used APs: lib_obte_interface is required.
	 *	For IRTA used APs: lib_obte_interface is not required. OBTE can interface with IRTA on user space.
	 */
	if (pablo_obte_use_ddk() && !lib_obte_interface) {
		err_obte("lib_obte_interface is NULL\n");
		return -EINVAL;
	}

	pr_debug("tune_instance(%d) tune_instance_rep(%d) tune_id(%d)\n",
		 tune_instance, tune_instance_rep, tune_id);

	if (tune_stream == TUNE_MAIN_STREAM)
		instance = tune_instance;
	else
		instance = tune_instance_rep;

	if (tune_id == TUNE_ID_REGISTER_DUMP_BY_OBTE) {
		reg_dump_temp_info =
			(dump_register_ip_kernel *)pablo_obte_buf_getaddr(BUF_ID_CRDUMP_NORMAL_IP);
		if (!reg_dump_temp_info) {
			err_obte("reg_dump_temp_info is NULL: BUF_ID_CRDUMP_NORMAL_IP\n");
			return -EFAULT;
		}
		memset(reg_dump_temp_info, 0, sizeof(dump_register_ip_kernel));
		ret = copy_from_user(reg_dump_temp_info, (char *)buf,
				     sizeof(dump_register_ip_kernel));
		if (ret) {
			err_obte("failed copy_from_user(reg_dump_temp_info), ret(%d) size(%zu)\n",
				 ret, sizeof(dump_register_ip_kernel));
			return ret;
		}

		dump_id = reg_dump_temp_info->dump_id;

		if (dump_id >= HW_REG_DUMP_ID_MAX) {
			err_obte("dump_id(%d) is out-of-range (MAX:%d)\n",
					dump_id, HW_REG_DUMP_ID_MAX);
			return -EFAULT;
		}

		if (reg_dump_temp_info->image_num >= MAX_STRIPE_NUM) {
			err_obte("image_num(%d) is out-of-range (MAX:%d)\n",
					reg_dump_temp_info->image_num, MAX_STRIPE_NUM);
			return -EFAULT;
		}

		pablo_obte_stripe_buf_alloc(dump_id,
					reg_dump_temp_info->image_num, ALL_REGISTER_MAX * 4);

		reg_dump_ip_info =
			pablo_obte_get_register_dump_buffer(dump_id);
		if (!reg_dump_ip_info) {
			err_obte("reg_dump_ip_info is NULL, dump id: %d\n", dump_id);
			return -EFAULT;
		}

		/* set OBTE user binary address value buffer to kernel */
		for (i = 0; i < MAX_STRIPE_NUM; i++) {
			reg_dump_ip_info->register_value[i] =
				reg_dump_temp_info->register_value[i];
		}

		strip_id = (reg_dump_ip_info->image_num >= 1) ? reg_dump_ip_info->image_num - 1 : 0;

		pablo_obte_read_register(reg_dump_ip_info, strip_id);
		pablo_obte_copy_register_to_user(reg_dump_ip_info);
		ret = copy_to_user(buf, reg_dump_ip_info, sizeof(dump_register_ip_kernel));
	} else if (lib_obte_interface) {
		readsize = JSON_SIZE / 2;
		readsize_upperbuffer = 0;
		readbuffer = pablo_obte_buf_getaddr(BUF_ID_READ);
		if (!readbuffer) {
			err_obte("readbuffer is NULL\n");
			return -EINVAL;
		}

		pr_debug("readbuffer(%p) readsize(%d) readsize_upperbuffer(%d)\n",
			 readbuffer, readsize, readsize_upperbuffer);
		/* Indirect access using ISP DDK. So if we use IRTA, this code can be meaningless. */
		spin_lock(&slock_json);
		lib_obte_interface->json_readwrite_ctl(NULL, instance, JASON_TYPE_READ, tune_id,
				(ulong)readbuffer, &readsize);
		spin_unlock(&slock_json);

		if (count > JSON_SIZE / 2) {
			err_obte("count size is too large (max size:%d, required size:%zu)\n",
				JSON_SIZE / 2, count);
			return -EINVAL;
		}

		ret = copy_to_user(buf, readbuffer, count);
	}

	return ret;
}

#if IS_ENABLED(EN_VRAINFO_MEMCPY)
bool __nocfi pablo_obte_user_setparam_vra_info(char *user_buf)
{
	int ret;

	ret = copy_from_user((char *)&emul_param_fd_data, (char __user *)user_buf, sizeof(struct fd_info_ext));

	if (ret) {
		err_obte("failed copy_from_user : ret(%d)\n", ret);
		return false;
	}
	return true;
}
#endif

char *get_ioctl_name(unsigned int cmd)
{
	switch (cmd) {
	case PABLO_OBTE_INIT:
		return __stringify_1(PABLO_OBTE_INIT);
	case PABLO_OBTE_WRITE:
		return __stringify_1(PABLO_OBTE_WRITE);
	case PABLO_SET_TUNEID:
		return __stringify_1(PABLO_SET_TUNEID);
	case PABLO_TUNE_STREAM_TYPE:
		return __stringify_1(PABLO_TUNE_STREAM_TYPE);
	case PABLO_TUNE_MODE_TYPE:
		return __stringify_1(PABLO_TUNE_MODE_TYPE);
	case PABLO_TUNE_EMULATOR_TYPE:
		return __stringify_1(PABLO_TUNE_EMULATOR_TYPE);
	case PABLO_TUNE_SENSOR_POSITION:
		return __stringify_1(PABLO_TUNE_SENSOR_POSITION);
	case PABLO_OBTE_READ:
		return __stringify_1(PABLO_OBTE_READ);
	case PABLO_OBTE_READ_UPPER_BUFFER:
		return __stringify_1(PABLO_OBTE_READ_UPPER_BUFFER);
	case PABLO_SHAREBASE_READ:
		return __stringify_1(PABLO_SHAREBASE_READ);
	case PABLO_DEBUGBASE_READ:
		return __stringify_1(PABLO_DEBUGBASE_READ);
	case PABLO_SENSOR_INFO_CHG:
		return __stringify_1(PABLO_SENSOR_INFO_CHG);
	case PABLO_DDK_VERSION:
		return __stringify_1(PABLO_DDK_VERSION);
	case PABLO_AFLOG_READ:
		return __stringify_1(PABLO_AFLOG_READ);
	case PABLO_SETFILE_VERSION:
		return __stringify_1(PABLO_SETFILE_VERSION);
	case PABLO_TUNE_SET_DUMP_STAT_INFO:
		return __stringify_1(PABLO_TUNE_SET_DUMP_STAT_INFO);
	case PABLO_TUNE_GET_DUMP_STAT_INFO:
		return __stringify_1(PABLO_TUNE_GET_DUMP_STAT_INFO);
	case PABLO_TUNE_SET_NFD_INFO_PARAM:
		return __stringify_1(PABLO_TUNE_SET_NFD_INFO_PARAM);
	case PABLO_TUNE_SET_DUMP_REGISTERS_INFO:
		return __stringify_1(PABLO_TUNE_SET_DUMP_REGISTERS_INFO);
	case PABLO_GET_DEBUG_SIZE_INFO:
		return __stringify_1(PABLO_GET_DEBUG_SIZE_INFO);
	case PABLO_SET_VMALLOC_MEMORY:
		return __stringify_1(PABLO_SET_VMALLOC_MEMORY);
	default:
		return "UNKNOWN_OBTE_IOCTL_PARAM";
	}
}

int __nocfi pablo_obte_logdump(unsigned long arg)
{
#if IS_ENABLED(USE_LEGACY_PABLO_OBTE_INTERFACE)
	return 0;
#else
	struct is_minfo *minfo = is_get_is_minfo();
	size_t write_vptr, read_vptr;
	size_t read_cnt, read_cnt1, read_cnt2;
	void *read_ptr;
	int ret;
	struct is_debug *is_debug;

	ulong debug_kva = minfo->kvaddr_debug;
	ulong ctrl_kva  = minfo->kvaddr_debug_cnt;

	is_debug = is_debug_get();

	if (!test_bit(IS_DEBUG_OPEN, &is_debug->state))
		return 0;

	write_vptr = *((int *)(ctrl_kva));
	read_vptr = is_debug->read_vptr;

	if (write_vptr > DEBUG_REGION_SIZE)
		write_vptr = (read_vptr + DEBUG_REGION_SIZE) % (DEBUG_REGION_SIZE + 1);

	if (write_vptr >= read_vptr) {
		read_cnt1 = write_vptr - read_vptr;
		read_cnt2 = 0;
	} else {
		read_cnt1 = DEBUG_REGION_SIZE - read_vptr;
		read_cnt2 = write_vptr;
	}

	read_cnt = read_cnt1 + read_cnt2;
	info_obte("library log start(%zd)\n", read_cnt);

	if (read_cnt1) {
		read_ptr = (void *)(debug_kva + is_debug->read_vptr);
		ret = copy_to_user((void __user *)arg, (char *)read_ptr, read_cnt1);
		arg += read_cnt1;
		is_debug->read_vptr += read_cnt1;
	}

	if (is_debug->read_vptr >= DEBUG_REGION_SIZE) {
		if (is_debug->read_vptr > DEBUG_REGION_SIZE)
			err_obte("[DBG] read_vptr(%zd) is invalid", is_debug->read_vptr);

		is_debug->read_vptr = 0;
	}

	if (read_cnt2) {
		read_ptr = (void *)(debug_kva + is_debug->read_vptr);
		ret = copy_to_user((void __user *)arg, (char *)read_ptr, read_cnt2);
		is_debug->read_vptr += read_cnt2;
	}

	info_obte("library log end\n");

	return read_cnt;
#endif
}

bool pablo_obte_getstatus_ssx(u32 id, pablo_ssx_status_t *curr_status_ptr)
{
	if (curr_status_ptr == NULL) {
		err_obte("ssx[%d] 'curr_status_ptr == NULL'\n", id);
		return false;
	}

	if (id >= TOTALCOUNT_SSX_ID) {
		err_obte("invalid ssx_id(%d)\n", id);
		memset(curr_status_ptr, 0, sizeof(pablo_ssx_status_t));
		return false;
	}

	memcpy(curr_status_ptr, &pablo_ssx_status[id], sizeof(pablo_ssx_status_t));

	return true;
}

EXPORT_SYMBOL_GPL(pablo_obte_getstatus_ssx);

int pablo_obte_config_sensor(struct v4l2_subdev *subdev,
			     struct v4l2_ext_control *ext_ctrl)
{

	struct is_module_enum *module;
	struct is_device_sensor *sensor;
	int ret;

	ret = copy_from_user(&obte_sensor, ext_ctrl->ptr, sizeof(struct pablo_obte_sensor));
	if (ret) {
		err("failed to copy OBTE user data(%d)", ret);
		return ret;
	}

	sensor = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	sensor->obte_sensor = &obte_sensor;
	sensor->sensor_width = obte_sensor.width;
	sensor->sensor_height = obte_sensor.height;

	module = (struct is_module_enum *)v4l2_get_subdevdata(subdev);
	module->active_width = obte_sensor.active_width;
	module->active_height = obte_sensor.active_height;

	info_obte("obte_sensor - width: %d, height: %d\n", obte_sensor.width, obte_sensor.height);
	info_obte("obte_sensor - active width: %d, acticve height: %d\n",
						obte_sensor.active_width, obte_sensor.active_height);
	info_obte("obte_sensor - bayer_order: %d\n", obte_sensor.bayer_order);
	info_obte("obte_sensor - binning x: %d, y: %d\n", obte_sensor.binning_x, obte_sensor.binning_y);
	info_obte("obte_sensor - crop x: %d, y: %d\n", obte_sensor.crop_x, obte_sensor.crop_y);
	info_obte("obte_sensor - crop width: %d, height: %d\n", obte_sensor.crop_width,
								obte_sensor.crop_height);
	info_obte("obte_sensor - position: %d\n", obte_sensor.position);
	info_obte("obte_sensor - type: %d\n", obte_sensor.type);
	info_obte("obte_sensor - pixel_order: %d\n", obte_sensor.pixel_order);
	info_obte("obte_sensor - mono_en: %d\n", obte_sensor.mono_en);
	info_obte("obte_sensor - bayer_pattern: %d\n", obte_sensor.bayer_pattern);
	info_obte("obte_sensor - hdr_type: %d\n", obte_sensor.hdr_type);
	info_obte("obte_sensor - hdr_mode: %d\n", obte_sensor.hdr_mode);
	info_obte("obte_sensor - sensor_12bit_mode: %d\n", obte_sensor.sensor_12bit_mode);

	return 0;
}
EXPORT_SYMBOL_GPL(pablo_obte_config_sensor);

struct pablo_obte_sensor *pablo_obte_get_sensor(void)
{
	return &obte_sensor;
}
EXPORT_SYMBOL_GPL(pablo_obte_get_sensor);

long __nocfi pablo_obte_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	u32 data;
	char version[128];
	u32 ssx_id;
	int i = 0;
	void __iomem *dump_address;
	pablo_ssx_status_t curr_ssx_info;
	pablo_debug_size_info_t curr_debug_size;
	dump_register_ip_kernel *reg_dump_temp_info;
	dump_register_ip_kernel *reg_dump_ip_info;

	dbg_obte("cmd=%s\n", get_ioctl_name(cmd));

	switch (cmd) {
	case PABLO_OBTE_INIT:
		memset(&obte_init_param, 0, sizeof(obte_init_param_t));

		/* simmian is terminated abnormally. -> pablo_obte_deinit_3aa() is not called. */
		lib_obte_interface = NULL;
		if (get_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_OBTE_INIT: get_user\n");
			return -EFAULT;
		}

		if (data > 1) { /* old version: 0 or 1, new version: 2 or more */
			if (copy_from_user((char *)&obte_init_param,
					   (char __user *) arg, sizeof(obte_init_param_t))) {
				err_obte("PABLO_OBTE_INIT: copy_from_user\n");
				return -EFAULT;
			}

			info_obte("PABLO_OBTE_INIT\n");
			info_obte("-> version %d\n", obte_init_param.version);
			info_obte("-> emul_repro_flag %d\n", obte_init_param.emul_repro_flag);
		} else {
			obte_init_param.emul_repro_flag = data;
		}

		sharebase.update_shared_region = 1;
		sharebase.emul_repro_flag = obte_init_param.emul_repro_flag;

		if (pablo_obte_use_ddk()) {
			pablo_obte_get_ddk_tuning_interface();
			if (lib_obte_interface)
				lib_obte_interface->set_tuning_config(&sharebase);
			else
				err_obte("PABLO_OBTE_INIT: invalid lib_obte_interface\n");
		} else if (!pablo_obte_buf_alloc_cr_dump())  {
			err_obte("failed buffer allocation\n");
		}

		info_obte("PABLO_OBTE_INIT %d\n", sharebase.emul_repro_flag);
		break;
	case PABLO_OBTE_WRITE:
		if (get_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_OBTE_WRITE: get_user\n");
			return -EFAULT;
		}
		info_obte("PABLO_OBTE_WRITE size %u\n", data);
		break;
	case PABLO_SET_TUNEID:
		if (get_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_SET_TUNEID: get_user\n");
			return -EFAULT;
		}
		tune_id = data;
		dbg_obte("PABLO_SET_TUNEID %x\n", tune_id);
		break;
	case PABLO_TUNE_STREAM_TYPE:
		if (get_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_TUNE_STREAM_TYPE: get_user\n");
			return -EFAULT;
		}
		tune_stream = data;
		info_obte("PABLO_TUNE_STREAM_TYPE %d\n", tune_stream);
		break;
	case PABLO_TUNE_MODE_TYPE:
		if (get_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_TUNE_MODE_TYPE: get_user\n");
			return -EFAULT;
		}
		tune_mode = data;
		info_obte("PABLO_TUNE_MODE_TYPE %d\n", tune_mode);
		break;
	case PABLO_TUNE_EMULATOR_TYPE:
		if (get_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_TUNE_EMULATOR_TYPE: get_user\n");
			return -EFAULT;
		}
		tune_emulator = data;
		info_obte("PABLO_TUNE_EMULATOR_TYPE %d\n", tune_emulator);
		break;
	case PABLO_TUNE_SENSOR_POSITION:
		if (get_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_TUNE_SENSOR_POSITION: get_user\n");
			return -EFAULT;
		}
		tune_sensor_position = data;
		info_obte("PABLO_TUNE_SENSOR_POSITION %d\n", tune_sensor_position);
		break;
	case PABLO_OBTE_READ:
		data = readsize;
		if (put_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_OBTE_READ: put_user\n");
			return -EFAULT;
		}
		info_obte("PABLO_OBTE_READ size %u\n", data);
		break;
	case PABLO_OBTE_READ_UPPER_BUFFER:
		data = readsize_upperbuffer;
		if (put_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_OBTE_READ_UPPER_BUFFER: put_user\n");
			return -EFAULT;
		}
		info_obte("PABLO_OBTE_READ_UPPER_BUFFER size %u\n", data);
		break;
	case PABLO_SHAREBASE_READ:
		info_obte("PABLO_SHAREBASE_READ\n");
		ret = copy_to_user((void __user *)arg,
				   (char *)sharebase.shared_addr, PABLO_SHARE_BASE_SIZE);
		if (ret) {
			err_obte("PABLO_SHAREBASE_READ: copy_to_user, ret %d\n", ret);
			return -EFAULT;
		}
		break;
	case PABLO_DEBUGBASE_READ:
		info_obte("PABLO_DEBUGBASE_READ\n");
		pablo_obte_logdump(arg);

		break;
	case PABLO_SENSOR_INFO_CHG:
		info_obte("PABLO_SENSOR_INFO_CHG\n");

		ret = copy_from_user((char *)&curr_ssx_info,
				     (u32 __user *) arg, sizeof(pablo_ssx_status_t));
		if (ret) {
			err_obte("PABLO_SENSOR_INFO_CHG: copy_from_user, ret %d\n", ret);
			return -EFAULT;
		}
		ssx_id = curr_ssx_info.status;
		pablo_obte_getstatus_ssx(ssx_id, &curr_ssx_info);

		ret = copy_to_user((void __user *)arg,
				   (char *)&curr_ssx_info, sizeof(pablo_ssx_status_t));
		if (ret) {
			err_obte("PABLO_SENSOR_INFO_CHG: copy_to_user, ret %d\n", ret);
			return -EFAULT;
		}
		break;
	case PABLO_DDK_VERSION:
		info_obte("PABLO_DDK_VERSION\n");
		strncpy(version, get_binary_version(IS_BIN_LIBRARY, IS_BIN_LIB_HINT_DDK), 60);
		ret = copy_to_user((void __user *)arg, (char *)version, 60);
		if (ret) {
			err_obte("PABLO_DDK_VERSION: copy_to_user, ret %d\n", ret);
			return -EFAULT;
		}
		break;
	case PABLO_AFLOG_READ:
		info_obte("PABLO_AFLOG_READ\n");
		ret = copy_to_user((u32 __user *)arg,
				   (char *)sharebase.af_tuning_addr, AF_LOG_V5_SIZE);
		if (ret) {
			err_obte("PABLO_AFLOG_READ: copy_to_user, ret %d\n", ret);
			return -EFAULT;
		}
		break;
	case PABLO_SETFILE_VERSION:
		info_obte("PABLO_SETFILE_VERSION\n");
		strncpy(version, get_binary_version(IS_BIN_SETFILE, tune_sensor_position), 60);
		ret = copy_to_user((void __user *)arg, (char *)version, 60);
		if (ret) {
			err_obte("PABLO_SETFILE_VERSION: copy_to_user, ret %d\n", ret);
			return -EFAULT;
		}
		break;
	case PABLO_TUNE_SET_DUMP_REGISTERS_INFO:
		reg_dump_temp_info =
			(dump_register_ip_kernel *)pablo_obte_buf_getaddr(BUF_ID_CRDUMP_NORMAL_IP);
		if (!reg_dump_temp_info) {
			err_obte("reg_dump_info is NULL\n");
			return -EFAULT;
		}

		ret = copy_from_user((char *)reg_dump_temp_info,
				     (char *) arg, sizeof(dump_register_ip_kernel));
		if (ret) {
			err_obte("PABLO_TUNE_SET_DUMP_REGISTERS_INFO: copy_from_user, ret %d\n",
				 ret);
			return ret;
		}

		reg_dump_ip_info =
			pablo_obte_get_register_dump_buffer(reg_dump_temp_info->dump_id);
		if (reg_dump_ip_info) {
			memcpy(reg_dump_ip_info, reg_dump_temp_info,
			       sizeof(dump_register_ip_kernel));

			if (reg_dump_ip_info->base_address && reg_dump_ip_info->address_range) {
				dump_address = ioremap(reg_dump_ip_info->base_address,
						       reg_dump_ip_info->address_range);
				if (!dump_address) {
					err_obte("ioremap failed\n");
					return -EFAULT;
				}
			} else {
				err_obte("register_ip_info: base addr(0x%x) addr range(0x%x)\n",
					 reg_dump_ip_info->base_address,
					 reg_dump_ip_info->address_range);
				return -EFAULT;
			}

			if (addr_conv_tbl_num < MAXSIZE_CONVERSION_TABLE) {
				for (i = 0; i <= addr_conv_tbl_num; i++) {
					if (base_addr_conv_tbl[i].base_address ==
					    reg_dump_ip_info->base_address)
						break;

					if (i == addr_conv_tbl_num) {
						base_addr_conv_tbl[addr_conv_tbl_num].base_address
							= reg_dump_ip_info->base_address;
						base_addr_conv_tbl[addr_conv_tbl_num].dump_address
							= dump_address;
						addr_conv_tbl_num++;
						break;
					}
				}
			} else {
				err_obte("base_addr_conv_tbl is full\n");
			}
		} else
			err_obte("reg_dump_ip_info buffer is NULL: dump_id(%d)\n",
				 reg_dump_temp_info->dump_id);

		break;
	case PABLO_GET_DEBUG_SIZE_INFO:
		curr_debug_size.size_of_camera2_node = (u32)sizeof(struct camera2_node);
		curr_debug_size.size_of_camera2_shot = (u32)sizeof(struct camera2_shot);
		ret = copy_to_user((void __user *)arg,
				   (char *)&curr_debug_size, sizeof(pablo_debug_size_info_t));
		if (ret) {
			err_obte("PABLO_GET_DEBUG_SIZE_INFO: copy_to_user, ret %d\n", ret);
			return -EFAULT;
		}
		break;
	case PABLO_SET_VMALLOC_MEMORY:
		if (get_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_SET_VMALLOC_MEMORY: get_user\n");
			return -EFAULT;
		}
		use_vmalloc_memory = data ? true : false;
		info_obte("PABLO_SET_VMALLOC_MEMORY %d\n", use_vmalloc_memory);
		break;
	case PABLO_TUNE_SET_NFD_INFO_PARAM:
#if IS_ENABLED(EN_VRAINFO_MEMCPY)
		info_obte("PABLO_TUNE_SET_NFD_INFO_PARAM\n");
		if (pablo_obte_user_setparam_vra_info((char __user *)arg) != true) {
			err_obte("PABLO_TUNE_SET_NFD_INFO_PARAM: is_user_setparam_vra_info\n");
			return -EFAULT;
		}
#else
		err_obte("PABLO_TUNE_SET_NFD_INFO_PARAM: unknown memcpy roution for FD_INFO\n");
#endif
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static long __nocfi pablo_obte_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return pablo_obte_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}

static const struct file_operations pablo_obte_fops = {
	.owner = THIS_MODULE,
	.open = pablo_obte_open,
	.release = pablo_obte_release,
	.write = pablo_user_write_json,
	.read = pablo_user_read_json,
	.unlocked_ioctl = pablo_obte_ioctl,
	.compat_ioctl = pablo_obte_compat_ioctl,
};

static struct miscdevice pablo_obte_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = pablo_obte_devname,
	.fops = &pablo_obte_fops,
};

int pablo_obte_init(void)
{
	int ret;
	init_alloc_memory = false;

	ret = misc_register(&pablo_obte_device);
	spin_lock_init(&slock_json);
	spin_lock_init(&slock_dump);

	if (ret)
		err_obte("misc_register is-simmian error (%d)\n", ret);

	return ret;
}
EXPORT_SYMBOL_GPL(pablo_obte_init);

void pablo_obte_exit(void)
{
	pablo_obte_buf_free_all();
	addr_conv_tbl_num = 0;
	memset(base_addr_conv_tbl, 0, sizeof(base_addr_conv_tbl));

	misc_deregister(&pablo_obte_device);
	info_obte("misc_deregister is-simmian\n");
}
EXPORT_SYMBOL_GPL(pablo_obte_exit);

MODULE_AUTHOR("Sehoon Kim<sehoon.kim@samsung.com>");
MODULE_DESCRIPTION("Exynos PABLO OBTE driver");
MODULE_LICENSE("GPL");
