/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : sdio_dev.c
 * Abstract : This file is a implementation for itm sipc command/event function
 *
 * Authors	: gaole.zhang
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "sdio_dev.h"

#if defined(CONFIG_SDIODEV_TEST)

#include "sdio_dev_test.h"

#endif

static struct mutex    g_sdio_func_lock;
static struct timeval  ack_irq_time = {0};
static int             ack_gpio_status = 0;

static int marlin_sdio_probe(struct sdio_func *func, const struct sdio_device_id *id);
static void marlin_sdio_remove(struct sdio_func *func);
static int marlin_sdio_suspend(struct device *dev);
static int marlin_sdio_resume(struct device *dev);

//extern void set_bt_pm_func(void *wakeup,void *sleep);

struct sdio_func *sprd_sdio_func[SDIODEV_MAX_FUNCS];
struct sdio_func sdio_func_0;
SDIO_CHN_HANDLER sdio_tran_handle[17] = {0};

static struct sdio_data *sdio_data=NULL;
static int sdio_irq_num;
static int marlinwake_irq_num;

static int marlin_sdio_ready_irq_num;


static struct wake_lock marlin_wakelock;
static struct wake_lock BT_AP_wakelock;

static struct wake_lock marlinpub_wakelock;


static struct work_struct marlin_wq;
static struct work_struct marlinwake_wq;

static bool ap_sdio_ready = 0;
static bool have_card = 0;
static MARLIN_SDIO_READY_T marlin_sdio_ready = {0};



static const struct sdio_device_id marlin_sdio_ids[] = {
	{SDIO_DEVICE(MARLIN_VENDOR_ID, MARLIN_DEVICE_ID)},
	{},
};

static const struct dev_pm_ops marlin_sdio_pm_ops = {
	.suspend = marlin_sdio_suspend,
	.resume	= marlin_sdio_resume,
};

static struct sdio_driver marlin_sdio_driver = {
	.probe = marlin_sdio_probe,
	.remove = marlin_sdio_remove,
	.name = "marlin_sdio",
	.id_table = marlin_sdio_ids,
	.drv = {
		.pm = &marlin_sdio_pm_ops,
	},
};

static struct mmc_host *sdio_dev_host = NULL;
char *sync_data_ptr = NULL;
static int gpio_marlin_req_tag = 0;
static int gpio_marlinwake_tag = 0;

static bool sdio_w_flag = 0;
//static bool req_marlin_wait_tag = 0; //0: don't wait   1: wait
//atomic_t req_marlin_wait_tag;
atomic_t gpioreq_need_pulldown;
static bool bt_wake_flag = 0;
static bool marlin_bt_wake_flag = 0;


static struct completion marlin_ack = {0};

static SLEEP_POLICY_T sleep_para = {0};
spinlock_t sleep_spinlock ;
volatile bool marlin_mmc_suspend = 0;
MARLIN_PM_RESUME_WAIT_INIT(marlin_sdio_wait);

/************************************************************************/
typedef struct
{
	struct mutex          func_lock;
	struct early_suspend  early_suspend;
	atomic_t              in_early_suspend;
	atomic_t              refcnt;
}mgr_suspend_t;

#define SDIO_OS_LOCK()  do{\
	if(1 == atomic_read(&g_mgr_suspend.in_early_suspend)) \
	{\
		mutex_lock(&(g_mgr_suspend.func_lock));\
		lock_flag = 1;\
	}else{\
		lock_flag = 0;\
		atomic_inc(&(g_mgr_suspend.refcnt));\
	}\
}while(0)

#define SDIO_OS_UNLOCK() do{\
	if(lock_flag){\
		mutex_unlock(&(g_mgr_suspend.func_lock));\
	}else{\
		atomic_dec(&(g_mgr_suspend.refcnt));}\
}while(0)


mgr_suspend_t    g_mgr_suspend = {0};

static void sdio_early_suspend(struct early_suspend *es)
{
	unsigned long  timeout;
	SDIOTRAN_ERR("[%s]enter\n", __func__);
	timeout = jiffies + msecs_to_jiffies(3000);
	atomic_set(&g_mgr_suspend.in_early_suspend, 1);
	while(0 != atomic_read(&(g_mgr_suspend.refcnt)) )
	{
		usleep_range(50, 100);
		if (time_after(jiffies, timeout))
		{
			SDIOTRAN_ERR("[%s][timeout 3000ms][EEROR]\n", __func__);
			return;
		}
	}
	SDIOTRAN_ERR("[%s]ok\n", __func__);
	return;
}

static void sdio_late_resume(struct early_suspend *es)
{
	atomic_set(&g_mgr_suspend.in_early_suspend, 0);
	SDIOTRAN_ERR("[%s]ok\n", __func__);
	return;
}

int mgr_suspend_init(void )
{
	int ret;
	atomic_set(&g_mgr_suspend.refcnt, 0);
	atomic_set(&g_mgr_suspend.in_early_suspend, 0);
	mutex_init(&g_mgr_suspend.func_lock);
	g_mgr_suspend.early_suspend.suspend = sdio_early_suspend;
	g_mgr_suspend.early_suspend.resume  = sdio_late_resume;
	register_early_suspend(&g_mgr_suspend.early_suspend);
	return 0;
}

void mgr_suspend_defint(void )
{
	unregister_early_suspend(&g_mgr_suspend.early_suspend);
	return;
}

/************************************************************************/

static int get_sdio_dev_func(struct sdio_func *func)
{
	if(SDIODEV_MAX_FUNCS <= func->num)
	{
		SDIOTRAN_ERR("func num err!!! func num is %d!!!",func->num);
		return -1;
	}
	SDIOTRAN_DEBUG("func num is %d!!!",func->num);

	if (func->num == 1) 
	{
		sdio_func_0.num = 0;
		sdio_func_0.card = func->card;
		sprd_sdio_func[0] = &sdio_func_0;
	}	

	sprd_sdio_func[func->num]  = func;

	return 0;
}

static int free_sdio_dev_func(struct sdio_func *func)
{
	if(SDIODEV_MAX_FUNCS <= func->num)
	{
		SDIOTRAN_ERR("func num err!!! func num is %d!!!",func->num);
		return -1;
	}
	
	sprd_sdio_func[0]  = NULL;
	sprd_sdio_func[func->num] = NULL;
	sdio_func_0.num = 0;
	sdio_func_0.card = NULL;

	return 0;

}

void gpio_timer_handler(unsigned long data)
{
	if(gpio_get_value(sdio_data->wake_ack))
	{
		sleep_para.gpio_opt_tag = 1;
		mod_timer(&(sleep_para.gpio_timer),\
			jiffies + msecs_to_jiffies(300));//   250<300<2000-1500
		SDIOTRAN_ERR("ack high");
	}
	else
	{
		sleep_para.gpio_opt_tag = 0;
		SDIOTRAN_ERR("gpio_opt_tag=0\n" );
	}
}

int fwdownload_fin = 0;

int get_sprd_download_fin(void)
{
	return fwdownload_fin;
}
EXPORT_SYMBOL_GPL(get_sprd_download_fin);

void set_sprd_download_fin(int dl_tag)
{
	return fwdownload_fin = dl_tag;
}
EXPORT_SYMBOL_GPL(set_sprd_download_fin);

unsigned int irq_count_change = 0;
unsigned int irq_count_change_last = 0;
bool get_sprd_marlin_status(void)
{
	printk(KERN_INFO "irq_count_change:%d\n",irq_count_change);
	printk(KERN_INFO "irq_count_change_last:%d\n",irq_count_change_last);

	if((irq_count_change == 0)&&(irq_count_change_last == 0))
		return 1;
	else if(irq_count_change != irq_count_change_last)
	{
		irq_count_change_last = irq_count_change;
		return 1;
	}
	else
		return 0;
}
EXPORT_SYMBOL_GPL(get_sprd_marlin_status);

atomic_t              is_wlan_open;    //0: wlan close,  1: wlan open
atomic_t              set_marlin_cmplete;    //0: not complete,  1:  complete
int set_wlan_status(int status)
{
	atomic_set(&(is_wlan_open), status);
	return 0;
}
EXPORT_SYMBOL_GPL(set_wlan_status);

/*add start by sam.sun, 20150108, for get the screen status (on or off)
			return: 1 screen on
			     0 screen off
*/
#define BRIGHTNESS_PATH "/sys/class/backlight/sprd_backlight/brightness"
int get_screen_status(void)
{
	struct file *file = NULL;
	int is_screen_on = 1;
	char * buf; //0~255
	mm_segment_t old_fs;
	int size =0;
	loff_t pos = 0;

	file=filp_open(BRIGHTNESS_PATH, O_RDONLY, 0);
	if (IS_ERR(file))
		SDIOTRAN_ERR("Open file %s failed /n",BRIGHTNESS_PATH);


	old_fs = get_fs();
	set_fs(KERNEL_DS);

	buf = (int *)kzalloc(4,GFP_KERNEL);

	vfs_read(file, buf, 3, &pos);

	size = strlen(buf);

	if( strncmp(buf,"0",(size-1))==0 )
		is_screen_on = 0; //screen off 224
	else
		is_screen_on = 1;

	//SDIOTRAN_ERR("*brightness is %s, is_screen_on is %d\r",buf, is_screen_on);
	kfree(buf);
	filp_close(file, NULL);
	set_fs(old_fs);
	return is_screen_on;
}
/*add end by sam.sun, for get the screen status (on or off)*/

extern bool get_bt_state(void);   // 1: bt open , 0: bt close
int set_marlin_wakeup(uint32 chn,uint32 user_id)
{
	//user_id: wifi=0x1;bt=0x2
#if !defined(CONFIG_MARLIN_NO_SLEEP)

	int ret;
	int flags;
	int screen_status = 1;
	int bt_state ;
	SDIOTRAN_DEBUG("entry");

	if(get_sprd_download_fin() != 1)
	{
		SDIOTRAN_ERR("marlin unready");
		return -1;
	}

	/* when bt only, marlin will don't stay 2s wakeup;
	so add for avoid bt ack 250ms high, then in below cases ap don't req cp and send data directly;
	case1: first open wlan, then user_id = 1;
	case2: send AT cmd or loop check, then user_id = 3; mdbg module has done re-try action
	*/
	if((1 == atomic_read(&(is_wlan_open))) && (1 == user_id) )
	{
		if((gpio_get_value(sdio_data->wake_ack)) && (0 == sleep_para.gpio_opt_tag) )
			msleep(300);
		if(gpio_get_value(sdio_data->wake_ack))
			SDIOTRAN_ERR("err:bt req delay 300ms\n");
		atomic_set(&is_wlan_open, 0); // avoid repeat into this func
	}

 //screen_status = get_screen_status();   // for wifi screen download
 //bt_state = get_bt_state();

    spin_lock_irqsave(&sleep_spinlock, flags);
    //if((0 == sleep_para.gpio_opt_tag) ||((!screen_status) && (1 == user_id) && (!bt_state)) )
    if(0 == sleep_para.gpio_opt_tag)
	{
		if(gpio_get_value(sdio_data->wake_ack))  //bt ack 250ms
		{
			spin_unlock_irqrestore(&sleep_spinlock, flags);
			SDIOTRAN_ERR("%d-high,user_id-%d \n", sdio_data->wake_ack,user_id);
			return -2;
	}

		if(0 == atomic_read(&(set_marlin_cmplete)))  //last wakeup don't complete
	{
			spin_unlock_irqrestore(&sleep_spinlock, flags);
			SDIOTRAN_ERR("set_marlin_complete -%d\n",  set_marlin_cmplete);
			return -3;
		}

		atomic_set(&(set_marlin_cmplete), 0); //invoid re-entry this func.
		if((0x1 == user_id) ||(0x3 == user_id))
		{
			sdio_w_flag = 1;
		}

		if(0x2 == user_id)
		{
			bt_wake_flag = 1;
		}
		atomic_set(&gpioreq_need_pulldown, 1);//sleep_para.gpioreq_need_pulldown = 1;

		gpio_direction_output(sdio_data->wake_out,1);
		spin_unlock_irqrestore(&sleep_spinlock, flags);

		SDIOTRAN_ERR("%d-1\n", sdio_data->wake_out );

		if((0x1 == user_id) ||(0x3 == user_id))
		{
			ret = wait_for_completion_timeout( &marlin_ack, msecs_to_jiffies(100) );
			if (ret == 0){
				atomic_set(&(set_marlin_cmplete), 1);
				SDIOTRAN_ERR("marlin chn %d ack timeout, user_id-%d!!!",chn, user_id);
				return -ETIMEDOUT;//timeout
			}
		}
		}
		else
		{
		spin_unlock_irqrestore(&sleep_spinlock, flags);
	}

	atomic_set(&(set_marlin_cmplete), 1);
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(set_marlin_wakeup);


int set_marlin_sleep(uint32 chn,uint32 user_id)
{
	//user_id: wifi=0x1;bt=0x2
#if !defined(CONFIG_MARLIN_NO_SLEEP)
	SDIOTRAN_DEBUG("entry");
	if( atomic_read(&gpioreq_need_pulldown) )//if(sleep_para.gpioreq_need_pulldown)
	{
		gpio_direction_output(sdio_data->wake_out,0);
		if((user_id == 0x1) ||(user_id == 0x3))
		{
			sdio_w_flag = 0;
		}
		if(user_id == 0x2)
		{
			bt_wake_flag = 0;
		}
		atomic_set(&gpioreq_need_pulldown, 0);//sleep_para.gpioreq_need_pulldown = 0;
		SDIOTRAN_ERR("gpio%d-0,user_id-%d\n", sdio_data->wake_out, user_id);
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(set_marlin_sleep);


/*only idle chn can be used for ap write*/
int  sdio_dev_chn_idle(uint32 chn)
{
	uint32 active_status;
	uint8  status0,status1;	
	int err_ret;
	
	MARLIN_PM_RESUME_WAIT(marlin_sdio_wait);
	MARLIN_PM_RESUME_RETURN_ERROR(-1);
	if(NULL == sprd_sdio_func[SDIODEV_FUNC_0])
	{
		SDIOTRAN_ERR("func uninit!!!");
		return -1;
	}

	sdio_claim_host(sprd_sdio_func[SDIODEV_FUNC_0]);
	
	status0 = sdio_readb(sprd_sdio_func[SDIODEV_FUNC_0],0x840,&err_ret);

	status1 = sdio_readb(sprd_sdio_func[SDIODEV_FUNC_0],0x841,&err_ret);
     
	sdio_release_host(sprd_sdio_func[SDIODEV_FUNC_0]);
	
	active_status = (uint32)(status0) + ((uint32)(status1) << 8);

	if( BIT_0 & ( active_status >> chn )){
		SDIOTRAN_DEBUG("SDIO channel num = %x is active!!",chn);
		return 0;	//active
	}
	else{
		SDIOTRAN_DEBUG("SDIO channel num = %x is idle!!",chn);
		return 1;  //idle
	}
}
EXPORT_SYMBOL_GPL(sdio_dev_chn_idle);

#if defined(CONFIG_SDIODEV_TEST)

/*only active chn can be readed*/
int  sdio_dev_get_read_chn(void)
{
	uint8  rw_flag0,rw_flag1;
	uint8  status0,status1;
	uint32 chn_status;
	uint32 chn_rw_flag;
	uint32 tmp;
	int err_ret,read_chn;
	MARLIN_PM_RESUME_WAIT(marlin_sdio_wait);
	MARLIN_PM_RESUME_RETURN_ERROR(-1);

	sdio_claim_host(sprd_sdio_func[SDIODEV_FUNC_0]);

	status0 = sdio_readb(sprd_sdio_func[SDIODEV_FUNC_0],0x840,&err_ret);
	
	status1 = sdio_readb(sprd_sdio_func[SDIODEV_FUNC_0],0x841,&err_ret);
	
	rw_flag0 = sdio_readb(sprd_sdio_func[SDIODEV_FUNC_0],0x842,&err_ret);
	
	rw_flag1 = sdio_readb(sprd_sdio_func[SDIODEV_FUNC_0],0x843,&err_ret);

	sdio_release_host(sprd_sdio_func[SDIODEV_FUNC_0]);

	chn_status = (uint32)(status0) + ((uint32)(status1) << 8);
	chn_rw_flag = (uint32)(rw_flag0) + ((uint32)(rw_flag1) << 8);

	SDIOTRAN_DEBUG("ap recv: chn_status is 0x%x!!!",chn_status);
	SDIOTRAN_DEBUG("ap read: chn_rw_flag is 0x%x!!!",chn_rw_flag);

	tmp = (chn_status&(~chn_rw_flag))&(0x0000ffff);

	for(read_chn=0;read_chn<16;read_chn++)
	{
		if(BIT_0 & (tmp>>read_chn))
		{
		 	return read_chn;
		}
	}
	return -1;

}



#else

int  sdio_dev_get_read_chn(void)
{
	uint8  chn_status;
	int err_ret;
	int lock_flag;
	MARLIN_PM_RESUME_WAIT(marlin_sdio_wait);
	MARLIN_PM_RESUME_RETURN_ERROR(-1);
	SDIO_OS_LOCK();//mutex_lock(&g_sdio_func_lock);
	sdio_claim_host(sprd_sdio_func[SDIODEV_FUNC_0]);

	chn_status = sdio_readb(sprd_sdio_func[SDIODEV_FUNC_0],0x841,&err_ret);

	while( chn_status == 0 ){
		chn_status = sdio_readb(sprd_sdio_func[SDIODEV_FUNC_0],0x841,&err_ret);
	}

	sdio_release_host(sprd_sdio_func[SDIODEV_FUNC_0]);
	SDIO_OS_UNLOCK();//mutex_unlock(&g_sdio_func_lock);
	SDIOTRAN_DEBUG("ap read: chn_status is 0x%x!!!",chn_status);

	if(chn_status == 0xff)
	{
		SDIOTRAN_ERR("Sdio Err!!!chn status 0x%x!!!",chn_status);
		return -1;
	}
	else if(chn_status & SDIO_CHN_8)
		return 8;
	else if(chn_status & SDIO_CHN_9)
		return 9;
	else if(chn_status & SDIO_CHN_11)
		return 11;
	else if(chn_status & SDIO_CHN_12)
		return 12;
	else if(chn_status & SDIO_CHN_13)
		return 13;
	else if(chn_status & SDIO_CHN_14)
		return 14;
	else if(chn_status & SDIO_CHN_15)
		return 15;
	else
	{		
		SDIOTRAN_ERR("Invalid sdio read chn!!!chn status 0x%x!!!",chn_status);
		return -1;
	}
}

#endif
EXPORT_SYMBOL_GPL(sdio_dev_get_read_chn);


/*ONLY WIFI WILL USE THIS FUNC*/
int  sdio_chn_status(unsigned short chn, unsigned short *status)
{
	unsigned char status0 = 0;
	unsigned char status1 = 0;
	int err_ret;
	int ret = 0;
	int lock_flag;
	SDIOTRAN_DEBUG("ENTRY");
	MARLIN_PM_RESUME_WAIT(marlin_sdio_wait);
	MARLIN_PM_RESUME_RETURN_ERROR(-1);
	if(NULL == sprd_sdio_func[SDIODEV_FUNC_0])
	{
		SDIOTRAN_ERR("func err");
		return -1;
	}
	SDIO_OS_LOCK();//mutex_lock(&g_sdio_func_lock);
	sdio_claim_host(sprd_sdio_func[SDIODEV_FUNC_0]);

	if (0x00FF & chn) {
		status0 = sdio_readb(sprd_sdio_func[SDIODEV_FUNC_0],0x840,&err_ret);
		if (status0 == 0xFF){
			SDIOTRAN_ERR("sdio chn reg0 err");
			ret = -1;
	}
	}

	if (0xFF00 & chn) {
		status1 = sdio_readb(sprd_sdio_func[SDIODEV_FUNC_0],0x841,&err_ret);
		if (status1 == 0xFF){
			SDIOTRAN_ERR("sdio chn reg1 err");
			ret = -1;
	}
	}

	sdio_release_host(sprd_sdio_func[SDIODEV_FUNC_0]);
	SDIO_OS_UNLOCK();//mutex_unlock(&g_sdio_func_lock);
	*status = ( (status0+(status1 << 8)) & chn );

	return ret;
}
EXPORT_SYMBOL_GPL(sdio_chn_status);

int  sdio_dev_get_chn_datalen(uint32 chn)
{
	uint32 usedata;
	uint8  status0,status1,status2;
	int err_ret;
	int lock_flag;
	MARLIN_PM_RESUME_WAIT(marlin_sdio_wait);
	MARLIN_PM_RESUME_RETURN_ERROR(-1);
	if(NULL == sprd_sdio_func[SDIODEV_FUNC_0])
	{
		SDIOTRAN_ERR("func uninit!!!");
		return -1;
	}
	SDIO_OS_LOCK();//mutex_lock(&g_sdio_func_lock);
	sdio_claim_host(sprd_sdio_func[SDIODEV_FUNC_0]);

	status0 = sdio_readb(sprd_sdio_func[SDIODEV_FUNC_0],(0x800+ 4*chn),&err_ret);

	status1 = sdio_readb(sprd_sdio_func[SDIODEV_FUNC_0],(0x801+ 4*chn),&err_ret);

	status2 = sdio_readb(sprd_sdio_func[SDIODEV_FUNC_0],(0x802+ 4*chn),&err_ret);

	sdio_release_host(sprd_sdio_func[SDIODEV_FUNC_0]);
	SDIO_OS_UNLOCK();//mutex_unlock(&g_sdio_func_lock);
	usedata = ((uint32)(status0) ) + ((uint32)(status1) << 8) + ((uint32)(status2)  << 16);

	if(status0 == 0xff || status1 == 0xff || status2 == 0xff)
	{
		SDIOTRAN_ERR("read err!!!");
		return -1;
	}

	return usedata;	
	
}
EXPORT_SYMBOL_GPL(sdio_dev_get_chn_datalen);


int sdio_dev_write(uint32 chn,void* data_buf,uint32 count)
{
	int ret,data_len;
	int lock_flag;
	MARLIN_PM_RESUME_WAIT(marlin_sdio_wait);
	MARLIN_PM_RESUME_RETURN_ERROR(-1);

	if(NULL == sprd_sdio_func[SDIODEV_FUNC_1])
	{
		SDIOTRAN_ERR("func uninit!!!");
		return -1;
	}

	if(chn != 0 && chn != 1)
	{
		data_len = sdio_dev_get_chn_datalen(chn);
		if(data_len < count || data_len <= 0)
		{
			SDIOTRAN_ERR("chn %d, len %d, cnt %d, err!!!",chn,data_len,count);
			return -1;
		}
	}
	SDIO_OS_LOCK();//mutex_lock(&g_sdio_func_lock);
	sdio_claim_host(sprd_sdio_func[SDIODEV_FUNC_1]);

	ret = sdio_memcpy_toio(sprd_sdio_func[SDIODEV_FUNC_1],chn,data_buf,count);
	SDIOTRAN_DEBUG("chn %d send ok!!!",chn);

	sdio_release_host(sprd_sdio_func[SDIODEV_FUNC_1]);
	SDIO_OS_UNLOCK();//mutex_unlock(&g_sdio_func_lock);
	return ret;

}
EXPORT_SYMBOL_GPL(sdio_dev_write);


int  sdio_dev_read(uint32 chn,void* read_buf,uint32 *count)
{
	int err_ret,data_len;
	int lock_flag;
	MARLIN_PM_RESUME_WAIT(marlin_sdio_wait);
	MARLIN_PM_RESUME_RETURN_ERROR(-1);

	if(NULL == sprd_sdio_func[SDIODEV_FUNC_1])
	{
		SDIOTRAN_ERR("func uninit!!!");
		return -1;
	}

	data_len = sdio_dev_get_chn_datalen(chn);
	if(data_len <= 0)
	{
		SDIOTRAN_ERR("chn %d,datelen %d err!!!",chn,data_len);
		return -1;
	}
	SDIOTRAN_DEBUG("ap recv chn %d: read_datalen is 0x%x!!!",chn,data_len);

	SDIO_OS_LOCK();//mutex_lock(&g_sdio_func_lock);
	sdio_claim_host(sprd_sdio_func[SDIODEV_FUNC_1]);
	err_ret = sdio_memcpy_fromio(sprd_sdio_func[SDIODEV_FUNC_1],read_buf,chn,data_len);
	sdio_release_host(sprd_sdio_func[SDIODEV_FUNC_1]);
	SDIO_OS_UNLOCK();//mutex_unlock(&g_sdio_func_lock);
	if(NULL != count)
		*count = data_len;

	return err_ret;

}

EXPORT_SYMBOL_GPL(sdio_dev_read);

int  sdio_read_wlan(uint32 chn,void* read_buf,uint32 *count)
{
	int err_ret,data_len;
	data_len = 16384;
	MARLIN_PM_RESUME_WAIT(marlin_sdio_wait);
	MARLIN_PM_RESUME_RETURN_ERROR(-1);
	if(NULL == sprd_sdio_func[SDIODEV_FUNC_1])
	{
		SDIOTRAN_ERR("func uninit!!!");
		return -1;
	}
	sdio_claim_host(sprd_sdio_func[SDIODEV_FUNC_1]);
	err_ret = sdio_memcpy_fromio(sprd_sdio_func[SDIODEV_FUNC_1],read_buf,chn,data_len);
	sdio_release_host(sprd_sdio_func[SDIODEV_FUNC_1]);
	if(NULL != count)
		*count = data_len;
	return err_ret;

}

EXPORT_SYMBOL_GPL(sdio_read_wlan);

int sdiodev_readchn_init(uint32 chn,void* callback,bool with_para)
{
	SDIOTRAN_DEBUG("Param, chn = %d, callback = %p",chn,callback);
	if(chn >= INVALID_SDIO_CHN)
	{
		SDIOTRAN_ERR("err input chn %d",chn);
		return -1;
	}
	sdio_tran_handle[chn].chn = chn;
	
	if(with_para == 1)
	{
		sdio_tran_handle[chn].tran_callback_para = callback;		
		SDIOTRAN_DEBUG("sdio_tran_handle, chn = %d, callback = %p",sdio_tran_handle[chn].chn,sdio_tran_handle[chn].tran_callback_para);
	}
	else
	{
		sdio_tran_handle[chn].tran_callback = callback;		
		SDIOTRAN_DEBUG("sdio_tran_handle, chn = %d, callback = %p",sdio_tran_handle[chn].chn,sdio_tran_handle[chn].tran_callback);
	}	
	
	return 0;
}
EXPORT_SYMBOL_GPL(sdiodev_readchn_init);

int sdiodev_readchn_uninit(uint32 chn)
{
	if(chn >= INVALID_SDIO_CHN)
	{
		SDIOTRAN_ERR("err input chn %d",chn);
		return -1;
	}

	sdio_tran_handle[chn].chn = INVALID_SDIO_CHN;
	sdio_tran_handle[chn].tran_callback = NULL;
	sdio_tran_handle[chn].tran_callback_para = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(sdiodev_readchn_uninit);

void invalid_recv_flush(uint32 chn)
{
	int len ;
	char* pbuff = NULL;
	
	if(chn < 0 || chn > 15)
	{
		SDIOTRAN_ERR("err chn");
		return;
	}

	len = sdio_dev_get_chn_datalen(chn);	
	if(len <= 0){
		SDIOTRAN_ERR("chn %d, len err %d!!",chn,len);
		return;
	}
	SDIOTRAN_ERR("NO VALID DATA! CHN %d FLUSH! DATALEN %d!",chn,len);	
	pbuff = kmalloc(len,GFP_KERNEL);
	if(pbuff){
		SDIOTRAN_DEBUG("Read to Flush,chn=%d,pbuff=%p,len=%d",chn, pbuff, len);
		sdio_dev_read(chn, pbuff, NULL);
		kfree(pbuff);
	}else{
		SDIOTRAN_ERR("Kmalloc %d failed!",len);
	}
}
EXPORT_SYMBOL_GPL(invalid_recv_flush);

void flush_blkchn(void)
{
	uint8  rw_flag0,rw_flag1;
	uint8  status0,status1;
	uint32 chn_status;
	uint32 chn_rw_flag;
	uint32 tmp;
	int err_ret,read_chn;
	MARLIN_PM_RESUME_WAIT(marlin_sdio_wait);
	MARLIN_PM_RESUME_RETURN_ERROR(-1);

	sdio_claim_host(sprd_sdio_func[SDIODEV_FUNC_0]);

	status0 = sdio_readb(sprd_sdio_func[SDIODEV_FUNC_0],0x840,&err_ret);
	if (status0 == 0xFF){
		SDIOTRAN_ERR("sdio chn reg0 err");
		return;
		}

	status1 = sdio_readb(sprd_sdio_func[SDIODEV_FUNC_0],0x841,&err_ret);
	if (status1 == 0xFF){
		SDIOTRAN_ERR("sdio chn reg1 err");
		return;
		}

	rw_flag0 = sdio_readb(sprd_sdio_func[SDIODEV_FUNC_0],0x842,&err_ret);
	if (rw_flag0 == 0xFF){
		SDIOTRAN_ERR("sdio RW reg0 err");
		return;
		}

	rw_flag1 = sdio_readb(sprd_sdio_func[SDIODEV_FUNC_0],0x843,&err_ret);
	if (rw_flag1 == 0xFF){
		SDIOTRAN_ERR("sdio RW reg1 err");
		return;
		}

	sdio_release_host(sprd_sdio_func[SDIODEV_FUNC_0]);

	chn_status = (uint32)(status0) + ((uint32)(status1) << 8);
	chn_rw_flag = (uint32)(rw_flag0) + ((uint32)(rw_flag1) << 8);

	SDIOTRAN_ERR("chn_status is 0x%x!!!",chn_status);
	SDIOTRAN_ERR("chn_rw_flag is 0x%x!!!",chn_rw_flag);

	tmp = (chn_status&(~chn_rw_flag))&(0x0000ffff);

	for(read_chn=0;read_chn<16;read_chn++)
	{
		if(BIT_0 & (tmp>>read_chn))
		{
			SDIOTRAN_ERR("BLK CHN is 0x%x!!!",read_chn);
			invalid_recv_flush(read_chn);
            return;
		}
	}

	SDIOTRAN_ERR("NO BLK CHN!!!");
	return;

}
EXPORT_SYMBOL_GPL(flush_blkchn);


int sdiolog_handler(void)
{
	if(NULL != sdio_tran_handle[SDIOLOG_CHN].tran_callback)
	{
		SDIOTRAN_DEBUG("SDIOLOG_CHN tran_callback=%p",sdio_tran_handle[SDIOLOG_CHN].tran_callback);
		sdio_tran_handle[SDIOLOG_CHN].tran_callback();
		return 0;
	}
	else
	{		
		return -1;
	}
}

int sdio_download_handler(void)
{
	if(NULL != sdio_tran_handle[DOWNLOAD_CHANNEL_READ].tran_callback)
	{
		SDIOTRAN_DEBUG("DOWNLOAD_CHN tran_callback=%p",sdio_tran_handle[DOWNLOAD_CHANNEL_READ].tran_callback);
		sdio_tran_handle[DOWNLOAD_CHANNEL_READ].tran_callback();
		return 0;
	}
	else
	{
		return -1;
	}
}

int sdio_pseudo_atc_handler(void)
{
	SDIOTRAN_ERR("ENTRY");

	if(NULL != sdio_tran_handle[PSEUDO_ATC_CHANNEL_READ].tran_callback)
	{
		SDIOTRAN_ERR("tran_callback=%p",sdio_tran_handle[PSEUDO_ATC_CHANNEL_READ].tran_callback);
		sdio_tran_handle[PSEUDO_ATC_CHANNEL_READ].tran_callback();
		return 0;
	}
	else
	{
		return -1;
	}
}

int sdio_pseudo_loopcheck_handler(void)
{
	SDIOTRAN_ERR("ENTRY");

	if(NULL != sdio_tran_handle[PSEUDO_ATC_CHANNEL_LOOPCHECK].tran_callback)
	{
		SDIOTRAN_ERR("tran_callback=%p",sdio_tran_handle[PSEUDO_ATC_CHANNEL_LOOPCHECK].tran_callback);
		sdio_tran_handle[PSEUDO_ATC_CHANNEL_LOOPCHECK].tran_callback();
		return 0;
	}
	else
	{
		return -1;
	}
}


int sdio_wifi_handler(uint32 chn)
{
	if(NULL != sdio_tran_handle[chn].tran_callback_para)
	{
		SDIOTRAN_DEBUG("wifi_handle_chn %d  tran_callback=%p",chn,sdio_tran_handle[chn].tran_callback_para);
		sdio_tran_handle[chn].tran_callback_para(chn);
		return 0;
	}
	else
	{
		return -1;
	}
}

int sdio_assertinfo_handler(void)
{
	SDIOTRAN_ERR("ENTRY");

	if(NULL != sdio_tran_handle[MARLIN_ASSERTINFO_CHN].tran_callback)
	{
		SDIOTRAN_ERR("tran_callback=%p",sdio_tran_handle[MARLIN_ASSERTINFO_CHN].tran_callback);
		sdio_tran_handle[MARLIN_ASSERTINFO_CHN].tran_callback();
		return 0;
	}
	else
	{
		return -1;
	}
}


static void marlin_workq(void)
{
	int read_chn;
	int ret = 0;

	SDIOTRAN_DEBUG("ENTRY");

#if !defined(CONFIG_SDIODEV_TEST)
		
	read_chn = sdio_dev_get_read_chn();
	switch(read_chn)
	{
		case SDIOLOG_CHN:
			ret = sdiolog_handler();
			break;
		case DOWNLOAD_CHANNEL_READ:
			ret = sdio_download_handler();
			break;
		case PSEUDO_ATC_CHANNEL_READ:
			ret = sdio_pseudo_atc_handler();
			break;
		case PSEUDO_ATC_CHANNEL_LOOPCHECK:
			ret = sdio_pseudo_loopcheck_handler();
			break;
		case MARLIN_ASSERTINFO_CHN:
			ret = sdio_assertinfo_handler();
			break;
		case WIFI_CHN_8:
		case WIFI_CHN_9:
			ret = sdio_wifi_handler(read_chn);
			break;
		default:
			SDIOTRAN_ERR("no handler for this chn",read_chn);
			invalid_recv_flush(read_chn);
	}

	if(-1 == ret)
		invalid_recv_flush(read_chn);

#else

#if defined(SDIO_TEST_CASE4)
	case4_workq_handler();
#endif

#if defined(SDIO_TEST_CASE5)
	case5_workq_handler();
#endif

#if defined(SDIO_TEST_CASE6)
	case6_workq_handler();
#endif

#if defined(SDIO_TEST_CASE9)
	case9_workq_handler();
#endif

#endif//CONFIG_SDIODEV_TEST

	wake_unlock(&marlin_wakelock);

}


static irqreturn_t marlin_irq_handler(int irq, void * para)
{
	disable_irq_nosync(irq);

	wake_lock(&marlin_wakelock);
	irq_set_irq_type(irq,IRQF_TRIGGER_RISING);

	if(get_sprd_download_fin() != 1)
	{
		schedule_work(&marlin_wq);
	}
	else if(NULL != sdio_tran_handle[WIFI_CHN_8].tran_callback_para)
	{
		sdio_tran_handle[WIFI_CHN_8].tran_callback_para(WIFI_CHN_8);
		wake_unlock(&marlin_wakelock);
	}
	else
		schedule_work(&marlin_wq);

	enable_irq(irq);

	return IRQ_HANDLED;
}

static int sdio_dev_intr_init(void)
{
	int ret;

	wake_lock_init(&marlin_wakelock, WAKE_LOCK_SUSPEND, "marlin_wakelock");

	INIT_WORK(&marlin_wq, marlin_workq);

	sdio_irq_num = gpio_to_irq(sdio_data->data_irq);

	ret = request_irq(sdio_irq_num, marlin_irq_handler,IRQF_TRIGGER_RISING |IRQF_NO_SUSPEND, "sdio_dev_intr", NULL );
	//ret = request_irq(sdio_irq_num, marlin_irq_handler,IRQF_TRIGGER_HIGH | IRQF_ONESHOT |IRQF_NO_SUSPEND, "sdio_dev_intr", NULL );
	if (ret != 0) {
		SDIOTRAN_ERR("request irq err!!!gpio is %d!!!",sdio_data->data_irq);
		return ret;
	}

	SDIOTRAN_ERR("ok!!!irq is %d!!!",sdio_irq_num);

	return 0;
}

static void sdio_dev_intr_uninit(void)
{
	disable_irq_nosync(sdio_irq_num);
	free_irq(sdio_irq_num,NULL);
	gpio_free(sdio_data->data_irq);
	gpio_free(sdio_data->wake_out);

	cancel_work_sync(&marlin_wq);
	//flush_workqueue(&marlin_wq);
	//destroy_workqueue(&marlin_wq);

	wake_lock_destroy(&marlin_wakelock);

	SDIOTRAN_ERR("ok!!!");

}
static void set_apsdiohal_ready(void)
{
	ap_sdio_ready = 1;
}

static void set_apsdiohal_unready(void)
{
	ap_sdio_ready = 0;
}

//return 1 means ap sdiohal ready
bool get_apsdiohal_status(void)
{
	return ap_sdio_ready;
}
EXPORT_SYMBOL_GPL(get_apsdiohal_status);

//return 1 means marlin sdiohal ready
bool get_sdiohal_status(void)
{
	return marlin_sdio_ready.marlin_sdio_init_end_tag;
}
EXPORT_SYMBOL_GPL(get_sdiohal_status);

static void clear_sdiohal_status(void)
{
	marlin_sdio_ready.marlin_sdio_init_start_tag = 0;
	marlin_sdio_ready.marlin_sdio_init_end_tag = 0;
}

static irqreturn_t marlinsdio_ready_irq_handler(int irq, void * para)
{
	disable_irq_nosync(irq);

	SDIOTRAN_ERR("entry");

	if(!marlin_sdio_ready.marlin_sdio_init_start_tag){
		SDIOTRAN_ERR("start");
		irq_set_irq_type(irq,IRQF_TRIGGER_LOW);
		marlin_sdio_ready.marlin_sdio_init_start_tag = 1;}
	else{
		SDIOTRAN_ERR("end");
		irq_set_irq_type(irq,IRQF_TRIGGER_HIGH);
		marlin_sdio_ready.marlin_sdio_init_end_tag = 1;}

	if(!marlin_sdio_ready.marlin_sdio_init_end_tag)
		enable_irq(irq);

	return IRQ_HANDLED;

}

static int marlin_sdio_sync_init(void)
{
	int ret;

	SDIOTRAN_ERR("entry");

	sci_glb_clr(SPRD_PIN_BASE + sdio_data->rfctl_off,(BIT(3)|BIT(7)|0));
	sci_glb_set(SPRD_PIN_BASE + sdio_data->rfctl_off,(BIT(4)|BIT(5)|BIT(2)|BIT(6)));

	marlin_sdio_ready_irq_num = gpio_to_irq(sdio_data->io_ready);
	SDIOTRAN_ERR("req irq");
	ret = request_irq(marlin_sdio_ready_irq_num, marlinsdio_ready_irq_handler,IRQF_TRIGGER_RISING |IRQF_NO_SUSPEND, "m_sdio_ready_intr", NULL );
	if (ret != 0) {
		SDIOTRAN_ERR("request irq err!!!gpio is %d!!!",sdio_data->io_ready);
		return ret;
	}

	SDIOTRAN_ERR("ok!!!irq is %d!!!",marlin_sdio_ready_irq_num);

	return 0;
}

void marlin_sdio_sync_uninit(void)
{
	free_irq(marlin_sdio_ready_irq_num,NULL);
	gpio_free(sdio_data->io_ready);

	sci_glb_clr(SPRD_PIN_BASE + sdio_data->rfctl_off,(BIT(4)|BIT(5)|0));

	SDIOTRAN_ERR("ok!!!");
}
EXPORT_SYMBOL_GPL(marlin_sdio_sync_uninit);


static void marlinack_workq(void)
{
	int read_chn;
	int ret = 0;

	SDIOTRAN_ERR("ENTRY");
	if(sdio_w_flag == 1)
		complete(&marlin_ack);

}

int time_d_value(struct timeval *start, struct timeval *end)
{
	return (end->tv_sec - start->tv_sec)*1000000 + (end->tv_usec - start->tv_usec);
}

static irqreturn_t marlinwake_irq_handler(int irq, void * para)
{
	struct timeval cur_time;
	uint32 gpio_wake_status = 0, usec;
	disable_irq_nosync(irq);
	//irq_set_irq_type(irq,IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING);
	gpio_wake_status = gpio_get_value(sdio_data->wake_ack);

	/*add count irq for loopcheck*/
	irq_count_change++;

	/* avoid gpio jump , so need check the last and cur gpio value.*/
	do_gettimeofday(&cur_time);
	if(ack_gpio_status == gpio_wake_status)
	{
		usec = time_d_value(&ack_irq_time, &cur_time);
		if(usec < 200)    //means invalid gpio value, so discard
		{
			SDIOTRAN_ERR("discard gpio%d irq\n", sdio_data->wake_ack);
			enable_irq(irq);
			return;
		}
		SDIOTRAN_ERR("gpio%d %d-->%d\n",sdio_data->wake_ack, gpio_wake_status, 1 - gpio_wake_status );
		gpio_wake_status = 1 - gpio_wake_status;

	}
	ack_irq_time    = cur_time;
	ack_gpio_status = gpio_wake_status;
	SDIOTRAN_ERR("%d-%d\n",sdio_data->wake_ack, gpio_wake_status );

	if(gpio_wake_status)
		irq_set_irq_type(irq, IRQ_TYPE_EDGE_FALLING);
	else
		irq_set_irq_type(irq, IRQ_TYPE_EDGE_RISING);

	if(gpio_wake_status)
	{
		wake_lock(&marlinpub_wakelock);
		sleep_para.gpio_up_time = jiffies;
	}
	else
	{
		wake_unlock(&marlinpub_wakelock);
		sleep_para.gpio_down_time = jiffies;
	}

	if((!gpio_wake_status) && time_after(sleep_para.gpio_down_time,sleep_para.gpio_up_time))
	{
		if(jiffies_to_msecs(sleep_para.gpio_down_time -\
			sleep_para.gpio_up_time)>200)
		{
			SDIOTRAN_ERR("BT REQ!!!");
			marlin_bt_wake_flag = 1;
			wake_lock_timeout(&BT_AP_wakelock, HZ*2);    //wsh
		}

	}

	//schedule_work(&marlinack_wq);
	if(gpio_wake_status)
	{
		if( atomic_read(&gpioreq_need_pulldown) )//if(sleep_para.gpioreq_need_pulldown)
		{
			sleep_para.gpio_opt_tag = 1;
			SDIOTRAN_ERR("gpio_opt_tag-1\n");
			mod_timer(&(sleep_para.gpio_timer), jiffies + msecs_to_jiffies(sleep_para.marlin_waketime) );
			if(sdio_w_flag == 1)
			{
				complete(&marlin_ack);
					SDIOTRAN_ERR("ack-sem\n");
				set_marlin_sleep(0xff,0x1);
			}
			else
			{
				SDIOTRAN_ERR("sdio_w_flag:%d\n", sdio_w_flag);
				set_marlin_sleep(0xff,0x2);
			}
		}
	}

	enable_irq(irq);

	return IRQ_HANDLED;
}



static int marlin_wake_intr_init(void)
{
	int ret;
	sdio_w_flag = 0;
	bt_wake_flag = 0;

	INIT_WORK(&marlinwake_wq, marlinack_workq);

	init_completion (&marlin_ack);


	wake_lock_init(&BT_AP_wakelock, WAKE_LOCK_SUSPEND, "marlinbtup_wakelock");

	wake_lock_init(&marlinpub_wakelock, WAKE_LOCK_SUSPEND, "marlinpub_wakelock");

	marlinwake_irq_num = gpio_to_irq(sdio_data->wake_ack);

	ret = request_irq(marlinwake_irq_num, marlinwake_irq_handler,IRQF_TRIGGER_RISING |IRQF_NO_SUSPEND, "marlin_wake_intr", NULL );
	//ret = request_irq(marlinwake_irq_num, marlinwake_irq_handler,IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND, "marlin_wake_intr", NULL );
	if (ret != 0) {
		SDIOTRAN_ERR("request irq err!!!gpio is %d!!!",sdio_data->wake_ack);
		return ret;
	}

	SDIOTRAN_ERR("ok!!!irq is %d!!!",marlinwake_irq_num);

	return 0;
}

static void marlin_wake_intr_uninit(void)
{
	disable_irq_nosync(marlinwake_irq_num);
	free_irq(marlinwake_irq_num,NULL);
	gpio_free(sdio_data->wake_ack);


	cancel_work_sync(&marlinwake_wq);
	//flush_workqueue(&marlinwake_wq);
	//destroy_workqueue(&marlinwake_wq);


	wake_lock_destroy(&BT_AP_wakelock);
	wake_lock_destroy(&marlinpub_wakelock);

	SDIOTRAN_ERR("ok!!!");

}



static void sdio_tran_sync(void)
{

	int datalen,i,j;

	SDIOTRAN_ERR("entry");

	set_blklen(4);

	while(!sdio_dev_chn_idle(SDIO_SYNC_CHN))
	{
		SDIOTRAN_ERR("sync chn is active!!!");
		//return;
	}

	while(1)
	{
		datalen = sdio_dev_get_chn_datalen(SDIO_SYNC_CHN);
		if(datalen > 0)
		{
			SDIOTRAN_ERR("channel %d len is =%d!!!",SDIO_SYNC_CHN,datalen);
			break;
		}
		else
			SDIOTRAN_ERR("channel %d len is =%d!!!",SDIO_SYNC_CHN,datalen);

	}

	sync_data_ptr = kmalloc(4,GFP_KERNEL);
	if (NULL == sync_data_ptr)
	{
		SDIOTRAN_ERR("kmalloc sync buf err!!!");
		return -1;
	}

	SDIOTRAN_ERR("kmalloc sync buf ok!!!");

	for(i=0;i<4;i++)
		*(sync_data_ptr+i) = i;

	sdio_dev_write(SDIO_SYNC_CHN,sync_data_ptr,\
		((datalen > 4) ? 4: datalen));

	kfree(sync_data_ptr);

	set_blklen(512);
}


void set_blklen(int blklen)
{
	sdio_claim_host(sprd_sdio_func[1]);
	sdio_set_block_size(sprd_sdio_func[1], blklen);
	sdio_release_host(sprd_sdio_func[1]);

	SDIOTRAN_ERR("set blklen = %d!!!",blklen);

}

static void sdio_init_timer(void)
{
	init_timer(&sleep_para.gpio_timer);
	sleep_para.gpio_timer.function = gpio_timer_handler;
	sleep_para.marlin_waketime = 1500;
	sleep_para.gpio_opt_tag = 0;
	atomic_set(&gpioreq_need_pulldown, 1);//sleep_para.gpioreq_need_pulldown = 1;
}


static void sdio_uninit_timer(void)
{
	del_timer(&sleep_para.gpio_timer);
	sleep_para.marlin_waketime = 0;
	sleep_para.gpio_opt_tag = 0;
	atomic_set(&gpioreq_need_pulldown, 1);//sleep_para.gpioreq_need_pulldown = 1;
}

static int marlin_sdio_probe(struct sdio_func *func, const struct sdio_device_id *id)
{
	int ret;

	SDIOTRAN_ERR("sdio drv and dev match!!!");
	mutex_init(&g_sdio_func_lock);
	ret = get_sdio_dev_func(func);
	if(ret < 0)
	{
		SDIOTRAN_ERR("get func err!!!\n");
		return ret;
	}
	SDIOTRAN_ERR("get func ok!!!\n");

	/* Enable Function 1 */
	sdio_claim_host(sprd_sdio_func[1]);
	ret = sdio_enable_func(sprd_sdio_func[1]);
	sdio_set_block_size(sprd_sdio_func[1], 512);
	sdio_release_host(sprd_sdio_func[1]);
	if (ret < 0) {
		SDIOTRAN_ERR("enable func1 err!!! ret is %d",ret);
		return ret;
	}
	SDIOTRAN_ERR("enable func1 ok!!!");
	sdio_init_timer();

	spin_lock_init(&sleep_spinlock);
	marlin_wake_intr_init();
	marlin_sdio_sync_init();
//case6
#if defined(CONFIG_SDIODEV_TEST)
		gaole_creat_test();
#endif

	sdio_dev_intr_init();

#if defined(CONFIG_SDIODEV_TEST)
	sdio_tran_sync();
#endif

/*case1
#if defined(CONFIG_SDIODEV_TEST)
	gaole_creat_test();
#endif
*/
//	set_bt_pm_func(set_marlin_wakeup,set_marlin_sleep);

	set_apsdiohal_ready();
	have_card = 1;
	return 0;
}

static void marlin_sdio_remove(struct sdio_func *func)
{
	SDIOTRAN_DEBUG("entry");
	sdio_uninit_timer();
	free_sdio_dev_func(func);
	sdio_dev_intr_uninit();
	marlin_wake_intr_uninit();
	//marlin_sdio_sync_uninit();
	set_apsdiohal_unready();
	clear_sdiohal_status();

	SDIOTRAN_DEBUG("ok");

}

static int marlin_sdio_suspend(struct device *dev)
{
	mutex_lock(&g_mgr_suspend.func_lock);
	SDIOTRAN_ERR("[%s]enter\n", __func__);
	marlin_mmc_suspend = 1;

	gpio_marlin_req_tag = gpio_get_value(sdio_data->data_irq);

	if(gpio_marlin_req_tag)
		SDIOTRAN_ERR("err marlin_req!!!");
	else
		irq_set_irq_type(sdio_irq_num,IRQF_TRIGGER_HIGH);

	gpio_marlinwake_tag = gpio_get_value(sdio_data->wake_ack);

	if(gpio_marlinwake_tag)
		SDIOTRAN_ERR("err marlinwake!!!");
	else
		irq_set_irq_type(marlinwake_irq_num,IRQF_TRIGGER_HIGH);



	smp_mb();
	SDIOTRAN_ERR("[%s]ok\n", __func__);
	return 0;
}
static int marlin_sdio_resume(struct device *dev)
{
	SDIOTRAN_ERR("[%s]enter\n", __func__);
	/****************/
	mutex_unlock(&g_mgr_suspend.func_lock);
	sleep_para.gpio_opt_tag = 0;
	set_marlin_wakeup(0, 1);
	mod_timer(&(sleep_para.gpio_timer),jiffies + msecs_to_jiffies(1500));
	/***************/

	marlin_mmc_suspend = 0;
	gpio_marlin_req_tag = gpio_get_value(sdio_data->data_irq);
	if(!gpio_marlin_req_tag)
		irq_set_irq_type(sdio_irq_num,IRQF_TRIGGER_RISING);
	gpio_marlinwake_tag = gpio_get_value(sdio_data->wake_ack);
	if(!gpio_marlinwake_tag)
		irq_set_irq_type(marlinwake_irq_num,IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING);
	smp_mb();
	SDIOTRAN_ERR("[%s]ok\n", __func__);
	return 0;
}

static void* sdio_dev_get_host(void)
{
	struct device *dev;

	struct sdhci_host *host;

	struct platform_device *pdev;

	dev = bus_find_device_by_name(&platform_bus_type, NULL, sdio_data->sdhci);
	if(dev == NULL)
	{
		SDIOTRAN_ERR("sdio find dev by name failed!!!");
		return NULL;
	}

	pdev  = to_platform_device(dev);
	if(pdev == NULL)
	{
		SDIOTRAN_ERR("sdio dev get platform device failed!!!");
		return NULL;
	}

	host = platform_get_drvdata(pdev);
	return container_of(host, struct mmc_host, private);


}

void  marlin_sdio_uninit(void)
{
	set_sprd_download_fin(0);
	sdio_unregister_driver(&marlin_sdio_driver);
	mgr_suspend_defint();
	sdio_dev_host = NULL;
	have_card = 0;

	kfree(sdio_data);
	sdio_data = NULL;
	SDIOTRAN_ERR("ok");
}

int marlin_sdio_init(void)
{
	int ret;

	SDIOTRAN_ERR("entry");
	if(have_card == 1){
		marlin_sdio_uninit();
	}
	mgr_suspend_init();
	//atomic_set(&req_marlin_wait_tag, 0);
	atomic_set(&gpioreq_need_pulldown, 1);
	atomic_set(&is_wlan_open, 0);
	atomic_set(&set_marlin_cmplete, 1);

	sdio_dev_host = sdio_dev_get_host();
	if(NULL == sdio_dev_host)
	{
		SDIOTRAN_ERR("get host failed!!!");
		return -1;
	}
	SDIOTRAN_ERR("sdio get host ok!!!");

	ret = sdio_register_driver(&marlin_sdio_driver);
	if(0 != ret)
	{
		SDIOTRAN_ERR("sdio register drv err!!!ret is %d!!!",ret);
		return -1;
	}

	SDIOTRAN_ERR("sdio register drv succ!!!");

	flush_delayed_work(&sdio_dev_host->detect);
	mmc_detect_change(sdio_dev_host, 0);

	return ret;
}
EXPORT_SYMBOL_GPL(marlin_sdio_init);

static int __init marlin_gpio_init(void)
{
	int ret;
	struct device_node *np;

	sdio_data = kzalloc(sizeof(*sdio_data), GFP_KERNEL);
	if (!sdio_data)
		return -ENOMEM;
	np = of_find_node_by_name(NULL, "sprd-marlin");
	if (!np) {
		SDIOTRAN_ERR("sprd-marlin not found");
		return -1;
	}

	sdio_data->wake_ack = of_get_gpio(np, 0);
	sdio_data->data_irq = of_get_gpio(np, 1);
	sdio_data->wake_out = of_get_gpio(np, 2);
	sdio_data->io_ready = of_get_gpio(np, 3);

	ret = of_property_read_u32(np, "cp-rfctl-offset", &(sdio_data->rfctl_off));
	if (ret) {
		SDIOTRAN_ERR("get cp-rfctl-offset error");
		return -1;
	}

	ret = of_property_read_string(np, "sdhci-name", &(sdio_data->sdhci));
	if (ret) {
		SDIOTRAN_ERR("get sdhci-name failed");
		return -1;
	}

	ret = gpio_direction_output(sdio_data->wake_out,0);
	if (ret)
	{
		SDIOTRAN_ERR("GPIO_AP_TO_MARLIN = %d input set fail!!!",\
			sdio_data->wake_out);
		return ret;
	}

	SDIOTRAN_ERR("req GPIO_AP_TO_MARLIN = %d succ!!!",\
		sdio_data->wake_out);

	ret = gpio_direction_input(sdio_data->data_irq);
	if (ret<0)
	{
		SDIOTRAN_ERR("GPIO_MARLIN_TO_AP = %d input set fail!!!",\
			sdio_data->data_irq);
		return ret;
	}

	SDIOTRAN_ERR("req GPIO_MARLIN_TO_CP = %d succ!!!",\
		sdio_data->data_irq);

	ret = gpio_direction_input(sdio_data->wake_ack);
	if (ret<0)
	{
		SDIOTRAN_ERR("GPIO_MARLIN_WAKE = %d input set fail!!!",\
			sdio_data->wake_ack);
		return ret;
	}

	SDIOTRAN_ERR("req GPIO_MARLIN_WAKE = %d succ!!!",\
		sdio_data->wake_ack);

	ret = gpio_direction_input(sdio_data->io_ready);
	if (ret<0)
	{
		SDIOTRAN_ERR("GPIO_MARLIN_SDIO_READY = %d input set fail!!!",\
			sdio_data->io_ready);
		return ret;
	}

	SDIOTRAN_ERR("req GPIO_MARLIN_SDIO_READY = %d succ!!!",\
		sdio_data->io_ready);

	return ret;
}

static void __exit marlin_gpio_cleanup(void)
{
}

module_init(marlin_gpio_init);
module_exit(marlin_gpio_cleanup);

