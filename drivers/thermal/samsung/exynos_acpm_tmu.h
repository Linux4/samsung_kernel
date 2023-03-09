/*
 * exynos_acpm_tmu.h - ACPM TMU plugin interface
 *
 * Copyright (C) 2017 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __EXYNOS_ACPM_TMU_H__
#define __EXYNOS_ACPM_TMU_H__

#include <soc/samsung/acpm_ipc_ctrl.h>

/* Return values */
#define RET_SUCCESS		0
#define RET_FAIL		-1
#define RET_OK			RET_SUCCESS
#define RET_NOK			RET_FAIL

/* Return values - error types (minus) */
#define ERR_REQ_TYPE		2
#define ERR_TZ_ID		3
#define ERR_TEMP		4
#define ERR_APM_IRQ		5
#define ERR_APM_DIVIDER		6

/* Return values - capabilities */
#define CAP_APM_IRQ		0x1
#define CAP_APM_DIVIDER		0x2

/* IPC Request Types */
#define TMU_IPC_INIT		0x01
#define TMU_IPC_READ_TEMP	0x02
#define	TMU_IPC_AP_SUSPEND	0x04
#define	TMU_IPC_CP_CALL		0x08
#define	TMU_IPC_AP_RESUME	0x10

#define TMU_IPC_THRESHOLD	0x11
#define TMU_IPC_INTEN		0x12
#define TMU_IPC_TMU_CONTROL	0x13
#define TMU_IPC_IRQ_CLEAR	0x14
#define TMU_IPC_EMUL_TEMP	0x15
#define TMU_IPC_CHANGE_THRESHOLD	0x16

#define TMU_IPC_SET_TMU_DATA		0x17
#define TMU_IPC_SET_CPUFREQ_CDEV	0x18
#define TMU_IPC_INSTANCE_UPDATE		0x19
#define TMU_IPC_CDEV_GET_CUR_STATE	0x1A
#define TMU_IPC_SET_DEVICE_CDEV		0x1B
#define TMU_IPC_GET_PI_PARAMS		0x1C
#define TMU_IPC_SET_GPUFREQ_CDEV	0x1D
#define TMU_IPC_SET_ISP_CDEV		0x1E
#define TMU_IPC_GET_TMU_DATA		0x20
#define TMU_IPC_CLEAR_CDEV_UPDATE	0x21
#define TMU_IPC_GET_PI_TRACE_MODE	0x22
#define TMU_IPC_SET_PI_TRACE_MODE	0x23

#define TMU_IPC_INIT_PI_WORK		0x24
#define TMU_IPC_START_PI_POLLING	0x25
#define TMU_IPC_REPORT_TRIGGER		0x26

#define TMU_IPC_SET_BOOST_PARAM		0x27
#define TMU_IPC_SET_BOOST_MODE		0x28
#define TMU_IPC_GET_BOOST_PARAM		0x29
#define TMU_IPC_STOP_PI_POLLING		0x2A
#define TMU_IPC_GET_CUR_FREQ		0x2B
#define TMU_IPC_SET_TMU_MODE		0x2E
#define TMU_IPC_GET_TMU_MODE		0x2F

/*
 * 16-byte TMU IPC message format (REQ)
 *  (MSB)    3          2          1          0
 * ---------------------------------------------
 * |        fw_use       |         ctx         |
 * ---------------------------------------------
 * |          | tzid     |          | type     |
 * ---------------------------------------------
 * |          |          |          |          |
 * ---------------------------------------------
 * |          |          |          |          |
 * ---------------------------------------------
 */
struct tmu_ipc_request {
	u16 ctx;	/* LSB */
	u16 fw_use;	/* MSB */
	u8 type;
	u8 rsvd;
	u8 tzid;
	u8 rsvd2;
	u8 req_rsvd0;
	u8 req_rsvd1;
	u8 req_rsvd2;
	u8 req_rsvd3;
	u8 req_rsvd4;
	u8 req_rsvd5;
	u8 req_rsvd6;
	u8 req_rsvd7;
};

struct tmu_ipc_sync_data {
	u16 ctx;
	u16 fw_use;
	u8 type;
	u8 rsvd;
	u8 tzid;
	u8 rsvd2;
	u32 data;
	u32 data2;
};

/*
 * 16-byte TMU IPC message format (RESP)
 *  (MSB)    3          2          1          0
 * ---------------------------------------------
 * |        fw_use       |         ctx         |
 * ---------------------------------------------
 * | temp     |  tz_id   | ret      | type     |
 * ---------------------------------------------
 * |          |          |          | stat     |
 * ---------------------------------------------
 * |          |          |          |          |
 * ---------------------------------------------
 */
struct tmu_ipc_response {
	u16 ctx;	/* LSB */
	u16 fw_use;	/* MSB */
	u8 type;
	s8 ret;
	u8 tzid;
	u8 temp;
	u8 stat;
	u8 rsvd;
	u8 rsvd2;
	u8 rsvd3;
	u32 reserved;
};

union tmu_ipc_message {
	u32 data[4];
	struct tmu_ipc_request req;
	struct tmu_ipc_response resp;
	struct tmu_ipc_sync_data sdata;
};

struct acpm_tmu_cap {
	bool acpm_irq;
	bool acpm_divider;
};

int exynos_acpm_tmu_set_init(struct acpm_tmu_cap *cap);
int exynos_acpm_tmu_set_read_temp(int tz, int *temp, int *stat, int *data);
int exynos_acpm_tmu_set_suspend(int flag);
int exynos_acpm_tmu_set_cp_call(void);
int exynos_acpm_tmu_set_resume(void);
int exynos_acpm_tmu_ipc_dump(int no, unsigned int dump[]);
bool exynos_acpm_tmu_is_test_mode(void);
void exynos_acpm_tmu_set_test_mode(bool mode);
void exynos_acpm_tmu_log(bool mode);

void exynos_acpm_tmu_set_threshold(int tz, unsigned char temp[]);
void exynos_acpm_tmu_set_interrupt_enable(int tz, unsigned char inten);
void exynos_acpm_tmu_tz_control(int tz, bool enable);
void exynos_acpm_tmu_clear_tz_irq(int tz);
void exynos_acpm_tmu_set_emul_temp(int tz, unsigned char temp);
void exynos_acpm_tmu_change_threshold(int tz, unsigned char temp, unsigned char point);
void exynos_acpm_tmu_set_tmu_data(int tz, unsigned char member,
		unsigned char index, int data);
void exynos_acpm_tmu_set_cpufreq_cdev(int tz, unsigned char member,
		unsigned char index, int data);
void exynos_acpm_tmu_instance_update(int cdev_id, int trip,
		unsigned char member, int data);
void exynos_acpm_tmu_init_pi_work(int tz);
void exynos_acpm_tmu_start_pi_polling(int tz);
void exynos_acpm_tmu_report_trigger(int tz, int intr_num);
int exynos_acpm_tmu_cdev_get_cur_state(int cdev_id, unsigned long *state);
void exynos_acpm_tmu_set_device_cdev(int tz, unsigned char member,
		unsigned char index, int data);
void exynos_acpm_tmu_get_pi_params(int tz, unsigned char member, s64 *data);
void exynos_acpm_tmu_set_gpufreq_cdev(int tz, unsigned char member,
		unsigned char index, int data);
void exynos_acpm_tmu_set_isp_cdev(int tz, unsigned char member,
		unsigned char index, int data);
void exynos_acpm_tmu_get_tmu_data(int tz, unsigned char member, int *data);
void exynos_acpm_tmu_clear_cdev_update(int tz);
void exynos_acpm_tmu_get_pi_trace_mode(unsigned long long *mode);
void exynos_acpm_tmu_set_pi_trace_mode(unsigned long long mode);
void exynos_acpm_tmu_set_boost_param(int tz, unsigned char member, s64 data);
void exynos_acpm_tmu_set_boost_mode(int tz, int new_mode, int from_amb);
void exynos_acpm_tmu_get_boost_param(int tz, unsigned char member, s64 *data);
void exynos_acpm_tmu_stop_pi_polling(int tz);
int exynos_acpm_tmu_cdev_get_cur_freq(int cdev_id, unsigned long *freq);
void exynos_acpm_tmu_set_tmu_mode(int tz, unsigned int mode);
void exynos_acpm_tmu_get_tmu_mode(int tz, unsigned int *mode);

int exynos_acpm_tmu_init(ipc_callback handler);

#endif /* __EXYNOS_ACPM_TMU_H__ */
