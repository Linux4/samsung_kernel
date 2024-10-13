#ifndef SR2351_FM_CTRL_H__
#define SR2351_FM_CTRL_H_

#include <soc/sprd/hardware.h>
#include <linux/sprd_2351.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/sci.h>
#include <linux/ioctl.h>
#include <linux/time.h>

#define	SR2351_FM_DEV_NAME	"fm"


extern int sr2351_fm_en(void);
extern int sr2351_fm_dis(void);
extern int sr2351_fm_get_status(int *status);
extern int sr2351_fm_get_frequency(u32 *);
extern void sr2351_fm_enter_sleep(void);
extern void sr2351_fm_exit_sleep(void);
extern void sr2351_fm_mute(void);
extern void sr2351_fm_unmute(void);
extern int sr2351_fm_init(void);
extern int sr2351_fm_deinit(void);
extern int sr2351_fm_set_tune(u32 freq);
extern int sr2351_fm_seek(u32 frequency, u32 seek_dir, u32 time_out, u32 *freq_found);
extern int sr2351_fm_stop_seek(void);
extern int sr2351_fm_get_rssi(u32 *);
extern int sr2351_fm_check_status(void *);

typedef signed char fm_s8;
typedef signed short fm_s16;
typedef signed int fm_s32;
typedef signed long long fm_s64;
typedef unsigned char fm_u8;
typedef unsigned short fm_u16;
typedef unsigned int fm_u32;
typedef unsigned long long fm_u64;
typedef enum fm_bool {
    fm_false = 0,
    fm_true  = 1
} fm_bool;

/*  scan sort algorithm */
enum {
    FM_SCAN_SORT_NON = 0,
    FM_SCAN_SORT_UP,
    FM_SCAN_SORT_DOWN,
    FM_SCAN_SORT_MAX
};

/*  scan methods */
enum {
	FM_SCAN_SEL_HW = 0, /*  select hardware scan, advantage: fast */
	FM_SCAN_SEL_SW, /*  select software scan, advantage: more accurate */
	FM_SCAN_SEL_MAX
};

/*********FM config for customer*******/
/*  FM radio long antenna RSSI threshold(11.375dBuV) */
#define FMR_RSSI_TH_LONG    0x0301
/*  FM radio short antenna RSSI threshold(-1dBuV) */
#define FMR_RSSI_TH_SHORT   0x02E0
/*  FM radio Channel quality indicator threshold(0x0000~0x00FF) */
#define FMR_CQI_TH          0x00E9
/*  FM radio seek space,1:100KHZ; 2:200KHZ */
#define FMR_SEEK_SPACE      1
/* FM radio scan max channel size */
#define FMR_SCAN_CH_SIZE    40
/*  FM radio band, 1:87.5MHz~108.0MHz; 2:76.0MHz~90.0MHz;
      3:76.0MHz~108.0MHz; 4:special    */
#define FMR_BAND            1
/*  FM radio special band low freq(Default 87.5MHz) */
#define FMR_BAND_FREQ_L     875
/*  FM radio special band high freq(Default 108.0MHz) */
#define FMR_BAND_FREQ_H     1080
#define FM_SCAN_SORT_SELECT FM_SCAN_SORT_NON
#define FM_SCAN_SELECT      FM_SCAN_SEL_HW
/*  soft-mute threshold when software scan, rang: 0~3, */
/*  0 means better audio quality but less channel */
#define FM_SCAN_SOFT_MUTE_GAIN_TH  3
#define FM_CHIP_DESE_RSSI_TH (-102) /*  rang: -102 ~ -72 */

/*
*******FM config for engineer *********
*/
#define FMR_MR_TH           0x01BD /* FM radio MR threshold */
#define ADDR_SCAN_TH        0xE0 /* scan thrshold register */
#define ADDR_CQI_TH         0xE1 /* scan CQI register */

#define FM_NAME             "fm"
#define FM_DEVICE_NAME      "/dev/fm"

/*  errno */
#define FM_SUCCESS      0
#define FM_FAILED       1
#define FM_EPARM        2
#define FM_BADSTATUS    3
#define FM_TUNE_FAILED  4
#define FM_SEEK_FAILED  5
#define FM_BUSY         6
#define FM_SCAN_FAILED  7

/*  band */
#define FM_BAND_UNKNOWN 0
#define FM_BAND_UE      1 /*  US/Europe band  87.5MHz ~ 108MHz (DEFAULT) */
#define FM_BAND_JAPAN   2 /*  Japan band      76MHz   ~ 90MHz  */
#define FM_BAND_JAPANW  3 /*  Japan wideband  76MHZ   ~ 108MHz */
#define FM_BAND_SPECIAL 4 /*  special   band  between 76MHZ   and  108MHz */
#define FM_BAND_DEFAULT FM_BAND_UE

#define FM_UE_FREQ_MIN  875
#define FM_UE_FREQ_MAX  1080
#define FM_JP_FREQ_MIN  760
#define FM_JP_FREQ_MAX  1080
#define FM_FREQ_MIN  FMR_BAND_FREQ_L
#define FM_FREQ_MAX  FMR_BAND_FREQ_H
#define FM_RAIDO_BAND FM_BAND_UE

/* space */
#define FM_SPACE_UNKNOWN    0
#define FM_SPACE_100K       1
#define FM_SPACE_200K       2
#define FM_SPACE_50K        5
#define FM_SPACE_DEFAULT    FM_SPACE_100K

#define FM_SEEK_SPACE FMR_SEEK_SPACE

/* max scan channel num */
#define FM_MAX_CHL_SIZE FMR_SCAN_CH_SIZE
/* auto HiLo */
#define FM_AUTO_HILO_OFF    0
#define FM_AUTO_HILO_ON     1

/*  seek direction */
#define FM_SEEK_UP          0
#define FM_SEEK_DOWN        1

/*  seek threshold */
#define FM_SEEKTH_LEVEL_DEFAULT 4

struct fm_tune_parm {
	uint8_t err;
	uint8_t band;
	uint8_t space;
	uint8_t hilo;
	uint16_t freq;
};

struct fm_seek_parm {
	uint8_t err;
	uint8_t band;
	uint8_t space;
	uint8_t hilo;
	uint8_t seekdir;
	uint8_t seekth;
	uint16_t freq;
};

struct fm_scan_parm {
	uint8_t  err;
	uint8_t  band;
	uint8_t  space;
	uint8_t  hilo;
	uint16_t freq;
	uint16_t scantbl[16];
	uint16_t scantblsize;
};

struct fm_ch_rssi {
	uint16_t freq;
	int rssi;
};

struct fm_check_status {
	uint8_t status;
	int rssi;
	uint16_t freq;
};

enum fm_scan_cmd_t {
	FM_SCAN_CMD_INIT = 0,
	FM_SCAN_CMD_START,
	FM_SCAN_CMD_GET_NUM,
	FM_SCAN_CMD_GET_CH,
	FM_SCAN_CMD_GET_RSSI,
	FM_SCAN_CMD_GET_CH_RSSI,
	FM_SCAN_CMD_MAX
};

struct fm_scan_t {
	enum fm_scan_cmd_t cmd;
	int ret; /* 0, success; else error code */
	uint16_t lower; /*  lower band, Eg, 7600 -> 76.0Mhz */
	uint16_t upper; /* upper band, Eg, 10800 -> 108.0Mhz */
	int space; /*  5: 50KHz, 10: 100Khz, 20: 200Khz */
	int num; /*  valid channel number */
	void *priv;
	int sr_size; /* scan result buffer size in bytes */
	union {
		uint16_t *ch_buf; /* channel buffer */
		int *rssi_buf; /*  rssi buffer */
		struct fm_ch_rssi *ch_rssi_buf; /* channel and RSSI buffer */
    } sr;
};

struct fm_seek_t {
	int ret; /*  0, success; else error code */
	uint16_t freq;
	uint16_t lower; /*  lower band, Eg, 7600 -> 76.0Mhz */
	uint16_t upper; /*  upper band, Eg, 10800 -> 108.0Mhz */
	int space; /*  5: 50KHz, 10: 100Khz, 20: 200Khz */
	int dir; /*  0: up; 1: down */
	int th; /*  seek threshold in dbm(Eg, -95dbm) */
	void *priv;
};

struct fm_tune_t {
	int ret; /*  0, success; else error code */
	uint16_t freq;
	uint16_t lower; /*  lower band, Eg, 7600 -> 76.0Mhz */
	uint16_t upper; /*  upper band, Eg, 10800 -> 108.0Mhz */
	int space; /*  5: 50KHz, 10: 100Khz, 20: 200Khz */
	void *priv;
};

struct fm_softmute_tune_t {
	fm_s32 rssi; /* RSSI of current channel */
	fm_u16 freq; /* current frequency */
	fm_bool valid; /* current channel is valid(true) or not(false) */
};

struct fm_rssi_req {
	uint16_t num;
	uint16_t read_cnt;
	struct fm_ch_rssi cr[16*16];
};

struct fm_hw_info {
	int chip_id;
	int eco_ver;
	int rom_ver;
	int patch_ver;
	int reserve;
};

struct fm_search_threshold_t {
	fm_s32 th_type;/* 0, RSSI. 1,desense RSSI. 2,SMG. */
	fm_s32 th_val; /*threshold value */
	fm_s32 reserve;
};

#define NEED_DEF_RDS 1

#if NEED_DEF_RDS
/* For RDS feature */
typedef struct {
	uint8_t TP;
	uint8_t TA;
	uint8_t music;
	uint8_t stereo;
	uint8_t artificial_head;
	uint8_t compressed;
	uint8_t dynamic_pty;
	uint8_t text_ab;
	uint32_t flag_status;
} rdslag_struct;

typedef struct {
	uint16_t month;
	uint16_t day;
	uint16_t year;
	uint16_t hour;
	uint16_t minute;
	uint8_t local_time_offset_signbit;
	uint8_t local_time_offset_half_hour;
} ct_struct;

typedef struct {
	int16_t AF_NUM;
	int16_t AF[2][25];
	uint8_t addr_cnt;
	uint8_t ismethod_a;
	uint8_t isafnum_get;
} af_info;

typedef struct {
	uint8_t PS[4][8];
	uint8_t addr_cnt;
} ps_info;

typedef struct {
	uint8_t textdata[4][64];
	uint8_t getlength;
	uint8_t isrtdisplay;
	uint8_t textlength;
	uint8_t istypea;
	uint8_t bufcnt;
	uint16_t addr_cnt;
} rt_info;

struct rds_raw_data {
	int dirty; /*  indicate if the data changed or not */
	int len; /* the data len form chip */
	uint8_t data[146];
};

struct rds_group_cnt {
	unsigned long total;
	unsigned long groupA[16]; /* RDS groupA counter*/
	unsigned long groupB[16]; /* RDS groupB counter */
};

enum rds_group_cnt_opcode {
	RDS_GROUP_CNT_READ = 0,
	RDS_GROUP_CNT_WRITE,
	RDS_GROUP_CNT_RESET,
	RDS_GROUP_CNT_MAX
};

struct rds_group_cnt_req {
	int err;
	enum rds_group_cnt_opcode op;
	struct rds_group_cnt gc;
};
/*
typedef struct {
	CT_Struct CT;
	RDSFlag_Struct RDSFlag;
	uint16_t PI;
	uint8_t Switch_TP;
	uint8_t PTY;
	AF_Info AF_Data;
	AF_Info AFON_Data;
	uint8_t Radio_Page_Code;
	uint16_t Program_Item_Number_Code;
	uint8_t Extend_Country_Code;
	uint16_t Language_Code;
	PS_Info PS_Data;
	uint8_t PS_ON[8];
	RT_Info RT_Data;
	uint16_t event_status;
	struct rds_group_cnt gc;
} RDSData_Struct;

*/
/* valid Rds Flag for notify */
typedef enum {
/* Program is a traffic program */
	RDS_FLAG_IS_TP              = 0x0001,
/* Program currently broadcasts a traffic ann. */
	RDS_FLAG_IS_TA              = 0x0002,
/*  Program currently broadcasts music */
	RDS_FLAG_IS_MUSIC           = 0x0004,
/*  Program is transmitted in stereo */
	RDS_FLAG_IS_STEREO          = 0x0008,
/*  Program is an artificial head recording */
	RDS_FLAG_IS_ARTIFICIAL_HEAD = 0x0010,
/*  Program content is compressed */
	RDS_FLAG_IS_COMPRESSED      = 0x0020,
/*  Program type can change */
	RDS_FLAG_IS_DYNAMIC_PTY     = 0x0040,
/*  If this flag changes state, a new radio text string begins */
	RDS_FLAG_TEXT_AB            = 0x0080
} rdsflag;

typedef enum {
/*  One of the RDS flags has changed state */
	RDS_EVENT_FLAGS          = 0x0001,
/*  The program identification code has changed */
	RDS_EVENT_PI_CODE        = 0x0002,
/*  The program type code has changed */
	RDS_EVENT_PTY_CODE       = 0x0004,
/*  The program name has changed */
	RDS_EVENT_PROGRAMNAME    = 0x0008,
/* A new UTC date/time is available */
	RDS_EVENT_UTCDATETIME    = 0x0010,
/*  A new local date/time is available */
	RDS_EVENT_LOCDATETIME    = 0x0020,
/*  A radio text string was completed */
	RDS_EVENT_LAST_RADIOTEXT = 0x0040,
/*  Current Channel RF signal strength too weak, need do AF switch */
	RDS_EVENT_AF             = 0x0080,
/*  An alternative frequency list is ready */
	RDS_EVENT_AF_LIST        = 0x0100,
/*  An alternative frequency list is ready */
	RDS_EVENT_AFON_LIST      = 0x0200,
/*  Other Network traffic announcement start */
	RDS_EVENT_TAON           = 0x0400,
/*  Other Network traffic announcement finished. */
	RDS_EVENT_TAON_OFF       = 0x0800,
/*  RDS Interrupt had arrived durint timer period */
	RDS_EVENT_RDS            = 0x2000,
/*  RDS Interrupt not arrived durint timer period */
	RDS_EVENT_NO_RDS         = 0x4000,
/* Timer for RDS Bler Check. ---- BLER  block error rate */
	RDS_EVENT_RDS_TIMER      = 0x8000
} rds_event;
#endif

typedef enum {
	FM_I2S_ON = 0,
	FM_I2S_OFF,
	FM_I2S_STATE_ERR
} fm_i2s_state_e;

typedef enum {
	FM_I2S_MASTER = 0,
	FM_I2S_SLAVE,
	FM_I2S_MODE_ERR
} fm_i2s_mode_e;

typedef enum {
	FM_I2S_32K = 0,
	FM_I2S_44K,
	FM_I2S_48K,
	FM_I2S_SR_ERR
} fm_i2s_sample_e;

struct fm_i2s_setting {
	int onoff;
	int mode;
	int sample;
};

typedef enum {
	FM_RX = 0,
	FM_TX = 1
} FM_PWR_T;

typedef struct fm_i2s_info {
/* 0:FM_I2S_ON, 1:FM_I2S_OFF,2:error */
	int status;
/* 0:FM_I2S_MASTER, 1:FM_I2S_SLAVE,2:error */
	int mode;
/* 0:FM_I2S_32K:32000,1:FM_I2S_44K:44100,2:FM_I2S_48K:48000,3:error */
	int rate;
} fm_i2s_info_t;

typedef enum {
	FM_AUD_ANALOG = 0,
	FM_AUD_I2S = 1,
	FM_AUD_MRGIF = 2,
	FM_AUD_ERR
} fm_audio_path_e;

typedef enum {
	FM_I2S_PAD_CONN = 0,
	FM_I2S_PAD_IO = 1,
	FM_I2S_PAD_ERR
} fm_i2s_pad_sel_e;

typedef struct fm_audio_info {
	fm_audio_path_e aud_path;
	fm_i2s_info_t i2s_info;
	fm_i2s_pad_sel_e i2s_pad;
} fm_audio_info_t;

struct fm_cqi {
	int ch;
	int rssi;
	int reserve;
};

struct fm_cqi_req {
	uint16_t ch_num;
	int buf_size;
	char *cqi_buf;
};

typedef struct {
	int freq;
	int rssi;
} fm_desense_check_t;

typedef struct {
	uint16_t lower; /*  lower band, Eg, 7600 -> 76.0Mhz */
	uint16_t upper; /*  upper band, Eg, 10800 -> 108.0Mhz */
	int space; /* 0x1: 50KHz, 0x2: 100Khz, 0x4: 200Khz */
	int cycle; /*  repeat times */
} fm_full_cqi_log_t;

/* ********** ***********FM IOCTL define start ****************/
#define  Google_FM_APP         1
#if Google_FM_APP
#define FM_IOC_MAGIC        0xf5

#define FM_IOCTL_POWERUP       _IOWR(FM_IOC_MAGIC, 0, struct fm_tune_parm*)
#define FM_IOCTL_POWERDOWN     _IOWR(FM_IOC_MAGIC, 1, int32_t*)
#define FM_IOCTL_TUNE          _IOWR(FM_IOC_MAGIC, 2, struct fm_tune_parm*)
#define FM_IOCTL_SEEK          _IOWR(FM_IOC_MAGIC, 3, struct fm_seek_parm*)
#define FM_IOCTL_SETVOL        _IOWR(FM_IOC_MAGIC, 4, uint32_t*)
#define FM_IOCTL_GETVOL        _IOWR(FM_IOC_MAGIC, 5, uint32_t*)
#define FM_IOCTL_MUTE          _IOWR(FM_IOC_MAGIC, 6, uint32_t*)
#define FM_IOCTL_GETRSSI       _IOWR(FM_IOC_MAGIC, 7, int32_t*)
#define FM_IOCTL_SCAN          _IOWR(FM_IOC_MAGIC, 8, struct fm_scan_parm*)
#define FM_IOCTL_STOP_SCAN     _IO(FM_IOC_MAGIC,   9)

/* IOCTL and struct for test */
#define FM_IOCTL_GETCHIPID     _IOWR(FM_IOC_MAGIC, 10, uint16_t*)
#define FM_IOCTL_EM_TEST       _IOWR(FM_IOC_MAGIC, 11, struct fm_em_parm*)
#define FM_IOCTL_RW_REG        _IOWR(FM_IOC_MAGIC, 12, struct fm_ctl_parm*)
#define FM_IOCTL_GETMONOSTERO  _IOWR(FM_IOC_MAGIC, 13, uint16_t*)
#define FM_IOCTL_GETCURPAMD    _IOWR(FM_IOC_MAGIC, 14, uint16_t*)
#define FM_IOCTL_GETGOODBCNT   _IOWR(FM_IOC_MAGIC, 15, uint16_t*)
#define FM_IOCTL_GETBADBNT     _IOWR(FM_IOC_MAGIC, 16, uint16_t*)
#define FM_IOCTL_GETBLERRATIO  _IOWR(FM_IOC_MAGIC, 17, uint16_t*)

/* IOCTL for RDS */
#define FM_IOCTL_RDS_ONOFF     _IOWR(FM_IOC_MAGIC, 18, uint16_t*)
#define FM_IOCTL_RDS_SUPPORT   _IOWR(FM_IOC_MAGIC, 19, int32_t*)

#define FM_IOCTL_RDS_SIM_DATA  _IOWR(FM_IOC_MAGIC, 23, uint32_t*)
#define FM_IOCTL_IS_FM_POWERED_UP  _IOWR(FM_IOC_MAGIC, 24, uint32_t*)

/* IOCTL for FM over BT */
#define FM_IOCTL_OVER_BT_ENABLE  _IOWR(FM_IOC_MAGIC, 29, int32_t*)

/* IOCTL for FM ANTENNA SWITCH */
#define FM_IOCTL_ANA_SWITCH     _IOWR(FM_IOC_MAGIC, 30, int32_t*)
#define FM_IOCTL_GETCAPARRAY      _IOWR(FM_IOC_MAGIC, 31, int32_t*)

/* IOCTL for FM I2S Setting  */
#define FM_IOCTL_I2S_SETTING  _IOWR(FM_IOC_MAGIC, 33, struct fm_i2s_setting*)

#define FM_IOCTL_RDS_GROUPCNT   _IOWR(FM_IOC_MAGIC, 34, struct rds_group_cnt_req*)
#define FM_IOCTL_RDS_GET_LOG    _IOWR(FM_IOC_MAGIC, 35, struct rds_raw_data*)

#define FM_IOCTL_SCAN_GETRSSI   _IOWR(FM_IOC_MAGIC, 36, struct fm_rssi_req*)
#define FM_IOCTL_SETMONOSTERO   _IOWR(FM_IOC_MAGIC, 37, int32_t)
#define FM_IOCTL_RDS_BC_RST     _IOWR(FM_IOC_MAGIC, 38, int32_t*)
#define FM_IOCTL_CQI_GET     _IOWR(FM_IOC_MAGIC, 39, struct fm_cqi_req*)
#define FM_IOCTL_GET_HW_INFO    _IOWR(FM_IOC_MAGIC, 40, struct fm_hw_info*)
#define FM_IOCTL_GET_I2S_INFO   _IOWR(FM_IOC_MAGIC, 41, fm_i2s_info_t*)
#define FM_IOCTL_IS_DESE_CHAN   _IOWR(FM_IOC_MAGIC, 42, int32_t*)
#define FM_IOCTL_TOP_RDWR _IOWR(FM_IOC_MAGIC, 43, struct fm_top_rw_parm*)
#define FM_IOCTL_HOST_RDWR  _IOWR(FM_IOC_MAGIC, 44, struct fm_host_rw_parm*)

#define FM_IOCTL_PRE_SEARCH _IOWR(FM_IOC_MAGIC, 45,int32_t)
#define FM_IOCTL_RESTORE_SEARCH _IOWR(FM_IOC_MAGIC, 46,int32_t)

#define FM_IOCTL_SET_SEARCH_THRESHOLD   _IOWR(FM_IOC_MAGIC, 47, fm_search_threshold_t*)

#define FM_IOCTL_GET_AUDIO_INFO _IOWR(FM_IOC_MAGIC, 48, fm_audio_info_t*)

#define FM_IOCTL_SCAN_NEW       _IOWR(FM_IOC_MAGIC, 60, struct fm_scan_t*)
#define FM_IOCTL_SEEK_NEW       _IOWR(FM_IOC_MAGIC, 61, struct fm_seek_t*)
#define FM_IOCTL_TUNE_NEW       _IOWR(FM_IOC_MAGIC, 62, struct fm_tune_t*)

#define FM_IOCTL_SOFT_MUTE_TUNE _IOWR(FM_IOC_MAGIC, 63, struct fm_softmute_tune_t*)
#define FM_IOCTL_DESENSE_CHECK   _IOWR(FM_IOC_MAGIC, 64, fm_desense_check_t*)

/* IOCTL for EM */
#define FM_IOCTL_FULL_CQI_LOG _IOWR(FM_IOC_MAGIC, 70, fm_full_cqi_log_t *)

/* IOCTL for SPRD autotest tool to check status */
#define FM_IOCTL_CHECK_STATUS _IOWR(FM_IOC_MAGIC, 71, struct fm_check_status*)

#define FM_IOCTL_DUMP_REG   _IO(FM_IOC_MAGIC, 0xFF)

/* ********** ***********FM IOCTL define end *******************************/
#endif

enum group_idx {
    mono = 0,
    stereo,
    RSSI_threshold,
    HCC_Enable,
    PAMD_threshold,
    Softmute_Enable,
    De_emphasis,
    HL_Side,
    Demod_BW,
    Dynamic_Limiter,
    Softmute_Rate,
    AFC_Enable,
    Softmute_Level,
    Analog_Volume,
    GROUP_TOTAL_NUMS
};

enum item_idx {
    Sblend_OFF = 0,
    Sblend_ON,
    ITEM_TOTAL_NUMS
};

struct fm_ctl_parm {
	uint8_t err;
	uint8_t addr;
	uint16_t val;
	uint16_t rw_flag; /* 0:write, 1:read */
};

struct fm_em_parm {
    uint16_t group_idx;
    uint16_t item_idx;
    uint32_t item_value;
};

/*FM Module Start*/
/* seek direction */
#define SR2351_SEEK_DIR_UP          0
#define SR2351_SEEK_DIR_DOWN        1
#define SR2351_SEEK_TIMEOUT         1
#if  !Google_FM_APP
/** The following define the IOCTL command values via the ioctl macros */
#define FM_IOCTL_BASE     'R'
#define FM_IOCTL_ENABLE      _IOW(FM_IOCTL_BASE, 0, int)
#define FM_IOCTL_GET_ENABLE  _IOW(FM_IOCTL_BASE, 1, int)
#define FM_IOCTL_SET_TUNE    _IOW(FM_IOCTL_BASE, 2, int)
#define FM_IOCTL_GET_FREQ    _IOW(FM_IOCTL_BASE, 3, int)
#define FM_IOCTL_SEARCH      _IOW(FM_IOCTL_BASE, 4, int[4])
#define FM_IOCTL_STOP_SEARCH _IOW(FM_IOCTL_BASE, 5, int)
#define FM_IOCTL_MUTE        _IOW(FM_IOCTL_BASE, 6, int)
#define FM_IOCTL_SET_VOLUME  _IOW(FM_IOCTL_BASE, 7, int)
#define FM_IOCTL_GET_VOLUME  _IOW(FM_IOCTL_BASE, 8, int)
#define FM_IOCTL_CONFIG      _IOW(FM_IOCTL_BASE, 9, int)
#define FM_IOCTL_GET_RSSI    _IOW(FM_IOCTL_BASE, 10, int)
#endif

#ifdef CONFIG_FM_SEEK_STEP_50KHZ
#define MAX_FM_FREQ	10800
#define MIN_FM_FREQ	8750
#else
#define MAX_FM_FREQ	        1080
#define MIN_FM_FREQ	        875
#endif

#define FM_CTL_STI_MODE_NORMAL	0x0
#define	FM_CTL_STI_MODE_SEEK    0x1
#define	FM_CTL_STI_MODE_TUNE    0x2

#ifndef CONFIG_OF
#define SHARK_FM_REG_BASE               SPRD_FM_BASE
#define SHARK_APB_BASE_ADDR         	SPRD_AONAPB_BASE
#define  SHARK_PMU_BASE_ADDR       		SPRD_PMU_BASE
#define SHARK_AON_CLK_BASE_ADDR      	SPRD_AONCKG_BASE
#define SHARK_PIN_BASE					SPRD_PIN_BASE
#else
extern unsigned long rf2351_fm_get_fm_base(void);
extern unsigned long rf2351_fm_get_apb_base(void);
extern unsigned long rf2351_fm_get_pmu_base(void);
extern unsigned long rf2351_fm_get_aonckg_base(void);
extern unsigned long rf2351_fm_get_pin_base(void);

#define SHARK_FM_REG_BASE			rf2351_fm_get_fm_base()
#define SHARK_APB_BASE_ADDR			rf2351_fm_get_apb_base()
#define SHARK_PMU_BASE_ADDR			rf2351_fm_get_pmu_base()
#define SHARK_AON_CLK_BASE_ADDR		rf2351_fm_get_aonckg_base()
#define SHARK_PIN_BASE				rf2351_fm_get_pin_base()
#endif

#define	FM_REG_FM_CTRL                  (SHARK_FM_REG_BASE + 0x0000)
#define	FM_REG_FM_EN                    (SHARK_FM_REG_BASE + 0x0004)
#define	FM_REG_RF_INIT_GAIN             (SHARK_FM_REG_BASE + 0x0008)
#define	FM_REG_CHAN                     (SHARK_FM_REG_BASE + 0x000C)
#define	FM_REG_AGC_TBL_CLK              (SHARK_FM_REG_BASE + 0x0010)
#define	FM_REG_SEEK_LOOP                (SHARK_FM_REG_BASE + 0x0014)
#define	FM_REG_FMCTL_STI                (SHARK_FM_REG_BASE + 0x0018)
#define	FM_REG_BAND_LMT                 (SHARK_FM_REG_BASE + 0x001C)
#define	FM_REG_BAND_SPACE               (SHARK_FM_REG_BASE + 0x0020)
#define	FM_REG_RF_CTL                   (SHARK_FM_REG_BASE + 0x0024)
#define FM_REG_RF_CTL1                  (SHARK_FM_REG_BASE + 0x0028)
#define	FM_REG_INT_EN                   (SHARK_FM_REG_BASE + 0x0030)
#define	FM_REG_INT_CLR                  (SHARK_FM_REG_BASE + 0x0034)
#define	FM_REG_INT_STS                  (SHARK_FM_REG_BASE + 0x0038)
#define	FM_REG_ADC_INFCTRL              (SHARK_FM_REG_BASE + 0x003C)
#define	FM_REG_SEEK_CH_TH               (SHARK_FM_REG_BASE + 0x0040)
#define	FM_REG_AGC_TBL_RFRSSI0          (SHARK_FM_REG_BASE + 0x0044)
#define	FM_REG_AGC_TBL_RFRSSI1          (SHARK_FM_REG_BASE + 0x0048)
#define	FM_REG_AGC_TBL_WBRSSI           (SHARK_FM_REG_BASE + 0x004C)
#define	FM_REG_AGC_IDX_TH               (SHARK_FM_REG_BASE + 0x0050)
#define	FM_REG_AGC_RSSI_TH              (SHARK_FM_REG_BASE + 0x0054)
#define	FM_REG_SEEK_ADPS                (SHARK_FM_REG_BASE + 0x0058)
#define	FM_REG_STER_PWR                 (SHARK_FM_REG_BASE + 0x005C)
#define	FM_REG_AGC_CTRL                 (SHARK_FM_REG_BASE + 0x0060)
#define	FM_REG_AGC_ITV_TH               (SHARK_FM_REG_BASE + 0x0064)
#define	FM_REG_AGC_ADPS0                (SHARK_FM_REG_BASE + 0x0068)
#define	FM_REG_AGC_ADPS1                (SHARK_FM_REG_BASE + 0x006C)
#define	FM_REG_PDP_TH                   (SHARK_FM_REG_BASE + 0x0070)
#define	FM_REG_PDP_DEV                  (SHARK_FM_REG_BASE + 0x0074)
#define	FM_REG_ADP_ST_CONF              (SHARK_FM_REG_BASE + 0x0078)
#define	FM_REG_ADP_LPF_CONF             (SHARK_FM_REG_BASE + 0x007C)
#define	FM_REG_DEPHA_SCAL               (SHARK_FM_REG_BASE + 0x0080)
#define	FM_REG_HW_MUTE		 	(SHARK_FM_REG_BASE + 0x0084)
#define	FM_REG_SW_MUTE		 	(SHARK_FM_REG_BASE + 0x0088)
#define	FM_REG_UPD_CTRL		 	(SHARK_FM_REG_BASE + 0x008C)
#define	FM_REG_AUD_BLD0			(SHARK_FM_REG_BASE + 0x0090)
#define	FM_REG_AUD_BLD1		 	(SHARK_FM_REG_BASE + 0x0094)
#define	FM_REG_AGC_HYS		 	(SHARK_FM_REG_BASE + 0x0098)
#define	FM_REG_MONO_PWR		 	(SHARK_FM_REG_BASE + 0x009C)
#define	FM_REG_RF_CFG_DLY    		(SHARK_FM_REG_BASE + 0x00A0)
#define	FM_REG_AGC_TBL_STS	 	(SHARK_FM_REG_BASE + 0x00A4)
#define	FM_REG_ADP_BIT_SFT	 	(SHARK_FM_REG_BASE + 0x00A8)
#define	FM_REG_SEEK_CNT		 	(SHARK_FM_REG_BASE + 0x00AC)
#define	FM_REG_RSSI_STS		 	(SHARK_FM_REG_BASE + 0x00B0)
#define	FM_REG_CHAN_FREQ_STS	 	(SHARK_FM_REG_BASE + 0x00B8)
#define	FM_REG_FREQ_OFF_STS	     	(SHARK_FM_REG_BASE + 0x00BC)
#define	FM_REG_INPWR_STS	     	(SHARK_FM_REG_BASE + 0x00C0)
#define	FM_REG_RF_RSSI_STS	     	(SHARK_FM_REG_BASE + 0x00C4)
#define	FM_REG_NO_LPF_STS	     	(SHARK_FM_REG_BASE + 0x00C8)
#define	FM_REG_SMUTE_STS	     	(SHARK_FM_REG_BASE + 0x00CC)
#define	FM_REG_WBRSSI_STS	     	(SHARK_FM_REG_BASE + 0x00D0)
#define	FM_REG_AGC_BITS_TBL0	 	(SHARK_FM_REG_BASE + 0x0100)
#define	FM_REG_AGC_BITS_TBL1	 	(SHARK_FM_REG_BASE + 0x0104)
#define	FM_REG_AGC_BITS_TBL2	 	(SHARK_FM_REG_BASE + 0x0108)
#define	FM_REG_AGC_BITS_TBL3	 	(SHARK_FM_REG_BASE + 0x010C)
#define	FM_REG_AGC_BITS_TBL4	 	(SHARK_FM_REG_BASE + 0x0110)
#define	FM_REG_AGC_BITS_TBL5	 	(SHARK_FM_REG_BASE + 0x0114)
#define	FM_REG_AGC_BITS_TBL6	 	(SHARK_FM_REG_BASE + 0x0118)
#define	FM_REG_AGC_BITS_TBL7	 	(SHARK_FM_REG_BASE + 0x011C)
#define	FM_REG_AGC_BITS_TBL8	 	(SHARK_FM_REG_BASE + 0x0120)
#define	FM_REG_AGC_BITS_TBL9	 	(SHARK_FM_REG_BASE + 0x0124)
#define	FM_REG_AGC_BITS_TBL10	 	(SHARK_FM_REG_BASE + 0x0128)
#define	FM_REG_AGC_BITS_TBL11	 	(SHARK_FM_REG_BASE + 0x012C)
#define	FM_REG_AGC_BITS_TBL12	 	(SHARK_FM_REG_BASE + 0x0130)
#define	FM_REG_AGC_BITS_TBL13	 	(SHARK_FM_REG_BASE + 0x0134)
#define	FM_REG_AGC_BITS_TBL14	 	(SHARK_FM_REG_BASE + 0x0138)
#define	FM_REG_AGC_BITS_TBL15	 	(SHARK_FM_REG_BASE + 0x013C)
#define	FM_REG_AGC_BITS_TBL16	 	(SHARK_FM_REG_BASE + 0x0140)
#define	FM_REG_AGC_BITS_TBL17	 	(SHARK_FM_REG_BASE + 0x0144)
#define	FM_REG_AGC_VAL_TBL0		(SHARK_FM_REG_BASE + 0x0200)
#define	FM_REG_AGC_VAL_TBL1		(SHARK_FM_REG_BASE + 0x0204)
#define	FM_REG_AGC_VAL_TBL2		(SHARK_FM_REG_BASE + 0x0208)
#define	FM_REG_AGC_VAL_TBL3		(SHARK_FM_REG_BASE + 0x020C)
#define	FM_REG_AGC_VAL_TBL4		(SHARK_FM_REG_BASE + 0x0210)
#define	FM_REG_AGC_VAL_TBL5		(SHARK_FM_REG_BASE + 0x0214)
#define	FM_REG_AGC_VAL_TBL6		(SHARK_FM_REG_BASE + 0x0218)
#define	FM_REG_AGC_VAL_TBL7		(SHARK_FM_REG_BASE + 0x021C)
#define	FM_REG_AGC_VAL_TBL8		(SHARK_FM_REG_BASE + 0x0220)
#define	FM_REG_AGC_VAL_TBL9		(SHARK_FM_REG_BASE + 0x0224)
#define	FM_REG_AGC_VAL_TBL10		(SHARK_FM_REG_BASE + 0x0228)
#define	FM_REG_AGC_VAL_TBL11	 	(SHARK_FM_REG_BASE + 0x022C)
#define	FM_REG_AGC_VAL_TBL12	 	(SHARK_FM_REG_BASE + 0x0230)
#define	FM_REG_AGC_VAL_TBL13	 	(SHARK_FM_REG_BASE + 0x0234)
#define	FM_REG_AGC_VAL_TBL14	 	(SHARK_FM_REG_BASE + 0x0238)
#define	FM_REG_AGC_VAL_TBL15	 	(SHARK_FM_REG_BASE + 0x023C)
#define	FM_REG_AGC_VAL_TBL16	 	(SHARK_FM_REG_BASE + 0x0240)
#define	FM_REG_AGC_VAL_TBL17	 	(SHARK_FM_REG_BASE + 0x0244)
#define	FM_REG_AGC_BOND_TBL0	 	(SHARK_FM_REG_BASE + 0x0300)
#define	FM_REG_AGC_BOND_TBL1	 	(SHARK_FM_REG_BASE + 0x0304)
#define	FM_REG_AGC_BOND_TBL2	 	(SHARK_FM_REG_BASE + 0x0308)
#define	FM_REG_AGC_BOND_TBL3	 	(SHARK_FM_REG_BASE + 0x030C)
#define	FM_REG_AGC_BOND_TBL4	 	(SHARK_FM_REG_BASE + 0x0310)
#define	FM_REG_AGC_BOND_TBL5	 	(SHARK_FM_REG_BASE + 0x0314)
#define	FM_REG_AGC_BOND_TBL6	 	(SHARK_FM_REG_BASE + 0x0318)
#define	FM_REG_AGC_BOND_TBL7	 	(SHARK_FM_REG_BASE + 0x031C)
#define	FM_REG_AGC_BOND_TBL8	 	(SHARK_FM_REG_BASE + 0x0320)
#define	FM_REG_AGC_BOND_TBL9	 	(SHARK_FM_REG_BASE + 0x0324)

#define FM_REG_SPUR_FREQ0       	(SHARK_FM_REG_BASE + 0x340) /* new reg*/
#define FM_REG_SPUR_FREQ1       	(SHARK_FM_REG_BASE + 0X344) /* new reg*/

#define FM_REG_SPUR_RM_CTL       		(SHARK_FM_REG_BASE + 0X348)
#define FM_SPUR_RM_CTL_DEFAULT_VALUE	(0X88FFFF)
#define FM_SPUR_RM_CTL_SET_VALUE		(0X551F1F)
#define FM_REG_SPUR_DC_RM_CTL       	(SHARK_FM_REG_BASE + 0X34C)
#define FM_SPUR_DC_RM_CTL_DEFAULT_VALUE	(0X10FFFF)
#define FM_SPUR_DC_RM_CTL_SET_VALUE		(0XA03FF)


#define	FM_REG_AGC_DB_TBL_BEGIN	 	(SHARK_FM_REG_BASE + 0x0500)
#define FM_REG_AGC_DB_TBL_END	 	(SHARK_FM_REG_BASE + 0x05A0)
#define FM_REG_AGC_DB_TBL_CNT	 \
	((0x5a0-0x500)/4 + 1)

#define	FM_REG_SPI_CTL			(SHARK_FM_REG_BASE + 0x0810)
#define	FM_REG_SPI_WD0			(SHARK_FM_REG_BASE + 0x0814)
#define FM_REG_SPI_WD1			(SHARK_FM_REG_BASE + 0x0818)
#define FM_REG_SPI_RD			(SHARK_FM_REG_BASE + 0x081C)
#define FM_REG_SPI_FIFO_STS		(SHARK_FM_REG_BASE + 0x0820)

#define SHARK_APB_EB0                  REG_AON_APB_APB_EB0 
#define SHARK_APB_EB1                  REG_AON_APB_APB_EB1
#define SHARK_APB_RST0                 REG_AON_APB_APB_RST0
#define SHARK_APB_EB0_SET              (REG_AON_APB_APB_EB0+0x1000)

#define  SHARK_PMU_SLEEP_CTRL           REG_PMU_APB_SLEEP_CTRL

#define SHARK_PMU_APB_MEM_PD_CFG0		REG_PMU_APB_MEM_PD_CFG0

#if defined(CONFIG_ARCH_SCX30G)
#define SHARK_MSPI_CLK_SWITCH       (SHARK_AON_CLK_BASE_ADDR + 0x0054)
#elif defined(CONFIG_ARCH_SCX15) || defined(CONFIG_ARCH_SCX35)
#define SHARK_MSPI_CLK_SWITCH       (SHARK_AON_CLK_BASE_ADDR + 0x0050)
#endif

#define  SHARK_PMU_APB_PD_AP_SYS_CFG    REG_PMU_APB_PD_AP_SYS_CFG

#define INT_NUM_FM_test 27

#define SHARK_CHIP_BD   1

#define SHARK_PIN_U0TXD			(SHARK_PIN_BASE + 0x0048)
#define SHARK_PIN_U0RXD			(SHARK_PIN_BASE + 0x004c)
#define SHARK_PIN_U0CTS			(SHARK_PIN_BASE + 0x0050)	


struct shark_fm_info_t {
	int		int_happen;
	int		seek_success;
	u32		freq_set;
	u32		freq_seek;
	u32		freq_offset;
	u32		rssi;
	u32		rf_rssi;
	u32		seek_cnt;
	u32		inpwr_sts;
	u32		fm_sts;
	u32      agc_sts;
    /*void	(*irq_handler)(u32);*/
};


struct shark_reg_cfg_t {
	unsigned int reg_addr;
	unsigned int reg_data;
};

/*FM Module End*/

/*2351 RF Start*/

#define	FM_SR2351_ADC_CLK            0x06F  
#define	FM_SR2351_DCOC_CAL_TIMER     0x471
#define	FM_SR2351_RC_TUNER           0x477
#define	FM_SR2351_RX_ADC_CLK         0x478
#define	FM_SR2351_OVERLOAD_DET       0x431
#define	FM_SR2351_RX_GAIN            0x404
#define	FM_SR2351_MODE               0x400
#define	FM_SR2351_FREQ               0x402

/*2351 RF End*/

#define SR2351_PRINT(format, arg...)  \
	do { \
		printk("sr2351_fm %s-%d -- "format"\n", \
		__func__, __LINE__, ## arg); \
	} while (0)
	
#endif
