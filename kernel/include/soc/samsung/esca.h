#ifndef __ESCA_H__
#define __ESCA_H__

#include <linux/types.h>
#include <linux/of.h>
#include <linux/ktime.h>

/******************************************************************************
  IPC
  *****************************************************************************/
typedef void (*ipc_callback)(unsigned int *cmd, unsigned int size);

struct ipc_config {
	unsigned int *cmd;
	unsigned int *indirection_base;
	unsigned int indirection_size;
	bool response;
	bool indirection;
};

#if IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
extern unsigned int esca_ipc_request_channel(struct device_node *np,
		ipc_callback handler, unsigned int *id,	unsigned int *size);
extern unsigned int esca_ipc_release_channel(struct device_node *np,
						unsigned int channel_id);
extern int esca_ipc_send_data(unsigned int channel_id, struct ipc_config *cfg);
extern bool is_esca_ipc_busy(unsigned ch_id);
#else
static inline unsigned int esca_ipc_request_channel(struct device_node *np,
		ipc_callback handler, unsigned int *id,	unsigned int *size)
{
	return -1;
}
static inline unsigned int esca_ipc_release_channel(struct device_node *np,
							unsigned int channel_id)
{
	return -1;
}
static inline int esca_ipc_send_data(unsigned int channel_id,
					struct ipc_config *cfg)
{
	return -1;
}
bool is_esca_ipc_busy(unsigned ch_id)
{
	return true;
}
#endif

/******************************************************************************
  DRV
  *****************************************************************************/
#if IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
extern void exynos_esca_force_apm_wdt_reset(void);
#else
static inline void exynos_esca_force_apm_wdt_reset(void)
{
	return;
}
#endif

/******************************************************************************
  Debugging
  *****************************************************************************/
#if IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
extern void esca_stop_log(void);
extern ktime_t esca_time_calc(u32 start, u32 end);
extern u32 esca_get_gptimer(void);
extern u32 get_esca_phy_num(void);
extern void __iomem *get_esca_dump_addr(u32 layer);
extern unsigned int get_esca_dump_size(u32 layer);
#else
static inline void esca_stop_log(void)
{
	return;
}

static inline ktime_t esca_time_calc(u32 start, u32 end)
{
	return 0;
}

static inline u32 esca_get_gptimer(void)
{
	return 0;
}
static inline u32 get_esca_phy_num(void)
{
	return 0;
}
static inline void __iomem *get_esca_dump_addr(u32 layer)
{
	return 0;
}
static inline unsigned int get_esca_dump_size(u32 layer)
{
	return 0;
}
#endif

/******************************************************************************
  Plugins
  *****************************************************************************/
struct nfc_clk_req_log {
	unsigned int is_on;
	unsigned int timestamp;
};

#if IS_ENABLED(CONFIG_EXYNOS_ESCAV2) || IS_ENABLED(CONFIG_EXYNOS_FLEXPMU_DBG)
extern u32 esca_get_apsoc_dsupd_count(void);
extern u32 esca_get_apsoc_dsupd_early_wakeup_count(void);
extern u32 esca_get_apsoc_sicd_early_wakeup_count(void);
extern u32 esca_get_apsoc_sleep_early_wakeup_count(void);
extern u32 esca_get_apsoc_count(void);
extern u32 esca_get_apsicd_count(void);
extern u32 esca_get_apsleep_count(void);
extern u32 esca_get_mifdn_count(void);
extern u32 esca_get_mifdn_sicd_count(void);
extern u32 esca_get_mifdn_sleep_count(void);
extern u32 esca_get_mif_request(void);
extern u32 esca_noti_dsu_cpd(bool is_dsu_cpd);
extern u32 esca_get_dsu_cpd(void);
extern int esca_get_nfc_log_buf(struct nfc_clk_req_log **buf,
				u32 *last_ptr, u32 *len);
extern void exynos_flexpmu_dbg_mif_req(void);
extern void esca_print_mif_request(void);
#else
static inline u32 esca_get_apsoc_dsupd_count(void) { return 0; }
static inline u32 esca_get_apsoc_dsupd_early_wakeup_count(void) { return 0; }
static inline u32 esca_get_apsoc_sicd_early_wakeup_count(void) { return 0; }
static inline u32 esca_get_apsoc_sleep_early_wakeup_count(void) { return 0; }
static inline u32 esca_get_apsoc_count(void) { return 0; }
static inline u32 esca_get_apsicd_count(void) { return 0; }
static inline u32 esca_get_apsleep_count(void) { return 0; }
static inline u32 esca_get_mifdn_count(void) { return 0; }
static inline u32 esca_get_mifdn_sicd_count(void) { return 0; }
static inline u32 esca_get_mifdn_sleep_count(void) { return 0; }
static inline u32 esca_get_mif_request(void) { return 0; }
static inline u32 esca_noti_dsu_cpd(bool is_dsu_cpd){ return 0; }
static inline u32 esca_get_dsu_cpd(void){ return 0; }
static inline int esca_get_nfc_log_buf(struct nfc_clk_req_log **buf,
					u32 *last_ptr, u32 *len)
{
	return 0;
}
#endif

/******************************************************************************
  MFD 
  *****************************************************************************/

#if IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
extern int exynos_esca_read_reg(struct device_node *esca_mfd_node, u8 channel,
				u16 type, u8 reg, u8 *dest);
extern int exynos_esca_bulk_read(struct device_node *esca_mfd_node, u8 channel,
				u16 type, u8 reg, int count, u8 *buf);
extern int exynos_esca_write_reg(struct device_node *esca_mfd_node, u8 channel,
				u16 type, u8 reg, u8 value);
extern int exynos_esca_bulk_write(struct device_node *esca_mfd_node, u8 channel,
				u16 type, u8 reg, int count, u8 *buf);
extern int exynos_esca_update_reg(struct device_node *esca_mfd_node, u8 channel,
				u16 type, u8 reg, u8 value, u8 mask);
#else
static inline int exynos_esca_read_reg(struct device_node *esca_mfd_node,
					u8 channel, u16 type, u8 reg, u8 *dest)
{
	return -1;
}
static inline int exynos_esca_bulk_read(struct device_node *esca_mfd_node,
			u8 channel, u16 type, u8 reg, int count, u8 *buf)
{
	return -1;
}
static inline int exynos_esca_write_reg(struct device_node *esca_mfd_node,
					u8 channel, u16 type, u8 reg, u8 value)
{
	return -1;
}
static inline int exynos_esca_bulk_write(struct device_node *esca_mfd_node,
			u8 channel, u16 type, u8 reg, int count, u8 *buf)
{
	return -1;
}
static inline int exynos_esca_update_reg(struct device_node *esca_mfd_node,
				u8 channel, u16 type, u8 reg, u8 value, u8 mask)
{
	return -1;
}
#endif

#endif			/* __ESCA_H__ */
