/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *
 * Boojin Kim <boojin.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "chub_ipc.h"

#if defined(CHUB_IPC)
#if defined(SEOS)
#include <seos.h>
#include <errno.h>
#include <cmsis.h>
#elif defined(EMBOS)
#include <Device.h>
#define EINVAL 22
#endif
#include <mailboxDrv.h>
#include <csp_common.h>
#include <csp_printf.h>
#include <string.h>
#include <string.h>
#elif defined(AP_IPC)
#include <linux/delay.h>
#include <linux/io.h>
#include "chub.h"
#endif

/* ap-chub ipc */
struct ipc_area ipc_addr[IPC_REG_MAX];

struct ipc_owner_ctrl {
	enum ipc_direction src;
	void *base;
} ipc_own[IPC_OWN_MAX];

struct ipc_map_area *ipc_map;

#if defined(CHUB_IPC)
#define NAME_PREFIX "ipc"
#else
#define NAME_PREFIX "nanohub-ipc"
#endif

#ifdef PACKET_LOW_DEBUG
#define GET_IPC_REG_STRING(a) (((a) == IPC_REG_IPC_C2A) ? "wt" : "rd")

static char *get_cs_name(enum channel_status cs)
{
	switch (cs) {
	case CS_IDLE:
		return "I";
	case CS_AP_WRITE:
		return "AW";
	case CS_CHUB_RECV:
		return "CR";
	case CS_CHUB_WRITE:
		return "CW";
	case CS_AP_RECV:
		return "AR";
	case CS_MAX:
		break;
	};
	return NULL;
}

void content_disassemble(struct ipc_content *content, enum ipc_region act)
{
	CSP_PRINTF_INFO("[content-%s-%d: status:%s: buf: 0x%x, size: %d]\n",
			GET_IPC_REG_STRING(act), content->num,
			get_cs_name(content->status),
			(unsigned int)content->buf, content->size);
}
#endif

#define SENSORMAP_MAGIC	"SensorMap"
#define MAX_ACTIVE_SENSOR_NUM (10)

bool ipc_have_sensor_info(struct sensor_map *sensor_map)
{
	if (sensor_map)
		if(!strncmp(SENSORMAP_MAGIC, sensor_map->magic, sizeof(SENSORMAP_MAGIC)))
			return true;
	return false;
}

#if !defined(CONFIG_SENSORS_SSP) && !defined(CONFIG_SHUB)
#ifdef CHUB_IPC
void ipc_set_sensor_id(enum sensor_type type, enum vendor_sensor_list_id id)
{
	struct sensor_map *ipc_sensor_map = ipc_get_base(IPC_REG_IPC_SENSORINFO);

	if (ipc_have_sensor_info(ipc_sensor_map)) {
		ipc_sensor_map->active_sensor_list[type] = id;
		ipc_sensor_map->index++;
	}
}

enum vendor_sensor_list_id ipc_get_sensor_id(enum sensor_type type)
{
	struct sensor_map *ipc_sensor_map = ipc_get_base(IPC_REG_IPC_SENSORINFO);

	if (ipc_have_sensor_info(ipc_sensor_map) && (type < SENSOR_TYPE_MAX))
		return (enum vendor_sensor_list_id)ipc_sensor_map->active_sensor_list[type];

	CSP_PRINTF_INFO("%s: fails to get ipc_sensor_map:%p, type:%d, magic:%s\n",
		__func__, ipc_sensor_map, type, ipc_sensor_map ? ipc_sensor_map->magic : NULL);
	return sensor_list_no_active;
}
#endif
#endif

void *ipc_get_sensor_base(void)
{
	struct sensor_map *ipc_sensor_map = ipc_get_base(IPC_REG_IPC_SENSORINFO);

	if (ipc_have_sensor_info(ipc_sensor_map))
		return &ipc_sensor_map->active_sensor_list[0];

	CSP_PRINTF_INFO("%s: fails to get ipc_sensor_map:%p, magic:%s\n",
		__func__, ipc_sensor_map, ipc_sensor_map ? ipc_sensor_map->magic : NULL);
	return NULL;
}

/* ipc address control functions */
void ipc_set_base(void *addr)
{
	ipc_addr[IPC_REG_BL].base = addr;
}

inline void *ipc_get_base(enum ipc_region area)
{
	return ipc_addr[area].base;
}

inline u32 ipc_get_offset(enum ipc_region area)
{
	return ipc_addr[area].offset;
}

u32 ipc_get_chub_mem_size(void)
{
	return ipc_addr[IPC_REG_DUMP].offset;
}

void ipc_set_chub_clk(u32 clk)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	map->chubclk = clk;
}

u32 ipc_get_chub_clk(void)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	return map->chubclk;
}

void ipc_set_chub_bootmode(u16 bootmode, u16 rtlog)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	map->bootmode = bootmode;
	map->runtimelog = rtlog;
}

u16 ipc_get_chub_bootmode(void)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	return map->bootmode;
}

u16 ipc_get_chub_rtlogmode(void)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	return map->runtimelog;
}

void ipc_set_ap_wake(u16 wake)
{
	struct ipc_map_area *map = ipc_get_base(IPC_REG_IPC);

	map->wake = wake;
}

u16 ipc_get_ap_wake(void)
{
	struct ipc_map_area *map = ipc_get_base(IPC_REG_IPC);

	return map->wake;
}

#if defined(LOCAL_POWERGATE)
u32 *ipc_get_chub_psp(void)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	return &(map->psp);
}

u32 *ipc_get_chub_msp(void)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	return &(map->msp);
}
#endif

static void ipc_dump_mailbox_sfr(void)
{
	CSP_PRINTF_ERROR("%s: 0x%x/0x%x 0x%x 0x%x 0x%x 0x%x/0x%x 0x%x 0x%x 0x%x 0x%x\n",
		__func__,
		__raw_readl(ipc_own[AP].base + REG_MAILBOX_MCUCTL),
		__raw_readl(ipc_own[AP].base + REG_MAILBOX_INTGR0),
		__raw_readl(ipc_own[AP].base + REG_MAILBOX_INTCR0),
		__raw_readl(ipc_own[AP].base + REG_MAILBOX_INTMR0),
		__raw_readl(ipc_own[AP].base + REG_MAILBOX_INTSR0),
		__raw_readl(ipc_own[AP].base + REG_MAILBOX_INTMSR0),
		__raw_readl(ipc_own[AP].base + REG_MAILBOX_INTGR1),
		__raw_readl(ipc_own[AP].base + REG_MAILBOX_INTCR1),
		__raw_readl(ipc_own[AP].base + REG_MAILBOX_INTMR1),
		__raw_readl(ipc_own[AP].base + REG_MAILBOX_INTSR1),
		__raw_readl(ipc_own[AP].base + REG_MAILBOX_INTMSR1));
}

void *ipc_get_chub_map(void)
{
	char *sram_base = ipc_get_base(IPC_REG_BL);
	struct chub_bootargs *map = (struct chub_bootargs *)(sram_base + MAP_INFO_OFFSET);

	if (strncmp(OS_UPDT_MAGIC, map->magic, sizeof(OS_UPDT_MAGIC))) {
#ifdef AP_IPC
		CSP_PRINTF_ERROR("%s: %s: %p has wrong magic key: %s -> %s\n",
			NAME_PREFIX, __func__, map, OS_UPDT_MAGIC, map->magic);
#endif
		return 0;
	}

	if (map->ipc_version != IPC_VERSION) {
#ifdef AP_IPC
		CSP_PRINTF_ERROR
		    ("%s: %s: ipc_version doesn't match: AP %d, Chub: %d\n",
		     NAME_PREFIX, __func__, IPC_VERSION, map->ipc_version);
#endif
		return 0;
	}

	if (sizeof(struct chub_bootargs) > MAP_INFO_MAX_SIZE) {
#ifdef AP_IPC
		CSP_PRINTF_ERROR
		    ("%s: %s: map size bigger than max %d > %d", NAME_PREFIX, __func__,
		    sizeof(struct chub_bootargs), MAP_INFO_MAX_SIZE);
#endif
		return 0;
	}

	ipc_addr[IPC_REG_BL_MAP].base = map;
	ipc_addr[IPC_REG_OS].base = sram_base + map->code_start;
	ipc_addr[IPC_REG_SHARED].base = sram_base + map->shared_start;
	ipc_addr[IPC_REG_IPC].base = sram_base + map->ipc_start;
	ipc_addr[IPC_REG_RAM].base = sram_base + map->ram_start;
	ipc_addr[IPC_REG_DUMP].base = sram_base + map->dump_start;
	ipc_addr[IPC_REG_BL].offset = map->bl_end - map->bl_start;
	ipc_addr[IPC_REG_OS].offset = map->code_end - map->code_start;
	ipc_addr[IPC_REG_SHARED].offset = map->shared_end - map->shared_start;
	ipc_addr[IPC_REG_IPC].offset = map->ipc_end - map->ipc_start;
	ipc_addr[IPC_REG_RAM].offset = map->ram_end - map->ram_start;
	ipc_addr[IPC_REG_DUMP].offset = map->dump_end - map->dump_start;

	if (ipc_get_offset(IPC_REG_IPC) < sizeof(struct ipc_map_area)) {
#ifdef AP_IPC
		CSP_PRINTF_INFO
			("%s: fails. ipc size (0x%x) should be increase to 0x%x\n",
			__func__, ipc_get_offset(IPC_REG_IPC), sizeof(struct ipc_map_area));
#endif
		return 0;
	}

	ipc_map = ipc_addr[IPC_REG_IPC].base;
	ipc_map->logbuf.size = LOGBUF_TOTAL_SIZE;
	strncpy(&ipc_map->magic[0], CHUB_IPC_MAGIC, sizeof(CHUB_IPC_MAGIC));

	ipc_addr[IPC_REG_IPC_EVT_A2C].base = &ipc_map->evt[IPC_EVT_A2C].data;
	ipc_addr[IPC_REG_IPC_EVT_A2C].offset = sizeof(struct ipc_evt);
	ipc_addr[IPC_REG_IPC_EVT_A2C_CTRL].base =
	    &ipc_map->evt[IPC_EVT_A2C].ctrl;
	ipc_addr[IPC_REG_IPC_EVT_A2C_CTRL].offset = 0;
	ipc_addr[IPC_REG_IPC_EVT_C2A].base = &ipc_map->evt[IPC_EVT_C2A].data;
	ipc_addr[IPC_REG_IPC_EVT_C2A].offset = sizeof(struct ipc_evt);
	ipc_addr[IPC_REG_IPC_EVT_C2A_CTRL].base =
	    &ipc_map->evt[IPC_EVT_C2A].ctrl;
	ipc_addr[IPC_REG_IPC_EVT_C2A_CTRL].offset = 0;
	ipc_addr[IPC_REG_IPC_C2A].base = &ipc_map->data[IPC_DATA_C2A];
	ipc_addr[IPC_REG_IPC_A2C].base = &ipc_map->data[IPC_DATA_A2C];
	ipc_addr[IPC_REG_IPC_C2A].offset = sizeof(struct ipc_buf);
	ipc_addr[IPC_REG_IPC_A2C].offset = sizeof(struct ipc_buf);

	/* for rawlevel log */
	ipc_map->logbuf.logbuf.size = ipc_addr[IPC_REG_IPC].offset - sizeof(struct ipc_map_area);
	/* for runtime log */
	ipc_addr[IPC_REG_LOG].base = &ipc_map->logbuf.log;
	ipc_addr[IPC_REG_LOG].offset = ipc_map->logbuf.size + ipc_map->logbuf.logbuf.size;
	ipc_addr[IPC_REG_PERSISTBUF].base = &ipc_map->persist;
	ipc_addr[IPC_REG_PERSISTBUF].offset = CHUB_PERSISTBUF_SIZE;
	ipc_addr[IPC_REG_IPC_SENSORINFO].base = &ipc_map->sensormap;
#if defined(CONFIG_SENSORS_SSP) || defined(CONFIG_SHUB)
	ipc_addr[IPC_REG_IPC_SENSORINFO].offset = sizeof(u8) * 36;
#else
	ipc_addr[IPC_REG_IPC_SENSORINFO].offset = sizeof(u8) * SENSOR_TYPE_MAX;
#endif
#ifdef SEOS
	ipc_map->logbuf.loglevel = ipc_get_chub_rtlogmode();

	if (!ipc_have_sensor_info(&ipc_map->sensormap)) {
		CSP_PRINTF_INFO("%s:ipc set sensormap and magic:%p\n", __func__, &ipc_map->sensormap);
		memset(&ipc_map->sensormap, 0, sizeof(struct sensor_map));
		strncpy(&ipc_map->sensormap.magic[0], SENSORMAP_MAGIC, sizeof(SENSORMAP_MAGIC));
	}
#endif

	CSP_PRINTF_INFO
	    ("%s: map info(v%u,bt:%d,rt:%d)\n",
	     NAME_PREFIX, map->ipc_version, ipc_get_chub_bootmode(), ipc_get_chub_rtlogmode());
	CSP_PRINTF_INFO("bl(%p %d)\n", ipc_addr[IPC_REG_BL].base, ipc_addr[IPC_REG_BL].offset);
	CSP_PRINTF_INFO("os(%p %d)\n", ipc_addr[IPC_REG_OS].base, ipc_addr[IPC_REG_OS].offset);
	CSP_PRINTF_INFO("ipc(%p %d)\n", ipc_addr[IPC_REG_IPC].base, ipc_addr[IPC_REG_IPC].offset);
	CSP_PRINTF_INFO("ram(%p %d)\n", ipc_addr[IPC_REG_RAM].base, ipc_addr[IPC_REG_RAM].offset);
	CSP_PRINTF_INFO("shared(%p %d)\n", ipc_addr[IPC_REG_SHARED].base, ipc_addr[IPC_REG_SHARED].offset);
	CSP_PRINTF_INFO("dump(%p %d)\n", ipc_addr[IPC_REG_DUMP].base, ipc_addr[IPC_REG_DUMP].offset);

	CSP_PRINTF_INFO("%s: ipc_map info(size:%d/%d)\n",
		NAME_PREFIX, sizeof(struct ipc_map_area), ipc_get_offset(IPC_REG_IPC));
	CSP_PRINTF_INFO("data_c2a(%p %d)\n", ipc_get_base(IPC_REG_IPC_C2A), ipc_get_offset(IPC_REG_IPC_C2A));
	CSP_PRINTF_INFO("data_a2c(%p %d)\n", ipc_get_base(IPC_REG_IPC_A2C), ipc_get_offset(IPC_REG_IPC_A2C));
	CSP_PRINTF_INFO("evt_c2a(%p %d)\n", ipc_get_base(IPC_REG_IPC_EVT_C2A), ipc_get_offset(IPC_REG_IPC_EVT_C2A));
	CSP_PRINTF_INFO("evt_a2c(%p %d)\n", ipc_get_base(IPC_REG_IPC_EVT_A2C), ipc_get_offset(IPC_REG_IPC_EVT_A2C));
	CSP_PRINTF_INFO("sensormap(%p %d)\n", ipc_get_base(IPC_REG_IPC_SENSORINFO), ipc_get_offset(IPC_REG_IPC_SENSORINFO));
	CSP_PRINTF_INFO("persistbuf(%p %d)\n", ipc_get_base(IPC_REG_PERSISTBUF), ipc_get_offset(IPC_REG_PERSISTBUF));
	CSP_PRINTF_INFO("log(size:%d)(c:%p %d/r:%p %d)\n", sizeof(ipc_map->logbuf), ipc_get_base(IPC_REG_LOG),
		ipc_map->logbuf.size, ipc_map->logbuf.logbuf.buf, ipc_map->logbuf.logbuf.size);

	CSP_PRINTF_INFO
		("%s: data_ch:size:%d on %d,evt_ch:%d\n",
			NAME_PREFIX, PACKET_SIZE_MAX, IPC_CH_BUF_NUM, IPC_EVT_NUM);
#ifdef SEOS
	if (PACKET_SIZE_MAX < NANOHUB_PACKET_SIZE_MAX)
		CSP_PRINTF_ERROR("%s: %d should be bigger than %d\n", NAME_PREFIX, PACKET_SIZE_MAX, NANOHUB_PACKET_SIZE_MAX);
#endif

	return ipc_map;
}

#define EVT_WAIT_TIME (1)

enum lock_case {
	LOCK_ADD_EVT,
	LOCK_WT_DATA,
	LOCK_RD_DATA
};

#undef IPC_DEBUG
#ifdef CHUB_IPC
#include <os/inc/trylock.h>
#include <platform.h>

#define MAX_TRY_CNT (10000)
#define busywait(ms) msleep(ms);
#define getTime() (0) /* don't use for disable_irq: platGetTicks() */

static TRYLOCK_DECL_STATIC(ipcLockLog) = TRYLOCK_INIT_STATIC();

static void inline ENABLE_IRQ(enum lock_case lock, unsigned long *flag)
{
	__enable_irq();
}

static void inline DISABLE_IRQ(enum lock_case lock, unsigned long *flag)
{
	__disable_irq();
}
#else /* AP IPC doesn't need it */
#define MAX_TRY_CNT (10000) /* 1ms for 360 */
#define busywait(ms) msleep(ms);
#define getTime() sched_clock()

static DEFINE_SPINLOCK(ipc_evt_lock);
static DEFINE_SPINLOCK(ipc_write_lock);

static void inline ENABLE_IRQ(enum lock_case lock, unsigned long *flag)
{
	if (lock == LOCK_ADD_EVT)
		spin_unlock_irqrestore(&ipc_evt_lock, *flag);
	else if (lock == LOCK_WT_DATA)
		spin_unlock_irqrestore(&ipc_write_lock, *flag);
}

static void inline DISABLE_IRQ(enum lock_case lock, unsigned long *flag)
{
	if (lock == LOCK_ADD_EVT)
		spin_lock_irqsave(&ipc_evt_lock, *flag);
	else if (lock == LOCK_WT_DATA)
		spin_lock_irqsave(&ipc_write_lock, *flag);
}
#endif

#ifdef IPC_DEBUG
#define CUR_TIME() getTime()
#define DIFF_TIME(a) (getTime() - a)
#else
#define CUR_TIME() (0)
#define DIFF_TIME(a) (a)
#endif


/* evt functions */
enum {
	IPC_EVT_DQ,		/* empty */
	IPC_EVT_EQ,		/* fill */
};

static inline bool __ipc_evt_queue_empty(struct ipc_evt_ctrl *ipc_evt)
{
	return (ipc_evt->eq == ipc_evt->dq);
}

static inline bool __ipc_evt_queue_full(struct ipc_evt_ctrl *ipc_evt)
{
	return (((ipc_evt->eq + 1) % IPC_EVT_NUM) == ipc_evt->dq);
}

static inline bool __ipc_evt_queue_index_check(struct ipc_evt_ctrl *ipc_evt)
{
	return ((ipc_evt->eq >= IPC_EVT_NUM) || (ipc_evt->dq >= IPC_EVT_NUM));
}

struct ipc_evt_buf *ipc_get_evt(enum ipc_evt_list evtq)
{
	struct ipc_evt *ipc_evt = &ipc_map->evt[evtq];
	struct ipc_evt_buf *cur_evt = NULL;
#ifdef IPC_DEBUG
	bool retried = 0;
#endif

retry:
	if (__ipc_evt_queue_index_check(&ipc_evt->ctrl)) {
		CSP_PRINTF_ERROR("%s:%s: failed by ipc index corrupt\n", NAME_PREFIX, __func__);
		return NULL;
	}
	/* only called by isr DISABLE_IRQ(); */
	if (!__ipc_evt_queue_empty(&ipc_evt->ctrl)) {
		cur_evt = &ipc_evt->data[ipc_evt->ctrl.dq];
		ipc_evt->ctrl.dq = (ipc_evt->ctrl.dq + 1) % IPC_EVT_NUM;
		if (cur_evt->status == IPC_EVT_EQ)
			cur_evt->status = IPC_EVT_DQ;
		else {
			CSP_PRINTF_ERROR("%s:%s: get empty evt. skip it: %d\n",
				NAME_PREFIX, __func__, cur_evt->status);
			cur_evt->status = IPC_EVT_DQ;
#ifdef IPC_DEBUG
			retried = 1;
#endif
			goto retry;
		}
	}
	/* only called by ENABLE_IRQ(); */
#ifdef IPC_DEBUG
	if (retried)
		CSP_PRINTF_INFO("%s:%s: get evt:%p\n", NAME_PREFIX, __func__, cur_evt);
#endif
	return cur_evt;
}

static inline int __ipc_evt_wait_full(struct ipc_evt *ipc_evt)
{
	volatile u32 pass = 0;
	int trycnt = 0;
	u64 time = CUR_TIME();

	if (__ipc_evt_queue_index_check(&ipc_evt->ctrl)) {
		CSP_PRINTF_ERROR("%s:%s: failed by ipc index corrupt\n", NAME_PREFIX, __func__);
		return -1;
	}

	/* don't sleep on ap */
	do {
		pass = __ipc_evt_queue_full(&ipc_evt->ctrl);
	} while (pass && (trycnt++ < MAX_TRY_CNT));

	if (pass) {
		CSP_PRINTF_ERROR("%s:%s: fails full:0x%x,t:%lld\n",
			NAME_PREFIX, __func__, pass, (u64)DIFF_TIME(time));
		ipc_dump();
		return -1;
	} else {
		CSP_PRINTF_INFO("%s:%s: ok full:0x%x, cnt:%d t:%lld\n",
			NAME_PREFIX, __func__, pass, trycnt, (u64)DIFF_TIME(time));
		return 0;
	}
}

int ipc_add_evt(enum ipc_evt_list evtq, enum irq_evt_chub evt)
{
	struct ipc_evt *ipc_evt = &ipc_map->evt[evtq];
	enum ipc_owner owner = (evtq < IPC_EVT_AP_MAX) ? AP : IPC_OWN_MAX;
	struct ipc_evt_buf *cur_evt = NULL;
	int ret = 0;
	unsigned long flag;
	u32 trycnt = 0;
	int i;
	bool get_evt = 0;
#ifdef IPC_DEBUG
	u64 time = 0;
#endif

	if (!ipc_evt || (owner != AP)) {
		CSP_PRINTF_ERROR("%s:%s: invalid ipc_evt, owner:%d\n", NAME_PREFIX, __func__, owner);
		return -EINVAL;
	}

retry:
	if (__ipc_evt_queue_index_check(&ipc_evt->ctrl)) {
		CSP_PRINTF_ERROR("%s:%s: failed by ipc index corrupt\n", NAME_PREFIX, __func__);
		return -EINVAL;
	}
	DISABLE_IRQ(LOCK_ADD_EVT, &flag);
	if (!__ipc_evt_queue_full(&ipc_evt->ctrl)) {
		cur_evt = &ipc_evt->data[ipc_evt->ctrl.eq];
		if (!cur_evt) {
			CSP_PRINTF_ERROR("%s:%s: invalid cur_evt\n", NAME_PREFIX, __func__);
			ENABLE_IRQ(LOCK_ADD_EVT, &flag);
			return -EINVAL;
		}

		do {
#ifdef IPC_DEBUG
			if (trycnt == 1)
				time = CUR_TIME();
#endif
			for (i = 0; i < IRQ_NUM_EVT_END; i++) {
				if (!ipc_evt->ctrl.pending[i]) {
					if (!ipc_hw_read_gen_int_status_reg(AP, i)) {
						ipc_evt->ctrl.pending[i] = 1;
						cur_evt->irq = i;
						cur_evt->evt = evt;
						cur_evt->status = IPC_EVT_EQ;
						ipc_evt->ctrl.eq = (ipc_evt->ctrl.eq + 1) % IPC_EVT_NUM;
						ipc_hw_gen_interrupt(AP, cur_evt->irq);
						get_evt = 1;
						goto out;
					} else {
						CSP_PRINTF_ERROR("%s:%s: wrong pending: sw-off, hw-on\n", NAME_PREFIX, __func__);
					}
				}
			}
		} while(trycnt++ < MAX_TRY_CNT);
	} else {
		ENABLE_IRQ(LOCK_ADD_EVT, &flag);
		ret = __ipc_evt_wait_full(ipc_evt);
		if (ret)
			return -EINVAL;
		else
			goto retry;
	}
out:
#ifdef IPC_DEBUG
	if (trycnt > 2) {
		CSP_PRINTF_INFO("%s:%s: ok pending wait (%d): irq:%d, evt:%d, t:%lld\n",
			NAME_PREFIX, __func__, trycnt, cur_evt->irq, cur_evt->evt, (u64)DIFF_TIME(time));
	}
#endif
	if (!get_evt) {
		CSP_PRINTF_ERROR("%s:%s: fails pending wait (%d): irq:%d, evt:%d\n",
			NAME_PREFIX, __func__, trycnt, cur_evt ? cur_evt->irq : -1, cur_evt ? cur_evt->evt : -1);
		ipc_dump();
	}
	ENABLE_IRQ(LOCK_ADD_EVT, &flag);
	if (!get_evt)
		ret = -EINVAL;
	return ret;
}

static inline bool __ipc_queue_empty(struct ipc_buf *ipc_data)
{
	return (__raw_readl(&ipc_data->eq) == ipc_data->dq);
}

static inline bool __ipc_queue_full(struct ipc_buf *ipc_data)
{
	return (((ipc_data->eq + 1) % IPC_CH_BUF_NUM) == __raw_readl(&ipc_data->dq));
}

static inline bool __ipc_queue_index_check(struct ipc_buf *ipc_data)
{
	return ((ipc_data->eq >= IPC_CH_BUF_NUM) || (ipc_data->dq >= IPC_CH_BUF_NUM));
}

static inline int __ipc_data_wait_full(struct ipc_buf *ipc_data)
{
	volatile u32 pass;
	int trycnt = 0;
	u64 time = CUR_TIME();

	if (__ipc_queue_index_check(ipc_data)) {
		CSP_PRINTF_ERROR("%s:%s: failed by ipc index corrupt\n", NAME_PREFIX, __func__);
		return -EINVAL;
	}

	/* don't sleep on ap */
	do {
		pass = __ipc_queue_full(ipc_data);
	} while (pass && (trycnt++ < MAX_TRY_CNT));

	if (pass) {
		CSP_PRINTF_ERROR("%s:%s:fails:time:%lld\n", NAME_PREFIX, __func__, (u64)DIFF_TIME(time));
		return -1;
	} else {
#ifdef IPC_DEBUG
		CSP_PRINTF_INFO("%s:%s:ok:try:%d,time:%lld\n", NAME_PREFIX, __func__, trycnt, (u64)DIFF_TIME(time));
#endif
		return 0;
	}
}

#define INVALID_CHANNEL (0xffff)
int ipc_write_data(enum ipc_data_list dir, void *tx, u16 length)
{
	int ret = 0;
	enum ipc_region reg = (dir == IPC_DATA_C2A) ? IPC_REG_IPC_C2A : IPC_REG_IPC_A2C;
	struct ipc_buf *ipc_data = ipc_get_base(reg);
	struct ipc_channel_buf *ipc;
	enum ipc_evt_list evtq = (dir == IPC_DATA_C2A) ? IPC_EVT_C2A : IPC_EVT_A2C;
	int trycnt = 0;
	unsigned long flag;

	if (length <= PACKET_SIZE_MAX) {
retry:
		if (__ipc_queue_index_check(ipc_data)) {
			CSP_PRINTF_ERROR("%s:%s: failed by ipc index corrupt\n", NAME_PREFIX, __func__);
			return -EINVAL;
		}
		DISABLE_IRQ(LOCK_WT_DATA, &flag);

		if (!__ipc_queue_full(ipc_data)) {
			ipc = &ipc_data->ch[ipc_data->eq];
			ipc_data->eq = (ipc_data->eq + 1) % IPC_CH_BUF_NUM;
			ipc->size = length;
		} else {
			ENABLE_IRQ(LOCK_WT_DATA, &flag);
			ret = __ipc_data_wait_full(ipc_data);
			if (ret) {
				CSP_PRINTF_ERROR("%s:%s:fails by full: trycnt:%d\n", NAME_PREFIX, __func__, trycnt);
				if (!trycnt) {
					trycnt++;
					goto retry;
				}
				ipc_dump();
				return ret;
			} else {
				goto retry;
			}
		}

		ENABLE_IRQ(LOCK_WT_DATA, &flag);
#ifdef AP_IPC
		memcpy_toio(ipc->buf, tx, length);
#else
		memcpy(ipc->buf, tx, length);
#endif

add_evt_try:
		ret = ipc_add_evt(evtq, IRQ_EVT_CH0);
		if (ret) {
			CSP_PRINTF_ERROR("%s:%s:fails by add_evt. try:%d\n", NAME_PREFIX, __func__, trycnt);
			if (!trycnt) {
				trycnt++;
				goto add_evt_try;
			} else {
				/* set write fails */
				ipc->size = INVALID_CHANNEL;
			}
		}
		return ret;
	} else {
		CSP_PRINTF_INFO("%s: %s: fails invalid size:%d\n", NAME_PREFIX, __func__, length);
		return -EINVAL;
	}
}

void *ipc_read_data(enum ipc_data_list dir, u32 *len)
{
	enum ipc_region reg = (dir == IPC_DATA_C2A) ? IPC_REG_IPC_C2A : IPC_REG_IPC_A2C;
	struct ipc_buf *ipc_data = ipc_get_base(reg);
	void *buf = NULL;

	if (__ipc_queue_index_check(ipc_data)) {
		CSP_PRINTF_ERROR("%s:%s: failed by ipc index corrupt\n", NAME_PREFIX, __func__);
		return NULL;
	}

	DISABLE_IRQ(LOCK_RD_DATA, NULL);
retry:
	if (!__ipc_queue_empty(ipc_data)) {
		struct ipc_channel_buf *ipc;

		ipc = &ipc_data->ch[ipc_data->dq];
		if (ipc->size != INVALID_CHANNEL) {
			*len = ipc->size;
			buf = ipc->buf;
			ipc_data->dq = (ipc_data->dq + 1) % IPC_CH_BUF_NUM;
		} else {
			CSP_PRINTF_ERROR("%s:%s get empty data. goto next\n", NAME_PREFIX, __func__);
			ipc_data->dq = (ipc_data->dq + 1) % IPC_CH_BUF_NUM;
			goto retry;
		}
	}
	ENABLE_IRQ(LOCK_RD_DATA, NULL);
	return buf;
}

int ipc_get_evt_cnt(enum ipc_evt_list evtq)
{
	struct ipc_evt *ipc_evt = &ipc_map->evt[evtq];

	if (ipc_evt->ctrl.eq >= ipc_evt->ctrl.dq)
		return ipc_evt->ctrl.eq - ipc_evt->ctrl.dq;
	else
		return ipc_evt->ctrl.eq + (IPC_EVT_NUM - ipc_evt->ctrl.dq);
}

int ipc_get_data_cnt(enum ipc_data_list dataq)
{
	struct ipc_buf *ipc_data = &ipc_map->data[dataq];

	if (ipc_data->eq >= ipc_data->dq)
		return ipc_data->eq - ipc_data->dq;
	else
		return ipc_data->eq + (IPC_CH_BUF_NUM - ipc_data->dq);
}

static void ipc_print_databuf(void)
{
#ifdef IPC_DEBUG
	struct ipc_buf *ipc_data;
	int i;
	int j;

	for (j = 0; j < IPC_DATA_MAX; j++) {
		ipc_data = (j == IPC_DATA_C2A) ? ipc_get_base(IPC_REG_IPC_C2A) : ipc_get_base(IPC_REG_IPC_A2C);

		if (ipc_get_data_cnt(j))
			for (i = 0; i < IPC_CH_BUF_NUM; i++)
				CSP_PRINTF_INFO("%s: %s: ch%d(size:0x%x)\n",
					__func__, (j == IPC_DATA_C2A) ? "c2a" : "a2c",
					i, ipc_data->ch[i].size);
	}
#endif

	CSP_PRINTF_INFO("%s: c2a:eq:%d dq:%d/a2c:eq:%d dq:%d\n",
		__func__,
		ipc_map->data[IPC_DATA_C2A].eq, ipc_map->data[IPC_DATA_C2A].dq,
		ipc_map->data[IPC_DATA_A2C].eq, ipc_map->data[IPC_DATA_A2C].dq);
}

static void ipc_print_logbuf(void)
{
	struct ipc_logbuf *logbuf = &ipc_map->logbuf;

	CSP_PRINTF_INFO("%s: channel: eq:%d, dq:%d, size:%d, full:%d, dbg_full_cnt:%d, err(overwrite):%d, ipc-reqcnt:%d, fw:%d\n",
		__func__, logbuf->eq, logbuf->dq, logbuf->size, logbuf->full, logbuf->dbg_full_cnt,
		logbuf->errcnt, logbuf->reqcnt, logbuf->fw_num);
	CSP_PRINTF_INFO("%s: raw: eq:%d, dq:%d, size:%d, full:%d\n",
		__func__, logbuf->logbuf.eq, logbuf->logbuf.dq, logbuf->logbuf.size, logbuf->logbuf.full);
}

void ipc_init(void)
{
	int i, j;

	if (!ipc_map)
		CSP_PRINTF_ERROR("%s: ipc_map is NULL.\n", __func__);

	for (i = 0; i < IPC_DATA_MAX; i++) {
		ipc_map->data[i].eq = 0;
		ipc_map->data[i].dq = 0;
	}

	ipc_hw_clear_all_int_pend_reg(AP);
	for (j = 0; j < IPC_EVT_MAX; j++) {
		ipc_map->evt[j].ctrl.dq = 0;
		ipc_map->evt[j].ctrl.eq = 0;
		ipc_map->evt[j].ctrl.irq = 0;
		for (i = 0 ; i < IRQ_MAX; i++)
			ipc_map->evt[j].ctrl.pending[i] = 0;

		for (i = 0; i < IPC_EVT_NUM; i++) {
			ipc_map->evt[j].data[i].evt = IRQ_EVT_INVAL;
			ipc_map->evt[j].data[i].irq = IRQ_EVT_INVAL;
		}
	}
}

#define IPC_GET_EVT_NAME(a) (((a) == IPC_EVT_A2C) ? "A2C" : "C2A")

void ipc_print_evt(enum ipc_evt_list evtq)
{
	struct ipc_evt *ipc_evt = &ipc_map->evt[evtq];
#ifdef IPC_DEBUG
	int i;
#endif

	CSP_PRINTF_INFO("%s: evt(%p)-%s: eq:%d dq:%d irq:%d\n",
			__func__, ipc_evt, IPC_GET_EVT_NAME(evtq), ipc_evt->ctrl.eq,
			ipc_evt->ctrl.dq, ipc_evt->ctrl.irq);

#ifdef IPC_DEBUG
	for (i = 0; i < IRQ_MAX; i++)
		if (ipc_evt->ctrl.pending[i])
			CSP_PRINTF_INFO("%s: irq:%d filled\n", __func__, i);

	for (i = 0; i < IPC_EVT_NUM; i++) {
		if (ipc_evt->data[i].status)
			CSP_PRINTF_INFO("%s: evt%d(evt:%d,irq:%d,f:%d)\n",
				__func__, i, ipc_evt->data[i].evt,
				ipc_evt->data[i].irq, ipc_evt->data[i].status);
	}
#else
	if (IRQ_MAX != 16)
		CSP_PRINTF_ERROR("%s: modify hard coded logout: %d\n", __func__, IRQ_MAX);

	CSP_PRINTF_INFO("pend-irq:%d %d %d %d %d %d %d %d / %d %d %d %d %d %d %d %d\n",
		ipc_evt->ctrl.pending[0], ipc_evt->ctrl.pending[1], ipc_evt->ctrl.pending[2], ipc_evt->ctrl.pending[3],
		ipc_evt->ctrl.pending[4], ipc_evt->ctrl.pending[5], ipc_evt->ctrl.pending[6], ipc_evt->ctrl.pending[7],
		ipc_evt->ctrl.pending[8], ipc_evt->ctrl.pending[9], ipc_evt->ctrl.pending[10], ipc_evt->ctrl.pending[11],
		ipc_evt->ctrl.pending[12], ipc_evt->ctrl.pending[13], ipc_evt->ctrl.pending[14], ipc_evt->ctrl.pending[15]);
#endif
}

void ipc_dump(void)
{
	if (strncmp(CHUB_IPC_MAGIC, ipc_map->magic, sizeof(CHUB_IPC_MAGIC))) {
		CSP_PRINTF_INFO("%s: [%s]: ipc crash\n", NAME_PREFIX, __func__);
		return;
	}

	CSP_PRINTF_INFO("[%s]: magic:%s\n", __func__, ipc_map->magic);
	ipc_print_evt(IPC_EVT_A2C);
	ipc_print_evt(IPC_EVT_C2A);
	ipc_print_databuf();
	ipc_print_logbuf();
	ipc_dump_mailbox_sfr();
}

#ifdef CHUB_IPC
void ipc_logbuf_put_with_char(char ch)
{
	if (ipc_map) {
		struct logbuf_raw *logbuf = &ipc_map->logbuf.logbuf;
		if ((void *) logbuf < (void *) ipc_addr[IPC_REG_IPC].base ||
			(void *) logbuf > (void *) (ipc_addr[IPC_REG_IPC].base + ipc_addr[IPC_REG_IPC].offset))
			return;
		if (logbuf->eq >= LOGBUF_NUM || logbuf->dq >= LOGBUF_NUM)
			return;

		if (logbuf->size) {
			char *buf = (char *)&logbuf->buf[0];

			*(buf + logbuf->eq) = ch;
			logbuf->eq = (logbuf->eq + 1) % logbuf->size;
		}
	}
}

void *ipc_logbuf_inbase(bool force)
{
	if (ipc_map) {
		struct ipc_logbuf *logbuf = &ipc_map->logbuf;

		if (logbuf->eq >= LOGBUF_NUM || logbuf->dq >= LOGBUF_NUM)
			return NULL;

		if (force || logbuf->loglevel) {
			struct logbuf_content *log;
			int index;

			/* check the validataion of ipc index */
			if (logbuf->eq >= LOGBUF_NUM || logbuf->dq >= LOGBUF_NUM)
				return NULL;

			if (!trylockTryTake(&ipcLockLog))
				return NULL;

			if (logbuf->full) /* logbuf is full overwirte */
				logbuf->dbg_full_cnt++;

			index = logbuf->eq;
			logbuf->eq = (logbuf->eq + 1) % LOGBUF_NUM;
			if (logbuf->eq == logbuf->dq)
				logbuf->full = 1;
			trylockRelease(&ipcLockLog);

			log = &logbuf->log[index];
			memset(log, 0, sizeof(struct logbuf_content));
			return log;
		}
	}
	return NULL;
}

#ifndef USE_ONE_NEWLINE
struct logbuf_content *ipc_logbuf_get_curlogbuf(struct logbuf_content *log)
{
	int i;

	for (i = 0; i < log->newline; i++) {
		if (log->nextaddr)
			log = (struct logbuf_content *)log->nextaddr;
		else
			break;
	}

	return log;
}
#endif

void ipc_logbuf_set_req_num(struct logbuf_content *log)
{
	struct ipc_logbuf *logbuf = &ipc_map->logbuf;

	log->size = logbuf->fw_num++;
}

#undef USE_LOG_FLUSH_TRSHOLD
void ipc_logbuf_req_flush(struct logbuf_content *log)
{
	if (log) {
		struct ipc_logbuf *logbuf = &ipc_map->logbuf;

		if (logbuf->eq >= LOGBUF_NUM || logbuf->dq >= LOGBUF_NUM)
			return;

		/* debug check overwrite */
#ifdef USE_ONE_NEWLINE
		if (log->nextaddr) {
			struct logbuf_content *nextlog = log->nextaddr;

			nextlog->size = logbuf->fw_num++;
		} else {
			log->size = logbuf->fw_num++;
		}
#else
		if (log->nextaddr && log->newline) {
			struct logbuf_content *nextlog = ipc_logbuf_get_curlogbuf(log);

			nextlog->size = logbuf->fw_num++;
		} else {
			log->size = logbuf->fw_num++;
		}
#endif

		if (ipc_map) {
			if (!logbuf->flush_req && !logbuf->flush_active) {
#ifdef USE_LOG_FLUSH_TRSHOLD
				u32 eq = logbuf->eq;
				u32 dq = logbuf->dq;
				u32 logcnt = (eq >= dq) ? (eq - dq) : (eq + (logbuf->size - dq));

				if (((ipc_get_ap_wake() == AP_WAKE) && (logcnt > LOGBUF_FLUSH_THRESHOLD)) || log->error) {
					if (!logbuf->flush_req) {
						logbuf->flush_req = 1;
						ipc_hw_gen_interrupt(AP, IRQ_EVT_C2A_LOG);
					}
				}
#else
				if ((ipc_get_ap_wake() == AP_WAKE) || log->error) {
					if (!logbuf->flush_req) {
						logbuf->flush_req = 1;
						logbuf->reqcnt++;
						ipc_hw_gen_interrupt(AP, IRQ_EVT_C2A_LOG);
					}
				}
#endif
			}
		}
	}
}
#else
#define ipc_logbuf_put_with_char(a) ((void)0)
#define ipc_logbuf_inbase(a) ((void)0)
#define ipc_logbuf_req_flush(a) ((void)0)
#endif

#ifdef AP_IPC
#define LOGFILE_NUM_SIZE (12)

void ipc_logbuf_flush_on(bool on)
{
	struct ipc_logbuf *logbuf;

	if (ipc_map) {
		logbuf = &ipc_map->logbuf;
		logbuf->flush_active = on;
	}
}

bool ipc_logbuf_filled(void)
{
	struct ipc_logbuf *logbuf;

	if (ipc_map) {
		logbuf = &ipc_map->logbuf;
		return logbuf->eq != logbuf->dq;
	}
	return 0;
}

int ipc_logbuf_outprint(struct runtimelog_buf *rt_buf, u32 loop)
{
	if (ipc_map) {
		struct logbuf_content *log;
		struct ipc_logbuf *logbuf = &ipc_map->logbuf;
		u32 eq;
		u32 len;
		u32 lenout;

retry:
		eq = logbuf->eq;
		if (eq >= LOGBUF_NUM || logbuf->dq >= LOGBUF_NUM) {
			pr_err("%s: index  eq:%d, dq:%d\n", __func__, eq, logbuf->dq);
			logbuf->eq = logbuf->dq = 0;

			if (logbuf->eq != 0 || logbuf->dq != 0) {
				__raw_writel(0, &logbuf->eq);
				__raw_writel(0, &logbuf->dq);
				pr_err("%s: index after: eq:%d, dq:%d\n",
					__func__, __raw_readl(&logbuf->eq), __raw_readl(&logbuf->dq));
			}
			return -1;
		}

		if (logbuf->full) {
			logbuf->full = 0;
			logbuf->dq =  (eq + 1) % LOGBUF_NUM;
		}

		while (eq != logbuf->dq) {
			log = &logbuf->log[logbuf->dq];
			len = strlen((char *)log);

			if (len <= LOGBUF_DATA_SIZE) {
				if (logbuf->loglevel == CHUB_RT_LOG_DUMP_PRT)
					CSP_PRINTF_INFO("%s: %s", NAME_PREFIX, (char *)log);
				else if (log->error)
					CSP_PRINTF_INFO("%s: FW-ERR: %s", NAME_PREFIX, (char *)log);

				if (rt_buf) {
					if (rt_buf->write_index + len + LOGFILE_NUM_SIZE >= rt_buf->buffer_size)
						rt_buf->write_index = 0;
					lenout = snprintf(rt_buf->buffer + rt_buf->write_index, LOGFILE_NUM_SIZE + len, "%10d:%s\n", log->size, log);
					rt_buf->write_index += lenout;
					if (lenout != (LOGFILE_NUM_SIZE + len))
						pr_debug("%s: %s: size-n missmatch: %d -> %d\n", NAME_PREFIX, __func__, LOGFILE_NUM_SIZE + len, lenout);
				}
			} else {
				pr_err("%s: %s: size err:%d, eq:%d, dq:%d\n", NAME_PREFIX, __func__, len, eq, logbuf->dq);
			}
			logbuf->dq = (logbuf->dq + 1) % LOGBUF_NUM;
		}
		msleep(10);
		if ((eq != logbuf->eq) && loop) {
			loop--;
			goto retry;
		}

		if (logbuf->flush_req)
			logbuf->flush_req = 0;
	}
	return 0;
}

#else
#define ipc_logbuf_outprint(a) ((void)0, 0)
#endif
enum ipc_fw_loglevel ipc_logbuf_loglevel(enum ipc_fw_loglevel loglevel, int set)
{
	if (ipc_map) {
		struct ipc_logbuf *logbuf = &ipc_map->logbuf;

		if (set)
			logbuf->loglevel = (u8)loglevel;
		return (enum ipc_fw_loglevel)logbuf->loglevel;
	}

	return 0;
}

void ipc_set_owner(enum ipc_owner owner, void *base, enum ipc_direction dir)
{
	ipc_own[owner].base = base;
	ipc_own[owner].src = dir;
}

enum ipc_direction ipc_get_owner(enum ipc_owner owner)
{
	return ipc_own[owner].src;
}

int ipc_hw_read_int_start_index(enum ipc_owner owner)
{
	if (ipc_own[owner].src)
		return IRQ_EVT_CHUB_MAX;
	else
		return 0;
}

unsigned int ipc_hw_read_gen_int_status_reg(enum ipc_owner owner, int irq)
{
	if (ipc_own[owner].src)
		return __raw_readl((char *)ipc_own[owner].base +
				   REG_MAILBOX_INTSR1) & (1 << irq);
	else
		return __raw_readl((char *)ipc_own[owner].base +
				   REG_MAILBOX_INTSR0) & (1 << (irq + IRQ_EVT_CHUB_MAX));
}

unsigned int ipc_hw_read_gen_int_status_reg_all(enum ipc_owner owner)
{
	if (ipc_own[owner].src)
		return __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTSR1);
	else
		return __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTSR0) >> IRQ_EVT_CHUB_MAX;
}

void ipc_hw_write_shared_reg(enum ipc_owner owner, unsigned int val, int num)
{
	__raw_writel(val, (char *)ipc_own[owner].base + REG_MAILBOX_ISSR0 + num * 4);
}

unsigned int ipc_hw_read_shared_reg(enum ipc_owner owner, int num)
{
	return __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_ISSR0 + num * 4);
}

unsigned int ipc_hw_read_int_status_reg(enum ipc_owner owner)
{
	if (ipc_own[owner].src)
		return __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTSR0);
	else
		return __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTSR1);
}

unsigned int ipc_hw_read_int_gen_reg(enum ipc_owner owner)
{
	if (ipc_own[owner].src)
		return __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTGR0);
	else
		return __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTGR1);
}

void ipc_hw_clear_int_pend_reg(enum ipc_owner owner, int irq)
{
	if (ipc_own[owner].src)
		__raw_writel(1 << irq,
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTCR0);
	else
		__raw_writel(1 << irq,
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTCR1);
}

void ipc_hw_clear_all_int_pend_reg(enum ipc_owner owner)
{
	u32 val = 0xffff << ipc_hw_read_int_start_index(AP);
	/* hack: org u32 val = 0xff; */

	if (ipc_own[owner].src)
		__raw_writel(val, (char *)ipc_own[owner].base + REG_MAILBOX_INTCR0);
	else
		__raw_writel(val, (char *)ipc_own[owner].base + REG_MAILBOX_INTCR1);
}

void ipc_hw_gen_interrupt(enum ipc_owner owner, int irq)
{
	if (ipc_own[owner].src)
		__raw_writel(1 << irq,
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTGR1);
	else
		__raw_writel(1 << (irq + IRQ_EVT_CHUB_MAX),
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTGR0);
}

void ipc_hw_gen_interrupt_all(enum ipc_owner owner)
{
	if (ipc_own[owner].src)
		__raw_writel(0xffff,
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTGR1);
	else
		__raw_writel(0xffff << IRQ_EVT_CHUB_MAX,
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTGR0);
}

void ipc_hw_set_mcuctrl(enum ipc_owner owner, unsigned int val)
{
	__raw_writel(val, (char *)ipc_own[owner].base + REG_MAILBOX_MCUCTL);
}

void ipc_hw_mask_irq(enum ipc_owner owner, int irq)
{
	int mask;

	if (ipc_own[owner].src) {
		mask = __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTMR0);
		__raw_writel(mask | (1 << (irq + IRQ_EVT_CHUB_MAX)),
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTMR0);
	} else {
		mask = __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTMR1);
		__raw_writel(mask | (1 << irq),
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTMR1);
	}
}

void ipc_hw_unmask_irq(enum ipc_owner owner, int irq)
{
	int mask;

	if (ipc_own[owner].src) {
		mask = __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTMR0);
		__raw_writel(mask & ~(1 << (irq + IRQ_EVT_CHUB_MAX)),
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTMR0);
	} else {
		mask = __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTMR1);
		__raw_writel(mask & ~(1 << irq),
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTMR1);
	}
}

void ipc_write_debug_event(enum ipc_owner owner, enum ipc_debug_event action)
{
	ipc_map->dbg.event = action;
}

u32 ipc_read_debug_event(enum ipc_owner owner)
{
	return ipc_map->dbg.event;
}

void ipc_write_debug_val(enum ipc_data_list dir, u32 val)
{
	ipc_map->dbg.val[dir] = val;
}

u32 ipc_read_debug_val(enum ipc_data_list dir)
{
	return ipc_map->dbg.val[dir];
}

void ipc_write_val(enum ipc_owner owner, u64 val)
{
	u32 low = val & 0xffffffff;
	u32 high = val >> 32;

	ipc_hw_write_shared_reg(owner, low, SR_DEBUG_VAL_LOW);
	ipc_hw_write_shared_reg(owner, high, SR_DEBUG_VAL_HIGH);
}

u64 ipc_read_val(enum ipc_owner owner)
{
	u32 low = ipc_hw_read_shared_reg(owner, SR_DEBUG_VAL_LOW);
	u64 high = ipc_hw_read_shared_reg(owner, SR_DEBUG_VAL_HIGH);
	u64 val = low | (high << 32);

	return val;
}
