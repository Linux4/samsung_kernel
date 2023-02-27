#define CAPACITY_MAX			1000
#define CAPACITY_MAX_MARGIN     30
#define CAPACITY_MIN			0

static sec_bat_adc_table_data_t temp_table[] = {
	{694 ,900},
	{679 ,850},
	{661 ,800},
	{642 ,750},
	{618 ,700},
	{594 ,650},
	{565 ,600},
	{532 ,550},
	{499 ,500},
	{455 ,450},
	{414 ,400},
	{366 ,350},
	{318 ,300},
	{262 ,250},
	{207 ,200},
	{156 ,150},
	{95  ,100},
	{45  ,50},
	{-5  ,0},
	{-51 ,-50 },
	{-94 ,-100},
	{-133,-150},
	{-164,-200},
};

#define TEMP_HIGH_THRESHOLD_EVENT  500
#define TEMP_HIGH_RECOVERY_EVENT   450
#define TEMP_LOW_THRESHOLD_EVENT   0
#define TEMP_LOW_RECOVERY_EVENT    50
#define TEMP_HIGH_THRESHOLD_NORMAL 500
#define TEMP_HIGH_RECOVERY_NORMAL  450
#define TEMP_LOW_THRESHOLD_NORMAL  0
#define TEMP_LOW_RECOVERY_NORMAL   50
#define TEMP_HIGH_THRESHOLD_LPM    500
#define TEMP_HIGH_RECOVERY_LPM     450
#define TEMP_LOW_THRESHOLD_LPM     0
#define TEMP_LOW_RECOVERY_LPM      50
