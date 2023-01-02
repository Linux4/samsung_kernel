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

#ifndef _MAILBOX_CHUB_IPC_H
#define _MAILBOX_CHUB_IPC_H

#if defined(SEOS) || defined(EMBOS)
#define CHUB_IPC
#else
#define AP_IPC
#endif

#define IPC_VERSION (190128)

#if defined(CHUB_IPC)
#if defined(SEOS)
#include <nanohubPacket.h>
#elif defined(EMBOS)
/* TODO: Add embos */
#define SUPPORT_LOOPBACKTEST
#endif
#include <csp_common.h>
#elif defined(AP_IPC)
#if defined(CONFIG_NANOHUB)
#include "comms.h"
#elif defined(CONFIG_CONTEXTHUB_DRV)
// TODO: Add packet size.. #define PACKET_SIZE_MAX ()
#endif
#endif

#ifndef PACKET_SIZE_MAX
#define PACKET_SIZE_MAX (777)
#endif

#ifdef LOWLEVEL_DEBUG
#define DEBUG_LEVEL (0)
#else
#if defined(CHUB_IPC)
#define DEBUG_LEVEL (LOG_ERROR)
#elif defined(AP_IPC)
#define DEBUG_LEVEL (KERN_ERR)
#endif
#endif

#ifndef CSP_PRINTF_INFO
#ifdef AP_IPC
#ifdef LOWLEVEL_DEBUG
#define CSP_PRINTF_INFO(fmt, ...) log_printf(fmt, ##__VA_ARGS__)
#define CSP_PRINTF_ERROR(fmt, ...) log_printf(fmt, ##__VA_ARGS__)
#else
#define CSP_PRINTF_INFO(fmt, ...) pr_info(fmt, ##__VA_ARGS__)
#define CSP_PRINTF_ERROR(fmt, ...) pr_err(fmt, ##__VA_ARGS__)
#endif
#endif
#endif

#define CHECK_HW_TRIGGER
#define SENSOR_NAME_SIZE (32)

/* contexthub bootargs */
#define BL_OFFSET		(0x0)
#if defined(CONFIG_SOC_EXYNOS9630)
#define MAP_INFO_OFFSET (0x200)
#elif defined(CONFIG_SOC_EXYNOS9610) || defined(CONFIG_SOC_EXYNOS3830)
#define MAP_INFO_OFFSET (256)
#endif
#define MAP_INFO_MAX_SIZE (128)
#define CHUB_PERSISTBUF_SIZE (96)

#define OS_UPDT_MAGIC	"Nanohub OS"
#define CHUB_IPC_MAGIC "IPC magic"

#define BOOTMODE_COLD       (0x7733)
#define BOOTMODE_PWRGATING  (0x1188)

/* ap/chub status with alive check */
#define AP_WAKE             (0x1)
#define AP_SLEEP            (0x2)
#define AP_PREPARE          (0x3)
#define AP_COMPLETE         (0x4)
#define CHUB_REBOOT_REQ			(0xff)

#define READY_TO_GO 99

struct chub_bootargs {
	char magic[16];
	u32 ipc_version;
	u32 bl_start;
	u32 bl_end;
	u32 code_start;
	u32 code_end;
	u32 ipc_start;
	u32 ipc_end;
	u32 ram_start;
	u32 ram_end;
	u32 shared_start;
	u32 shared_end;
	u32 dump_start;
	u32 dump_end;
	u32 chubclk;
	u16 bootmode;
	u16 runtimelog;
#if defined(LOCAL_POWERGATE)
	u32 psp;
	u32 msp;
#endif
};

/* ipc map
 * data channel: AP -> CHUB
 * data channel: CHUB -> AP
 * event channel: AP -> CHUB / ctrl
 * event channel: CHUB -> AP / ctrl
 * logbuf / logbuf_ctrl
 */
#define IPC_BUF_NUM (IRQ_EVT_CH_MAX)
#define IPC_EVT_NUM (50)

enum sr_num {
	SR_0 = 0,
	SR_1 = 1,
	SR_2 = 2,
	SR_3 = 3,
};

#define SR_DEBUG_VAL_LOW SR_1
#define SR_DEBUG_VAL_HIGH SR_2
#define SR_CHUB_ALIVE SR_3
#define SR_BOOT_MODE SR_0

enum irq_chub {
	IRQ_NUM_EVT_START,
	IRQ_NUM_EVT_END = 14,
	IRQ_NUM_CHUB_LOG = IRQ_NUM_EVT_END,
	IRQ_NUM_CHUB_ALIVE,
	IRQ_INVAL = 0xff,
};

enum irq_evt_chub {
	IRQ_EVT_CH0,		/* data channel */
	IRQ_EVT_CH1,
	IRQ_EVT_CH2,
	IRQ_EVT_CH_MAX,
	IRQ_EVT_A2C_RESET = IRQ_EVT_CH_MAX,
	IRQ_EVT_A2C_WAKEUP,
	IRQ_EVT_A2C_WAKEUP_CLR,
	IRQ_EVT_A2C_SHUTDOWN,
	IRQ_EVT_A2C_LOG,
	IRQ_EVT_A2C_DEBUG,
	IRQ_EVT_C2A_DEBUG = IRQ_EVT_CH_MAX,
	IRQ_EVT_C2A_ASSERT,
	IRQ_EVT_C2A_INT,
	IRQ_EVT_C2A_INTCLR,
	IRQ_EVT_CHUB_EVT_MAX = IRQ_NUM_EVT_END, /* 14 */
	IRQ_EVT_C2A_LOG = IRQ_NUM_CHUB_LOG, /* 14 */
	IRQ_EVT_CHUB_ALIVE = IRQ_NUM_CHUB_ALIVE, /* 15 */
	IRQ_EVT_CHUB_MAX = 16, /* max irq number on mailbox */
	IRQ_EVT_INVAL = 0xff,
};

enum ipc_debug_event {
	IPC_DEBUG_UTC_STOP, /* no used. UTC_NONE */
	IPC_DEBUG_UTC_AGING,
	IPC_DEBUG_UTC_WDT,
	IPC_DEBUG_UTC_RTC,
	IPC_DEBUG_UTC_MEM,
	IPC_DEBUG_UTC_TIMER,
	IPC_DEBUG_UTC_GPIO,
	IPC_DEBUG_UTC_SPI,
	IPC_DEBUG_UTC_CMU,
	IPC_DEBUG_UTC_TIME_SYNC,
	IPC_DEBUG_UTC_ASSERT, /* 10 */
	IPC_DEBUG_UTC_FAULT,
	IPC_DEBUG_UTC_CHECK_STATUS,
	IPC_DEBUG_UTC_CHECK_CPU_UTIL,
	IPC_DEBUG_UTC_HEAP_DEBUG,
	IPC_DEBUG_UTC_HANG,
	IPC_DEBUG_UTC_HANG_ITMON,
	IPC_DEBUG_UTC_HANG_IPC_C2A_FULL,
	IPC_DEBUG_UTC_HANG_IPC_C2A_CRASH,
	IPC_DEBUG_UTC_HANG_IPC_C2A_DATA_FULL,
	IPC_DEBUG_UTC_HANG_IPC_C2A_DATA_CRASH,
	IPC_DEBUG_UTC_HANG_IPC_A2C_FULL,
	IPC_DEBUG_UTC_HANG_IPC_A2C_CRASH,
	IPC_DEBUG_UTC_HANG_IPC_A2C_DATA_FULL,
	IPC_DEBUG_UTC_HANG_IPC_A2C_DATA_CRASH,
	IPC_DEBUG_UTC_HANG_IPC_LOGBUF_EQ_CRASH,
	IPC_DEBUG_UTC_HANG_IPC_LOGBUF_DQ_CRASH,
	IPC_DEBUG_UTC_HANG_INVAL_INT,
	IPC_DEBUG_UTC_REBOOT,
	IPC_DEBUG_UTC_SENSOR_CHIPID, /* ap request */
	IPC_DEBUG_UTC_IPC_TEST_START,
	IPC_DEBUG_UTC_IPC_TEST_END,
	IPC_DEBUG_DUMP_STATUS,
	IPC_DEBUG_UTC_MAX,
	IPC_DEBUG_NANOHUB_MAX,
	IPC_DEBUG_CHUB_PRINT_LOG, /* chub request */
	IPC_DEBUG_CHUB_FULL_LOG,
	IPC_DEBUG_CHUB_FAULT,
	IPC_DEBUG_CHUB_ASSERT,
	IPC_DEBUG_CHUB_ERROR,
};

enum ipc_region {
	IPC_REG_BL,
	IPC_REG_BL_MAP,
	IPC_REG_OS,
	IPC_REG_IPC,
	IPC_REG_IPC_EVT_A2C,
	IPC_REG_IPC_EVT_A2C_CTRL,
	IPC_REG_IPC_EVT_C2A,
	IPC_REG_IPC_EVT_C2A_CTRL,
	IPC_REG_IPC_A2C,
	IPC_REG_IPC_C2A,
	IPC_REG_IPC_SENSORINFO,
	IPC_REG_SHARED,
	IPC_REG_RAM,
	IPC_REG_LOG,
	IPC_REG_PERSISTBUF,
	IPC_REG_DUMP,
	IPC_REG_MAX,
};

struct ipc_area {
	void *base;
	u32 offset;
};

enum ipc_owner {
	AP,
#if defined(CHUB_IPC)
	APM,
	CP,
	GNSS,
#endif
	IPC_OWN_MAX
};

enum ipc_data_list {
	IPC_DATA_C2A,
	IPC_DATA_A2C,
	IPC_DATA_MAX,
};

enum ipc_evt_list {
	IPC_EVT_C2A,
	IPC_EVT_A2C,
	IPC_EVT_AP_MAX,
	IPC_EVT_MAX = IPC_EVT_AP_MAX
};

enum ipc_packet {
	IPC_ALIVE_HELLO = 0xab,
	IPC_ALIVE_OK = 0xcd,
};

enum ipc_direction {
	IPC_DST,
	IPC_SRC,
};

/* channel status define
 * IDLE_A2C:	100
 * AP_WRITE :		110
 * CHUB_RECV:		101
 * IDLE_C2A:	000
 * CHUB_WRITE:		010
 * AP_RECV:		001
 */
#define CS_OWN_OFFSET (3)
#define CS_AP (0x1)
#define CS_CHUB (0x0)
#define CS_AP_OWN (CS_AP << CS_OWN_OFFSET)
#define CS_CHUB_OWN (CS_CHUB << CS_OWN_OFFSET)
#define CS_WRITE (0x2)
#define CS_RECV (0x1)
#define CS_IPC_REG_CMP (0x3)

enum channel_status {
#ifdef AP_IPC
	CS_IDLE = CS_AP_OWN,
#else
	CS_IDLE = CS_CHUB_OWN,
#endif
	CS_AP_WRITE = CS_AP_OWN | CS_WRITE,
	CS_CHUB_RECV = CS_AP_OWN | CS_RECV,
	CS_CHUB_WRITE = CS_CHUB_OWN | CS_WRITE,
	CS_AP_RECV = CS_CHUB_OWN | CS_RECV,
	CS_MAX = 0xf
};

#define INVAL_CHANNEL (-1)

#if defined(AP_IPC) || defined(EMBOS)
#define HOSTINTF_SENSOR_DATA_MAX    240
#endif

#define IRQ_MAX (16)
/* event structure */
struct ipc_evt_ctrl {
	u32 eq;
	u32 dq;
	u32 full;
	u32 empty;
	u32 irq;
	bool pending[IRQ_MAX];
};

struct ipc_evt_buf {
	u32 evt;
	u32 irq;
	u32 status;
};

struct ipc_evt {
	struct ipc_evt_buf data[IPC_EVT_NUM];
	struct ipc_evt_ctrl ctrl;
};

/* it's from struct HostIntfDataBuffer buf */
struct ipc_log_content {
	u8 pad0;
	u8 length;
	u16 pad1;
	u8 buffer[sizeof(u64) + HOSTINTF_SENSOR_DATA_MAX - sizeof(u32)];
};

#define LOGBUF_TOTAL_SIZE (LOGBUF_SIZE * LOGBUF_NUM)
#define LOGBUF_SIZE (84)
#define LOGBUF_NUM (80)
#define LOGBUF_DATA_SIZE (LOGBUF_SIZE - sizeof(u64) - sizeof(u16)) /* u8 error, u8 newline, u16 size, void *next(CM4) */
#define LOGBUF_FLUSH_THRESHOLD (LOGBUF_NUM / 2)

struct logbuf_content{
	char buf[LOGBUF_DATA_SIZE];
	u8 error;
	u8 newline;
	u32 size;
	u32 nextaddr;
};

struct logbuf_raw {
	u32 eq; /* write owner chub (index_writer) */
	u32 dq; /* read onwer ap (index_reader) */
	u32 size;
	u32 full;
	char buf[0];
};

enum ipc_fw_loglevel {
	CHUB_RT_LOG_OFF,
	CHUB_RT_LOG_DUMP,
	CHUB_RT_LOG_DUMP_PRT,
};

struct runtimelog_buf {
	char *buffer;
	unsigned int buffer_size;
	unsigned int write_index;
	enum ipc_fw_loglevel loglevel;
};

struct ipc_logbuf {
	struct logbuf_content log[LOGBUF_NUM];
	u32 eq;	/* write owner chub (index_writer) */
	u32 dq;	/* read onwer ap (index_reader) */
	u32 size;
	u8 full;
	u8 flush_req;
	u8 flush_active;
	u8 loglevel;
	/* for debug */
	int dbg_full_cnt;
	int errcnt;
	int reqcnt;
	u32 fw_num;
	/* rawlevel logout */
	struct logbuf_raw logbuf;
};

#ifndef IPC_DATA_SIZE
#define IPC_DATA_SIZE (4096)
#endif

struct ipc_channel_buf {
	u32 size;
	u8 buf[PACKET_SIZE_MAX];
};

#define IPC_CH_BUF_NUM (16)
struct ipc_buf {
	volatile u32 eq;
	volatile u32 dq;
	volatile u32 full;
	volatile u32 empty;
	struct ipc_channel_buf ch[IPC_CH_BUF_NUM];
};

#if !defined(CONFIG_SENSORS_SSP) && !defined(CONFIG_SHUB)
enum sensor_type {
    SENSOR_TYPE_META_DATA = 0,
    SENSOR_TYPE_ACCELEROMETER = 1,
    SENSOR_TYPE_MAGNETIC_FIELD = 2,
    SENSOR_TYPE_ORIENTATION = 3,
    SENSOR_TYPE_GYROSCOPE = 4,
    SENSOR_TYPE_LIGHT = 5,
    SENSOR_TYPE_PRESSURE = 6,
    SENSOR_TYPE_TEMPERATURE = 7,
    SENSOR_TYPE_PROXIMITY = 8,
    SENSOR_TYPE_GRAVITY = 9,
    SENSOR_TYPE_LINEAR_ACCELERATION = 10,
    SENSOR_TYPE_ROTATION_VECTOR = 11,
    SENSOR_TYPE_RELATIVE_HUMIDITY = 12,
    SENSOR_TYPE_AMBIENT_TEMPERATURE = 13,
    SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED = 14,
    SENSOR_TYPE_GAME_ROTATION_VECTOR = 15,
    SENSOR_TYPE_GYROSCOPE_UNCALIBRATED = 16,
    SENSOR_TYPE_SIGNIFICANT_MOTION = 17,
    SENSOR_TYPE_STEP_DETECTOR = 18,
    SENSOR_TYPE_STEP_COUNTER = 19,
    SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR = 20,
    SENSOR_TYPE_HEART_RATE = 21,
    SENSOR_TYPE_TILT_DETECTOR = 22,
    SENSOR_TYPE_WAKE_GESTURE = 23,
    SENSOR_TYPE_GLANCE_GESTURE = 24,
    SENSOR_TYPE_PICK_UP_GESTURE = 25,
    SENSOR_TYPE_WRIST_TILT_GESTURE = 26,
    SENSOR_TYPE_DEVICE_ORIENTATION = 27,
    SENSOR_TYPE_POSE_6DOF = 28,
    SENSOR_TYPE_STATIONARY_DETECT = 29,
    SENSOR_TYPE_MOTION_DETECT = 30,
    SENSOR_TYPE_HEART_BEAT = 31,
    SENSOR_TYPE_DYNAMIC_SENSOR_META = 32,
    SENSOR_TYPE_ADDITIONAL_INFO = 33,
    SENSOR_TYPE_LOW_LATENCY_OFFBODY_DETECT = 34,
    SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED = 35,
    SENSOR_TYPE_MAX,
};
#ifdef CHUB_IPC
struct sensor_info {
	bool active;
	int type;
	enum vendor_sensor_list_id id;
};
#endif

struct sensor_map {
	char magic[16];
	int index;
	u8 active_sensor_list[SENSOR_TYPE_MAX];
};
#else
struct sensor_map {
	char magic[16];
	int index;
	u8 active_sensor_list[36];
};
#endif

struct ipc_debug {
	u32 event;
	u32 val[IPC_DATA_MAX];
};

struct ipc_map_area {
	char persist[CHUB_PERSISTBUF_SIZE];
	char magic[16];
	u16 wake;
	struct ipc_buf data[IPC_DATA_MAX];
	struct ipc_evt evt[IPC_EVT_MAX];
	struct ipc_debug dbg;
	struct sensor_map sensormap;
	struct ipc_logbuf logbuf;
};

/*  mailbox Registers */
#define REG_MAILBOX_MCUCTL (0x000)
#define REG_MAILBOX_INTGR0 (0x008)
#define REG_MAILBOX_INTCR0 (0x00C)
#define REG_MAILBOX_INTMR0 (0x010)
#define REG_MAILBOX_INTSR0 (0x014)
#define REG_MAILBOX_INTMSR0 (0x018)
#define REG_MAILBOX_INTGR1 (0x01C)
#define REG_MAILBOX_INTCR1 (0x020)
#define REG_MAILBOX_INTMR1 (0x024)
#define REG_MAILBOX_INTSR1 (0x028)
#define REG_MAILBOX_INTMSR1 (0x02C)

#if defined(AP_IPC)
#if defined(CONFIG_SOC_EXYNOS9810)
#define REG_MAILBOX_VERSION (0x050)
#elif defined(CONFIG_SOC_EXYNOS9610) || defined(CONFIG_SOC_EXYNOS9630)
#define REG_MAILBOX_VERSION (0x070)
#else
//
//Need to check !!!
//
#define REG_MAILBOX_VERSION (0x0)
#endif
#endif

#define REG_MAILBOX_ISSR0 (0x080)
#define REG_MAILBOX_ISSR1 (0x084)
#define REG_MAILBOX_ISSR2 (0x088)
#define REG_MAILBOX_ISSR3 (0x08C)

#define IPC_HW_READ_STATUS(base) \
	__raw_readl((base) + REG_MAILBOX_INTSR0)
#define IPC_HW_READ_STATUS1(base) \
	__raw_readl((base) + REG_MAILBOX_INTSR1)
#define IPC_HW_READ_PEND(base, irq) \
	(__raw_readl((base) + REG_MAILBOX_INTSR1) & (1 << (irq)))
#define IPC_HW_CLEAR_PEND(base, irq) \
	__raw_writel(1 << (irq), (base) + REG_MAILBOX_INTCR0)
#define IPC_HW_CLEAR_PEND1(base, irq) \
	__raw_writel(1 << (irq), (base) + REG_MAILBOX_INTCR1)
#define IPC_HW_WRITE_SHARED_REG(base, num, data) \
	__raw_writel((data), (base) + REG_MAILBOX_ISSR0 + (num) * 4)
#define IPC_HW_READ_SHARED_REG(base, num) \
	__raw_readl((base) + REG_MAILBOX_ISSR0 + (num) * 4)
#define IPC_HW_GEN_INTERRUPT_GR1(base, num) \
	__raw_writel(1 << (num), (base) + REG_MAILBOX_INTGR1)
#define IPC_HW_GEN_INTERRUPT_GR0(base, num) \
	__raw_writel(1 << ((num) + 16), (base) + REG_MAILBOX_INTGR0)
#define IPC_HW_SET_MCUCTL(base, val) \
	__raw_write32((val), (base) + REG_MAILBOX_MCUCTL)


#if !defined(CONFIG_SENSORS_SSP) && !defined(CONFIG_SHUB)
bool ipc_have_sensor_info(struct sensor_map *sensor_map);
#ifdef CHUB_IPC
void ipc_set_sensor_id(enum sensor_type type, enum vendor_sensor_list_id id);
enum vendor_sensor_list_id ipc_get_sensor_id(enum sensor_type type);
#endif
void *ipc_get_sensor_base(void);
#endif

/* channel ctrl functions */
void ipc_print_channel(void);
int ipc_check_valid(void);
char *ipc_get_cs_name(enum channel_status cs);
void ipc_set_base(void *addr);
void *ipc_get_base(enum ipc_region area);
u32 ipc_get_offset(enum ipc_region area);
void ipc_init(void);
int ipc_hw_read_int_start_index(enum ipc_owner owner);
/* logbuf functions */
enum ipc_fw_loglevel ipc_logbuf_loglevel(enum ipc_fw_loglevel loglevel, int set);
void *ipc_logbuf_inbase(bool force);
void ipc_logbuf_flush_on(bool on);
bool ipc_logbuf_filled(void);
int ipc_logbuf_outprint(struct runtimelog_buf *rt_buf, u32 loop);
void ipc_logbuf_req_flush(struct logbuf_content *log);
void ipc_logbuf_set_req_num(struct logbuf_content *log);
struct logbuf_content *ipc_logbuf_get_curlogbuf(struct logbuf_content *log);
/* evt functions */
struct ipc_evt_buf *ipc_get_evt(enum ipc_evt_list evt);
int ipc_add_evt(enum ipc_evt_list evt, enum irq_evt_chub irq);
int ipc_add_evt_in_critical(enum ipc_evt_list evtq, enum irq_evt_chub evt);
void ipc_print_evt(enum ipc_evt_list evt);
int ipc_get_evt_cnt(enum ipc_evt_list evtq);
int ipc_get_data_cnt(enum ipc_data_list evtq);
/* mailbox hw access */
void ipc_set_owner(enum ipc_owner owner, void *base, enum ipc_direction dir);
enum ipc_direction ipc_get_owner(enum ipc_owner owner);
unsigned int ipc_hw_read_gen_int_status_reg(enum ipc_owner owner, int irq);
unsigned int ipc_hw_read_gen_int_status_reg_all(enum ipc_owner owner);
void ipc_hw_write_shared_reg(enum ipc_owner owner, unsigned int val, int num);
unsigned int ipc_hw_read_shared_reg(enum ipc_owner owner, int num);
unsigned int ipc_hw_read_int_status_reg(enum ipc_owner owner);
unsigned int ipc_hw_read_int_gen_reg(enum ipc_owner owner);
void ipc_hw_clear_int_pend_reg(enum ipc_owner owner, int irq);
void ipc_hw_clear_all_int_pend_reg(enum ipc_owner owner);
void ipc_hw_gen_interrupt(enum ipc_owner owner, int irq);
void ipc_hw_gen_interrupt_all(enum ipc_owner owner);
void ipc_hw_set_mcuctrl(enum ipc_owner owner, unsigned int val);
void ipc_hw_mask_irq(enum ipc_owner owner, int irq);
void ipc_hw_unmask_irq(enum ipc_owner owner, int irq);
void ipc_logbuf_put_with_char(char ch);
void ipc_write_debug_event(enum ipc_owner owner, enum ipc_debug_event action);
u32 ipc_read_debug_event(enum ipc_owner owner);
void ipc_write_debug_val(enum ipc_data_list dir, u32 val);
u32 ipc_read_debug_val(enum ipc_data_list dir);

void *ipc_get_chub_map(void);
u32 ipc_get_chub_mem_size(void);
u64 ipc_read_val(enum ipc_owner owner);
void ipc_write_val(enum ipc_owner owner, u64 result);
void ipc_set_chub_clk(u32 clk);
u32 ipc_get_chub_clk(void);
void ipc_set_chub_bootmode(u16 bootmode, u16 rtlog);
u16 ipc_get_chub_rtlogmode(void);
u16 ipc_get_chub_bootmode(void);
void ipc_set_ap_wake(u16 wake);
u16 ipc_get_ap_wake(void);
void ipc_dump(void);
#if defined(LOCAL_POWERGATE)
u32 *ipc_get_chub_psp(void);
u32 *ipc_get_chub_msp(void);
#endif
void *ipc_read_data(enum ipc_data_list dir, u32 *len);
int ipc_write_data(enum ipc_data_list dir, void *tx, u16 length);
#endif
