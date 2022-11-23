#ifndef __WCN_GNSS_OPS_H__
#define __WCN_GNSS_OPS_H__


#ifndef CONFIG_WCN_INTEG
/* start: address map on gnss side */
#ifdef CONFIG_UMW2652_REMOVE
#define GNSS_CALI_ADDRESS 0x40aabf4c
#define GNSS_CALI_DATA_SIZE 0x1c
#else
#define GNSS_CALI_ADDRESS 0x40aaff4c
#define GNSS_CALI_DATA_SIZE 0x14
#endif

#define GNSS_CALI_DONE_FLAG 0x1314520

#ifdef CONFIG_UMW2652_REMOVE
#define GNSS_EFUSE_ADDRESS 0x40aabf40
#else
#define GNSS_EFUSE_ADDRESS 0x40aaff40
#endif

#define GNSS_EFUSE_DATA_SIZE 0xc

#ifdef CONFIG_UMW2652_REMOVE
#define GNSS_BOOTSTATUS_ADDRESS  0x40aabf6c
#else
#define GNSS_BOOTSTATUS_ADDRESS  0x40aaff6c
#endif

#define GNSS_BOOTSTATUS_SIZE     0x4
#define GNSS_BOOTSTATUS_MAGIC    0x12345678
/* end: address map on gnss side */

int gnss_data_init(void);
int gnss_write_data(void);
int gnss_backup_data(void);
int gnss_boot_wait(void);
void gnss_file_path_set(char *buf);
#endif


#endif
