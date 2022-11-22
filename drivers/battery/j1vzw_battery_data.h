#define CAPACITY_MAX			980
#define CAPACITY_MAX_MARGIN     50
#define CAPACITY_MIN			-7


int Temperature_fn(void)
{
	return 0;
}

static struct battery_data_t stc3117_battery_data[] = {
	{
		.Vmode= 0,		 /*REG_MODE, BIT_VMODE 1=Voltage mode, 0=mixed mode */
		.Alm_SOC = 1,		/* SOC alm level %*/
		.Alm_Vbat = 3400,	/* Vbat alm level mV*/
		.CC_cnf = 823,		/* nominal CC_cnf, coming from battery characterisation*/
		.VM_cnf = 632,		/* nominal VM cnf , coming from battery characterisation*/
		.Rint = 155,		/* nominal internal impedance*/
		.Cnom = 4000,		/* nominal capacity in mAh, coming from battery characterisation*/
		.Rsense = 10,		/* sense resistor mOhms*/
		.RelaxCurrent = 150, /* current for relaxation in mA (< C/20) */
		.Adaptive = 1,	   /* 1=Adaptive mode enabled, 0=Adaptive mode disabled */

		.CapDerating[6] = 600,				/* capacity derating in 0.1%, for temp = -20C */
		.CapDerating[5] = 300,				/* capacity derating in 0.1%, for temp = -10C */
		.CapDerating[4] = 80,			   /* capacity derating in 0.1%, for temp = 0C */
		.CapDerating[3] = 20,			   /* capacity derating in 0.1%, for temp = 10C */
		.CapDerating[2] = 0,			  /* capacity derating in 0.1%, for temp = 25C */
		.CapDerating[1] = 0,			  /* capacity derating in 0.1%, for temp = 40C */
		.CapDerating[0] = 0,			  /* capacity derating in 0.1%, for temp = 60C */

		.OCVValue[15] = 4312,			 /* OCV curve adjustment */
		.OCVValue[14] = 4187,			 /* OCV curve adjustment */
		.OCVValue[13] = 4089,			 /* OCV curve adjustment */
		.OCVValue[12] = 3990,			 /* OCV curve adjustment */
		.OCVValue[11] = 3959,			 /* OCV curve adjustment */
		.OCVValue[10] = 3906,			 /* OCV curve adjustment */
		.OCVValue[9] = 3839,			 /* OCV curve adjustment */
		.OCVValue[8] = 3801,			 /* OCV curve adjustment */
		.OCVValue[7] = 3772,			 /* OCV curve adjustment */
		.OCVValue[6] = 3756,			 /* OCV curve adjustment */
		.OCVValue[5] = 3738,			 /* OCV curve adjustment */
		.OCVValue[4] = 3707,			 /* OCV curve adjustment */
		.OCVValue[3] = 3685,			 /* OCV curve adjustment */
		.OCVValue[2] = 3681,			 /* OCV curve adjustment */
		.OCVValue[1] = 3629,			 /* OCV curve adjustment */
		.OCVValue[0] = 3400,			 /* OCV curve adjustment */

		/*if the application temperature data is preferred than the STC3117 temperature*/
		.ExternalTemperature = Temperature_fn, /*External temperature fonction, return C*/
		.ForceExternalTemperature = 0, /* 1=External temperature, 0=STC3117 temperature */
	}
};


static sec_bat_adc_table_data_t temp_table[] = {
	{25950, 900},
	{26173, 850},
	{26520, 800},
	{26790, 750},
	{27158, 700},
	{27590, 650},
	{28190, 600},
	{28607, 550},
	{29275, 500},
	{30036, 450},
	{30960, 400},
	{31847, 350},
	{32855, 300},
	{33889, 250},
	{34991, 200},
	{36085, 150},
	{37117, 100},
	{38123, 50},
	{38971, 0},
	{39794, -50},
	{40462, -100},
	{41064, -150},
	{41548, -200},
};

#define TEMP_HIGH_THRESHOLD_EVENT  600
#define TEMP_HIGH_RECOVERY_EVENT   460
#define TEMP_LOW_THRESHOLD_EVENT   (-50)
#define TEMP_LOW_RECOVERY_EVENT    0
#define TEMP_HIGH_THRESHOLD_NORMAL 600
#define TEMP_HIGH_RECOVERY_NORMAL  460
#define TEMP_LOW_THRESHOLD_NORMAL  (-50)
#define TEMP_LOW_RECOVERY_NORMAL   0
#define TEMP_HIGH_THRESHOLD_LPM    600
#define TEMP_HIGH_RECOVERY_LPM     460
#define TEMP_LOW_THRESHOLD_LPM     (-50)
#define TEMP_LOW_RECOVERY_LPM      0

