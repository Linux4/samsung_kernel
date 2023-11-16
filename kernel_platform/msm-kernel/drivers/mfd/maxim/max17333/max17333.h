#ifndef __MAX17333_MFD_H__
#define __MAX17333_MFD_H__

#define MAX17333_RELEASE_VERSION	"2.0.17"

/* For Use Status Alert instead of Fuel Gauge */
//#define USE_MAX17333_FUELGAUGE

/* Useful Macros */
#undef  __CONST_FFS
#define __CONST_FFS(_x) ((_x) & 0x00FF ?\
			((_x) & 0x000F ? ((_x) & 0x0003 ? ((_x) & 0x0001 ? 0 : 1) :\
						((_x) & 0x0004 ? 2 : 3)) :\
					((_x) & 0x0030 ? ((_x) & 0x0010 ? 4 : 5) :\
						((_x) & 0x0040 ? 6 : 7))) :\
			((_x) & 0x0F00 ? ((_x) & 0x0300 ? ((_x) & 0x0100 ? 8 : 9) :\
						((_x) & 0x0400 ? 10 : 11)) :\
					((_x) & 0x3000 ? ((_x) & 0x1000 ? 12 : 13) :\
						((_x) & 0x4000 ? 14 : 15))))


#undef  FFS
#define FFS(_x) \
		((_x) ? __CONST_FFS(_x) : 0)

#undef  BIT_RSVD
#define BIT_RSVD  0

#undef  BITS
#define BITS(_end, _start) \
	((BIT(_end) - BIT(_start)) + BIT(_end))

#undef  __BITS_GET
#define __BITS_GET(_word, _mask, _shift) \
	(((_word) & (_mask)) >> (_shift))

#undef  BITS_GET
#define BITS_GET(_word, _bit) \
	__BITS_GET(_word, _bit, FFS(_bit))

#undef  __BITS_SET
#define __BITS_SET(_word, _mask, _shift, _val) \
	(((_word) & ~(_mask)) | (((_val) << (_shift)) & (_mask)))

#undef  BITS_SET
#define BITS_SET(_word, _bit, _val) \
	__BITS_SET(_word, _bit, FFS(_bit), _val)

#undef  BITS_MATCH
#define BITS_MATCH(_word, _bit) \
	(((_word) & (_bit)) == (_bit))

#undef  BITS_VALUE
#define BITS_VALUE(_v, _r) \
	((_v >> (u16)FFS(_r)) & (_r >> (u16)FFS(_r)))


/* MAX17333  Top Devices */
#define MAX17333_NAME                 "max17333"

/* MAX17333 Devices */
#define MAX17333_CHARGER_NAME         MAX17333_NAME"-charger"
#define MAX17333_BATTERY_NAME         MAX17333_NAME"-battery"
#define MAX17333_CHARGER_SUB_NAME     MAX17333_NAME"-charger-sub"
#define MAX17333_BATTERY_SUB_NAME     MAX17333_NAME"-battery-sub"

/* MAX17333 ModelGauge Register */
#define REG_STATUS               0x00
#define REG_VALRTTH              0x01
#define REG_TALRTTH              0x02
#define REG_SALRTTH              0x03
#define REG_REPCAP               0x05
#define REG_REPSOC               0x06
#define REG_AGE                  0x07
#define REG_MAXMINVOLT           0x08
#define REG_CONFIG               0x0B
#define REG_AVSOC                0x0E
#define REG_MIXSOC               0x0D
#define REG_FULLCAPREP           0x10
#define REG_TTE                  0x11
#define REG_CYCLES               0x17
#define REG_DESIGNCAP            0x18
#define REG_AVGVCELL             0x19
#define REG_VCELL                0x1A
#define REG_TEMP                 0x1B
#define REG_CURRENT              0x1C
#define REG_AVGCURRENT           0x1D
#define REG_AVCAP                0x1F
#define REG_TTF                  0x20
#define REG_VERSION              0x21
#define REG_FULLCAPNOM           0x23
#define REG_CHARGING_CURRENT     0x28
#define REG_CHARGING_VOLTAGE     0x2A
#define REG_MIXCAP               0x2B
#define REG_FSTAT2               0x39
#define REG_VEMPTY               0x3A
#define REG_FSTAT                0x3D
#define REG_QH                   0x4D
#define REG_COMMAND              0x60
#define REG_COMMSTAT             0x61
#define REG_BATTPROFILE          0x80
#define REG_CHGSTAT              0xA3
#define REG_CONFIG2              0xAB
#define REG_IALRTTH              0xAC
#define REG_PROTALRTS            0xAF
#define REG_STATUS2              0xB0
#define REG_FOTPSTAT             0xBB
#define REG_AGEFORECAST          0xB9
#define REG_AVGCELL1             0xD4
#define REG_PROTSTATUS           0xD9
#define REG_FPROTSTAT            0xDA
#define REG_PCKP                 0xDB
#define REG_HPROTCFG             0xF0
#define REG_HPROTCFG2            0xF1
#define REG_VFOCV                0xFB

/* Hardware Registers */
#define REG_VG_TRIM              0xE8
#define REG_CG_TRIM              0xEA
#define REG_TG_TRIM              0xEC
#define REG_HCONFIG2             0xF5
#define REG_TEMP_RAW             0xF8
#define REG_VOLT_RAW             0xF9
#define REG_CURR_RAW             0xFA
#define REG_HCONFIG              0xFD

/* Shadow Ram Registers */
#define REG_I2CCMD_SHDW           0x2B
#define REG_NXTABLE0_SHDW         0x80
#define REG_NXTABLE1_SHDW         0x81
#define REG_NXTABLE2_SHDW         0x82
#define REG_NXTABLE3_SHDW         0x83
#define REG_NXTABLE4_SHDW         0x84
#define REG_NXTABLE5_SHDW         0x85
#define REG_NXTABLE6_SHDW         0x86
#define REG_NXTABLE7_SHDW         0x87
#define REG_NXTABLE8_SHDW         0x88
#define REG_NXTABLE9_SHDW         0x89
#define REG_NXTABLE10_SHDW        0x8A
#define REG_NXTABLE11_SHDW        0x8B
#define REG_NVALRTTH_SHDW         0x8C
#define REG_NTALRTTH_SHDW         0x8D
#define REG_NIALRTTH_SHDW         0x8E
#define REG_NSALRTTH_SHDW         0x8F
#define REG_NOCVTABLE0_SHDW       0x90
#define REG_NOCVTABLE1_SHDW       0x91
#define REG_NOCVTABLE2_SHDW       0x92
#define REG_NOCVTABLE3_SHDW       0x93
#define REG_NOCVTABLE4_SHDW       0x94
#define REG_NOCVTABLE5_SHDW       0x95
#define REG_NOCVTABLE6_SHDW       0x96
#define REG_NOCVTABLE7_SHDW       0x97
#define REG_NOCVTABLE8_SHDW       0x98
#define REG_NOCVTABLE9_SHDW       0x99
#define REG_NOCVTABLE10_SHDW      0x9A
#define REG_NOCVTABLE11_SHDW      0x9B
#define REG_NRSENSE_SHDW          0x9C
#define REG_NFILTERCFG_SHDW       0x9D
#define REG_NVEMPTY_SHDW          0x9E
#define REG_NLEARNCFG_SHDW        0x9F
#define REG_NQRTABLE00_SHDW       0xA0
#define REG_NQRTABLE10_SHDW       0xA1
#define REG_NQRTABLE20_SHDW       0xA2
#define REG_NQRTABLE30_SHDW       0xA3
#define REG_NCYCLES_SHDW          0xA4
#define REG_NFULLCAPNOM_SHDW      0xA5
#define REG_NRCOMP0_SHDW          0xA6
#define REG_NTEMPCO_SHDW          0xA7
#define REG_NBATTSTATUS_SHDW      0xA8
#define REG_NFULLCAPREP_SHDW      0xA9
#define REG_NVOLTTEMP_SHDW        0xAA
#define REG_NMAXMINCURR_SHDW      0xAB
#define REG_NMAXMINVOLT_SHDW      0xAC
#define REG_NMAXMINTEMP_SHDW      0xAD
#define REG_NFAULTLOG_SHDW        0xAE
#define REG_NTIMERH_SHDW          0xAF
#define REG_NCONFIG_SHDW          0xB0
#define REG_NRIPPLECFG_SHDW       0xB1
#define REG_NMISCCFG_SHDW         0xB2
#define REG_NDESIGNCAP_SHDW       0xB3
#define REG_NI2CCFG_SHDW          0xB4
#define REG_NFULLCFG_SHDW         0xB5
#define REG_NRELAXCFG_SHDW        0xB6
#define REG_NCONVGCFG_SHDW        0xB7
#define REG_NRGAIN_SHDW           0xB8
#define REG_NAGECHGCFG_SHDW       0xB9
#define REG_NTTFCFG_SHDW          0xBA
#define REG_NHIBCFG_SHDW          0xBB
#define REG_NROMID0_SHDW          0xBC
#define REG_NROMID1_SHDW          0xBD
#define REG_NROMID2_SHDW          0xBE
#define REG_NROMID3_SHDW          0xBF
#define REG_NCHGCTRL1_SHDW        0xC0
#define REG_NICHGTERM_SHDW        0xC1
#define REG_NCHGCFG0_SHDW         0xC2
#define REG_NCHGCTRL0_SHDW        0xC3
#define REG_NSTEPCURR_SHDW        0xC4
#define REG_NSTEPVOLT_SHDW        0xC5
#define REG_NCHGCTRL2_SHDW        0xC6
#define REG_NPACKCFG_SHDW         0xC7
#define REG_NCGAIN_SHDW           0xC8
#define REG_NADCCFG_SHDW          0xC9
#define REG_NTHERMCFG_SHDW        0xCA
#define REG_NCHGCFG1_SHDW         0xCB
#define REG_NVCHGCFG1_SHDW        0xCC
#define REG_NVCHGCFG2_SHDW        0xCD
#define REG_NICHGCFG1_SHDW        0xCE
#define REG_NICHGCFG2_SHDW        0xCF
#define REG_NUVPRTTH_SHDW         0xD0
#define REG_NTPRTTH1_SHDW         0xD1
#define REG_NTPRTTH3_SHDW         0xD2
#define REG_NIPRTTH1_SHDW         0xD3
#define REG_NIPRTTH2_SHDW         0xD4
#define REG_NTPRTTH2_SHDW         0xD5
#define REG_NPROTMISCTH_SHDW      0xD6
#define REG_NPROTCFG_SHDW         0xD7
#define REG_NNVCFG0_SHDW          0xD8
#define REG_NNVCFG1_SHDW          0xD9
#define REG_NOVPRTTH_SHDW         0xDA
#define REG_NNVCFG2_SHDW          0xDB
#define REG_NDELAYCFG_SHDW        0xDC
#define REG_NODSCTH_SHDW          0xDD
#define REG_NODSCCFG_SHDW         0xDE
#define REG_NPROTCFG2_SHDW        0xDF
#define REG_NDPLIMIT_SHDW         0xE0
#define REG_NSCOCVLIM_SHDW        0xE1
#define REG_NAGEFCCFG_SHDW        0xE2
#define REG_NDESIGNVOLTAGE_SHDW   0xE3
#define REG_NCHGCFG2_SHDW         0xE4
#define REG_NMANFCTRDATE_SHDW     0xE6
#define REG_NFIRSTUSED_SHDW       0xE7
#define REG_NSERIALNUMBER0_SHDW   0xE8
#define REG_NSERIALNUMBER1_SHDW   0xE9
#define REG_NSERIALNUMBER2_SHDW   0xEA
#define REG_NDEVICENAME0_SHDW     0xEB
#define REG_NDEVICENAME1_SHDW     0xEC
#define REG_NMANFCTRNAME0_SHDW    0xED
#define REG_NMANFCTRNAME1_SHDW    0xEE
#define REG_NMANFCTRNAME2_SHDW    0xEF
#define REG_VERSION_SS_SHDW       REG_NMANFCTRNAME2_SHDW

#define REG_PROTALRTS_MASK	   0xFF7C
#define REG_CHGSTAT_MASK       0x7FF3

/* PermFal bit for MAX17333 */
#define BIT_NBATTSTATUS_PERMFAIL BIT(15)
#define BIT_PROTSTATUS_PERMFAIL BIT(6)
#define BIT_NBATTSTATUS_PERMFAIL_MASK (0x7FFF)
#define BIT_PROTSTATUS_PERMFAIL_MASK (0xFFBF)

/* Config register bits for MAX17333 */
#define BIT_CONFIG_ALRT_EN		BIT(2)

/* nDesignCap register bits for MAX17333 */
#define MAX17333_DESIGNCAP_QSCALE BITS(2, 0)
#define MAX17333_DESIGNCAP_VSCALE BIT(3)
#define MAX17333_DESIGNCAP_DESIGNCAP BITS(15, 6)

/* nChgCfg0 register bits for MAX17333 */
#define MAX17333_CHGCFG0_PRECHGCURR BITS(5, 0)
#define MAX17333_CHGCFG0_PREQUALVOLT BITS(13, 9)

/* ProtStatus register bits for MAX17333 */
#define BIT_SHDN_INT          BIT(0)
#define BIT_TOOCOLDD_INT      BIT(1)
#define BIT_ODCP_INT          BIT(2)
#define BIT_UVP_INT           BIT(3)
#define BIT_TOOHOTD_INT       BIT(4)
#define BIT_DIEHOT_INT        BIT(5)
#define BIT_PERMFAIL_INT      BIT(6)
#define BIT_PREQF_INT         BIT(8)
#define BIT_QOVFLW_INT        BIT(9)
#define BIT_OCCP_INT          BIT(10)
#define BIT_OVP_INT           BIT(11)
#define BIT_TOOCOLDC_INT      BIT(12)
#define BIT_FULL_INT          BIT(13)
#define BIT_TOOHOTC_INT       BIT(14)
#define BIT_CHGWDT_INT        BIT(15)

/* Status register bits for MAX17333 */
#define BIT_STATUS_PA         BIT(15)
#define BIT_STATUS_SMX        BIT(14)
#define BIT_STATUS_TMX        BIT(13)
#define BIT_STATUS_VMX        BIT(12)
#define BIT_STATUS_CA         BIT(11)
#define BIT_STATUS_SMN        BIT(10)
#define BIT_STATUS_TMN        BIT(9)
#define BIT_STATUS_VMN        BIT(8)
#define BIT_STATUS_DSOCI      BIT(7)
#define BIT_STATUS_IMX        BIT(6)
#define BIT_STATUS_ALLOWCHGB  BIT(5)
#define BIT_STATUS_SHIPRDY    BIT(4)
#define BIT_STATUS_BST        BIT(3)
#define BIT_STATUS_IMN        BIT(2)
#define BIT_STATUS_POR        BIT(1)

#define REG_STATUS_MASK       0x807F

#define BIT_CLEAR             (0)

/*ChgStat register bits for MAX17333 */
#define BIT_STATUS_DROPOUT    BIT(15)
#define BIT_STATUS_CP         BIT(3)
#define BIT_STATUS_CT         BIT(2)
#define BIT_STATUS_CC         BIT(1)
#define BIT_STATUS_CV         BIT(0)

/* CommStat register bits for MAX17333 */
#define MAX17333_COMMSTAT_CHGOFF_POS 8
#define MAX17333_COMMSTAT_CHGOFF BIT(8)

/* nProtCfg register bits for MAX17333 */
#define MAX17333_NPROTCFG_DEEPSHPEN BIT(5)
#define MAX17333_NPROTCFG_DEEPSHP2EN BIT(0)

/* Config2 register bits for MAX17333 */
#define MAX17333_CONFIG2_POR_CMD (0x8000)

/* Function Commands */
#define MAX17333_COMM_FULL_RESET (0x000F)

/* nPackCfg register bits for MAX1733 2*/
#define MAX17333_NPACKCFG_PAREN BIT(6)

/* Config register bits for MAX17333 */
#define MAX17333_CONFIG_MANCHG BIT(15)
#define MAX17333_CONFIG_SHIP BIT(7)
#define MAX17333_CONFIG_COMMSH BIT(6)

/* ProStat register bits for MAX17333 */
#define MAX17333_PROTSTATUS_ALL_CLEAR (0x0000)

/* nFullCfg register bits for MAX17333 */
#define MAX17333_NFULLCFG_BLOCKCHGEN BIT(14)

/* I2CCmd register bits for MAX17333 */
#define MAX17333_I2CCMD_CHGDONE  BIT(15)
#define MAX17333_I2CCMD_BLOCKCHG BIT(13)
#define MAX17333_I2CCMD_AUTOSHIP BIT(11)

/* nODSCTH register bits for MAX17333 */
#define MAX17333_NODSCTH_OCMARGIN  BITS(15, 13)
#define MAX17333_NODSCTH_OCMARGIN_SHIFT	(13)

/* Chip Interrupts */
enum {
	MAX17333_FG_POR_INT = 0,
	MAX17333_FG_IMN_INT,
	MAX17333_FG_BST_INT,
	MAX17333_FG_ALLOWCHGB_INT,
	MAX17333_FG_IMX_INT,
	MAX17333_FG_DSOCI_INT,
	MAX17333_FG_VMN_INT,
	MAX17333_FG_TMN_INT,
	MAX17333_FG_SMN_INT,
	MAX17333_FG_CA_INT,
	MAX17333_FG_VMX_INT,
	MAX17333_FG_TMX_INT,
	MAX17333_FG_SMX_INT,
	MAX17333_FG_PROT_INT,

	MAX17333_FG_PROT_INT_START,
	MAX17333_FG_PROT_INT_ODCP = MAX17333_FG_PROT_INT_START,
	MAX17333_FG_PROT_INT_UVP,
	MAX17333_FG_PROT_INT_TOOHOTD,
	MAX17333_FG_PROT_INT_DIEHOTD,
	MAX17333_FG_PROT_INT_PERMFAIL,
	MAX17333_FG_PROT_INT_QOVFLW,
	MAX17333_FG_PROT_INT_OCCP,
	MAX17333_FG_PROT_INT_OVP,
	MAX17333_FG_PROT_INT_TOOCOLDC,
	MAX17333_FG_PROT_INT_FULL,
	MAX17333_FG_PROT_INT_TOOHOTC,
	MAX17333_FG_PROT_INT_CHGWDT,

	MAX17333_NUM_OF_INTS,
};

/* Ship Mode Level */
enum {
	MAX17333_SHIP_MODE = 0,
	MAX17333_DEEPSHIP1_MODE,
	MAX17333_DEEPSHIP2_MODE,
};

struct max17333_shdw_data {
	unsigned int nVAlrtTh;
	unsigned int nTAlrtTh;
	unsigned int nIAlrtTh;
	unsigned int nSAlrtTh;
	unsigned int nRSense;
	unsigned int nFilterCfg;
	unsigned int nVEmpty;
	unsigned int nLearnCfg;
	unsigned int nQRTable00;
	unsigned int nQRTable10;
	unsigned int nQRTable20;
	unsigned int nQRTable30;
	unsigned int nCycles;
	unsigned int nFullCapNom;
	unsigned int nRComp0;
	unsigned int nTempCo;
	unsigned int nBattStatus;
	unsigned int nFullCapRep;
	unsigned int nVoltTemp;
	unsigned int nMaxMinCurr;
	unsigned int nMaxMinVolt;
	unsigned int nMaxMinTemp;
	unsigned int nFaultLog;
	unsigned int nTimerH;
	unsigned int nConfig;
	unsigned int nRippleCfg;
	unsigned int nMiscCfg;
	unsigned int nDesignCap;
	unsigned int nI2CCfg;
	unsigned int nFullCfg;
	unsigned int nRelaxCfg;
	unsigned int nConvgCfg;
	unsigned int nRGain;
	unsigned int nAgeChgCfg;
	unsigned int nTTFCfg;
	unsigned int nHibCfg;
	unsigned int nChgCtrl1;
	unsigned int nIChgTerm;
	unsigned int nChgCfg0;
	unsigned int nChgCtrl0;
	unsigned int nStepCurr;
	unsigned int nStepVolt;
	unsigned int nChgCtrl2;
	unsigned int nPackCfg;
	unsigned int nCGain;
	unsigned int nADCCfg;
	unsigned int nThermCfg;
	unsigned int nChgCfg1;
	unsigned int nVChgCfg1;
	unsigned int nVChgCfg2;
	unsigned int nIChgCfg1;
	unsigned int nIChgCfg2;
	unsigned int nUVPrtTh;
	unsigned int nTPrtTh1;
	unsigned int nTPrtTh3;
	unsigned int nIPrtTh1;
	unsigned int nIPrtTh2;
	unsigned int nTPrtTh2;
	unsigned int nProtMiscTh;
	unsigned int nProtCfg;
	unsigned int nNVCfg0;
	unsigned int nNVCfg1;
	unsigned int nOVPrtTh;
	unsigned int nNVCfg2;
	unsigned int nDelayCfg;
	unsigned int nODSCTh;
	unsigned int nODSCCfg;
	unsigned int nProtCfg2;
	unsigned int nDPLimit;
	unsigned int nScOcvLim;
	unsigned int nAgeFcCfg;
	unsigned int nDesignVoltage;
	unsigned int nChgCfg2;
	unsigned int nRFastVShdn;
	unsigned int nManfctrDate;
	unsigned int nFirstUsed;
	unsigned int nSerialNumber[3];
	unsigned int nDeviceName[2];
	unsigned int nManfctrName[3];
	unsigned int nROMID[4];
	unsigned int nXTable[12];
	unsigned int nOCVTable[12];
	unsigned int battProfile[32];
};

struct max17333_dev {
	struct mutex				lock;
	struct device				*dev;

	int					irq;
	int					irq_gpio;

	struct regmap_irq_chip_data	*irqc_intsrc;

	struct i2c_client	*pmic;			/* 0x6C, MODEL GAUGE */
	struct i2c_client	*shdw;			/* 0x16, SHADOW */

	struct regmap		*regmap_pmic;			/* MODEL GAUGE */
	struct regmap		*regmap_shdw;			/* SHADOW */

	struct max17333_pmic_platform_data  *pdata;
};

/*******************************************************************************
 * Platform Data
 ******************************************************************************/

struct max17333_pmic_platform_data {
	int irq; /* system interrupt number for PMIC */
	unsigned int rsense;
	unsigned int battery_type;
	bool check_shdw_update;

	struct max17333_shdw_data shdw_data;
};

/*******************************************************************************
 * Chip IO
 ******************************************************************************/
int max17333_read(struct regmap *regmap, u8 addr, u16 *val);
int max17333_write(struct regmap *regmap, u8 addr, u16 val);
int max17333_update_bits(struct regmap *regmap, u8 addr, u16 mask, u16 val);

#endif /* !__MAX17333_MFD_H__ */

