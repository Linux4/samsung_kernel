/*****************************************************************************
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <scsc/scsc_mx.h>
#include <scsc/scsc_logring.h>

#include "mxfwconfig.h"
#include "miframman.h"
#include "scsc_mx_impl.h"
#include "mxconf.h"

#ifdef CONFIG_SCSC_LOG_COLLECTION
#include <scsc/scsc_log_collector.h>
#endif

#define MXFWCONFIG_CFG_SUBDIR	"common"
#define MXFWCONFIG_CFG_FILE_HW	"common.hcf"
#define MXFWCONFIG_CFG_FILE_SW	"common_sw.hcf"

static void mxfwconfig_get_dram_ref(struct scsc_mx *mx, struct mxmibref *cfg_ref);

/* Load config into non-shared DRAM */
static int mxfwconfig_load_cfg(struct scsc_mx *mx, struct mxfwconfig *cfg, const char *filename)
{
	int r = 0;
	u32 i;

	if (cfg->configs >= SCSC_MX_MAX_COMMON_CFG) {
		SCSC_TAG_ERR(MX_CFG, "Too many common config files (%u)\n", cfg->configs);
		return -E2BIG;
	}

	i = cfg->configs++; /* Claim next config slot */

	/* Load config file from file system into DRAM */
	r = mx140_file_request_conf(mx, &cfg->config[i].fw, MXFWCONFIG_CFG_SUBDIR, filename);
	if (r)
		return r;

	/* Initial size of file */
	cfg->config[i].cfg_len = cfg->config[i].fw->size;
	cfg->config[i].cfg_data = cfg->config[i].fw->data;

	/* Validate file in DRAM */
	if (cfg->config[i].cfg_len >= MX_COMMON_HCF_HDR_SIZE && /* Room for header */
		/*(cfg->config[i].cfg[6] & 0xF0) == 0x10 && */	/* Curator subsystem */
		cfg->config[i].cfg_data[7] == 1) {		/* First file format */
		int j;

		cfg->config[i].cfg_hash = 0;

		/* Calculate hash */
		for (j = 0; j < MX_COMMON_HASH_SIZE_BYTES; j++) {
			cfg->config[i].cfg_hash =
				(cfg->config[i].cfg_hash << 8) | cfg->config[i].cfg_data[j + MX_COMMON_HASH_OFFSET];
		}

		SCSC_TAG_INFO(MX_CFG, "CFG hash: 0x%.04x\n", cfg->config[i].cfg_hash);

		/* All good - consume header and continue */
		cfg->config[i].cfg_len -= MX_COMMON_HCF_HDR_SIZE;
		cfg->config[i].cfg_data += MX_COMMON_HCF_HDR_SIZE;
	} else {
		SCSC_TAG_ERR(MX_CFG, "Invalid HCF header size %zu\n", cfg->config[i].cfg_len);

		/* Caller must call mxfwconfig_unload_cfg() to release the buffer */
		return -EINVAL;
	}

	/* Running shtotal payload */
	cfg->shtotal += cfg->config[i].cfg_len;

	SCSC_TAG_INFO(MX_CFG, "Loaded common config %s, size %zu, payload size %zu, shared dram total %zu\n",
		filename, cfg->config[i].fw->size, cfg->config[i].cfg_len, cfg->shtotal);

	return r;
}

/* Unload config from non-shared DRAM */
static int mxfwconfig_unload_cfg(struct scsc_mx *mx, struct mxfwconfig *cfg, u32 index)
{
	if (index >= SCSC_MX_MAX_COMMON_CFG) {
		SCSC_TAG_ERR(MX_CFG, "Out of range index (%u)\n", index);
		return -E2BIG;
	}

	if (cfg->config[index].fw) {
		SCSC_TAG_DBG3(MX_CFG, "Unload common config %u\n", index);

		mx140_file_release_conf(mx, cfg->config[index].fw);

		cfg->config[index].fw = NULL;
		cfg->config[index].cfg_data = NULL;
		cfg->config[index].cfg_len = 0;
	}

	return 0;
}

/*
 * Load Common config files
 */
int mxfwconfig_load(struct scsc_mx *mx, struct mxmibref *cfg_ref)
{
	struct mxfwconfig *cfg = scsc_mx_get_mxfwconfig(mx);
	struct miframman *miframman = scsc_mx_get_ramman(mx);
	int r;
	u32 i;
	u8 *dest;

	/* HW file is optional */
	r = mxfwconfig_load_cfg(mx, cfg, MXFWCONFIG_CFG_FILE_HW);
	if (r)
		goto done;

	/* SW file is optional, but not without HW file */
	r = mxfwconfig_load_cfg(mx, cfg, MXFWCONFIG_CFG_FILE_SW);
	if (r == -EINVAL) {
		/* If SW file is corrupt, abandon both HW and SW */
		goto done;
	}

	/* Allocate shared DRAM */
	cfg->shdram = miframman_alloc(miframman, cfg->shtotal, 4, MIFRAMMAN_OWNER_COMMON);
	if (!cfg->shdram) {
		SCSC_TAG_ERR(MX_CFG, "MIF alloc failed for %zu octets\n", cfg->shtotal);
		r = -ENOMEM;
		goto done;
	}

	/* Copy files into shared DRAM */
	for (i = 0, dest = (u8 *)cfg->shdram;
	     i < cfg->configs;
	     i++) {
		/* Add to shared DRAM block */
		memcpy(dest, cfg->config[i].cfg_data, cfg->config[i].cfg_len);
		dest += cfg->config[i].cfg_len;
	}

done:
	/* Release the files from non-shared DRAM */
	for (i = 0; i < cfg->configs; i++)
		mxfwconfig_unload_cfg(mx, cfg, i);

	/* Configs abandoned on error */
	if (r)
		cfg->configs = 0;

	/* Pass offset of common HCF data.
	 * FW must ignore if zero length, so set up even if we loaded nothing.
	 */
	mxfwconfig_get_dram_ref(mx, cfg_ref);

	return r;
}

/*
 * Unload Common config data
 */
void mxfwconfig_unload(struct scsc_mx *mx)
{
	struct mxfwconfig *cfg = scsc_mx_get_mxfwconfig(mx);
	struct miframman *miframman = scsc_mx_get_ramman(mx);

	/* Free config block in shared DRAM */
	if (cfg->shdram) {
		SCSC_TAG_INFO(MX_CFG, "Free common config %zu bytes shared DRAM\n", cfg->shtotal);

		miframman_free(miframman, cfg->shdram);

		cfg->configs = 0;
		cfg->shtotal = 0;
		cfg->shdram = NULL;
	}
}

/*
 * Get ref (offset) of config block in shared DRAM
 */
static void mxfwconfig_get_dram_ref(struct scsc_mx *mx, struct mxmibref *cfg_ref)
{
	struct mxfwconfig *mxfwconfig = scsc_mx_get_mxfwconfig(mx);
	struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(mx);

	if (!mxfwconfig->shdram) {
		cfg_ref->offset = (scsc_mifram_ref)0;
		cfg_ref->size = 0;
	} else {
		mif->get_mifram_ref(mif, mxfwconfig->shdram, &cfg_ref->offset);
		cfg_ref->size = (uint32_t)mxfwconfig->shtotal;
	}

	SCSC_TAG_INFO(MX_CFG, "cfg_ref: 0x%x, size %u\n", cfg_ref->offset, cfg_ref->size);
}

/*
 * Init config file module
 */
int mxfwconfig_init(struct scsc_mx *mx)
{
	struct mxfwconfig *cfg = scsc_mx_get_mxfwconfig(mx);

	cfg->configs = 0;
	cfg->shtotal = 0;
	cfg->shdram = NULL;

	return 0;
}

/*
 * Exit config file module
 */
void mxfwconfig_deinit(struct scsc_mx *mx)
{
	struct mxfwconfig *cfg = scsc_mx_get_mxfwconfig(mx);

	/* Leaked memory? */
	WARN_ON(cfg->configs > 0);
	WARN_ON(cfg->shdram);
}

