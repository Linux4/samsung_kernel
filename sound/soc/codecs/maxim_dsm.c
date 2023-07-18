#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/time.h>
#include <sound/max98505.h>
#include "max98505.h"
#include "maxim_dsm.h"

static DEFINE_MUTEX(dsm_lock);

/* Q6 ADSP */
extern int32_t dsm_open(int32_t port_id,uint32_t*  dsm_params, u8* param);

enum	{
	param_voice_coil_temp=0,	// 0
	param_voice_coil_temp_sz=1,	// 1
	param_excursion=2,			// 2
	param_excursion_sz=3,			// 3
	param_rdc=4,					// 4
	param_rdc_sz=5,				// 5
	param_q=6,					// 6
	param_q_sz=7,					// 7
	param_fres=8,					// 8
	param_fres_sz=9,				// 9
	param_excur_limit=10,			// 10
	param_excur_limit_sz=11,		// 11
	param_voice_coil=12,			// 12
	param_voice_coil_sz=13,		// 13
	param_thermal_limit=14,		// 14
	param_thermal_limit_sz=15,		// 15
	param_release_time=16, 		// 16
	param_release_time_sz=17,		// 17
	param_onoff=18,				// 18
	param_onoff_sz=19,				// 19
	param_static_gain=20,			// 20
	param_static_gain_sz=21,		// 21
	param_lfx_gain=22,				// 22
	param_lfx_gain_sz=23,			// 23
	param_pilot_gain=24,			// 24
	param_pilot_gain_sz=25,		// 25
	param_wirte_flag=26,			// 26
	param_wirte_flag_sz=27,		// 27
	param_max,
};

static int param_dsm[param_max];
static void dsm_dump_func(int onoff);
static uint32_t exSeqCountTemp=0;
static uint32_t exSeqCountExcur=0;
static uint32_t newLogAvail=0;

static void set_dsm_onoff_status(int onoff);
extern int get_dsm_onoff_status(void);

#define LOG_BUFFER_ARRAY_SIZE 10
#define BEFORE_BUFSIZE 4+(LOG_BUFFER_ARRAY_SIZE*2)
#define AFTER_BUFSIZE LOG_BUFFER_ARRAY_SIZE*4

static DEFINE_MUTEX(maxim_dsm_log_lock);
static bool maxim_dsm_log_present = false;
static struct tm maxim_dsm_log_timestamp;
static uint8_t maxim_dsm_byteLogArray[BEFORE_BUFSIZE];
static uint32_t maxim_dsm_intLogArray[BEFORE_BUFSIZE];
static uint8_t maxim_dsm_afterProbByteLogArray[AFTER_BUFSIZE];
static uint32_t maxim_dsm_afterProbIntLogArray[AFTER_BUFSIZE];

void maxim_dsm_log_update(const void *byteLogArray, const void *intLogArray, const void *afterProbByteLogArray, const void *afterProbIntLogArray)
{

	struct timeval tv;
	
	mutex_lock(&maxim_dsm_log_lock);

	memcpy(maxim_dsm_byteLogArray, byteLogArray, sizeof(maxim_dsm_byteLogArray));
	memcpy(maxim_dsm_intLogArray, intLogArray, sizeof(maxim_dsm_intLogArray));

	memcpy(maxim_dsm_afterProbByteLogArray, afterProbByteLogArray, sizeof(maxim_dsm_afterProbByteLogArray));
	memcpy(maxim_dsm_afterProbIntLogArray, afterProbIntLogArray, sizeof(maxim_dsm_afterProbIntLogArray));

	do_gettimeofday(&tv);
	time_to_tm(tv.tv_sec, 0, &maxim_dsm_log_timestamp);

	maxim_dsm_log_present = true;

	mutex_unlock(&maxim_dsm_log_lock);
}

static void maxim_dsm_log_free (void **byteLogArray, void **intLogArray, void **afterbyteLogArray, void **afterintLogArray)
{
	if (likely(*byteLogArray)) {
		kfree(*byteLogArray);
		*byteLogArray = NULL;
	}

	if (likely(*intLogArray)) {
		kfree(*intLogArray);
		*intLogArray = NULL;
	}

	if (likely(*afterbyteLogArray)) {
		kfree(*afterbyteLogArray);
		*afterbyteLogArray = NULL;
	}

	if (likely(*afterintLogArray)) {
		kfree(*afterintLogArray);
		*afterintLogArray = NULL;
	}
}

static int maxim_dsm_log_duplicate (void **byteLogArray, void **intLogArray, void **afterbyteLogArray, void **afterintLogArray)
{
	void *blog_buf = NULL, *ilog_buf = NULL, *after_blog_buf = NULL, *after_ilog_buf = NULL;
	int rc = 0;

	mutex_lock(&maxim_dsm_log_lock);

	if (unlikely(!maxim_dsm_log_present)) {
		rc = -ENODATA;
		goto abort;
	}

	blog_buf = kzalloc(sizeof(maxim_dsm_byteLogArray), GFP_KERNEL);
	ilog_buf = kzalloc(sizeof(maxim_dsm_intLogArray), GFP_KERNEL);
	after_blog_buf = kzalloc(sizeof(maxim_dsm_afterProbByteLogArray), GFP_KERNEL);
	after_ilog_buf = kzalloc(sizeof(maxim_dsm_afterProbIntLogArray), GFP_KERNEL);

	if (unlikely(!blog_buf || !ilog_buf || !after_blog_buf || !after_ilog_buf)) {
		rc = -ENOMEM;
		goto abort;
	}

	memcpy(blog_buf, maxim_dsm_byteLogArray, sizeof(maxim_dsm_byteLogArray));
	memcpy(ilog_buf, maxim_dsm_intLogArray, sizeof(maxim_dsm_intLogArray));
	memcpy(after_blog_buf, maxim_dsm_afterProbByteLogArray, sizeof(maxim_dsm_afterProbByteLogArray));
	memcpy(after_ilog_buf, maxim_dsm_afterProbIntLogArray, sizeof(maxim_dsm_afterProbIntLogArray));

	goto out;

abort:
	maxim_dsm_log_free(&blog_buf, &ilog_buf, &after_blog_buf, &after_ilog_buf);
out:
	*byteLogArray = blog_buf;
	*intLogArray  = ilog_buf;
	*afterbyteLogArray = after_blog_buf;
	*afterintLogArray  = after_ilog_buf;
	mutex_unlock(&maxim_dsm_log_lock);

	return rc;
}

static ssize_t maxim_dsm_log_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t *byteLogArray = NULL;
	uint32_t *intLogArray = NULL;
	uint8_t *afterbyteLogArray = NULL;
	uint32_t *afterintLogArray = NULL;
	int rc = 0;

	uint8_t logAvailable;
	uint8_t versionID;
	uint8_t *coilTempLogArray;
	uint8_t *exCurLogArray;
	uint8_t *AftercoilTempLogArray;
	uint8_t *AfterexCurLogArray;
	uint8_t *ExcurAftercoilTempLogArray;
	uint8_t *ExcurAfterexCurLogArray;

    uint32_t seqCountTemp;
    uint32_t seqCountExcur;
	uint32_t *rdcLogArray;
	uint32_t *freqLogArray;
	uint32_t *AfterrdcLogArray;
	uint32_t *AfterfreqLogArray;
	uint32_t *ExcurAfterrdcLogArray;
	uint32_t *ExcurAfterfreqLogArray;

	rc = maxim_dsm_log_duplicate((void**)&byteLogArray, (void**)&intLogArray, (void**)&afterbyteLogArray, (void**)&afterintLogArray);

	if (unlikely(rc)) {
		rc = snprintf(buf, PAGE_SIZE, "no log\n");
#ifdef USE_DSM_MISC_DEV
 		if(param_dsm[param_excur_limit]!=0 && param_dsm[param_thermal_limit]!=0)	{
			rc += snprintf(buf+rc, PAGE_SIZE,  "[Parameter Set] excursionlimit:0x%x, rdcroomtemp:0x%x, coilthermallimit:0x%x, releasetime:0x%x\n",
							param_dsm[param_excur_limit],
							param_dsm[param_voice_coil],
							param_dsm[param_thermal_limit],
							param_dsm[param_release_time]);
			rc += snprintf(buf+rc, PAGE_SIZE,  "[Parameter Set] staticgain:0x%x, lfxgain:0x%x, pilotgain:0x%x\n",
							param_dsm[param_static_gain],
							param_dsm[param_lfx_gain],
							param_dsm[param_pilot_gain]);
		}
#endif /* USE_DSM_MISC_DEV */

		goto out;
	}

	logAvailable     = byteLogArray[0];
	versionID        = byteLogArray[1];
	coilTempLogArray = &byteLogArray[2];
	exCurLogArray    = &byteLogArray[2+LOG_BUFFER_ARRAY_SIZE];

	seqCountTemp       = intLogArray[0];
	seqCountExcur   = intLogArray[1];
	rdcLogArray  = &intLogArray[2];
	freqLogArray = &intLogArray[2+LOG_BUFFER_ARRAY_SIZE];

	AftercoilTempLogArray = &afterbyteLogArray[0];
	AfterexCurLogArray = &afterbyteLogArray[LOG_BUFFER_ARRAY_SIZE];
	AfterrdcLogArray = &afterintLogArray[0];
	AfterfreqLogArray = &afterintLogArray[LOG_BUFFER_ARRAY_SIZE];

	ExcurAftercoilTempLogArray = &afterbyteLogArray[LOG_BUFFER_ARRAY_SIZE*2];
	ExcurAfterexCurLogArray = &afterbyteLogArray[LOG_BUFFER_ARRAY_SIZE*3];
	ExcurAfterrdcLogArray = &afterintLogArray[LOG_BUFFER_ARRAY_SIZE*2];
	ExcurAfterfreqLogArray = &afterintLogArray[LOG_BUFFER_ARRAY_SIZE*3];

	if(logAvailable>0 && (exSeqCountTemp!=seqCountTemp || exSeqCountExcur!=seqCountExcur))	{
		exSeqCountTemp = seqCountTemp;
		exSeqCountExcur = seqCountExcur;
		newLogAvail |= 0x2;
	}

	rc += snprintf(buf+rc, PAGE_SIZE, "DSM LogData saved at %4d-%02d-%02d %02d:%02d:%02d (UTC)\n",
		(int)(maxim_dsm_log_timestamp.tm_year + 1900),
		(int)(maxim_dsm_log_timestamp.tm_mon + 1),
		(int)(maxim_dsm_log_timestamp.tm_mday),
		(int)(maxim_dsm_log_timestamp.tm_hour),
		(int)(maxim_dsm_log_timestamp.tm_min),
		(int)(maxim_dsm_log_timestamp.tm_sec));

	if ((logAvailable & 0x1) == 0x1) { // excursion limit exceeded.
		rc += snprintf(buf+rc, PAGE_SIZE, "*** Excursion Limit was exceeded.\n");
		rc += snprintf(buf+rc, PAGE_SIZE, "Seq:%d, logAvailable=%d, versionID:2.5.%d\n", seqCountExcur, logAvailable, versionID);
		rc += snprintf(buf+rc, PAGE_SIZE, "Temperature={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
						ExcurAftercoilTempLogArray[0], ExcurAftercoilTempLogArray[1], ExcurAftercoilTempLogArray[2], ExcurAftercoilTempLogArray[3], ExcurAftercoilTempLogArray[4],
						ExcurAftercoilTempLogArray[5], ExcurAftercoilTempLogArray[6], ExcurAftercoilTempLogArray[7], ExcurAftercoilTempLogArray[8], ExcurAftercoilTempLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,  "Excursion={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
						ExcurAfterexCurLogArray[0], ExcurAfterexCurLogArray[1], ExcurAfterexCurLogArray[2], ExcurAfterexCurLogArray[3], ExcurAfterexCurLogArray[4],
						ExcurAfterexCurLogArray[5], ExcurAfterexCurLogArray[6], ExcurAfterexCurLogArray[7], ExcurAfterexCurLogArray[8], ExcurAfterexCurLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,  "Rdc={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
						ExcurAfterrdcLogArray[0], ExcurAfterrdcLogArray[1], ExcurAfterrdcLogArray[2], ExcurAfterrdcLogArray[3], ExcurAfterrdcLogArray[4],
						ExcurAfterrdcLogArray[5], ExcurAfterrdcLogArray[6], ExcurAfterrdcLogArray[7], ExcurAfterrdcLogArray[8], ExcurAfterrdcLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,  "Frequency={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
						ExcurAfterfreqLogArray[0], ExcurAfterfreqLogArray[1], ExcurAfterfreqLogArray[2], ExcurAfterfreqLogArray[3], ExcurAfterfreqLogArray[4],
						ExcurAfterfreqLogArray[5], ExcurAfterfreqLogArray[6], ExcurAfterfreqLogArray[7], ExcurAfterfreqLogArray[8], ExcurAfterfreqLogArray[9]);	}

	if ((logAvailable&0x2) == 0x2) { // temperature limit exceeded.
		rc += snprintf(buf+rc, PAGE_SIZE, "*** Temperature Limit was exceeded.\n");
		rc += snprintf(buf+rc, PAGE_SIZE, "Seq:%d, logAvailable=%d, versionID:2.5.%d\n", seqCountTemp, logAvailable, versionID);
		rc += snprintf(buf+rc, PAGE_SIZE, "Temperature={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
						coilTempLogArray[0], coilTempLogArray[1], coilTempLogArray[2], coilTempLogArray[3], coilTempLogArray[4],
						coilTempLogArray[5], coilTempLogArray[6], coilTempLogArray[7], coilTempLogArray[8], coilTempLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE, "              %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
						AftercoilTempLogArray[0], AftercoilTempLogArray[1], AftercoilTempLogArray[2], AftercoilTempLogArray[3], AftercoilTempLogArray[4],
						AftercoilTempLogArray[5], AftercoilTempLogArray[6], AftercoilTempLogArray[7], AftercoilTempLogArray[8], AftercoilTempLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,  "Excursion={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
						exCurLogArray[0], exCurLogArray[1], exCurLogArray[2], exCurLogArray[3], exCurLogArray[4],
						exCurLogArray[5], exCurLogArray[6], exCurLogArray[7], exCurLogArray[8], exCurLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,  "            %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
						AfterexCurLogArray[0], AfterexCurLogArray[1], AfterexCurLogArray[2], AfterexCurLogArray[3], AfterexCurLogArray[4],
						AfterexCurLogArray[5], AfterexCurLogArray[6], AfterexCurLogArray[7], AfterexCurLogArray[8], AfterexCurLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,  "Rdc={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
						rdcLogArray[0], rdcLogArray[1], rdcLogArray[2], rdcLogArray[3], rdcLogArray[4],
						rdcLogArray[5], rdcLogArray[6], rdcLogArray[7], rdcLogArray[8], rdcLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,  "      %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
						AfterrdcLogArray[0], AfterrdcLogArray[1], AfterrdcLogArray[2], AfterrdcLogArray[3], AfterrdcLogArray[4],
						AfterrdcLogArray[5], AfterrdcLogArray[6], AfterrdcLogArray[7], AfterrdcLogArray[8], AfterrdcLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,  "Frequency={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
						freqLogArray[0], freqLogArray[1], freqLogArray[2], freqLogArray[3], freqLogArray[4],
						freqLogArray[5], freqLogArray[6], freqLogArray[7], freqLogArray[8], freqLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,  "            %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
						AfterfreqLogArray[0], AfterfreqLogArray[1], AfterfreqLogArray[2], AfterfreqLogArray[3], AfterfreqLogArray[4],
						AfterfreqLogArray[5], AfterfreqLogArray[6], AfterfreqLogArray[7], AfterfreqLogArray[8], AfterfreqLogArray[9]);
	}

	rc += snprintf(buf+rc, PAGE_SIZE,  "[Parameter Set] excursionlimit:0x%x, rdcroomtemp:0x%x, coilthermallimit:0x%x, releasetime:0x%x\n",
					param_dsm[param_excur_limit],
					param_dsm[param_voice_coil],
					param_dsm[param_thermal_limit],
					param_dsm[param_release_time]);
	rc += snprintf(buf+rc, PAGE_SIZE,  "[Parameter Set] staticgain:0x%x, lfxgain:0x%x, pilotgain:0x%dx\n",
					param_dsm[param_static_gain],
					param_dsm[param_lfx_gain],
					param_dsm[param_pilot_gain]);

out:
	maxim_dsm_log_free((void**)&byteLogArray, (void**)&intLogArray, (void**)&afterbyteLogArray, (void**)&afterintLogArray);

	return (ssize_t)rc;
}

static DEVICE_ATTR(dsm_log, S_IRUGO, maxim_dsm_log_show, NULL);

static struct attribute *maxim_attributes[] = {
	&dev_attr_dsm_log.attr,
	NULL
};

struct attribute_group maxim_attribute_group = {
	.attrs = maxim_attributes
};

int maxim_dsm_status_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = get_dsm_onoff_status()>0?1:0;

	return 0;
}

int maxim_dsm_status_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	unsigned int sel = ucontrol->value.integer.value[0];

	set_dsm_onoff_status(sel>0?1:0);

	return 0;
}

int maxim_dsm_dump_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = (newLogAvail&0x3);

	newLogAvail &= 0x1;

	return 0;
}

int maxim_dsm_dump_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	unsigned int sel = 0;
	sel = (long)ucontrol->value.integer.value[0];

	dsm_dump_func(1);

	return 0;
}

int get_dsm_onoff_status(void)
{
	return param_dsm[param_onoff];
}

static void set_dsm_onoff_status(int onoff)
{
	param_dsm[param_onoff] = onoff;
}

static void write_param(int *param)
{
	uint32_t filter_set;
    int32_t port_id = DSM_RX_PORT_ID;
	int x;

	if(param[param_wirte_flag] == 0)	{
		// validation check
		param[param_wirte_flag] = 0;
		for(x=0;x<param_max;x+=2)	{
			if(x!= param_onoff && param_dsm[x]!=0) {
				param[param_wirte_flag] = 0xabefcdab;
				break;
			}
		}
	}

	if(param[param_wirte_flag]==0xabefcdab || param[param_wirte_flag] == 0xcdababef) {
		memcpy(param_dsm, param, sizeof(param_dsm));

	/* set params from the algorithm to application */
		filter_set = 3;	// SET PARAM
		dsm_open(port_id, &filter_set, (u8*)param);
	}
}

static void read_param(int *param)
{
	uint32_t filter_set;
	int32_t port_id =DSM_RX_PORT_ID;

	filter_set = 2;	// GET PARAM
	dsm_open(port_id, &filter_set, (u8*)param);
	memcpy(param_dsm, param, sizeof(param_dsm));
	if(param_dsm[param_excur_limit]!=0 && param_dsm[param_thermal_limit]!=0)	{
		newLogAvail |= 0x1;
	}

}

static void dsm_dump_func(int onoff)
{
	static int param[param_max];

	mutex_lock(&dsm_lock);
	read_param((int*)param);

	mutex_unlock(&dsm_lock);
}

static int __devexit dsm_ctrl_remove(struct i2c_client *client)
{
	return 0;
}

static int dsm_ctrl_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int dsm_ctrl_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t dsm_ctrl_read(struct file *fp, char __user *buf,
			 size_t count, loff_t *pos)
{
	int rc;

	static int param[param_max];
	mutex_lock(&dsm_lock);

	read_param((int*)param);

	rc = copy_to_user(buf, param, count);
	if (rc != 0) {
		pr_err("copy_to_user failed. (rc=%d)\n", rc);
		mutex_unlock(&dsm_lock);
		return 0;
	}

	mutex_unlock(&dsm_lock);

	return rc;
}

static ssize_t dsm_ctrl_write(struct file *file,
				const char __user *buf,
				size_t count,
				loff_t *ppos)
{
	int rc=0;
	static int param[param_max];

	mutex_lock(&dsm_lock);

	rc = copy_from_user(param, buf, count);
	if (rc != 0) {
		pr_err("copy_from_user failed. (rc=%d)", rc);
		mutex_unlock(&dsm_lock);
		return rc;
	}

	write_param((int*)param);
	mutex_unlock(&dsm_lock);

	return rc;
}

static struct file_operations dsm_ctrl_fops = {
	.owner		= THIS_MODULE,
	.open		= dsm_ctrl_open,
	.release	= dsm_ctrl_release,
	.read = dsm_ctrl_read,
	.write = dsm_ctrl_write,
};
static struct miscdevice dsm_ctrl_miscdev = {
	.minor =    MISC_DYNAMIC_MINOR,
	.name =     "dsm_ctrl_dev0",
	.fops =     &dsm_ctrl_fops
};

int dsm_misc_device_init(void)
{
    int result=0;

    result = misc_register(&dsm_ctrl_miscdev);

    return result;
}

int dsm_misc_device_deinit(void)
{
    int result=0;

    result = misc_deregister(&dsm_ctrl_miscdev);

    return result;
}

