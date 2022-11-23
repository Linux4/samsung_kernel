#ifndef _WCN_PROCFS
#define _WCN_PROCFS

#define MDBG_SNAP_SHOOT_SIZE		(32*1024)
#define MDBG_WRITE_SIZE			(64)
#define MDBG_ASSERT_SIZE		(1024)
#define MDBG_LOOPCHECK_SIZE		(128)
#define MDBG_AT_CMD_SIZE		(128)

void mdbg_fs_channel_init(void);
void mdbg_fs_channel_destroy(void);
int proc_fs_init(void);
int mdbg_memory_alloc(void);
void proc_fs_exit(void);
int get_loopcheck_status(void);
void wakeup_loopcheck_int(void);
void loopcheck_ready_clear(void);
void loopcheck_ready_set(void);
void mdbg_assert_interface(char *str);

#ifdef CONFIG_WCN_SLEEP_INFO
extern unsigned int wcn_reboot_count;
#define WCN_CP2_SLP_INFO_SIZE		(128)
#define WCN_CP2_SLP_INFO_ADDR 0x8417ff80

int wcn_sleep_info_open(void);
struct subsys_sleep_info *wcn_sleep_info_read(void *data);
int wcn_send_atcmd(void *cmd, unsigned char cmd_len,
		   void *response, size_t *response_len);


struct wcn_cp2_slp_duration_total {
	unsigned long long int total_dut;
	unsigned long long int slp_dut_total;
	unsigned long long int idle_dut_total;
} __aligned(4);
#endif
#endif
