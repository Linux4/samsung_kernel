// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/vmalloc.h>
#include "mbraink_gpu.h"

#if IS_ENABLED(CONFIG_MTK_FPSGO_V3) || IS_ENABLED(CONFIG_MTK_FPSGO)
#include <fpsgo_common.h>
#include <fstb.h>
#endif

#include <ged_dvfs.h>
#include <gpufreq_v2.h>

#define Q2QTIMEOUT 500000000 //500ms
#define Q2QTIMEOUT_HIST 70000000 //70ms
#define PERFINDEX_TIMEOUT 8000000000 //8sec
#define PERFINDEX_LIMIT 100
#define PERFINDEX_BUF 30
#define PERFINDEX_SLOT 20

struct mbraink_gpu_perfidx_info {
	struct hlist_node hlist;

	int pid;
	unsigned long long bufid;
	int perf_idx[PERFINDEX_BUF];
	unsigned long long ts[PERFINDEX_BUF];
	int currentIdx;
	int sbe_ctrl[PERFINDEX_BUF];
	unsigned long long err_ts;
	unsigned long long err_counter;
};

static unsigned long long gq2qTimeoutInNs = Q2QTIMEOUT;
unsigned int TimeoutCounter[10] = {0};
unsigned int TimeoutRange[10] = {70, 120, 170, 220, 270, 320, 370, 420, 470, 520};

static unsigned long long gperfIdxTimeoutInNs = PERFINDEX_TIMEOUT;
static int gperfIdxLimit = PERFINDEX_LIMIT;

static HLIST_HEAD(mbk_g_perfidx_list);
static DEFINE_MUTEX(mbk_g_perfidx_lock);


static void calculateTimeoutCouter(unsigned long long q2qTimeInNS)
{
	if (q2qTimeInNS < 120000000) //70~120ms
		TimeoutCounter[0]++;
	else if (q2qTimeInNS < 170000000) //120~170ms
		TimeoutCounter[1]++;
	else if (q2qTimeInNS < 220000000) //170~220ms
		TimeoutCounter[2]++;
	else if (q2qTimeInNS < 270000000) //220~270ms
		TimeoutCounter[3]++;
	else if (q2qTimeInNS < 320000000) //270~320ms
		TimeoutCounter[4]++;
	else if (q2qTimeInNS < 370000000) //320~370ms
		TimeoutCounter[5]++;
	else if (q2qTimeInNS < 420000000) //370~420ms
		TimeoutCounter[6]++;
	else if (q2qTimeInNS < 470000000) //420~470ms
		TimeoutCounter[7]++;
	else if (q2qTimeInNS < 520000000) //470~520ms
		TimeoutCounter[8]++;
	else //>520ms
		TimeoutCounter[9]++;
}

int delete_perfidx_info(int pid, unsigned long long bufID)
{
	int ret = 0;
	struct mbraink_gpu_perfidx_info *iter = NULL;
	struct hlist_node *h = NULL;

	mutex_lock(&mbk_g_perfidx_lock);

	hlist_for_each_entry_safe(iter, h, &mbk_g_perfidx_list, hlist) {
		if (iter->pid == pid && iter->bufid == bufID) {
			hlist_del(&iter->hlist);
			vfree(iter);
			ret = 1;
			break;
		}
	}

	mutex_unlock(&mbk_g_perfidx_lock);

	return ret;
}

int recycle_perfidx_list(void)
{
	int ret = 0;
	struct mbraink_gpu_perfidx_info *iter;
	struct hlist_node *h;

	mutex_lock(&mbk_g_perfidx_lock);

	if (hlist_empty(&mbk_g_perfidx_list)) {
		ret = 1;
		goto out;
	}

	hlist_for_each_entry_safe(iter, h, &mbk_g_perfidx_list, hlist) {
		hlist_del(&iter->hlist);
		vfree(iter);
		ret = 1;
	}

out:
	mutex_unlock(&mbk_g_perfidx_lock);
	return ret;
}


ssize_t getTimeoutCouterReport(char *pBuf)
{
	ssize_t size = 0;

	if (pBuf == NULL)
		return size;

	size += scnprintf(pBuf+size, 1024-size, "%d~%d:%d\n%d~%d:%d\n",
		TimeoutRange[0], TimeoutRange[1], TimeoutCounter[0],
		TimeoutRange[1], TimeoutRange[2], TimeoutCounter[1]);
	size += scnprintf(pBuf+size, 1024-size, "%d~%d:%d\n%d~%d:%d\n",
		TimeoutRange[2], TimeoutRange[3], TimeoutCounter[2],
		TimeoutRange[3], TimeoutRange[4], TimeoutCounter[3]);
	size += scnprintf(pBuf+size, 1024-size, "%d~%d:%d\n%d~%d:%d\n",
		TimeoutRange[4], TimeoutRange[5], TimeoutCounter[4],
		TimeoutRange[5], TimeoutRange[6], TimeoutCounter[5]);
	size += scnprintf(pBuf+size, 1024-size, "%d~%d:%d\n%d~%d:%d\n",
		TimeoutRange[6], TimeoutRange[7], TimeoutCounter[6],
		TimeoutRange[7], TimeoutRange[8], TimeoutCounter[7]);
	size += scnprintf(pBuf+size, 1024-size, "%d~%d:%d\n>%d:%d\n",
		TimeoutRange[8], TimeoutRange[9], TimeoutCounter[8],
		TimeoutRange[9], TimeoutCounter[9]);

	return size;
}

void fpsgo2mbrain_hint_frameinfo(int pid, unsigned long long bufID,
	int fps, unsigned long long time)
{
	if (time > Q2QTIMEOUT_HIST)
		calculateTimeoutCouter(time);

	if (time > gq2qTimeoutInNs) {
		pr_info("q2q (%d) (%llu) (%llu) ns limit (%llu) ns\n",
			pid,
			bufID,
			time,
			gq2qTimeoutInNs);
		mbraink_netlink_send_msg(NETLINK_EVENT_Q2QTIMEOUT);
	}
}

static void sendPerfTimeoutEvent(struct mbraink_gpu_perfidx_info *iter)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;
	int i = 0;

	if (iter == NULL)
		return;

	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s:%d:%llu:%llu:%llu",
			NETLINK_EVENT_PERFTIMEOUT,
			iter->pid,
			iter->bufid,
			iter->err_counter,
			iter->err_ts);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;

	pos += n;

	for (i = 0; i < PERFINDEX_BUF; i++) {
		if (pos+PERFINDEX_SLOT > NETLINK_EVENT_MESSAGE_SIZE) {
			pr_info("WG: over buffer size (%d) pos (%d), PERFINDEX_SLOT(%d)\n",
				NETLINK_EVENT_MESSAGE_SIZE,
				pos,
				PERFINDEX_SLOT);
			mbraink_netlink_send_msg(netlink_buf);

			//reset nl buffer.
			memset(netlink_buf, 0x00, sizeof(netlink_buf));
			pos = 0;
			n = 0;

			//create new buffer.
			n = snprintf(netlink_buf + pos,
					NETLINK_EVENT_MESSAGE_SIZE - pos,
					"%s:%d:%llu:%llu:%llu",
					NETLINK_EVENT_PERFTIMEOUT,
					iter->pid,
					iter->bufid,
					iter->err_counter,
					iter->err_ts);

			if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
				return;

			pos += n;
		}

		n = snprintf(netlink_buf + pos,
				NETLINK_EVENT_MESSAGE_SIZE - pos,
				":%d:%d:%llu",
				iter->perf_idx[i],
				iter->sbe_ctrl[i],
				iter->ts[i]);
		if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
			return;

		pos += n;
	}

	mbraink_netlink_send_msg(netlink_buf);

}

void fpsgo2mbrain_hint_perfinfo(int pid, unsigned long long bufID,
	int perf_idx, int sbe_ctrl, unsigned long long ts)
{
	struct mbraink_gpu_perfidx_info *iter = NULL;
	int currentIdx = 0;

	mutex_lock(&mbk_g_perfidx_lock);

	hlist_for_each_entry(iter, &mbk_g_perfidx_list, hlist) {
		if (iter->pid == pid && (iter->bufid == bufID ||
				iter->bufid == 0))
			break;
	}

	//add a new one.
	if (iter == NULL) {
		struct mbraink_gpu_perfidx_info *new_perfidx_info = NULL;

		new_perfidx_info = vmalloc(sizeof(struct mbraink_gpu_perfidx_info));
		if (new_perfidx_info == NULL)
			goto out;

		memset(new_perfidx_info, 0x00, sizeof(struct mbraink_gpu_perfidx_info));
		new_perfidx_info->pid = pid;
		new_perfidx_info->bufid = bufID;
		new_perfidx_info->sbe_ctrl[0] = 0;
		new_perfidx_info->err_ts = 0;
		new_perfidx_info->err_counter = 0;
		new_perfidx_info->currentIdx = 0;
		new_perfidx_info->perf_idx[0] = perf_idx;
		new_perfidx_info->ts[0] = ts;

		iter = new_perfidx_info;
		hlist_add_head(&iter->hlist, &mbk_g_perfidx_list);
	}

	if (iter->bufid == 0)
		iter->bufid = bufID;

	//update info.
	currentIdx = (iter->currentIdx > PERFINDEX_BUF-1) ? 0 : iter->currentIdx;
	iter->perf_idx[currentIdx] = perf_idx;
	iter->ts[currentIdx] = ts;
	iter->sbe_ctrl[currentIdx] = sbe_ctrl;
	iter->currentIdx = (currentIdx > PERFINDEX_BUF-2) ? 0 : currentIdx+1;

	//process logic here.
	if (perf_idx >= gperfIdxLimit) {
		if (iter->err_counter != 0) {
			iter->err_counter++;
			if ((ts - iter->err_ts) > gperfIdxTimeoutInNs) {
				sendPerfTimeoutEvent(iter);
				//reset
				iter->err_counter = 0;
				iter->err_ts = 0;
			}
		} else {
			iter->err_ts = ts;
			iter->err_counter++;
		}
	} else {
		iter->err_counter = 0;
		iter->err_ts = 0;
	}

out:
	mutex_unlock(&mbk_g_perfidx_lock);

}

void fpsgo2mbrain_hint_deleteperfinfo(int pid, unsigned long long bufID,
	int perf_idx, int sbe_ctrl, unsigned long long ts)
{
	struct mbraink_gpu_perfidx_info *iter = NULL;

	mutex_lock(&mbk_g_perfidx_lock);
	hlist_for_each_entry(iter, &mbk_g_perfidx_list, hlist) {
		if ((iter->pid == pid) && (iter->bufid == bufID)) {
			hlist_del(&iter->hlist);
			vfree(iter);
			break;
		}
	}
	mutex_unlock(&mbk_g_perfidx_lock);

}

int mbraink_gpu_init(void)
{
#if IS_ENABLED(CONFIG_MTK_FPSGO_V3) || IS_ENABLED(CONFIG_MTK_FPSGO)
	fpsgo_other2fstb_register_info_callback(FPSGO_Q2Q_TIME,
		fpsgo2mbrain_hint_frameinfo);

	fpsgo_other2fstb_register_perf_callback(FPSGO_PERF_IDX,
		fpsgo2mbrain_hint_perfinfo);

	fpsgo_other2fstb_register_perf_callback(FPSGO_DELETE,
		fpsgo2mbrain_hint_deleteperfinfo);
#endif
	return 0;
}

int mbraink_gpu_deinit(void)
{
#if IS_ENABLED(CONFIG_MTK_FPSGO_V3) || IS_ENABLED(CONFIG_MTK_FPSGO)
	fpsgo_other2fstb_unregister_info_callback(FPSGO_Q2Q_TIME,
		fpsgo2mbrain_hint_frameinfo);

	fpsgo_other2fstb_unregister_perf_callback(FPSGO_PERF_IDX,
		fpsgo2mbrain_hint_perfinfo);

	fpsgo_other2fstb_unregister_perf_callback(FPSGO_DELETE,
		fpsgo2mbrain_hint_deleteperfinfo);
#endif
	return 0;
}

void mbraink_gpu_setQ2QTimeoutInNS(unsigned long long q2qTimeoutInNS)
{
	gq2qTimeoutInNs = q2qTimeoutInNS;
}

unsigned long long mbraink_gpu_getQ2QTimeoutInNS(void)
{
	return gq2qTimeoutInNs;
}

int mbraink_gpu_getOppInfo(struct mbraink_gpu_opp_info *gOppInfo)
{
	int i = 0;
	int ret = 0;
	unsigned int u32Count = 0;
	unsigned int u32Level = 0;
	struct GED_DVFS_OPP_STAT *report = NULL;
	u64 u64ts = 0;

	if (gOppInfo == NULL) {
		pr_info("Null gOppInfo\n");
		return -1;
	}

	u32Count = ged_dvfs_get_real_oppfreq_num();

	if (u32Count)
		report = vmalloc(sizeof(struct GED_DVFS_OPP_STAT) * u32Count);

	if ((report != NULL) &&
		ged_dvfs_query_opp_cost(report, u32Count, false, &u64ts) == 0) {
		gOppInfo->data1 = u64ts;
		for (i = 0; i < u32Count; i++) {
			u32Level = gpufreq_get_freq_by_idx(TARGET_DEFAULT, i)/1000;
			if (i < MAX_GPU_OPP_INFO_SZ) {
				gOppInfo->raw[i].data1 = u32Level;
				gOppInfo->raw[i].data2 = report[i].ui64Active;
				gOppInfo->raw[i].data3 = report[i].ui64Idle;
			}
		}
	} else {
		pr_info("can't allocate ged dvfs opp stat memory\n");
		ret = -1;
	}

	if (report != NULL)
		vfree(report);

	return ret;
}

int mbraink_gpu_getStateInfo(struct mbraink_gpu_state_info *gStateInfo)
{
	int ret = 0;

	if (gStateInfo != NULL) {
		ret = ged_dvfs_query_power_state_time(&gStateInfo->data1,
							&gStateInfo->data2,
							&gStateInfo->data3,
							&gStateInfo->data4);
	} else {
		pr_info("gStateInfo is Null\n");
		ret = -1;
	}
	return ret;
}

int mbraink_gpu_getLoadingInfo(struct mbraink_gpu_loading_info *gLoadingInfo)
{
	int ret = 0;

	if (gLoadingInfo != NULL) {
		ret = ged_dvfs_query_loading(&gLoadingInfo->data1,
						&gLoadingInfo->data2);
	} else {
		pr_info("gLoadingInfo is Null\n");
		ret = -1;
	}
	return ret;
}

void mbraink_gpu_setPerfIdxTimeoutInNS(unsigned long long perfIdxTimeoutInNS)
{
	gperfIdxTimeoutInNs = perfIdxTimeoutInNS;
}

void mbraink_gpu_setPerfIdxLimit(int perfIdxLimit)
{
	if (perfIdxLimit <= 100)
		gperfIdxLimit = perfIdxLimit;
}

void mbraink_gpu_dumpPerfIdxList(void)
{
	struct mbraink_gpu_perfidx_info *iter;
	int n = 0;

	mutex_lock(&mbk_g_perfidx_lock);
	hlist_for_each_entry(iter, &mbk_g_perfidx_list, hlist) {
		pr_info("perf info pid(%d) bid(%llu) cIdx (%d) last %d record\n",
			iter->pid,
			iter->bufid,
			iter->currentIdx,
			PERFINDEX_BUF);

		for (n = 0; n < PERFINDEX_BUF; n++) {
			pr_info("perf info (%d):  perf(%d) sbe(%d) ts(%llu)\n",
				n,
				iter->perf_idx[n],
				iter->sbe_ctrl[n],
				iter->ts[n]);
		}
	}
	mutex_unlock(&mbk_g_perfidx_lock);

}

