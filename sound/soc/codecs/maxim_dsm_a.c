#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/maxim_dsm_a.h>

static uint32_t param[PARAM_DSM_MAX];

static DEFINE_MUTEX(dsm_lock);

static DEFINE_MUTEX(maxdsm_log_lock);

static uint32_t exSeqCountTemp;
static uint32_t exSeqCountExcur;
static uint32_t newLogAvail;

static bool maxdsm_log_present;
static struct tm maxdsm_log_timestamp;
static uint32_t maxdsm_byteLogArray[BEFORE_BUFSIZE];
static uint32_t maxdsm_intLogArray[BEFORE_BUFSIZE];
static uint32_t maxdsm_afterProbByteLogArray[AFTER_BUFSIZE];
static uint32_t maxdsm_afterProbIntLogArray[AFTER_BUFSIZE];

void maxdsm_param_update(const void *ParamArray)
{
	memcpy(param, ParamArray, sizeof(param));
}
void maxdsm_log_update(const void *byteLogArray, const void *intLogArray,
	const void *afterProbByteLogArray, const void *afterProbIntLogArray)
{
	struct timeval tv;
	mutex_lock(&maxdsm_log_lock);

	memcpy(maxdsm_byteLogArray, byteLogArray, sizeof(maxdsm_byteLogArray));
	memcpy(maxdsm_intLogArray, intLogArray, sizeof(maxdsm_intLogArray));

	memcpy(maxdsm_afterProbByteLogArray, afterProbByteLogArray,
		sizeof(maxdsm_afterProbByteLogArray));
	memcpy(maxdsm_afterProbIntLogArray, afterProbIntLogArray,
		sizeof(maxdsm_afterProbIntLogArray));

	do_gettimeofday(&tv);
	time_to_tm(tv.tv_sec, 0, &maxdsm_log_timestamp);

	maxdsm_log_present = true;

	mutex_unlock(&maxdsm_log_lock);
}

static void maxdsm_log_free(void **byteLogArray, void **intLogArray,
	void **afterbyteLogArray, void **afterintLogArray)
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

static int maxdsm_log_duplicate(void **byteLogArray, void **intLogArray,
	void **afterbyteLogArray, void **afterintLogArray)
{
	void *blog_buf = NULL, *ilog_buf = NULL;
	void *after_blog_buf = NULL, *after_ilog_buf = NULL;
	int rc = 0;

	mutex_lock(&maxdsm_log_lock);

	if (unlikely(!maxdsm_log_present)) {
		rc = -ENODATA;
		goto abort;
	}

	blog_buf = kzalloc(sizeof(maxdsm_byteLogArray), GFP_KERNEL);
	ilog_buf = kzalloc(sizeof(maxdsm_intLogArray), GFP_KERNEL);
	after_blog_buf =
		kzalloc(sizeof(maxdsm_afterProbByteLogArray), GFP_KERNEL);
	after_ilog_buf =
		kzalloc(sizeof(maxdsm_afterProbIntLogArray), GFP_KERNEL);

	if (unlikely(!blog_buf || !ilog_buf \
		|| !after_blog_buf || !after_ilog_buf)) {
		rc = -ENOMEM;
		goto abort;
	}

	memcpy(blog_buf, maxdsm_byteLogArray, sizeof(maxdsm_byteLogArray));
	memcpy(ilog_buf, maxdsm_intLogArray, sizeof(maxdsm_intLogArray));
	memcpy(after_blog_buf, maxdsm_afterProbByteLogArray,
		sizeof(maxdsm_afterProbByteLogArray));
	memcpy(after_ilog_buf, maxdsm_afterProbIntLogArray,
		sizeof(maxdsm_afterProbIntLogArray));

	goto out;

abort:
	maxdsm_log_free(&blog_buf, &ilog_buf, &after_blog_buf, &after_ilog_buf);
out:
	*byteLogArray = blog_buf;
	*intLogArray  = ilog_buf;
	*afterbyteLogArray = after_blog_buf;
	*afterintLogArray  = after_ilog_buf;
	mutex_unlock(&maxdsm_log_lock);

	return rc;
}

ssize_t maxdsm_log_prepare(char *buf)
{
	uint32_t *byteLogArray = NULL;
	uint32_t *intLogArray = NULL;
	uint32_t *afterbyteLogArray = NULL;
	uint32_t *afterintLogArray = NULL;
	int rc = 0;

	uint32_t logAvailable;
	uint32_t versionID;
	uint32_t *coilTempLogArray;
	uint32_t *exCurLogArray;
	uint32_t *AftercoilTempLogArray;
	uint32_t *AfterexCurLogArray;
	uint32_t *ExcurAftercoilTempLogArray;
	uint32_t *ExcurAfterexCurLogArray;
	uint32_t seqCountTemp;
	uint32_t seqCountExcur;
	uint32_t *rdcLogArray;
	uint32_t *freqLogArray;
	uint32_t *AfterrdcLogArray;
	uint32_t *AfterfreqLogArray;
	uint32_t *ExcurAfterrdcLogArray;
	uint32_t *ExcurAfterfreqLogArray;

	rc = maxdsm_log_duplicate((void **)&byteLogArray,
		(void **)&intLogArray, (void **)&afterbyteLogArray,
		(void **)&afterintLogArray);

	if (unlikely(rc)) {
		rc = snprintf(buf, PAGE_SIZE, "no log\n");
		if (param[PARAM_EXCUR_LIMIT] != 0 && \
			param[PARAM_THERMAL_LIMIT] != 0)	{
			rc += snprintf(buf+rc, PAGE_SIZE,
				"[Parameter Set] excursionlimit:0x%x, "\
				"rdcroomtemp:0x%x, coilthermallimit:0x%x, "
				"releasetime:0x%x\n"\
				, param[PARAM_EXCUR_LIMIT],
				param[PARAM_VOICE_COIL],
				param[PARAM_THERMAL_LIMIT],
				param[PARAM_RELEASE_TIME]);
			rc += snprintf(buf+rc, PAGE_SIZE,  "[Parameter Set] "\
				"staticgain:0x%x, lfxgain:0x%x, "\
				"pilotgain:0x%x\n",
				param[PARAM_STATIC_GAIN],
				param[PARAM_LFX_GAIN],
				param[PARAM_PILOT_GAIN]);
		}
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

	ExcurAftercoilTempLogArray =
		&afterbyteLogArray[LOG_BUFFER_ARRAY_SIZE*2];
	ExcurAfterexCurLogArray = &afterbyteLogArray[LOG_BUFFER_ARRAY_SIZE*3];
	ExcurAfterrdcLogArray = &afterintLogArray[LOG_BUFFER_ARRAY_SIZE*2];
	ExcurAfterfreqLogArray = &afterintLogArray[LOG_BUFFER_ARRAY_SIZE*3];

	if (logAvailable > 0 && \
		(exSeqCountTemp != seqCountTemp \
		|| exSeqCountExcur != seqCountExcur))	{
		exSeqCountTemp = seqCountTemp;
		exSeqCountExcur = seqCountExcur;
		newLogAvail |= 0x2;
	}

	rc += snprintf(buf+rc, PAGE_SIZE, "DSM LogData saved at "\
		"%4d-%02d-%02d %02d:%02d:%02d (UTC)\n",
		(int)(maxdsm_log_timestamp.tm_year + 1900),
		(int)(maxdsm_log_timestamp.tm_mon + 1),
		(int)(maxdsm_log_timestamp.tm_mday),
		(int)(maxdsm_log_timestamp.tm_hour),
		(int)(maxdsm_log_timestamp.tm_min),
		(int)(maxdsm_log_timestamp.tm_sec));

	if ((logAvailable & 0x1) == 0x1) {
		rc += snprintf(buf+rc, PAGE_SIZE,
			"*** Excursion Limit was exceeded.\n");
		rc += snprintf(buf+rc, PAGE_SIZE,
			"Seq:%d, logAvailable=%d, versionID:3.1.%d\n",
			seqCountExcur, logAvailable, versionID);
		rc += snprintf(buf+rc, PAGE_SIZE, "Temperature="\
			"{ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
			ExcurAftercoilTempLogArray[0],
			ExcurAftercoilTempLogArray[1],
			ExcurAftercoilTempLogArray[2],
			ExcurAftercoilTempLogArray[3],
			ExcurAftercoilTempLogArray[4],
			ExcurAftercoilTempLogArray[5],
			ExcurAftercoilTempLogArray[6],
			ExcurAftercoilTempLogArray[7],
			ExcurAftercoilTempLogArray[8],
			ExcurAftercoilTempLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,  \
			"Excursion="\
			"{ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
			ExcurAfterexCurLogArray[0],
			ExcurAfterexCurLogArray[1],
			ExcurAfterexCurLogArray[2],
			ExcurAfterexCurLogArray[3],
			ExcurAfterexCurLogArray[4],
			ExcurAfterexCurLogArray[5],
			ExcurAfterexCurLogArray[6],
			ExcurAfterexCurLogArray[7],
			ExcurAfterexCurLogArray[8],
			ExcurAfterexCurLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,  "Rdc="\
			"{ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
			ExcurAfterrdcLogArray[0],
			ExcurAfterrdcLogArray[1],
			ExcurAfterrdcLogArray[2],
			ExcurAfterrdcLogArray[3],
			ExcurAfterrdcLogArray[4],
			ExcurAfterrdcLogArray[5],
			ExcurAfterrdcLogArray[6],
			ExcurAfterrdcLogArray[7],
			ExcurAfterrdcLogArray[8],
			ExcurAfterrdcLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,  "Frequency="\
			"{ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
			ExcurAfterfreqLogArray[0],
			ExcurAfterfreqLogArray[1],
			ExcurAfterfreqLogArray[2],
			ExcurAfterfreqLogArray[3],
			ExcurAfterfreqLogArray[4],
			ExcurAfterfreqLogArray[5],
			ExcurAfterfreqLogArray[6],
			ExcurAfterfreqLogArray[7],
			ExcurAfterfreqLogArray[8],
			ExcurAfterfreqLogArray[9]);
	}

	if ((logAvailable & 0x2) == 0x2) {
		rc += snprintf(buf+rc, PAGE_SIZE,
			"*** Temperature Limit was exceeded.\n");
		rc += snprintf(buf+rc, PAGE_SIZE,
			"Seq:%d, logAvailable=%d, versionID:3.1.%d\n",
			seqCountTemp, logAvailable, versionID);
		rc += snprintf(buf+rc, PAGE_SIZE, "Temperature="\
			"{ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
			coilTempLogArray[0],
			coilTempLogArray[1],
			coilTempLogArray[2],
			coilTempLogArray[3],
			coilTempLogArray[4],
			coilTempLogArray[5],
			coilTempLogArray[6],
			coilTempLogArray[7],
			coilTempLogArray[8],
			coilTempLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE, "              "\
			"%d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
			AftercoilTempLogArray[0],
			AftercoilTempLogArray[1],
			AftercoilTempLogArray[2],
			AftercoilTempLogArray[3],
			AftercoilTempLogArray[4],
			AftercoilTempLogArray[5],
			AftercoilTempLogArray[6],
			AftercoilTempLogArray[7],
			AftercoilTempLogArray[8],
			AftercoilTempLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE, \
			"Excursion={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
			exCurLogArray[0],
			exCurLogArray[1],
			exCurLogArray[2],
			exCurLogArray[3],
			exCurLogArray[4],
			exCurLogArray[5],
			exCurLogArray[6],
			exCurLogArray[7],
			exCurLogArray[8],
			exCurLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE, "            "\
			"%d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
			AfterexCurLogArray[0],
			AfterexCurLogArray[1],
			AfterexCurLogArray[2],
			AfterexCurLogArray[3],
			AfterexCurLogArray[4],
			AfterexCurLogArray[5],
			AfterexCurLogArray[6],
			AfterexCurLogArray[7],
			AfterexCurLogArray[8],
			AfterexCurLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,
			"Rdc={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
			rdcLogArray[0],
			rdcLogArray[1],
			rdcLogArray[2],
			rdcLogArray[3],
			rdcLogArray[4],
			rdcLogArray[5],
			rdcLogArray[6],
			rdcLogArray[7],
			rdcLogArray[8],
			rdcLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,
			"      %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
			AfterrdcLogArray[0],
			AfterrdcLogArray[1],
			AfterrdcLogArray[2],
			AfterrdcLogArray[3],
			AfterrdcLogArray[4],
			AfterrdcLogArray[5],
			AfterrdcLogArray[6],
			AfterrdcLogArray[7],
			AfterrdcLogArray[8],
			AfterrdcLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,
			"Frequency={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
			freqLogArray[0],
			freqLogArray[1],
			freqLogArray[2],
			freqLogArray[3],
			freqLogArray[4],
			freqLogArray[5],
			freqLogArray[6],
			freqLogArray[7],
			freqLogArray[8],
			freqLogArray[9]);
		rc += snprintf(buf+rc, PAGE_SIZE, "            "\
			"%d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
			AfterfreqLogArray[0],
			AfterfreqLogArray[1],
			AfterfreqLogArray[2],
			AfterfreqLogArray[3],
			AfterfreqLogArray[4],
			AfterfreqLogArray[5],
			AfterfreqLogArray[6],
			AfterfreqLogArray[7],
			AfterfreqLogArray[8],
			AfterfreqLogArray[9]);
	}

	if (param[PARAM_EXCUR_LIMIT] != 0 &&\
		param[PARAM_THERMAL_LIMIT] != 0)	{
		rc += snprintf(buf+rc, PAGE_SIZE,
			"[Parameter Set] excursionlimit:0x%x, "\
			"rdcroomtemp:0x%x, coilthermallimit:0x%x, "\
			"releasetime:0x%x\n",
			param[PARAM_EXCUR_LIMIT],
			param[PARAM_VOICE_COIL],
			param[PARAM_THERMAL_LIMIT],
			param[PARAM_RELEASE_TIME]);
		rc += snprintf(buf+rc, PAGE_SIZE,  "[Parameter Set] "\
			"staticgain:0x%x, lfxgain:0x%x, pilotgain:0x%x\n",
			param[PARAM_STATIC_GAIN],
			param[PARAM_LFX_GAIN],
			param[PARAM_PILOT_GAIN]);
	}

out:
	maxdsm_log_free((void **)&byteLogArray, (void **)&intLogArray,
		(void **)&afterbyteLogArray, (void **)&afterintLogArray);

	return (ssize_t)rc;
}
