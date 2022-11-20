#define EMC_REPOWER_PARAM_FLAG 0x454D4350
struct emc_repower_param {
	u32 flag;//must be 0x454D4350//EMCP
	u32 mem_type;
	u32 emc_freq;
	u32 mem_drv;
	u32 mem_width;
	u32 cs_number;
	u32 cs0_size;
	u32 cs1_size;
	u32 cs0_training_addr_v;//cs0 training virtual address
	u32 cs0_training_addr_p;//cs0 training phy address
	u32 cs0_training_data_size;
	u32 cs1_training_addr_v;//cs1 training virtual address
	u32 cs1_training_addr_p;//cs1 training phy address
	u32 cs1_training_data_size;
	u32 cs0_saved_data[8];
	u32 cs1_saved_data[8];
};
//param must be in IRAM
void set_emc_repower_param(struct emc_repower_param *param, u32 umctl_base, u32 publ_base);
void save_emc_trainig_data(struct emc_repower_param *param);
struct emc_repower_param * get_emc_repower_param(void);
void emc_init_repowered(u32 power_off);
