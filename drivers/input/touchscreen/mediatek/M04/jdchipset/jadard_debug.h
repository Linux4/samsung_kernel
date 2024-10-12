#ifndef H_JADARD_DEBUG
#define H_JADARD_DEBUG

#define JADARD_PROC_REPORT_DEBUG_FILE "report_debug"
#define JADARD_PROC_FW_PACKAGE_FILE   "fw_package"
#define JADARD_PROC_RESET_FILE        "reset"
#define JADARD_PROC_ATTN_FILE         "attn"
#define JADARD_PROC_INT_EN_FILE       "int_en"
#define JADARD_PROC_DIAG_FILE         "diag"
#define JADARD_PROC_DIAG_ARR_FILE	  "diag_arr"
#define JADARD_PROC_FW_DUMP_FILE      "fw_dump"
#define JADARD_PROC_REGISTER_FILE	  "register"
#define JADARD_PROC_DISPLAY_FILE	  "display"
#define JADARD_PROC_DEBUG_FILE	      "debug"

static struct proc_dir_entry *jadard_proc_report_debug_file = NULL;
static struct proc_dir_entry *jadard_proc_fw_package_file   = NULL;
static struct proc_dir_entry *jadard_proc_reset_file        = NULL;
static struct proc_dir_entry *jadard_proc_attn_file         = NULL;
static struct proc_dir_entry *jadard_proc_int_en_file       = NULL;
static struct proc_dir_entry *jadard_proc_diag_file         = NULL;
static struct proc_dir_entry *jadard_proc_diag_arr_file     = NULL;
static struct proc_dir_entry *jadard_proc_fw_dump_file      = NULL;
static struct proc_dir_entry *jadard_proc_register_file     = NULL;
static struct proc_dir_entry *jadard_proc_display_file      = NULL;
static struct proc_dir_entry *jadard_proc_debug_file        = NULL;

#define JD_RAWDATA_DUMP_FILE "/sdcard/JD_RAWDATA_Dump.txt"
#define JD_DIFF_DUMP_FILE    "/sdcard/JD_DIFF_Dump.txt"
#define JD_BASE_DUMP_FILE    "/sdcard/JD_BASE_Dump.txt"
/*hs03s code for SR-AL5625-01-439 by yuanliding at 20210616 start*/
#define JD_LISTEN_DUMP_FILE  "/sdcard/JD_LISTEN_Dump.txt"
#define JD_LABEL_DUMP_FILE   "/sdcard/JD_LABEL_Dump.txt"
/*hs03s code for SR-AL5625-01-439 by yuanliding at 20210616 end*/

struct jadard_diag_mutual_data {
	char *buf;
	int  buf_len;
	int  write_len;
};

static int DataType;
static int KeepType;
static int KeepFrame;

static bool        DIAG_THREAD_FLAG = false;
static struct file *jd_diag_mutual_fn = NULL;
int                *jd_diag_mutual = NULL;
static int         jd_diag_mutual_cnt;
static uint8_t     diag_arr_num = 0;
static uint8_t     reg_cmd[4];
static uint8_t     reg_cmd_len;
static uint8_t     reg_read_len = 1;
static uint8_t     dd_reg_cmd;
static uint8_t     dd_reg_read_len = 1;

static uint8_t *fw_buffer = NULL;
static bool    fw_dump_busy;
static bool    fw_dump_complete;
static bool    fw_dump_going;

static char debug_cmd = 0;
static bool fw_upgrade_complete = false;
bool jd_g_dbg_enable = false;
int jadard_touch_proc_init(void);

#endif
