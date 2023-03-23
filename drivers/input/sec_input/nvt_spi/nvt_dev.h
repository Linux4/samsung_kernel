#ifndef _LINUX_NVT_TS_H_
#define _LINUX_NVT_TS_H_

#include <asm/unaligned.h>
#include <linux/completion.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/pm_wakeup.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include "nvt_reg.h"

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif

#if IS_ENABLED(CONFIG_DISPLAY_SAMSUNG)
#include <linux/sec_panel_notifier.h>
#elif defined(EXYNOS_DISPLAY_INPUT_NOTIFIER)
#include <linux/panel_notify.h>
#endif

#include "../sec_tclm_v2.h"
#if IS_ENABLED(CONFIG_INPUT_TOUCHSCREEN_TCLMV2)
#define TCLM_CONCEPT
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
#include "../sec_tsp_dumpkey.h"
extern struct tsp_dump_callbacks dump_callbacks;
#endif

#include "../sec_input.h"
#include "../sec_tsp_log.h"
#include "../sec_cmd.h"
#ifndef I2C_M_DMA_SAFE
#define I2C_M_DMA_SAFE		0
#endif


#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
#include <linux/input/stui_inf.h>
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
#include "../sec_secure_touch.h"
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>

#define SECURE_TOUCH_ENABLE	1
#define SECURE_TOUCH_DISABLE	0

#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/sort.h>

#if IS_ENABLED(CONFIG_GH_RM_DRV)
#include <linux/gunyah/gh_msgq.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/gunyah/gh_irq_lend.h>
#include <linux/gunyah/gh_mem_notifier.h>

#define TRUSTED_TOUCH_MEM_LABEL 0x7
enum trusted_touch_mode_config {
	TRUSTED_TOUCH_VM_MODE,
	TRUSTED_TOUCH_MODE_NONE
};

enum trusted_touch_pvm_states {
	TRUSTED_TOUCH_PVM_INIT,
	PVM_I2C_RESOURCE_ACQUIRED,
	PVM_INTERRUPT_DISABLED,
	PVM_IOMEM_LENT,
	PVM_IOMEM_LENT_NOTIFIED,
	PVM_IRQ_LENT,
	PVM_IRQ_LENT_NOTIFIED,
	PVM_IOMEM_RELEASE_NOTIFIED,
	PVM_IRQ_RELEASE_NOTIFIED,
	PVM_ALL_RESOURCES_RELEASE_NOTIFIED,
	PVM_IRQ_RECLAIMED,
	PVM_IOMEM_RECLAIMED,
	PVM_INTERRUPT_ENABLED,
	PVM_I2C_RESOURCE_RELEASED,
	TRUSTED_TOUCH_PVM_STATE_MAX
};

enum trusted_touch_tvm_states {
	TRUSTED_TOUCH_TVM_INIT,
	TVM_IOMEM_LENT_NOTIFIED,
	TVM_IRQ_LENT_NOTIFIED,
	TVM_ALL_RESOURCES_LENT_NOTIFIED,
	TVM_IOMEM_ACCEPTED,
	TVM_I2C_SESSION_ACQUIRED,
	TVM_IRQ_ACCEPTED,
	TVM_INTERRUPT_ENABLED,
	TVM_INTERRUPT_DISABLED,
	TVM_IRQ_RELEASED,
	TVM_I2C_SESSION_RELEASED,
	TVM_IOMEM_RELEASED,
	TRUSTED_TOUCH_TVM_STATE_MAX
};

#define TOUCH_INTR_GPIO_BASE 0xF12E000
#define TOUCH_INTR_GPIO_SIZE 0x1000
#define TOUCH_INTR_GPIO_OFFSET 0x8

#define TRUSTED_TOUCH_EVENT_LEND_FAILURE -1
#define TRUSTED_TOUCH_EVENT_LEND_NOTIFICATION_FAILURE -2
#define TRUSTED_TOUCH_EVENT_ACCEPT_FAILURE -3
#define TRUSTED_TOUCH_EVENT_FUNCTIONAL_FAILURE -4
#define TRUSTED_TOUCH_EVENT_RELEASE_FAILURE -5
#define TRUSTED_TOUCH_EVENT_RECLAIM_FAILURE -6
#define TRUSTED_TOUCH_EVENT_I2C_FAILURE -7
#define TRUSTED_TOUCH_EVENT_NOTIFICATIONS_PENDING 5

struct trusted_touch_vm_info {
	enum gh_irq_label irq_label;
	enum gh_mem_notifier_tag mem_tag;
	enum gh_vm_names vm_name;
	const char *trusted_touch_type;
	u32 hw_irq;
	gh_memparcel_handle_t vm_mem_handle;
	u32 *iomem_bases;
	u32 *iomem_sizes;
	u32 iomem_list_size;
	void *mem_cookie;
	atomic_t vm_state;
};
#endif
#endif

enum NVT_TSP_FW_INDEX {
	NVT_TSP_FW_IDX_BIN	= 0,
	NVT_TSP_FW_IDX_UMS	= 1,
	NVT_TSP_FW_IDX_MP	= 2,
};

typedef enum {
	GLOVE = 1,
	CHARGER,
	PROXIMITY = 3,
	EDGE_REJECT_L = 6,
	EDGE_REJECT_H,
	EDGE_PIXEL,		// 8
	HOLE_PIXEL,
	SPAY_SWIPE,		// 10
	DOUBLE_CLICK,
	BLOCK_AREA,		// 12
	HIGH_SENSITIVITY = 14,
	SENSITIVITY,
	FUNCT_MAX,
} FUNCT_BIT;

typedef enum {
	GLOVE_MASK		= 0x0002,	// bit 1
	CHARGER_MASK		= 0x0004,	// bit 2
	PROXIMITY_MASK	= 0x0008,	// bit 3
	EDGE_REJECT_MASK	= 0x00C0,	// bit [6|7]
	EDGE_PIXEL_MASK		= 0x0100,	// bit 8
	HOLE_PIXEL_MASK		= 0x0200,	// bit 9
	SPAY_SWIPE_MASK		= 0x0400,	// bit 10
	DOUBLE_CLICK_MASK	= 0x0800,	// bit 11
	BLOCK_AREA_MASK		= 0x1000,	// bit 12
	NOISE_MASK			= 0x2000,	// bit 13
	HIGH_SENSITIVITY_MASK = 0x4000, // bit 14
	SENSITIVITY_MASK	= 0x8000,	// bit 15
	PROX_IN_AOT_MASK	= 0x80000,	// bit 19
	FUNCT_ALL_MASK		= 0xFFCA,

} FUNCT_MASK;

typedef enum {
	TEST_RESULT_PASS = 0x00,
	TEST_RESULT_FAIL = 0x01,
} TEST_RESULT;

typedef enum {
	DEBOUNCE_NORMAL = 0,
	DEBOUNCE_LOWER = 1,		//to optimize tapping performance for SIP
} TOUCH_DEBOUNCE;

typedef enum {
	GAME_MODE_DISABLE = 0,
	GAME_MODE_ENABLE = 1,
} GAME_MODE;

typedef enum {
	HIGH_SENSITIVITY_DISABLE = 0,
	HIGH_SENSITIVITY_ENABLE = 1,
} HIGH_SENSITIVITY_MODE;

enum {
	LP_OFF = 0,
	LP_AOT = (1 << 0),
	LP_SPAY = (1 << 1)
};

struct nvt_ts_event_proximity {
	u8 reserved_1:3;
	u8 id:5;
	u8 data_page;
	u8 status;
	u8 reserved_2;
} __attribute__ ((packed));

struct nvt_ts_event_coord {
	u8 status:3;
	u8 id:5;
	u8 x_11_4;
	u8 y_11_4;
	u8 y_3_0:4;
	u8 x_3_0:4;
	u8 w_major;
	u8 pressure_7_0;
} __attribute__ ((packed));

struct nvt_ts_coord {
	u16 x;
	u16 y;
	u16 p;
	u16 p_x;
	u16 p_y;
	u8 w_major;
	u8 w_minor;
	u8 status;
	u8 p_status;
	bool press;
	bool p_press;
	int move_count;
};

struct nvt_firmware {
	u8 *data;
	size_t size;
};

struct nvt_ts_lpwg_coordinate_event {
	u8 event_type:2;
	u8 touch_status:2;
	u8 tid:2;
	u8 frame_count_9_8:2;
	u8 frame_count_7_0;
	u8 x_11_4;
	u8 y_11_4;
	u8 y_3_0:4;
	u8 x_3_0:4;
} __attribute__ ((packed));

struct nvt_ts_lpwg_gesture_event {
	u8 event_type:2;
	u8 gesture_id:6;
	u8 reserved_1;
	u8 reserved_2;
	u8 reserved_3;
	u8 reserved_4;
} __attribute__ ((packed));

struct nvt_ts_lpwg_vendor_event {
	u8 event_type:2;
	u8 info_type:6;
	u8 ng_code;
	u8 reserved_1;
	u8 reserved_2;
	u8 reserved_3;
} __attribute__ ((packed));

enum {
	LPWG_EVENT_COOR = 0x00,
	LPWG_EVENT_GESTURE = 0x01,
	LPWG_EVENT_VENDOR = 0x02,
	LPWG_EVENT_RESERVED_1 = 0x03
};

enum {
	LPWG_TOUCH_STATUS_PRESS = 0x00,
	LPWG_TOUCH_STATUS_RELEASE = 0x01,
	LPWG_TOUCH_STATUS_RESERVED_1 = 0x02,
	LPWG_TOUCH_STATUS_RESERVED_2 = 0x03
};

enum {
	LPWG_GESTURE_DT = 0x00,	// Double tap
	LPWG_GESTURE_SW = 0x01,	// Swipe up
	LPWG_GESTURE_RESERVED_1 = 0x02,
	LPWG_GESTURE_RESERVED_2 = 0x03
};

typedef enum {
	LPWG_DUMP_DISABLE = 0,
	LPWG_DUMP_ENABLE = 1,
} LPWG_DUMP;

typedef enum {
	NVTWRITE = 0,
	NVTREAD  = 1
} NVT_SPI_RW;

typedef enum {
	RESET_STATE_INIT = 0xA0,// IC reset
	RESET_STATE_REK,		// ReK baseline
	RESET_STATE_REK_FINISH,	// baseline is ready
	RESET_STATE_NORMAL_RUN,	// normal run
	RESET_STATE_MAX  = 0xAF
} RST_COMPLETE_STATE;

#define LPWG_DUMP_PACKET_SIZE	5		/* 5 byte */
#define LPWG_DUMP_TOTAL_SIZE	500		/* 5 byte * 100 */

typedef enum {
	SET_GRIP_EXECPTION_ZONE = 1,
	SET_GRIP_PORTRAIT_MODE = 2,
	SET_GRIP_LANDSCAPE_MODE = 3,
	SET_TOUCH_DEBOUNCE = 5,	// for SIP
	SET_GAME_MODE = 6,
	SET_HIGH_SENSITIVITY_MODE = 7,
	PROX_SLEEP_IN = 8,
	PROX_SLEEP_OUT = 9,
	SET_LPWG_DUMP = 0x0B
} EXTENDED_CUSTOMIZED_CMD_TYPE;

struct nvt_ts_platdata {
	u8 max_touch_num;
	bool prox_lp_scan_enabled;
	bool enable_glove_mode;
	u32 open_test_spec[2];
	u32 short_test_spec[2];
	int diff_test_frame;
	u32 fdm_x_num;
	int lcd_id;
	int swrst_n8_addr; //read from dtsi
	int spi_rd_fast_addr;	//read from dtsi
	bool support_nvt_touch_proc;
	bool support_nvt_touch_ext_proc;
	bool support_nvt_lpwg_dump;

	const char *firmware_name_mp;

	const char *name_lcd_rst;
	const char *name_lcd_vddi;
	const char *name_lcd_bl_en;
	const char *name_lcd_vsp;
	const char *name_lcd_vsn;

	u32 resume_lp_delay;
};

struct nvt_ts_data {
	struct spi_device *client;
	struct nvt_ts_platdata *nplat_data;
	struct sec_ts_plat_data *plat_data;	/* only for tui */
	struct sec_cmd_data sec;
	//struct nvt_ts_coord coords[TOUCH_MAX_FINGER_NUM];
	bool press[TOUCH_MAX_FINGER_NUM];
	bool p_press[TOUCH_MAX_FINGER_NUM];
	struct input_dev *input_dev;
	struct input_dev *input_dev_proximity;
	uint8_t fw_ver;
	struct mutex lock;
	struct mutex read_write_lock;
	const struct nvt_ts_mem_map *mmap;
	uint8_t carrier_system;
	uint8_t hw_crc;
	uint16_t nvt_pid;
	u8 fw_ver_ic[4];
	u8 fw_ver_ic_bar;
	u8 fw_ver_bin[4];
	u8 fw_ver_bin_bar;
	int debug_flag;
	bool ed_reset_flag;
	u8 hover_event;	//virtual_prox
	bool lcdoff_test;
	int irq;
	char *tbuf;
	char *rbuf;

	struct delayed_work work_read_info;
	struct delayed_work work_print_info;

	struct regulator *regulator_lcd_rst;
	struct regulator *regulator_lcd_vddi;
	struct regulator *regulator_lcd_bl_en;
	struct regulator *regulator_lcd_vsp;
	struct regulator *regulator_lcd_vsn;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	atomic_t secure_enabled;
	atomic_t secure_pending_irqs;
	struct completion secure_powerdown;
	struct completion secure_interrupt;

#if IS_ENABLED(CONFIG_GH_RM_DRV)
	struct trusted_touch_vm_info *vm_info;
	struct mutex clk_io_ctrl_mutex;
	struct mutex transition_lock;
	const char *touch_environment;
	struct completion trusted_touch_powerdown;
	struct clk *core_clk;
	struct clk *iface_clk;
	atomic_t trusted_touch_initialized;
	atomic_t trusted_touch_enabled;
	atomic_t trusted_touch_transition;
	atomic_t trusted_touch_event;
	atomic_t trusted_touch_abort_status;
	atomic_t delayed_vm_probe_pending;
	atomic_t trusted_touch_mode;
#endif
#endif

	u8 noise_mode;
	u8 prox_in_aot;
	unsigned int scrub_id;
	u8 fw_status_record;

	int fw_index;
	struct nvt_firmware	*cur_fw;
	struct nvt_firmware	*nvt_bin_fw;
	struct nvt_firmware	*nvt_mp_fw;
	struct nvt_firmware	*nvt_ums_fw;

	u8 *lpwg_dump_buf;
	u16 lpwg_dump_buf_idx;
	u16 lpwg_dump_buf_size;

	struct notifier_block nb;
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	struct notifier_block vbus_nb;
#endif
	int lcd_esd_recovery;
	bool cmd_result_all_onboot;

	int (*nvt_ts_write)(struct nvt_ts_data *ts, u8 *reg, int cunum, u8 *data, int len);
	int (*nvt_ts_read)(struct nvt_ts_data *ts, u8 *reg, int cnum, u8 *data, int len);
};


struct nvt_flash_data{
	rwlock_t lock;
};

extern struct device *ptsp;


//spi
void nvt_pm_runtime_put_sync(struct nvt_ts_data *ts);
int nvt_pm_runtime_get_sync(struct nvt_ts_data *ts);
void nvt_bootloader_reset(struct nvt_ts_data *ts);
void nvt_eng_reset(struct nvt_ts_data *ts);
int nvt_ts_write_addr(struct nvt_ts_data *ts, int addr, char data);

//core
irqreturn_t nvt_ts_irq_thread(int irq, void *ptr);
int nvt_ts_probe(struct nvt_ts_data *ts);
int nvt_ts_remove(struct nvt_ts_data *ts);
void nvt_ts_shutdown(struct nvt_ts_data *ts);
int nvt_ts_pm_suspend(struct nvt_ts_data *ts);
int nvt_ts_pm_resume(struct nvt_ts_data *ts);
int nvt_check_fw_reset_state(struct nvt_ts_data *ts, RST_COMPLETE_STATE check_reset_state);
void nvt_ts_early_resume(struct device *dev);
int nvt_ts_input_resume(struct device *dev);
void nvt_ts_input_suspend(struct device *dev);

//spi
int nvt_ts_write_addr(struct nvt_ts_data *ts, int addr, char data);
int nvt_set_page(struct nvt_ts_data *ts, int addr);

//fn
int nvt_ts_check_chip_ver_trim(struct nvt_ts_data *ts);
void nvt_ts_print_info_work(struct work_struct *work);
int nvt_ts_lpwg_dump_checksum(struct nvt_ts_data *ts, char *buf, int length);
int nvt_ts_point_data_checksum(struct nvt_ts_data *ts, char *buf, int length);
int nvt_get_fw_info(struct nvt_ts_data *ts);
void nvt_sw_reset_idle(struct nvt_ts_data *ts);
int nvt_clear_fw_status(struct nvt_ts_data *ts);
int nvt_check_fw_status(struct nvt_ts_data *ts);
int nvt_flash_proc_init(struct nvt_ts_data *ts);
void nvt_ts_sec_fn_remove(struct nvt_ts_data *ts);
int nvt_ts_vbus_notification(struct notifier_block *nb, unsigned long cmd, void *data);
int nvt_notifier_call(struct notifier_block *n, unsigned long data, void *v);
void nvt_read_info_work(struct work_struct *work);
int nvt_ts_mode_restore(struct nvt_ts_data *ts);
void nvt_ts_release_all_finger(struct nvt_ts_data *ts);


//dump
int nvt_flash_proc_init(struct nvt_ts_data *ts);
void nvt_flash_proc_deinit(struct nvt_ts_data *ts);
void nvt_ts_lpwg_dump_buf_init(struct nvt_ts_data *ts);
int nvt_ts_lpwg_dump_buf_write(struct nvt_ts_data *ts, u8 *buf);
int nvt_ts_lpwg_dump_buf_read(struct nvt_ts_data *ts, u8 *buf);
void nvt_ts_lpwg_dump(struct nvt_ts_data *ts);
int32_t nvt_extra_proc_init(struct nvt_ts_data *ts);
void nvt_extra_proc_deinit(struct nvt_ts_data *ts);


//cmd
void read_tsp_info_onboot(void *device_data);
int nvt_ts_mode_switch(struct nvt_ts_data *ts, u8 cmd, bool print_log);
int set_ear_detect(struct nvt_ts_data *ts, int mode, bool print_log);
int nvt_ts_fn_init(struct nvt_ts_data *ts);
u16 nvt_ts_mode_read(struct nvt_ts_data *ts);



//fw
int nvt_update_firmware(struct nvt_ts_data *ts, const char *firmware_name);
int nvt_ts_fw_update_on_probe(struct nvt_ts_data *ts);
int nvt_ts_fw_update_from_bin(struct nvt_ts_data *ts);
int nvt_ts_fw_update_from_external(struct nvt_ts_data *ts, const char *file_path);
int nvt_ts_fw_update_from_mp_bin(struct nvt_ts_data *ts, bool is_start);

#endif
