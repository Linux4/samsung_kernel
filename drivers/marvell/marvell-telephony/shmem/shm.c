/*
    shm.c Created on: Aug 2, 2010, Jinhua Huang <jhhuang@marvell.com>

    Marvell PXA9XX ACIPC-MSOCKET driver for Linux
    Copyright (C) 2010 Marvell International Ltd.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/types.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#if 0
#include <linux/of.h>
#include <linux/of_device.h>
#endif

#include "shm.h"
#include "shm_map.h"
#include "acipcd.h"
#include "pxa_m3_rm.h"
#include "debugfs.h"

#define SHM_SKBUF_SIZE		2048	/* maximum packet size */
#define SHM_PSD_TX_SKBUF_SIZE	2048	/* PSD tx maximum packet size */
#define SHM_PSD_RX_SKBUF_SIZE	16384	/* PSD rx maximum packet size */

struct shm_rbctl shm_rbctl[shm_rb_total_cnt] = {
	[shm_rb_main] = {
		.grp_type = shm_grp_cp,
		.name = "cp-portq"
	},
	[shm_rb_psd] = {
		.grp_type = shm_grp_cp,
		.name = "cp-psd"
	},
	[shm_rb_diag] = {
		.grp_type = shm_grp_cp,
		.name = "cp-diag"
	},
	[shm_rb_m3] = {
		.grp_type = shm_grp_m3,
		.name = "m3"
	},
};

static struct dentry *tel_debugfs_root_dir;
static struct dentry *shm_debugfs_root_dir;

static int debugfs_show_ks_info(struct seq_file *s, void *data)
{
	struct shm_rbctl *rbctl = s->private;
	int ret = 0;

	ret += seq_printf(s, "skctl_pa\t: 0x%p\n", (void *)rbctl->skctl_pa);
	ret += seq_printf(s, "skctl_va\t: 0x%p\n", rbctl->skctl_va);

	return ret;
}

static int debugfs_show_tx_buf_info(struct seq_file *s, void *data)
{
	struct shm_rbctl *rbctl = s->private;
	int ret = 0;

	ret += seq_printf(s, "tx_pa\t\t: 0x%p\n", (void *)rbctl->tx_pa);
	ret += seq_printf(s, "tx_va\t\t: 0x%p\n", rbctl->tx_va);
	ret += seq_printf(s, "tx_total_size\t: %d\n",
		rbctl->tx_total_size);
	ret += seq_printf(s, "tx_skbuf_size\t: %d\n",
		rbctl->tx_skbuf_size);
	ret += seq_printf(s, "tx_skbuf_num\t: %d\n",
		rbctl->tx_skbuf_num);
	ret += seq_printf(s, "tx_skbuf_low_wm\t: %d\n",
		rbctl->tx_skbuf_low_wm);

	return ret;
}

static int debugfs_show_rx_buf_info(struct seq_file *s, void *data)
{
	struct shm_rbctl *rbctl = s->private;
	int ret = 0;

	ret += seq_printf(s, "rx_pa\t\t: 0x%p\n", (void *)rbctl->rx_pa);
	ret += seq_printf(s, "rx_va\t\t: 0x%p\n", rbctl->rx_va);
	ret += seq_printf(s, "rx_total_size\t: %d\n",
		rbctl->rx_total_size);
	ret += seq_printf(s, "rx_skbuf_size\t: %d\n",
		rbctl->rx_skbuf_size);
	ret += seq_printf(s, "rx_skbuf_num\t: %d\n",
		rbctl->rx_skbuf_num);
	ret += seq_printf(s, "rx_skbuf_low_wm\t: %d\n",
		rbctl->rx_skbuf_low_wm);

	return ret;
}

static int debugfs_show_fc_stat(struct seq_file *s, void *data)
{
	struct shm_rbctl *rbctl = s->private;
	int ret = 0;

	ret += seq_printf(s, "is_ap_xmit_stopped\t: %s\n",
		rbctl->is_ap_xmit_stopped ? "Y" : "N");
	ret += seq_printf(s, "is_cp_xmit_stopped\t: %s\n",
		rbctl->is_cp_xmit_stopped ? "Y" : "N");

	return ret;
}

static int debugfs_show_stat(struct seq_file *s, void *data)
{
	struct shm_rbctl *rbctl = s->private;
	int ret = 0;

	ret += seq_printf(s, "ap_stopped_num\t: %lu\n",
		rbctl->ap_stopped_num);
	ret += seq_printf(s, "ap_resumed_num\t: %lu\n",
		rbctl->ap_resumed_num);
	ret += seq_printf(s, "cp_stopped_num\t: %lu\n",
		rbctl->cp_stopped_num);
	ret += seq_printf(s, "cp_resumed_num\t: %lu\n",
		rbctl->cp_resumed_num);

	return ret;
}

TEL_DEBUG_ENTRY(ks_info);
TEL_DEBUG_ENTRY(tx_buf_info);
TEL_DEBUG_ENTRY(rx_buf_info);
TEL_DEBUG_ENTRY(fc_stat);
TEL_DEBUG_ENTRY(stat);

static int shm_rb_debugfs_init(struct shm_rbctl *rbctl)
{
	struct dentry *ksdir;
	char buf[16];

	sprintf(buf, "shm%d", rbctl->rb_type);

	rbctl->rbdir = debugfs_create_dir(rbctl->name ? rbctl->name : buf,
		shm_debugfs_root_dir);
	if (IS_ERR_OR_NULL(rbctl->rbdir))
		return -ENOMEM;

	if (IS_ERR_OR_NULL(debugfs_create_file("ks_info", S_IRUGO,
				rbctl->rbdir, rbctl, &fops_ks_info)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_file("tx_buf_info", S_IRUGO,
				rbctl->rbdir, rbctl, &fops_tx_buf_info)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_file("rx_buf_info", S_IRUGO,
				rbctl->rbdir, rbctl, &fops_rx_buf_info)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_file("fc_stat", S_IRUGO,
				rbctl->rbdir, rbctl, &fops_fc_stat)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_file("stat", S_IRUGO,
				rbctl->rbdir, rbctl, &fops_stat)))
		goto error;

	ksdir = debugfs_create_dir("key_section", rbctl->rbdir);
	if (IS_ERR_OR_NULL(ksdir))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_int(
				"ap_wptr", S_IRUGO | S_IWUSR, ksdir,
				(int *)
				&rbctl->skctl_va->ap_wptr)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_int(
				"cp_rptr", S_IRUGO | S_IWUSR, ksdir,
				(int *)
				&rbctl->skctl_va->cp_rptr)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_int(
				"ap_rptr", S_IRUGO | S_IWUSR, ksdir,
				(int *)
				&rbctl->skctl_va->ap_rptr)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_int(
				"cp_wptr", S_IRUGO | S_IWUSR, ksdir,
				(int *)
				&rbctl->skctl_va->cp_wptr)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_uint(
				"ap_port_fc", S_IRUGO | S_IWUSR, ksdir,
				(unsigned int *)
				&rbctl->skctl_va->ap_port_fc)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_uint(
				"cp_port_fc", S_IRUGO | S_IWUSR, ksdir,
				(unsigned int *)
				&rbctl->skctl_va->cp_port_fc)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_uint(
				"ap_pcm_master", S_IRUGO | S_IWUSR, ksdir,
				(unsigned int *)
				&rbctl->skctl_va->ap_pcm_master)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_uint(
				"cp_pcm_master", S_IRUGO | S_IWUSR, ksdir,
				(unsigned int *)
				&rbctl->skctl_va->cp_pcm_master)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_uint(
				"modem_ddrfreq", S_IRUGO | S_IWUSR, ksdir,
				(unsigned int *)
				&rbctl->skctl_va->modem_ddrfreq)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_uint(
				"reset_request", S_IRUGO | S_IWUSR, ksdir,
				(unsigned int *)
				&rbctl->skctl_va->reset_request)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_uint(
				"diag_header_ptr", S_IRUGO | S_IWUSR, ksdir,
				(unsigned int *)
				&rbctl->skctl_va->diag_header_ptr)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_uint(
				"diag_cp_db_ver", S_IRUGO | S_IWUSR, ksdir,
				(unsigned int *)
				&rbctl->skctl_va->diag_cp_db_ver)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_uint(
				"diag_ap_db_ver", S_IRUGO | S_IWUSR, ksdir,
				(unsigned int *)
				&rbctl->skctl_va->diag_ap_db_ver)))
		goto error;

	return 0;

error:
	debugfs_remove_recursive(rbctl->rbdir);
	rbctl->rbdir = NULL;
	return -1;
}

static int shm_rb_debugfs_exit(struct shm_rbctl *rbctl)
{
	debugfs_remove_recursive(rbctl->rbdir);
	rbctl->rbdir = NULL;
	return 0;
}

static int shm_param_init(enum shm_grp_type grp_type,
	const void *data)
{
	if (!data)
		return -1;

	if (grp_type == shm_grp_cp) {
		const struct cpload_cp_addr *addr =
			(const struct cpload_cp_addr *)data;

		/* main ring buffer */
		shm_rbctl[shm_rb_main].skctl_pa = addr->main_skctl_pa;

		shm_rbctl[shm_rb_main].tx_skbuf_size = SHM_SKBUF_SIZE;
		shm_rbctl[shm_rb_main].rx_skbuf_size = SHM_SKBUF_SIZE;

		shm_rbctl[shm_rb_main].tx_pa = addr->main_tx_pa;
		shm_rbctl[shm_rb_main].rx_pa = addr->main_rx_pa;

		shm_rbctl[shm_rb_main].tx_total_size = addr->main_tx_total_size;
		shm_rbctl[shm_rb_main].rx_total_size = addr->main_rx_total_size;
		
#ifdef CONFIG_SSIPC_SUPPORT
		shm_rbctl[shm_rb_main].uuid_high = addr->uuid_high;
		shm_rbctl[shm_rb_main].uuid_low = addr->uuid_low;
#endif

		shm_rbctl[shm_rb_main].tx_skbuf_num =
			shm_rbctl[shm_rb_main].tx_total_size /
			shm_rbctl[shm_rb_main].tx_skbuf_size;
		shm_rbctl[shm_rb_main].rx_skbuf_num =
			shm_rbctl[shm_rb_main].rx_total_size /
			shm_rbctl[shm_rb_main].rx_skbuf_size;

		shm_rbctl[shm_rb_main].tx_skbuf_low_wm =
			(shm_rbctl[shm_rb_main].tx_skbuf_num + 1) / 4;
		shm_rbctl[shm_rb_main].rx_skbuf_low_wm =
			(shm_rbctl[shm_rb_main].rx_skbuf_num + 1) / 4;

		/* psd dedicated ring buffer */
		shm_rbctl[shm_rb_psd].skctl_pa = addr->psd_skctl_pa;

		shm_rbctl[shm_rb_psd].tx_skbuf_size = SHM_PSD_TX_SKBUF_SIZE;
		shm_rbctl[shm_rb_psd].rx_skbuf_size = SHM_PSD_RX_SKBUF_SIZE;

		shm_rbctl[shm_rb_psd].tx_pa = addr->psd_tx_pa;
		shm_rbctl[shm_rb_psd].rx_pa = addr->psd_rx_pa;

		shm_rbctl[shm_rb_psd].tx_total_size = addr->psd_tx_total_size;
		shm_rbctl[shm_rb_psd].rx_total_size = addr->psd_rx_total_size;

		shm_rbctl[shm_rb_psd].tx_skbuf_num =
			shm_rbctl[shm_rb_psd].tx_total_size /
			shm_rbctl[shm_rb_psd].tx_skbuf_size;
		shm_rbctl[shm_rb_psd].rx_skbuf_num =
			shm_rbctl[shm_rb_psd].rx_total_size /
			shm_rbctl[shm_rb_psd].rx_skbuf_size;

		shm_rbctl[shm_rb_psd].tx_skbuf_low_wm =
			(shm_rbctl[shm_rb_psd].tx_skbuf_num + 1) / 4;
		shm_rbctl[shm_rb_psd].rx_skbuf_low_wm =
			(shm_rbctl[shm_rb_psd].rx_skbuf_num + 1) / 4;

		/* diag ring buffer */
		shm_rbctl[shm_rb_diag].skctl_pa = addr->diag_skctl_pa;

		shm_rbctl[shm_rb_diag].tx_skbuf_size = SHM_SKBUF_SIZE;
		shm_rbctl[shm_rb_diag].rx_skbuf_size = SHM_SKBUF_SIZE;

		shm_rbctl[shm_rb_diag].tx_pa = addr->diag_tx_pa;
		shm_rbctl[shm_rb_diag].rx_pa = addr->diag_rx_pa;

		shm_rbctl[shm_rb_diag].tx_total_size = addr->diag_tx_total_size;
		shm_rbctl[shm_rb_diag].rx_total_size = addr->diag_rx_total_size;

		shm_rbctl[shm_rb_diag].tx_skbuf_num =
			shm_rbctl[shm_rb_diag].tx_total_size /
			shm_rbctl[shm_rb_diag].tx_skbuf_size;
		shm_rbctl[shm_rb_diag].rx_skbuf_num =
			shm_rbctl[shm_rb_diag].rx_total_size /
			shm_rbctl[shm_rb_diag].rx_skbuf_size;

		shm_rbctl[shm_rb_diag].tx_skbuf_low_wm =
			(shm_rbctl[shm_rb_diag].tx_skbuf_num + 1) / 4;
		shm_rbctl[shm_rb_diag].rx_skbuf_low_wm =
			(shm_rbctl[shm_rb_diag].rx_skbuf_num + 1) / 4;
	} else if (grp_type == shm_grp_m3) {
		const struct rm_m3_addr *addr =
			(const struct rm_m3_addr *)data;

		shm_rbctl[shm_rb_m3].skctl_pa = addr->m3_rb_ctrl_start_addr;

		pr_info("M3 RB PA: 0x%08lx\n", shm_rbctl[shm_rb_m3].skctl_pa);

		shm_rbctl[shm_rb_m3].tx_skbuf_size = M3_SHM_SKBUF_SIZE;
		shm_rbctl[shm_rb_m3].rx_skbuf_size = M3_SHM_SKBUF_SIZE;

		pr_info("M3 RB PACKET TX SIZE: %d, RX SIZE: %d\n",
			shm_rbctl[shm_rb_m3].tx_skbuf_size,
			shm_rbctl[shm_rb_m3].rx_skbuf_size);

		shm_rbctl[shm_rb_m3].tx_pa = addr->m3_ddr_mb_start_addr;
		shm_rbctl[shm_rb_m3].tx_skbuf_num = M3_SHM_AP_TX_MAX_NUM;
		shm_rbctl[shm_rb_m3].tx_total_size =
			shm_rbctl[shm_rb_m3].tx_skbuf_num *
			shm_rbctl[shm_rb_m3].tx_skbuf_size;


		shm_rbctl[shm_rb_m3].rx_pa = shm_rbctl[shm_rb_m3].tx_pa +
			shm_rbctl[shm_rb_m3].tx_total_size;
		shm_rbctl[shm_rb_m3].rx_skbuf_num = M3_SHM_AP_RX_MAX_NUM;
		shm_rbctl[shm_rb_m3].rx_total_size =
			shm_rbctl[shm_rb_m3].rx_skbuf_num *
			shm_rbctl[shm_rb_m3].rx_skbuf_size;

		shm_rbctl[shm_rb_m3].tx_skbuf_low_wm =
			(shm_rbctl[shm_rb_m3].tx_skbuf_num + 1) / 4;
		shm_rbctl[shm_rb_m3].rx_skbuf_low_wm =
			(shm_rbctl[shm_rb_m3].rx_skbuf_num + 1) / 4;
	}

	return 0;
}

void shm_rb_data_init(struct shm_rbctl *rbctl)
{
	unsigned int network_mode;

	rbctl->is_ap_xmit_stopped = false;
	rbctl->is_cp_xmit_stopped = false;

	rbctl->ap_stopped_num = 0;
	rbctl->ap_resumed_num = 0;
	rbctl->cp_stopped_num = 0;
	rbctl->cp_resumed_num = 0;

	network_mode = rbctl->skctl_va->network_mode;
	memset(rbctl->skctl_va, 0, sizeof(struct shm_skctl));
	rbctl->skctl_va->network_mode = network_mode;
	rbctl->skctl_va->ap_pcm_master = PMIC_MASTER_FLAG;
#ifdef CONFIG_SSIPC_SUPPORT
	rbctl->skctl_va->uuid_high = rbctl->uuid_high;
	rbctl->skctl_va->uuid_low = rbctl->uuid_low;
#endif
}

void shm_lock_init(void)
{
	struct shm_rbctl *rbctl;
	struct shm_rbctl *rbctl_end = shm_rbctl + shm_rb_total_cnt;

	for (rbctl = shm_rbctl; rbctl != rbctl_end; ++rbctl)
		mutex_init(&rbctl->va_lock);
}

static inline void shm_rb_dump(struct shm_rbctl *rbctl)
{
	pr_info(
		"rb_type %d:\n"
		"\tskctl_pa: 0x%08lx, skctl_va: 0x%p\n"
		"\ttx_pa: 0x%08lx, tx_va: 0x%p\n"
		"\ttx_total_size: 0x%08x, tx_skbuf_size: 0x%08x\n"
		"\ttx_skbuf_num: 0x%08x, tx_skbuf_low_wm: 0x%08x\n"
		"\trx_pa: 0x%08lx, rx_va: 0x%p\n"
		"\trx_total_size: 0x%08x, rx_skbuf_size: 0x%08x\n"
		"\trx_skbuf_num: 0x%08x, rx_skbuf_low_wm: 0x%08x\n",
		(int)(rbctl - shm_rbctl),
		rbctl->skctl_pa, rbctl->skctl_va,
		rbctl->tx_pa, rbctl->tx_va,
		rbctl->tx_total_size, rbctl->tx_skbuf_size,
		rbctl->tx_skbuf_num, rbctl->tx_skbuf_low_wm,
		rbctl->rx_pa, rbctl->rx_va,
		rbctl->rx_total_size, rbctl->rx_skbuf_size,
		rbctl->rx_skbuf_num, rbctl->rx_skbuf_low_wm
	);
#ifdef CONFIG_SSIPC_SUPPORT
	pr_info("\tuuid_high: 0x%08x, uuid_low: 0x%08x\n",
			rbctl->uuid_high, rbctl->uuid_low);
#endif
}

#if 0
static void read_mv_profile(void)
{
	struct device_node *node;
	int pro = 0;

	node = of_find_compatible_node(NULL, NULL, "marvell,profile");
	if (node) {
		if (of_property_read_u32(node, "marvell,profile-number", &pro))
			pr_info("can't read profile number\n");
		else {
			pr_info("SoC profile number: %d\n", pro);
			shm_rbctl[shm_rb_main].skctl_va->profile_number = pro;
		}
	} else {
		pr_info("can't find marvell,profile dts\n");
	}
}

static void read_dvc_table(void)
{
	struct shm_skctl *skctl_va = shm_rbctl[shm_rb_main].skctl_va;
	unsigned int dvc_num, dvc_tbl[16];
	int i = 0;

#ifdef CONFIG_PXA_DVFS
	get_voltage_table_for_cp(dvc_tbl, &dvc_num);
#else
	dvc_num = 0;
#endif
	skctl_va->dvc_vol_tbl_num = dvc_num;

	pr_info("skctl_va->dvc_vol_tbl_num = %u\n", skctl_va->dvc_vol_tbl_num);
	for (i = 0; i < dvc_num; i++) {
		skctl_va->dvc_vol_tbl[i] = dvc_tbl[i];
		pr_info("skctl_va->dvc_vol_tbl[%d] = %u\n",
			i, skctl_va->dvc_vol_tbl[i]);
	}
	return;
}
#endif
static void set_version_numb(void)
{
	struct shm_skctl *skctl_va = shm_rbctl[shm_rb_main].skctl_va;

	skctl_va->version_magic = VERSION_MAGIC_FLAG;
	skctl_va->version_number = VERSION_NUMBER_FLAG;
	return;
}
#if 0
static void read_dfc_table(void)
{
	struct shm_skctl *skctl_va = shm_rbctl[shm_rb_main].skctl_va;
	struct pxa1928_ddr_opt *tbl;
	int len = 0, i = 0;

	get_dfc_tables(CIU_VIR_BASE, &tbl, &len);
	BUG_ON(len > (sizeof(skctl_va->dfc_dclk) / sizeof(skctl_va->dfc_dclk[0])));
	skctl_va->dfc_dclk_num = len;
	for (i = 0; i < len; i++) {
		skctl_va->dfc_dclk[i] = tbl[i].dclk;
		pr_info("skctl_va->dfc_dclk[%d] = %u\n", i, skctl_va->dfc_dclk[i]);
	}
	return;
}
#endif

static void get_dvc_info(void)
{
	struct shm_skctl *skctl_va = shm_rbctl[shm_rb_main].skctl_va;
	struct cpmsa_dvc_info dvc_vol_info;
	int i = 0;

	getcpdvcinfo(&dvc_vol_info);
	for (i = 0; i < MAX_CP_PPNUM; i++) {
		skctl_va->cp_freq[i] = dvc_vol_info.cpdvcinfo[i].cpfreq;
		skctl_va->cp_vol[i] = dvc_vol_info.cpdvcinfo[i].cpvl;
	}
	skctl_va->msa_dvc_vol = dvc_vol_info.msadvcvl;
}

static int shm_rb_init(struct shm_rbctl *rbctl)
{
	rbctl->rb_type = rbctl - shm_rbctl;

	mutex_lock(&rbctl->va_lock);
	/* map to non-cache virtual address */
	rbctl->skctl_va =
	    shm_map(rbctl->skctl_pa, sizeof(struct shm_skctl));
	if (!rbctl->skctl_va)
		goto exit1;

	/* map ring buffer to cacheable memeory, if it is DDR */
	if (pfn_valid(__phys_to_pfn(rbctl->tx_pa))) {
		rbctl->tx_cacheable = true;
		rbctl->tx_va = phys_to_virt(rbctl->tx_pa);
	} else {
		rbctl->tx_cacheable = false;
		rbctl->tx_va = shm_map(rbctl->tx_pa, rbctl->tx_total_size);
	}

	if (!rbctl->tx_va)
		goto exit2;

	if (pfn_valid(__phys_to_pfn(rbctl->rx_pa))) {
		rbctl->rx_cacheable = true;
		rbctl->rx_va = phys_to_virt(rbctl->rx_pa);
	} else {
		rbctl->rx_cacheable = false;
		rbctl->rx_va = shm_map(rbctl->rx_pa, rbctl->rx_total_size);
	}

	if (!rbctl->rx_va)
		goto exit3;

	mutex_unlock(&rbctl->va_lock);
	shm_rb_data_init(rbctl);
	shm_rb_dump(rbctl);

	if (shm_rb_debugfs_init(rbctl) < 0)
		goto exit4;

	return 0;

exit4:
	shm_rb_debugfs_exit(rbctl);
	if (!rbctl->rx_cacheable)
		shm_unmap(rbctl->rx_pa, rbctl->rx_va);
	rbctl->rx_cacheable = false;
exit3:
	if (!rbctl->tx_cacheable)
		shm_unmap(rbctl->tx_pa, rbctl->tx_va);
	rbctl->tx_cacheable = false;
exit2:
	shm_unmap(rbctl->skctl_pa, rbctl->skctl_va);
exit1:
	mutex_unlock(&rbctl->va_lock);
	return -1;
}

static int shm_rb_exit(struct shm_rbctl *rbctl)
{
	void *skctl_va = rbctl->skctl_va;
	void *tx_va = rbctl->tx_va;
	void *rx_va = rbctl->rx_va;

	shm_rb_debugfs_exit(rbctl);
	/* release memory */
	mutex_lock(&rbctl->va_lock);
	rbctl->skctl_va = NULL;
	rbctl->tx_va = NULL;
	rbctl->rx_va = NULL;
	mutex_unlock(&rbctl->va_lock);
	shm_unmap(rbctl->skctl_pa, skctl_va);
	if (!rbctl->tx_cacheable)
		shm_unmap(rbctl->tx_pa, tx_va);
	rbctl->tx_cacheable = false;
	if (!rbctl->rx_cacheable)
		shm_unmap(rbctl->rx_pa, rx_va);
	rbctl->rx_cacheable = false;

	return 0;
}

int shm_debugfs_init(void)
{
	tel_debugfs_root_dir = tel_debugfs_get();
	if (!tel_debugfs_root_dir)
		return -1;

	shm_debugfs_root_dir = debugfs_create_dir("shm", tel_debugfs_root_dir);
	if (IS_ERR_OR_NULL(shm_debugfs_root_dir))
		goto put_rootfs;

	return 0;

put_rootfs:
	tel_debugfs_put(tel_debugfs_root_dir);
	tel_debugfs_root_dir = NULL;

	return -1;
}

void shm_debugfs_exit(void)
{
	debugfs_remove(shm_debugfs_root_dir);
	shm_debugfs_root_dir = NULL;
	tel_debugfs_put(tel_debugfs_root_dir);
	tel_debugfs_root_dir = NULL;
}

int tel_shm_init(enum shm_grp_type grp_type,
	const void *data)
{
	int ret;
	struct shm_rbctl *rbctl;
	struct shm_rbctl *rbctl2;
	struct shm_rbctl *rbctl_end = shm_rbctl + shm_rb_total_cnt;

	ret = shm_param_init(grp_type, data);
	if (ret < 0)
		return -1;

	for (rbctl = shm_rbctl; rbctl != rbctl_end; ++rbctl) {
		if (rbctl->grp_type == grp_type)
			ret = shm_rb_init(rbctl);
		if (ret < 0)
			goto rb_exit;
	}

	if (grp_type == shm_grp_cp) {
		set_version_numb();
		get_dvc_info();
#if 0
		read_mv_profile();
		read_dvc_table();
		read_dfc_table();
#endif
	}

	return 0;

rb_exit:
	for (rbctl2 = shm_rbctl; rbctl2 != rbctl; ++rbctl2)
		if (rbctl->grp_type == grp_type)
			shm_rb_exit(rbctl2);

	return -1;
}
EXPORT_SYMBOL(tel_shm_init);

void tel_shm_exit(enum shm_grp_type grp_type)
{
	struct shm_rbctl *rbctl;
	struct shm_rbctl *rbctl_end = shm_rbctl + shm_rb_total_cnt;

	for (rbctl = shm_rbctl; rbctl != rbctl_end; ++rbctl)
		if (rbctl->grp_type == grp_type)
			shm_rb_exit(rbctl);
}
EXPORT_SYMBOL(tel_shm_exit);

int shm_free_rx_skbuf_safe(struct shm_rbctl *rbctl)
{
	int ret;

	mutex_lock(&rbctl->va_lock);
	if (rbctl->skctl_va)
		ret = shm_free_rx_skbuf(rbctl);
	else
		ret = -1;
	mutex_unlock(&rbctl->va_lock);
	return ret;
}
EXPORT_SYMBOL(shm_free_rx_skbuf_safe);

int shm_free_tx_skbuf_safe(struct shm_rbctl *rbctl)
{
	int ret;

	mutex_lock(&rbctl->va_lock);
	if (rbctl->skctl_va)
		ret = shm_free_tx_skbuf(rbctl);
	else
		ret = -1;
	mutex_unlock(&rbctl->va_lock);
	return ret;
}
EXPORT_SYMBOL(shm_free_tx_skbuf_safe);

struct shm_rbctl *shm_open(enum shm_rb_type rb_type,
			   struct shm_callback *cb, void *priv)
{
	if (rb_type >= shm_rb_total_cnt || rb_type < 0) {
		pr_err("%s: incorrect type %d\n", __func__, rb_type);
		return NULL;
	}

	if (!cb) {
		pr_err("%s: callback is NULL\n", __func__);
		return NULL;
	}

	shm_rbctl[rb_type].cbs = cb;
	shm_rbctl[rb_type].priv = priv;

	return &shm_rbctl[rb_type];
}

void shm_close(struct shm_rbctl *rbctl)
{
	if (!rbctl)
		return;

	rbctl->cbs = NULL;
	rbctl->priv = NULL;
}

/* write packet to share memory socket buffer */
void shm_xmit(struct shm_rbctl *rbctl, struct sk_buff *skb)
{
	struct shm_skctl *skctl = rbctl->skctl_va;

	/*
	 * we always write to the next slot !?
	 * thinking the situation of the first slot in the first accessing
	 */
	int slot = shm_get_next_tx_slot(rbctl, skctl->ap_wptr);
	void *data = SHM_PACKET_PTR(rbctl->tx_va, slot, rbctl->tx_skbuf_size);

	if (!skb) {
		pr_err("shm_xmit skb is null..\n");
		return;
	}
	memcpy(data, skb->data, skb->len);
	shm_flush_dcache(rbctl, data, skb->len);
	skctl->ap_wptr = slot;	/* advance pointer index */
}

/* read packet from share memory socket buffer */
struct sk_buff *shm_recv(struct shm_rbctl *rbctl)
{
	struct shm_skctl *skctl = rbctl->skctl_va;
	enum shm_rb_type rb_type = rbctl - shm_rbctl;

	/* yes, we always read from the next slot either */
	int slot = shm_get_next_rx_slot(rbctl, skctl->ap_rptr);

	/* get the total packet size first for memory allocate */
	unsigned char *hdr = SHM_PACKET_PTR(rbctl->rx_va, slot,
					    rbctl->rx_skbuf_size);
	int count = 0;
	struct sk_buff *skb = NULL;

	shm_invalidate_dcache(rbctl, hdr, rbctl->rx_skbuf_size);

	switch (rb_type) {
	case shm_rb_m3:
	case shm_rb_main: {
		struct shm_skhdr *skhdr = (struct shm_skhdr *)hdr;
		count = skhdr->length + sizeof(*skhdr);

		if (skhdr->length <= 0) {
			pr_emerg(
				"MSOCK: shm_recv: slot = %d, skhdr->length = %d\n",
				slot, skhdr->length);
			goto error_length;
		}
		break;
	}
	case shm_rb_psd: {
		struct shm_psd_skhdr *skhdr = (struct shm_psd_skhdr *)hdr;
		count = skhdr->length + sizeof(*skhdr);
		pr_warn(
			"MSOCK: shm_recv: calling in psd ring buffer!!!\n");
		break;
	}

	case shm_rb_diag: {
		struct direct_rb_skhdr *skhdr = (struct direct_rb_skhdr *)hdr;
		count = skhdr->length + sizeof(*skhdr);
		break;
	}

	default:
		pr_warn(
			"MSOCK: shm_recv: incorrect rb_type %d!!!\n", rb_type);
		return NULL;
	}

	if (count > rbctl->rx_skbuf_size) {
		pr_emerg(
		       "MSOCK: shm_recv: slot = %d, count = %d\n", slot, count);
		goto error_length;
	}

	skb = alloc_skb(count, GFP_ATOMIC);
	if (!skb)
		return NULL;

	/* write all the packet data including header to sk_buff */
	memcpy(skb_put(skb, count), hdr, count);

error_length:
	/* advance reader pointer */
	skctl->ap_rptr = slot;

	return skb;
}
