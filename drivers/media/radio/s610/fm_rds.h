#ifndef FM_RDS_H
#define FM_RDS_H

void fm_rds_write_data(struct s610_radio *radio, u16 rds_data,
	fm_rds_block_type_enum blk_type, fm_host_rds_errors_enum errors);
u16 fm_rds_get_avail_bytes(struct s610_radio *radio);
bool fm_read_rds_data(struct s610_radio *radio, u16 *buffer,
		u16 *size, u16 *blocks);
void fm_rds_change_state(struct s610_radio *radio,
		fm_rds_state_enum new_state);
void fm_rds_update_error_status(struct s610_radio *radio, u16 errors);
static fm_host_rds_errors_enum fm_rds_process_block(struct s610_radio *radio,
		u16 data, fm_rds_block_type_enum block_type, u16 err_count);
void fm_process_rds_data(struct s610_radio *radio);

extern void fm_update_rds_sync_status(struct s610_radio *radio, bool synced);

extern int fm_set_flags(struct s610_radio *radio, u16 flags);
extern void fm_timer_reset(fm_timer_t *timer, int usec, fm_callback_t *func,
		void *arg);
extern u32 fmspeedy_get_reg_int(u32 addr);

#define RDS_BLK_TYPE_MASK	0xE0
#define RDS_BLK_TYPE_SHIFT	5
#define RDS_ERR_CNT_MASK	0x1F

#endif	/*fm_rds.h*/
