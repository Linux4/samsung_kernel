/* sec_hw_param.c
 *
 * Copyright (C) 2016 Samsung Electronics
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/sec_class.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <soc/qcom/socinfo.h>
#include <linux/qcom/sec_debug.h>
#include <linux/qcom/sec_debug_partition.h>
#include <linux/qcom/sec_hw_param.h>

extern unsigned int system_rev;
extern char* get_ddr_vendor_name(void);
extern uint8_t get_ddr_revision_id_1(void);
extern uint8_t get_ddr_revision_id_2(void);
extern uint8_t get_ddr_total_density(void);
extern uint32_t get_ddr_DSF_version(void);
extern uint16_t get_ddr_wr_eyeRect(uint32_t ch, uint32_t cs, uint32_t dq);
extern uint8_t get_ddr_wr_eyeVref(uint32_t ch, uint32_t cs, uint32_t dq);
extern uint8_t get_ddr_wr_eyeHeight(uint32_t ch, uint32_t cs, uint32_t dq);
extern uint8_t get_ddr_wr_eyeWidth(uint32_t ch, uint32_t cs, uint32_t dq);
extern uint8_t get_ddr_rcw_tDQSCK(uint32_t ch, uint32_t cs, uint32_t dq);
extern uint8_t get_ddr_wr_coarseCDC(uint32_t ch, uint32_t cs, uint32_t dq);
extern uint8_t get_ddr_wr_fineCDC(uint32_t ch, uint32_t cs, uint32_t dq);
extern int cpr3_get_fuse_open_loop_voltage(int id, int fuse_corner);
extern int cpr3_get_fuse_corner_count(int id);
extern int cpr3_get_fuse_cpr_rev(int id);
extern int cpr3_get_fuse_speed_bin(int id);
extern unsigned sec_debug_get_reset_reason(void);
extern int sec_debug_get_reset_write_cnt(void);
extern char * sec_debug_get_reset_reason_str(unsigned int reason);

#define MAX_LEN_STR 1023
#define MAX_ETRA_LEN ((MAX_LEN_STR) - (31 + 5))
#define NUM_PARAM0 5

static ap_health_t *phealth = NULL;

void battery_last_dcvs(int cap, int volt, int temp, int curr)
{
	uint32_t tail = 0;

	if (phealth == NULL)
		return;

	if ((phealth->battery.tail & 0xf) >= MAX_BATT_DCVS) {
		phealth->battery.tail = 0x10;
	}

	tail = phealth->battery.tail & 0xf;

	phealth->battery.batt[tail].ktime = local_clock();
	phealth->battery.batt[tail].cap = cap;
	phealth->battery.batt[tail].volt = volt;
	phealth->battery.batt[tail].temp = temp;
	phealth->battery.batt[tail].curr = curr;

	phealth->battery.tail++;
}
EXPORT_SYMBOL(battery_last_dcvs);

static void check_format(char *buf, ssize_t *size, int max_len_str)
{
	int i = 0, cnt = 0, pos = 0;

	if (!buf || *size <= 0)
		return;

	if (*size >= max_len_str)
		*size = max_len_str - 1;

	while (i < *size && buf[i]) {
		if (buf[i] == '"') {
			cnt++;
			pos = i;
		}

		if (buf[i] == '\n')
			buf[i] = '/';
		i++;
	}

	if (cnt % 2) {
		if (pos == *size - 1) {
			buf[*size - 1] = '\0';
		} else {
			buf[*size - 1] = '"';
			buf[*size] = '\0';
		}
	}
}

static ssize_t show_last_dcvs(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;
	int i;
	unsigned int reset_reason;
	char prefix[MAX_CLUSTER_NUM][3] = {"SC", "GC"};
	char prefix_vreg[MAX_VREG_CNT][4] = {"MX", "CX", "EBI"};

	if (!phealth)
		phealth = ap_health_data_read();

	if (!phealth) {
		pr_err("%s: fail to get ap health info\n", __func__);
		return info_size;
	}

	reset_reason = sec_debug_get_reset_reason();
	if (reset_reason < USER_UPLOAD_CAUSE_MIN ||
		reset_reason > USER_UPLOAD_CAUSE_MAX) {
		return info_size;
	}

	if (reset_reason == USER_UPLOAD_CAUSE_MANUAL_RESET ||
		reset_reason == USER_UPLOAD_CAUSE_REBOOT ||
		reset_reason == USER_UPLOAD_CAUSE_BOOTLOADER_REBOOT ||
		reset_reason == USER_UPLOAD_CAUSE_POWER_ON) {
		return info_size;
	}

	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"RR\":\"%s\",",
				sec_debug_get_reset_reason_str(reset_reason));

	info_size += scnprintf((char *)(buf+info_size), MAX_LEN_STR - info_size,
			"\"RWC\":\"%d\",", sec_debug_get_reset_write_cnt());

	for (i = 0; i < MAX_CLUSTER_NUM; i++) {
		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"%sKHz\":\"%u\",", prefix[i],
				phealth->last_dcvs.apps[i].cpu_KHz);
		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"%s\":\"%u\",", prefix[i],
				phealth->last_dcvs.apps[i].apc_mV);
		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"%sl2\":\"%u\",", prefix[i],
				phealth->last_dcvs.apps[i].l2_mV);
	}

	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"DDRKHz\":\"%u\",", phealth->last_dcvs.rpm.ddr_KHz);

	for (i = 0; i < MAX_VREG_CNT; i++) {
		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"%s\":\"%u\",", prefix_vreg[i],
				phealth->last_dcvs.rpm.mV[i]);
	}
	
	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"BCAP\":\"%d\",", phealth->last_dcvs.batt.cap);
	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"BVOL\":\"%d\",", phealth->last_dcvs.batt.volt);
	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"BTEM\":\"%d\",", phealth->last_dcvs.batt.temp);
	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"BCUR\":\"%d\",", phealth->last_dcvs.batt.curr);

	// remove , character
	info_size--;

	check_format(buf, &info_size, MAX_LEN_STR);

	return info_size;
}
static DEVICE_ATTR(last_dcvs, 0440, show_last_dcvs, NULL);

static ssize_t store_ap_health(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	char clear = 0;

	sscanf(buf, "%1c", &clear);

	if (clear == 'c' || clear == 'C') {
		if (!phealth)
			phealth = ap_health_data_read();

		if (!phealth) {
			pr_err("%s: fail to get ap health info\n", __func__);
			return -1;
		}
		pr_info("%s: clear ap_health_data by HQM %lu\n", __func__, sizeof(ap_health_t));
		/*++ add here need init data by HQM ++*/
		memset((void *)&(phealth->daily_rr), 0, sizeof(reset_reason_t));
		memset((void *)&(phealth->daily_thermal), 0, sizeof(therm_health_t));
		memset((void *)&(phealth->daily_cache), 0, sizeof(cache_health_t));
		memset((void *)&(phealth->daily_pcie), 0, sizeof(pcie_health_t) * MAX_PCIE_NUM);

		ap_health_data_write(phealth);
	} else {
		pr_info("%s: command error\n", __func__);
	}

	return count;
}

static ssize_t show_ap_health(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;
	int i, cpu;
	char prefix[2][3] = {"SC", "GC"};

	if (!phealth)
		phealth = ap_health_data_read();

	if (!phealth) {
		pr_err("%s: fail to get ap health info\n", __func__);
		return info_size;
	}

	for (i = 0; i < MAX_CLUSTER_NUM; i++) {
		cpu = i * CPU_NUM_PER_CLUSTER;
		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"T%sT\":\"%d\",", prefix[i],
				phealth->thermal.cpu_throttle_cnt[cpu]);
		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"dT%sT\":\"%d\",", prefix[i],
				phealth->daily_thermal.cpu_throttle_cnt[cpu]);
	}

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"TC%dH\":\"%d\",",
				cpu, phealth->thermal.cpu_hotplug_cnt[cpu]);
		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"dTC%dH\":\"%d\",",
				cpu, phealth->daily_thermal.cpu_hotplug_cnt[cpu]);
	}

	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"TR\":\"%d\",", phealth->thermal.ktm_reset_cnt +
				phealth->thermal.gcc_t0_reset_cnt +
				phealth->thermal.gcc_t1_reset_cnt);

	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"dTR\":\"%d\",", phealth->daily_thermal.ktm_reset_cnt +
				phealth->daily_thermal.gcc_t0_reset_cnt +
				phealth->daily_thermal.gcc_t1_reset_cnt);

	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"CEG\":\"%u\",",
				phealth->cache.gld_err_cnt);

	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"dCEG\":\"%u\",",
				phealth->daily_cache.gld_err_cnt);

	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"CEO\":\"%u\",",
				phealth->cache.obsrv_err_cnt);

	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"dCEO\":\"%u\",",
				phealth->daily_cache.obsrv_err_cnt);
	
	for (i = 0; i < MAX_PCIE_NUM; i++) {
		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"P%dPF\":\"%d\",", i,
				phealth->pcie[i].phy_init_fail_cnt);
		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"P%dLD\":\"%d\",", i,
				phealth->pcie[i].link_down_cnt);
		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"P%dLF\":\"%d\",", i,
				phealth->pcie[i].link_up_fail_cnt);
		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"P%dLT\":\"%x\",", i,
				phealth->pcie[i].link_up_fail_ltssm);

		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"dP%dPF\":\"%d\",", i,
				phealth->daily_pcie[i].phy_init_fail_cnt);
		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"dP%dLD\":\"%d\",", i,
				phealth->daily_pcie[i].link_down_cnt);
		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"dP%dLF\":\"%d\",", i,
				phealth->daily_pcie[i].link_up_fail_cnt);
		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"dP%dLT\":\"%x\",", i,
				phealth->daily_pcie[i].link_up_fail_ltssm);
	}
	
	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
			"\"dNP\":\"%d\",", phealth->daily_rr.np);
	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
			"\"dRP\":\"%d\",", phealth->daily_rr.rp);
	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
			"\"dMP\":\"%d\",", phealth->daily_rr.mp);
	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
			"\"dKP\":\"%d\",", phealth->daily_rr.kp);
	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
			"\"dDP\":\"%d\",", phealth->daily_rr.dp);
	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
			"\"dWP\":\"%d\",", phealth->daily_rr.wp);
	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
			"\"dTP\":\"%d\",", phealth->daily_rr.tp);
	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
			"\"dSP\":\"%d\",", phealth->daily_rr.sp);
	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
			"\"dPP\":\"%d\"", phealth->daily_rr.pp);


	check_format(buf, &info_size, MAX_LEN_STR);

	return info_size;
}
static DEVICE_ATTR(ap_health, 0660, show_ap_health, store_ap_health);


static ssize_t show_ddr_info(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;
	uint32_t ch, cs, dq;

	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"DDRV\":\"%s\",", get_ddr_vendor_name());
	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"DSF\":\"%d.%d\",",
				(get_ddr_DSF_version() >> 16) & 0xFFFF,
				get_ddr_DSF_version() & 0xFFFF);
	info_size += scnprintf((char *)(buf+info_size), MAX_LEN_STR - info_size,
				"\"REV1\":\"%02x\",",
				get_ddr_revision_id_1());
	info_size += scnprintf((char *)(buf+info_size), MAX_LEN_STR - info_size,
				"\"REV2\":\"%02x\",",
				get_ddr_revision_id_2());
	info_size += scnprintf((char *)(buf+info_size), MAX_LEN_STR - info_size,
				"\"SIZE\":\"%d\",",
				get_ddr_total_density());

	for (ch = 0; ch < 2; ch++) {
		for (cs = 0; cs < 2; cs++) {
			for (dq = 0; dq < 4; dq++) {

				info_size += scnprintf((char*)(buf+info_size),
					MAX_LEN_STR - info_size,
					"\"RW_%d_%d_%d\":\"%d\",", ch, cs, dq,
					get_ddr_rcw_tDQSCK(ch, cs, dq));
				info_size += scnprintf((char*)(buf+info_size),
					MAX_LEN_STR - info_size,
					"\"WC_%d_%d_%d\":\"%d\",", ch, cs, dq,
					get_ddr_wr_coarseCDC(ch, cs, dq));
				info_size += scnprintf((char*)(buf+info_size),
					MAX_LEN_STR - info_size,
					"\"WF_%d_%d_%d\":\"%d\",", ch, cs, dq,
					get_ddr_wr_fineCDC(ch, cs, dq));
			}
		}
	}

	// remove , character
	info_size--;

	check_format(buf, &info_size, MAX_LEN_STR);

	return info_size;
}

static DEVICE_ATTR(ddr_info, 0440, show_ddr_info, NULL);

static ssize_t show_eye_info(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;
	uint32_t ch, cs, dq;

	info_size += scnprintf((char *)(buf+info_size), MAX_LEN_STR - info_size,
				"\"DDRV\":\"%s\",", get_ddr_vendor_name());
	info_size += scnprintf((char *)(buf+info_size), MAX_LEN_STR - info_size,
				"\"DSF\":\"%d.%d\",",
				(get_ddr_DSF_version() >> 16) & 0xFFFF,
				get_ddr_DSF_version() & 0xFFFF);

	for (ch = 0; ch < 2; ch++) {
		for (cs = 0; cs < 2; cs++) {
			for (dq = 0; dq < 4; dq++) {
				info_size += scnprintf((char *)(buf+info_size),
					MAX_LEN_STR - info_size,
					"\"R_%d_%d_%d\":\"%d\",", ch, cs, dq,
					get_ddr_wr_eyeRect(ch, cs, dq));
				info_size += scnprintf((char *)(buf+info_size),
					MAX_LEN_STR - info_size,
					"\"V_%d_%d_%d\":\"%d\",", ch, cs, dq,
					get_ddr_wr_eyeVref(ch, cs, dq));
				info_size += scnprintf((char *)(buf+info_size),
					MAX_LEN_STR - info_size,
					"\"H_%d_%d_%d\":\"%d\",", ch, cs, dq,
					get_ddr_wr_eyeHeight(ch, cs, dq));
				info_size += scnprintf((char *)(buf+info_size),
					MAX_LEN_STR - info_size,
					"\"W_%d_%d_%d\":\"%d\",", ch, cs, dq,
					get_ddr_wr_eyeWidth(ch, cs, dq));
			}
		}
	}

	// remove , character
	info_size--;

	check_format(buf, &info_size, MAX_LEN_STR);

	return info_size;
}

static DEVICE_ATTR(eye_info, 0440, show_eye_info, NULL);

static int get_param0(int id)
{
	struct device_node *np = of_find_node_by_path("/soc/sec_hw_param");
	u32 val;
	static int num_param = 0;
	static u32 hw_param0[NUM_PARAM0];
	struct property *prop;
	const __be32 *p;

	if (num_param != 0)
		goto out;

	if (!np) {
		pr_err("No sec_hw_param found\n");
		return -1;
	}

	of_property_for_each_u32(np, "param0", prop, p, val) {
		hw_param0[num_param++] = val;

		if (num_param >= NUM_PARAM0)
			break;
	}

out:
	if (id < 0)
		return -2;

	return id < num_param ? hw_param0[id] : -1;
}

static ssize_t show_ap_info(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;
	int i, corner, max_corner;
	char prefix[MAX_CLUSTER_NUM][3] = {"SC", "GC"};

	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				"\"HW_REV\":\"%d\"", system_rev);

	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				",\"SoC_ID\":\"%u\"", socinfo_get_id());

	info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				",\"SoC_REV\":\"%u.%u.%u\"",
				SOCINFO_VERSION_MAJOR(socinfo_get_version()),
				SOCINFO_VERSION_MINOR(socinfo_get_version()), 0);

	for (i = 0; i < MAX_CLUSTER_NUM; i++) {
		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				",\"%s_PRM\":\"%d\"", prefix[i], get_param0(i));

		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				",\"%s_SB\":\"%d\"", prefix[i], cpr3_get_fuse_speed_bin(i));

		info_size += scnprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,
				",\"%s_CR\":\"%d\"", prefix[i], cpr3_get_fuse_cpr_rev(i));

		max_corner = cpr3_get_fuse_corner_count(i);
		for (corner = 0; corner < max_corner; corner++) {
			info_size += scnprintf((char*)(buf+info_size),
				MAX_LEN_STR - info_size,
				",\"%s_OPV_%d\":\"%d\"", prefix[i], corner,
				cpr3_get_fuse_open_loop_voltage(i, corner));
		}

		if (i == MAX_CLUSTER_NUM-1) {
			info_size += scnprintf((char*)(buf+info_size),
					MAX_LEN_STR - info_size,
					",\"%s_OPV_%d\":\"%d\"", prefix[i], corner,
					get_param0(NUM_PARAM0-3)*1000);
		}
	}

	check_format(buf, &info_size, MAX_LEN_STR);

	return info_size;
}

static DEVICE_ATTR(ap_info, 0440, show_ap_info, NULL);

static ssize_t show_extra_info(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	ssize_t offset = 0;
	unsigned long rem_nsec;
	u64 ts_nsec;
	unsigned int reset_reason;

	rst_exinfo_t *p_rst_exinfo = NULL;
	_kern_ex_info_t *p_kinfo = NULL;
	int cpu = -1;

	if (!get_debug_reset_header()) {
		pr_info("%s : updated nothing.\n", __func__);
		goto out;
	}

	reset_reason = sec_debug_get_reset_reason();
	if (reset_reason < USER_UPLOAD_CAUSE_MIN ||
		reset_reason > USER_UPLOAD_CAUSE_MAX) {
		goto out;
	}

	if (reset_reason == USER_UPLOAD_CAUSE_MANUAL_RESET ||
		reset_reason == USER_UPLOAD_CAUSE_REBOOT ||
		reset_reason == USER_UPLOAD_CAUSE_BOOTLOADER_REBOOT ||
		reset_reason == USER_UPLOAD_CAUSE_POWER_ON) {
		goto out;
	}

	p_rst_exinfo = kmalloc(sizeof(rst_exinfo_t), GFP_KERNEL);
	if (!p_rst_exinfo) {
		pr_err("%s : fail - kmalloc\n", __func__);
		goto out;
	}

	if (!read_debug_partition(debug_index_reset_ex_info, p_rst_exinfo)) {
		pr_err("%s : fail - get param!!\n", __func__);
		goto out;
	}
	p_kinfo = &p_rst_exinfo->kern_ex_info.info;
	cpu = p_kinfo->cpu;

	offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
			"\"RR\":\"%s\",", sec_debug_get_reset_reason_str(reset_reason));

	offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
			"\"RWC\":\"%d\",", sec_debug_get_reset_write_cnt());

	ts_nsec = p_kinfo->ktime;
	rem_nsec = do_div(ts_nsec, 1000000000);

	offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
				"\"KTIME\":\"%lu.%06lu\",", (unsigned long)ts_nsec, rem_nsec / 1000);

	offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
			"\"CPU\":\"%d\",", p_kinfo->cpu);


	offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
			"\"TASK\":\"%s\",", p_kinfo->task_name);

	if (p_kinfo->smmu.fsr) {
		offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
				"\"SDN\":\"%s\",", p_kinfo->smmu.dev_name);

		offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
				"\"FSR\":\"%x\",", p_kinfo->smmu.fsr);

		if (p_kinfo->smmu.fsynr) {
			offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
					"\"FSRY\":\"%x\",", p_kinfo->smmu.fsynr);

			offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
					"\"IOVA\":\"%08lx\",", p_kinfo->smmu.iova);

			offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
					"\"FAR\":\"%016lx\",", p_kinfo->smmu.far);

			offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
					"\"SMN\":\"%s\",", p_kinfo->smmu.mas_name);

			offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
					"\"CB\":\"%d\",", p_kinfo->smmu.cbndx);

			offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
					"\"SOFT\":\"%016llx\",", p_kinfo->smmu.phys_soft);

			offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
					"\"ATOS\":\"%016llx\",", p_kinfo->smmu.phys_atos);

			offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
					"\"SID\":\"%x\",", p_kinfo->smmu.sid);
		}
	}

	if (p_kinfo->badmode.esr) {
		offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
				"\"BDR\":\"%08x\",", p_kinfo->badmode.reason);

		offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
				"\"BDRS\":\"%s\",", p_kinfo->badmode.handler_str);

		offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
				"\"BDE\":\"%08x\",", p_kinfo->badmode.esr);

		offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
				"\"BDES\":\"%s\",", p_kinfo->badmode.esr_str);
	}

	if ((cpu > -1) && (cpu < NR_CPUS)) { // 0 ~ 7
		if (p_kinfo->fault[cpu].esr) {
			offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
					"\"ESR\":\"%08x\",", p_kinfo->fault[cpu].esr);

			offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
					"\"FNM\":\"%s\",", p_kinfo->fault[cpu].str);

			offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
					"\"FV1\":\"%016llx\",", p_kinfo->fault[cpu].var1);

			offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
					"\"FV2\":\"%016llx\",", p_kinfo->fault[cpu].var2);
		}

		offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
				"\"FAULT\":\"pgd=%016llx VA=%016llx *pgd=%016llx *pud=%016llx *pmd=%016llx *pte=%016llx\",",
				p_kinfo->fault[cpu].pte[0], p_kinfo->fault[cpu].pte[1], p_kinfo->fault[cpu].pte[2],
				p_kinfo->fault[cpu].pte[3], p_kinfo->fault[cpu].pte[4], p_kinfo->fault[cpu].pte[5]);
	}

	offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
				"\"BUG\":\"%s\",", p_kinfo->bug_buf);

	offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
				"\"PANIC\":\"%s\",", p_kinfo->panic_buf);

	offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
				"\"PC\":\"%s\",", p_kinfo->pc);

	offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
				"\"LR\":\"%s\",", p_kinfo->lr);

	offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
				"\"GLE\":\"%s\",", p_kinfo->dbg0);

	offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
			"\"UFS\":\"%s\",", p_kinfo->ufs_err);

	offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
			"\"DISP\":\"%s\",", p_kinfo->display_err);

	offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
			"\"ROT\":\"W%dC%d\",", get_param0(3), get_param0(4));

	offset += scnprintf((char *)buf + offset, MAX_ETRA_LEN - offset,
				"\"STACK\":\"%s\"", p_kinfo->backtrace);
out:
	if (p_rst_exinfo)
		kfree(p_rst_exinfo);

	check_format(buf, &offset, MAX_ETRA_LEN);

	return offset;
}

static DEVICE_ATTR(extra_info, 0440, show_extra_info, NULL);


static ssize_t show_extrb_info(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	ssize_t offset = 0;
	int idx, cnt, max_cnt;
	unsigned long rem_nsec;
	u64 ts_nsec;
	unsigned int reset_reason;

	rst_exinfo_t *p_rst_exinfo = NULL;
	__rpm_log_t *pRPMlog = NULL;

	if (!get_debug_reset_header()) {
		pr_info("%s : updated nothing.\n", __func__);
		goto out;
	}

	reset_reason = sec_debug_get_reset_reason();
	if (reset_reason < USER_UPLOAD_CAUSE_MIN ||
		reset_reason > USER_UPLOAD_CAUSE_MAX) {
		goto out;
	}

	if (reset_reason == USER_UPLOAD_CAUSE_MANUAL_RESET ||
		reset_reason == USER_UPLOAD_CAUSE_REBOOT ||
		reset_reason == USER_UPLOAD_CAUSE_BOOTLOADER_REBOOT ||
		reset_reason == USER_UPLOAD_CAUSE_POWER_ON) {
		goto out;
	}

	p_rst_exinfo = kmalloc(sizeof(rst_exinfo_t), GFP_KERNEL);
	if (!p_rst_exinfo) {
		pr_err("%s : fail - kmalloc\n", __func__);
		goto out;
	}

	if (!read_debug_partition(debug_index_reset_ex_info, p_rst_exinfo)) {
		pr_err("%s : fail - get param!!\n", __func__);
		goto out;
	}

	offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset,
			"\"RR\":\"%s\",", sec_debug_get_reset_reason_str(reset_reason));

	offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset,
			"\"RWC\":\"%d\",", sec_debug_get_reset_write_cnt());

	if (p_rst_exinfo->rpm_ex_info.info.magic == RPM_EX_INFO_MAGIC
		 && p_rst_exinfo->rpm_ex_info.info.nlog > 0) {
		offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset, "\"RPM\":\"");

		if (p_rst_exinfo->rpm_ex_info.info.nlog > 5) {
			idx = p_rst_exinfo->rpm_ex_info.info.nlog % 5;
			max_cnt = 5;
		} else {
			idx = 0;
			max_cnt = p_rst_exinfo->rpm_ex_info.info.nlog;
		}

		for (cnt  = 0; cnt < max_cnt; cnt++, idx++) {
			pRPMlog = &p_rst_exinfo->rpm_ex_info.info.log[idx % 5];

			ts_nsec = pRPMlog->nsec;
			rem_nsec = do_div(ts_nsec, 1000000000);

			offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset,
					"%lu.%06lu ",
					(unsigned long)ts_nsec, rem_nsec / 1000);

			offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset,
					"%s ",	pRPMlog->msg);

			offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset,
					"%x %x %x %x",
					pRPMlog->arg[0], pRPMlog->arg[1], pRPMlog->arg[2], pRPMlog->arg[3]);

			if (cnt == max_cnt - 1) {
				offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset, "\",");
			} else {
				offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset, "/");
			}
		}
	}
	offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset,
			"\"TZ_RR\":\"%s\"", p_rst_exinfo->tz_ex_info.msg);
	offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset,
			",\"PIMEM\":\"0x%08x,0x%08x\"",
			p_rst_exinfo->pimem_info.esr, p_rst_exinfo->pimem_info.ear0);
	offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset,
			",\"HYP\":\"%s\"", p_rst_exinfo->hyp_ex_info.msg);

	offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset, ",\"LPM\":\"");
	max_cnt = sizeof(p_rst_exinfo->kern_ex_info.info.lpm_state);
	max_cnt = max_cnt/sizeof(p_rst_exinfo->kern_ex_info.info.lpm_state[0]);
	for (idx = 0; idx < max_cnt; idx++) {
		offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset,
				"%x", p_rst_exinfo->kern_ex_info.info.lpm_state[idx]);
		if (idx != max_cnt - 1)
			offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset, ",");
	}
	offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset, "\"");

	offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset, ",\"PKO\":\"%x\"",
			p_rst_exinfo->kern_ex_info.info.pko);

	offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset, ",\"LR\":\"");
	max_cnt = sizeof(p_rst_exinfo->kern_ex_info.info.lr_val);
	max_cnt = max_cnt/sizeof(p_rst_exinfo->kern_ex_info.info.lr_val[0]);
	for (idx = 0; idx < max_cnt; idx++) {
		offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset,
				"%llx", p_rst_exinfo->kern_ex_info.info.lr_val[idx]);
		if (idx != max_cnt - 1)
			offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset, ",");
	}
	offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset, "\"");

	offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset, ",\"PC\":\"");
	max_cnt = sizeof(p_rst_exinfo->kern_ex_info.info.pc_val);
	max_cnt = max_cnt/sizeof(p_rst_exinfo->kern_ex_info.info.pc_val[0]);
	for (idx = 0; idx < max_cnt; idx++) {
		offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset,
				"%llx", p_rst_exinfo->kern_ex_info.info.pc_val[idx]);
		if (idx != max_cnt - 1)
			offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset, ",");
	}
	offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset, "\"");
out:
	if (p_rst_exinfo)
		kfree(p_rst_exinfo);

	check_format(buf, &offset, MAX_LEN_STR);

	return offset;
}

static DEVICE_ATTR(extrb_info, 0440, show_extrb_info, NULL);

static ssize_t show_extrc_info(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t offset = 0;
	unsigned int reset_reason;
	char extrc_buf[1024];

	if (!get_debug_reset_header()) {
		pr_info("%s : updated nothing.\n", __func__);
		goto out;
	}

	reset_reason = sec_debug_get_reset_reason();
	if (reset_reason < USER_UPLOAD_CAUSE_MIN ||
		reset_reason > USER_UPLOAD_CAUSE_MAX) {
		goto out;
	}

	if (reset_reason == USER_UPLOAD_CAUSE_MANUAL_RESET ||
		reset_reason == USER_UPLOAD_CAUSE_REBOOT ||
		reset_reason == USER_UPLOAD_CAUSE_BOOTLOADER_REBOOT ||
		reset_reason == USER_UPLOAD_CAUSE_POWER_ON) {
		goto out;
	}

	if (!read_debug_partition(debug_index_reset_extrc_info, &extrc_buf)) {
		pr_err("%s : fail - get param!!\n", __func__);
		goto out;
	}

	offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset,
			"\"RR\":\"%s\",", sec_debug_get_reset_reason_str(reset_reason));

	offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset,
			"\"RWC\":\"%d\",", sec_debug_get_reset_write_cnt());

	extrc_buf[1023] = '\0';

	offset += scnprintf((char *)buf + offset, MAX_LEN_STR - offset,
			"\"LKL\":\"%s\"", &extrc_buf[offset]);
out:
	check_format(buf, &offset, MAX_LEN_STR);

	return offset;
}

static DEVICE_ATTR(extrc_info, 0440, show_extrc_info, NULL);


static int sec_hw_param_dbg_part_notifier_callback(
	struct notifier_block *nfb, unsigned long action, void *data)
{
	switch (action) {
		case DBG_PART_DRV_INIT_DONE:
			phealth = ap_health_data_read();

			memset((void *)&(phealth->battery), 0, sizeof(battery_health_t)); 
			ap_health_data_write(phealth);
			break;
		default:
			return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block sec_hw_param_dbg_part_notifier = {
	.notifier_call = sec_hw_param_dbg_part_notifier_callback,
};

static int __init sec_hw_param_init(void)
{
	struct device* sec_hw_param_dev;

	sec_hw_param_dev = device_create(sec_class, NULL, 0, NULL, "sec_hw_param");

	if (IS_ERR(sec_hw_param_dev)) {
		pr_err("%s: Failed to create devce\n",__func__);
		goto out;
	}

	if (device_create_file(sec_hw_param_dev, &dev_attr_ap_info) < 0) {
		pr_err("%s: could not create ap_info sysfs node\n", __func__);
	}

	if (device_create_file(sec_hw_param_dev, &dev_attr_ddr_info) < 0) {
		pr_err("%s: could not create ddr_info sysfs node\n", __func__);
	}

	if (device_create_file(sec_hw_param_dev, &dev_attr_eye_info) < 0) {
		pr_err("%s: could not create eye_info sysfs node\n", __func__);
	}

	if (device_create_file(sec_hw_param_dev, &dev_attr_ap_health) < 0) {
		pr_err("%s: could not create ap_health sysfs node\n", __func__);
	}

	if (device_create_file(sec_hw_param_dev, &dev_attr_last_dcvs) < 0) {
		pr_err("%s: could not create last_dcvs sysfs node\n", __func__);
	}

	if (device_create_file(sec_hw_param_dev, &dev_attr_extra_info) < 0) {
		pr_err("%s: could not create extra_info sysfs node\n", __func__);
	}

	if (device_create_file(sec_hw_param_dev, &dev_attr_extrb_info) < 0) {
		pr_err("%s: could not create extrb_info sysfs node\n", __func__);
	}

	if (device_create_file(sec_hw_param_dev, &dev_attr_extrc_info) < 0) {
		pr_err("%s: could not create extra_info sysfs node\n", __func__);
	}

	dbg_partition_notifier_register(&sec_hw_param_dbg_part_notifier);
out:
	return 0;
}
device_initcall(sec_hw_param_init);
