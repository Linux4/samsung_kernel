/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : sdio_dev_test.c
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


#if defined(SDIO_TEST_CASE1)

#define SEND_BUF_LEN 1024//SDIO_TEST_CASE1

char *data_send_ptr[17] = {0};


static void test_sdio_send_func(int chn,int senddatalen,void* data_buf)
{

	uint32 chndatalen,ret;

	char *tmp = (char*)data_buf;
	
	//SDIOTRAN_ERR("\nrun to test_sdio_send_func,senddatalen is %d!!!\n",senddatalen);
	SDIOTRAN_ERR("entry");



	set_marlin_wakeup(chn,1);



	if(!sdio_dev_chn_idle(chn))
	{
		SDIOTRAN_ERR("send chn active,can't send right now,chn is %d!!!",chn);
		return;
	}	


	chndatalen = sdio_dev_get_chn_datalen(chn);
	if(senddatalen > chndatalen)
	{
		SDIOTRAN_ERR("channel %d chndatalen is =%d,senddatalen is =%d, can't send!!",chn,chndatalen,senddatalen);
		return;
	}
	
	SDIOTRAN_ERR("ap chn %d send len %d data: %d,%d,%d,%d  %d,%d,%d,%d  %d,%d,%d,%d!!!",chn,senddatalen,\
		tmp[0],\
		tmp[1],\
		tmp[2],\
		tmp[3], \
		tmp[101], \
		tmp[102], \
		tmp[103], \
		tmp[104], \
		tmp[230], \
		tmp[231], \
		tmp[232], \
		tmp[233] \
		);

	sdio_dev_write(chn,data_buf,senddatalen);
	set_marlin_sleep(chn,1);


}



static int gaole_test_thread(void *data)
{
	int i,j,k;

	struct sched_param param = {.sched_priority = 90};

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);
	
	/*for(i=0;i<16;i++){

		data_send_ptr[i] = kmalloc(SEND_BUF_LEN,GFP_KERNEL);
		if (NULL == data_send_ptr[i])
		{
			SDIOTRAN_ERR("kmalloc send buf%d err!!!",i);
			return -1;
		}
		
		SDIOTRAN_DEBUG("kmalloc send buf%d ok!!!",i);

		for(j=0,k=0;j<SEND_BUF_LEN;j++,k++)
			*(data_send_ptr[i]+j) = ((k+i)<=255?(k+i):(k=0,k+i));
	
	}*/
	set_blklen(512);

	while(1)
	{
	
	/*
		for(j=0;j<8;j++)
		{
			set_blklen(32<<j);
			for(i=0;i<9;i++)
			{	
				if(((32<<j)<<i) > 8192)
					break;
				
				test_sdio_send_func(j,(32<<j)<<i,data_send_ptr[1]);
			}		
		}
	*/
		
	/*
		for(j=1;j<128;j++)
			test_sdio_send_func(0,j,data_send_ptr[1]);
		break;
		
		msleep(1000);
		test_sdio_send_func(0,64,data_send_ptr[1]);
		test_sdio_send_func(0,128,data_send_ptr[1]);
		test_sdio_send_func(0,192,data_send_ptr[1]);
	*/
		


	
		/*if test CASE3 please unmark msleep func and modify sleep time,
		CP side need not  to change*/
		//msleep(1000);	
	
		for(i=0;i<16;i++)
		{
			msleep(1000);
			for(j=0;j<2;j++)
			{	
				msleep(1000);
				test_sdio_send_func(i,0x200<<j,data_send_ptr[i]);
			}

			SDIOTRAN_ERR("chn %d test finish!!!",i);
		}
		
		SDIOTRAN_ERR("sdio test write finish!!!");	
		break;
	}	
	
	return 0;
}

void gaole_creat_test(void)
{
	struct task_struct * task;
	int i,j,k;
	for(i=0;i<16;i++){
	
			data_send_ptr[i] = kmalloc(SEND_BUF_LEN,GFP_KERNEL);
			if (NULL == data_send_ptr[i])
			{
				SDIOTRAN_ERR("kmalloc send buf%d err!!!",i);
				return -1;
			}
			
			SDIOTRAN_DEBUG("kmalloc send buf%d ok!!!",i);
	
			for(j=0,k=0;j<SEND_BUF_LEN;j++,k++)
				*(data_send_ptr[i]+j) = ((k+i)<=255?(k+i):(k=0,k+i));
		
		}

	task = kthread_create(gaole_test_thread, NULL, "GaoleTest");
	if(0 != task)
		wake_up_process(task);
}


#endif//SDIO_TEST_CASE1

#if defined(SDIO_TEST_CASE2)

#define RECV_BUF_LEN 1024//SDIO_TEST_CASE2

char *data_recv_ptr[17] = {0};

int invalid_num = 0;

static int active_chn = 0;
static int last_chn = 1;
static int dummy_recv = 0;

static int read_datalen = 0;

void test_sdio_recv_func(void)
{
	int i,j;
	SDIOTRAN_DEBUG("entry");

	if((active_chn = sdio_dev_get_read_chn()) == -1)
	{
		invalid_num++;
		return;
	}
	
	if(last_chn == active_chn)
	{		
		dummy_recv++;
	}
	else
	{
		last_chn = active_chn;
		dummy_recv--;
	}

	for(j=0;j<RECV_BUF_LEN;j++)
		*(data_recv_ptr[active_chn]+j) = 0;

	sdio_dev_read(active_chn,(data_recv_ptr[active_chn]+10),&read_datalen);
	
	SDIOTRAN_ERR("ap recv: active_chn is %d!!!",active_chn);
	SDIOTRAN_ERR("ap recv: read_datalen is 0x%x!!!",read_datalen);

	SDIOTRAN_ERR("ap recv data: %d,%d,%d,%d  %d,%d,%d,%d  %d,%d,%d,%d!!!",\
		*((data_recv_ptr[active_chn]+10)+read_datalen-9),\
		*((data_recv_ptr[active_chn]+10)+read_datalen-8),\
		*((data_recv_ptr[active_chn]+10)+read_datalen-7),\
		*((data_recv_ptr[active_chn]+10)+read_datalen-6), \
		*((data_recv_ptr[active_chn]+10)+read_datalen-5), \
		*((data_recv_ptr[active_chn]+10)+read_datalen-4), \
		*((data_recv_ptr[active_chn]+10)+read_datalen-3), \
		*((data_recv_ptr[active_chn]+10)+read_datalen-2), \
		*((data_recv_ptr[active_chn]+10)+read_datalen-1), \
		*((data_recv_ptr[active_chn]+10)+read_datalen), \
		*((data_recv_ptr[active_chn]+10)+read_datalen+1), \
		*((data_recv_ptr[active_chn]+10)+read_datalen+2) \
		);

	active_chn = 0;
	read_datalen = 0;
}

static int gaole_test_thread(void *data)
{
	int i,j;

	struct sched_param param = {.sched_priority = 90};

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);

	for(i=0;i<16;i++){

		data_recv_ptr[i] = kmalloc(RECV_BUF_LEN,GFP_KERNEL);
		if (NULL == data_recv_ptr[i])
		{
			SDIOTRAN_ERR("kmalloc recv buf%d err!!!",i);
			return -1;
		}
		
		SDIOTRAN_DEBUG("kmalloc recv buf%d ok!!!",i);

		for(j=0;j<RECV_BUF_LEN;j++)
			*(data_recv_ptr[i]+j) = 0;
	
	}
	
	set_blklen(512); 

	while(1)
	{

		test_sdio_recv_func();

		
		if(dummy_recv > 50)
			break;
		
		if(invalid_num > 50)
			break;
	}	
	
	return 0;
}

void gaole_creat_test(void)
{
	struct task_struct * task;

	task = kthread_create(gaole_test_thread, NULL, "GaoleTest");
	if(0 != task)
		wake_up_process(task);
}



#endif//SDIO_TEST_CASE2



#if defined(SDIO_TEST_CASE4)

#define BUF_LEN 1024//SDIO_TEST_CASE4

char *data_ptr[10] = {0};

bool continue_send = 1;

static int test_send_chn = 0;



static void test_sdio_send_func(int chn,void* data_buf)
{

	uint32 datalen;

	char *tmp = (char*)data_buf;
	
	//SDIOTRAN_ERR("\nrun to test_sdio_send_func!!!\n");

	while(!sdio_dev_chn_idle(chn));	

	while(1)
	{
		datalen = sdio_dev_get_chn_datalen(chn);
		if(datalen)
		{
			SDIOTRAN_ERR("channel %d len is =%d!!!",chn,datalen);
			break;
		}
		else
			SDIOTRAN_ERR("channel %d len is =%d!!!",chn,datalen);

	}
	
	SDIOTRAN_ERR("ap %d send data: %d,%d,%d,%d  %d,%d,%d,%d  %d,%d,%d,%d!!!",chn,\
		tmp[0],\
		tmp[1],\
		tmp[2],\
		tmp[3], \
		tmp[101], \
		tmp[102], \
		tmp[103], \
		tmp[104], \
		tmp[230], \
		tmp[231], \
		tmp[232], \
		tmp[233] \
		);

	sdio_dev_write(chn,data_buf,datalen);

}

void case4_workq_handler(void)
{
	int i;
	
	int read_chn = 0;
	int datalen = 0;

	read_chn = sdio_dev_get_read_chn();
	
	if(-1 == read_chn)
		return;
	

	SDIOTRAN_DEBUG("ap recv: active_chn is %d!!!",read_chn);
	

	for(i=0;i<BUF_LEN;i++)
		*(data_ptr[read_chn-7]+i) = 0;


	sdio_dev_read(read_chn,data_ptr[read_chn-7],&datalen);
	
	SDIOTRAN_ERR("ap recv chn %d data: %d,%d,%d,%d  %d,%d,%d,%d  %d,%d,%d,%d!!!",read_chn,\
		*(data_ptr[read_chn-7]+0),\
		*(data_ptr[read_chn-7]+1),\
		*(data_ptr[read_chn-7]+2),\
		*(data_ptr[read_chn-7]+3), \
		*(data_ptr[read_chn-7]+100), \
		*(data_ptr[read_chn-7]+101), \
		*(data_ptr[read_chn-7]+102), \
		*(data_ptr[read_chn-7]+103), \
		*(data_ptr[read_chn-7]+230), \
		*(data_ptr[read_chn-7]+231), \
		*(data_ptr[read_chn-7]+232), \
		*(data_ptr[read_chn-7]+233) \
		);	
	
	continue_send = 1;
}




static int gaole_test_thread(void *data)
{
	int i,j,k;

	struct sched_param param = {.sched_priority = 90};

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);
	
	for(i=0;i<10;i++){

		data_ptr[i] = kmalloc(BUF_LEN,GFP_KERNEL);
		if (NULL == data_ptr[i])
		{
			SDIOTRAN_ERR("kmalloc send buf%d err!!!",i);
			return -1;
		}
		
		SDIOTRAN_DEBUG("kmalloc send buf%d ok!!!",i);

		for(j=0;j<BUF_LEN;j++)
			*(data_ptr[i]+j) = 0;
	
	}
	
	for(j=0,k=0;j<BUF_LEN;j++,k++)
		*(data_ptr[0]+j) = (k<=255?k:(k=0,k));

	while(1)
	{

		if(1 == continue_send)
		{
			continue_send = 0;
			test_sdio_send_func(test_send_chn,data_ptr[test_send_chn]);
			

			test_send_chn++;
			if(8 == test_send_chn)
			{
				test_send_chn = 0;				
			}
		}
		else
			msleep(1);
	}	
	return 0;
}


void gaole_creat_test(void)
{
	struct task_struct * task;

	task = kthread_create(gaole_test_thread, NULL, "GaoleTest");
	if(0 != task)
		wake_up_process(task);
}


#endif//SDIO_TEST_CASE4


#if defined(SDIO_TEST_CASE5)

#define TEST_BUF_LEN 8192 //SDIO_TEST_CASE5

#define TEST_SEND_CHN 0

char *data_test_ptr[17] = {0};


int active_chn = 0;
int read_datalen = 0;

void case5_workq_handler(void)
{
	int i;
	
	active_chn = 0;
	read_datalen = 0;

	active_chn = sdio_dev_get_read_chn();
	
	if(-1 == active_chn)
		return;
	
	SDIOTRAN_DEBUG("ap recv: active_chn is %d!!!",active_chn);
	

	for(i=0;i<TEST_BUF_LEN;i++)
		*(data_test_ptr[active_chn]+i) = 0;


	sdio_dev_read(active_chn,data_test_ptr[active_chn],&read_datalen);
	
	SDIOTRAN_ERR("ap recv chn %d data: %d,%d,%d,%d  %d,%d,%d,%d  %d,%d,%d,%d!!!",active_chn,\
		*(data_test_ptr[active_chn]+0),\
		*(data_test_ptr[active_chn]+1),\
		*(data_test_ptr[active_chn]+2),\
		*(data_test_ptr[active_chn]+3), \
		*(data_test_ptr[active_chn]+100), \
		*(data_test_ptr[active_chn]+101), \
		*(data_test_ptr[active_chn]+102), \
		*(data_test_ptr[active_chn]+103), \
		*(data_test_ptr[active_chn]+230), \
		*(data_test_ptr[active_chn]+231), \
		*(data_test_ptr[active_chn]+232), \
		*(data_test_ptr[active_chn]+233) \
		);
}


static void test_sdio_send_func(void)
{

	uint32 datalen;
	
	SDIOTRAN_ERR("entry");

	while(!sdio_dev_chn_idle(TEST_SEND_CHN));	

	while(1)
	{
		datalen = sdio_dev_get_chn_datalen(TEST_SEND_CHN);
		if(datalen)
		{
			SDIOTRAN_ERR("channel %d len is =%d!!!",TEST_SEND_CHN,datalen);
			break;
		}
		else
			SDIOTRAN_ERR("channel %d len is =%d!!!",TEST_SEND_CHN,datalen);

	}
	
	SDIOTRAN_ERR("ap send data: %d,%d,%d,%d  %d,%d,%d,%d  %d,%d,%d,%d!!!",\
		*(data_test_ptr[active_chn]+0),\
		*(data_test_ptr[active_chn]+1),\
		*(data_test_ptr[active_chn]+2),\
		*(data_test_ptr[active_chn]+3), \
		*(data_test_ptr[active_chn]+100), \
		*(data_test_ptr[active_chn]+101), \
		*(data_test_ptr[active_chn]+102), \
		*(data_test_ptr[active_chn]+103), \
		*(data_test_ptr[active_chn]+230), \
		*(data_test_ptr[active_chn]+231), \
		*(data_test_ptr[active_chn]+232), \
		*(data_test_ptr[active_chn]+233)\
		);

	sdio_dev_write(TEST_SEND_CHN,data_test_ptr[active_chn],datalen);

}


static int gaole_test_thread(void *data)
{
	int i,j;

	struct sched_param param = {.sched_priority = 90};

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);

	for(i=0;i<16;i++){

		data_test_ptr[i] = kmalloc(TEST_BUF_LEN,GFP_KERNEL);
		if (NULL == data_test_ptr[i])
		{
			SDIOTRAN_ERR("kmalloc buf%d err!!!",i);
			return -1;
		}
		
		SDIOTRAN_ERR("kmalloc buf%d ok!!!",i);


	
	}

	while(1)
	{
		msleep(10);
		
		for(i=0,j=0;i<TEST_BUF_LEN;i++,j++)
			*(data_test_ptr[0]+i) = (j<=255?j:(j=0,j));		
	
		test_sdio_send_func();
		
		break;
	}	
	
	return 0;
}

void gaole_creat_test(void)
{
	struct task_struct * task;

	task = kthread_create(gaole_test_thread, NULL, "GaoleTest");
	if(0 != task)
		wake_up_process(task);
}
#endif//SDIO_TEST_CASE5

#if defined(SDIO_TEST_CASE6)

#define BUF_LEN 1024//SDIO_TEST_CASE6

char *data_ptr[10] = {0};

bool continue_send = 0;

static int test_send_chn = 0;

static void test_sdio_send_func(int chn,void* data_buf)
{

	uint32 datalen;
uint32 ret;
	char *tmp = (char*)data_buf;
	
	//SDIOTRAN_ERR("\nrun to test_sdio_send_func!!!\n");

	set_marlin_wakeup(chn,1);

	while(!sdio_dev_chn_idle(chn));	

	while(1)
	{
		datalen = sdio_dev_get_chn_datalen(chn);
		if(datalen)
		{
			SDIOTRAN_ERR("channel %d len is =%d!!",chn,datalen);
			break;
		}
		else
			SDIOTRAN_ERR("channel %d len is =%d!!",chn,datalen);

	}
	
	SDIOTRAN_ERR("ap %d send data: %d,%d,%d,%d  %d,%d,%d,%d  %d,%d,%d,%d!!!",chn,\
		tmp[0],\
		tmp[1],\
		tmp[2],\
		tmp[3], \
		tmp[101], \
		tmp[102], \
		tmp[103], \
		tmp[104], \
		tmp[230], \
		tmp[231], \
		tmp[232], \
		tmp[233] \
		);

	sdio_dev_write(chn,data_buf,datalen);
	set_marlin_sleep(chn,1);


}

void case6_workq_handler(void)
{
	int i;
	
	int read_chn = 0;
	int datalen = 0;

	read_chn = sdio_dev_get_read_chn();
	
	if(-1 == read_chn)
		return;	
	

	SDIOTRAN_ERR("ap recv: active_chn is %d!!!",read_chn);


	for(i=0;i<BUF_LEN;i++)
		*(data_ptr[read_chn-8]+i) = 0;


	sdio_dev_read(read_chn,data_ptr[read_chn-8],&datalen);
	
	SDIOTRAN_ERR("ap recv chn %d data: %d,%d,%d,%d  %d,%d,%d,%d  %d,%d,%d,%d!!!",read_chn,\
		*(data_ptr[read_chn-8]+0),\
		*(data_ptr[read_chn-8]+1),\
		*(data_ptr[read_chn-8]+2),\
		*(data_ptr[read_chn-8]+3), \
		*(data_ptr[read_chn-8]+100), \
		*(data_ptr[read_chn-8]+101), \
		*(data_ptr[read_chn-8]+102), \
		*(data_ptr[read_chn-8]+103), \
		*(data_ptr[read_chn-8]+230), \
		*(data_ptr[read_chn-8]+231), \
		*(data_ptr[read_chn-8]+232), \
		*(data_ptr[read_chn-8]+233) \
		);	
	
	continue_send = 1;
}




static int gaole_test_thread(void *data)
{
	int i,j,k;

	struct sched_param param = {.sched_priority = 90};

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);
	
	for(i=0;i<10;i++){

		data_ptr[i] = kmalloc(BUF_LEN,GFP_KERNEL);
		if (NULL == data_ptr[i])
		{
			SDIOTRAN_ERR("kmalloc send buf%d err!!!",i);
			return -1;
		}
		
		SDIOTRAN_DEBUG("kmalloc send buf%d ok!!!",i);

		for(j=0;j<BUF_LEN;j++)
			*(data_ptr[i]+j) = 0;
	
	}
	
	for(j=0,k=0;j<BUF_LEN;j++,k++)
		*(data_ptr[0]+j) = (k<=255?k:(k=0,k));

	while(1)
	{
		if(1 == continue_send)
		{
			continue_send = 0;
			msleep(1000);
			test_sdio_send_func(test_send_chn,data_ptr[test_send_chn]);
			

			test_send_chn++;
			if(8 == test_send_chn)
			{
				test_send_chn = 0;				
			}
		}
		else
			msleep(1);
	}	
	return 0;
}


void gaole_creat_test(void)
{
	struct task_struct * task;

	task = kthread_create(gaole_test_thread, NULL, "GaoleTest");
	if(0 != task)
		wake_up_process(task);
}

#endif//SDIO_TEST_CASE6

#if defined(SDIO_TEST_CASE7)//test speed

#define SEND_BUF_LEN 1024 //SDIO_TEST_CASE7

char *data_send_ptr[17] = {0};


static void test_sdio_send_func(int chn,void* data_buf)
{

	uint32 datalen;

	char *tmp = (char*)data_buf;
	
	//SDIOTRAN_ERR("\nrun to test_sdio_send_func!!!\n");

	while(!sdio_dev_chn_idle(chn));	

	while(1)
	{
		datalen = sdio_dev_get_chn_datalen(chn);
		if(datalen)
		{
			//SDIOTRAN_ERR("channel %d len is =%d!!\r\n",chn,datalen);
			break;
		}
		else
			;//SDIOTRAN_ERR("channel %d len is =%d!!\r\n",chn,datalen);

	}
	/*
	SDIOTRAN_ERR("\nap send data: %d,%d,%d,%d  %d,%d,%d,%d  %d,%d,%d,%d!!!\n",\
		tmp[0],\
		tmp[1],\
		tmp[2],\
		tmp[3], \
		tmp[101], \
		tmp[102], \
		tmp[103], \
		tmp[104], \
		tmp[230], \
		tmp[231], \
		tmp[232], \
		tmp[233] \
		);*/

	sdio_dev_write(chn,data_buf,datalen);

}



static int gaole_test_thread(void *data)
{
	int i,j,k;
	
	static struct timeval start_time;
	static struct timeval stop_time;
	static signed long long elapsed_time;

	struct sched_param param = {.sched_priority = 90};

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);
	
	for(i=0;i<16;i++){

		data_send_ptr[i] = kmalloc(SEND_BUF_LEN,GFP_KERNEL);
		if (NULL == data_send_ptr[i])
		{
			SDIOTRAN_ERR("kmalloc send buf%d err!!!",i);
			return -1;
		}
		
		SDIOTRAN_DEBUG("kmalloc send buf%d ok!!!",i);

		for(j=0,k=0;j<SEND_BUF_LEN;j++,k++)
			*(data_send_ptr[i]+j) = ((k+i)<=255?(k+i):(k=0,k+i));
	
	}
	//set_blklen(1); 


	while(1)
	{
		for(i=0;i<16;i++)
		{
			do_gettimeofday(&start_time);
			
			for(j=0;j<100;j++)
				test_sdio_send_func(i,data_send_ptr[i]);

			do_gettimeofday(&stop_time);
			elapsed_time = timeval_to_ns(&stop_time) - timeval_to_ns(&start_time);
			SDIOTRAN_ERR("Chn %d test elapsed_time is %lld!!!",i,elapsed_time);
			elapsed_time = 0;
		}
		break;
		
		SDIOTRAN_ERR("sdio test write finish!!!");		
	}	
	
	return 0;
}

void gaole_creat_test(void)
{
	struct task_struct * task;

	task = kthread_create(gaole_test_thread, NULL, "GaoleTest");
	if(0 != task)
		wake_up_process(task);
}


#endif//SDIO_TEST_CASE7

#if defined(SDIO_TEST_CASE8)

#define TEST_BUF_LEN 8192 //SDIO_TEST_CASE8

char *data_test_ptr = NULL;

static int gaole_test_count = 0;

static struct timeval start_time;
static struct timeval stop_time;
static signed long long elapsed_time;

static int last_chn = 0;
bool stop_test = 0;

void test_sdio_recv_func(void)
{
	int i;	
	int read_chn = 0;
	int datalen = 0;



	//SDIOTRAN_ERR("\nap recv: active_chn is 0x%x!!!\n",active_chn);
	

	if(last_chn != read_chn)
	{
		last_chn = read_chn;
		gaole_test_count = 0;

	}
	gaole_test_count ++;
	
		

	//SDIOTRAN_ERR("\nap recv: read_datalen is 0x%x!!!\n",read_datalen);

	//for(i=0;i<TEST_BUF_LEN;i++)
	//	data_test_ptr[i] = 0;

	sdio_dev_read(read_chn,data_test_ptr,&datalen);

	if(1 == gaole_test_count)
		do_gettimeofday(&start_time);	

	
	if(100 == gaole_test_count)
	{
		do_gettimeofday(&stop_time);
		elapsed_time = timeval_to_ns(&stop_time) - timeval_to_ns(&start_time);
		SDIOTRAN_ERR("Chn %d test elapsed_time is %lld!!!",read_chn,elapsed_time);

		elapsed_time = 0;
		if(15 == read_chn)
			stop_test = 1;
	}
	/*
	SDIOTRAN_ERR("\ncase7:ap recv data: %d,%d,%d,%d  %d,%d,%d,%d  %d,%d,%d,%d!!!\n",\
		data_test_ptr[0],\
		data_test_ptr[1],\
		data_test_ptr[2],\
		data_test_ptr[3], \
		data_test_ptr[101], \
		data_test_ptr[102], \
		data_test_ptr[103], \
		data_test_ptr[104], \
		data_test_ptr[251], \
		data_test_ptr[252], \
		data_test_ptr[253], \
		data_test_ptr[254] \
		);*/
}


static void test_sdio_send_func(void)
{

	uint32 datalen;
	
	SDIOTRAN_ERR("entry");

	while(!sdio_dev_chn_idle(0));	

	while(1)
	{
		datalen = sdio_dev_get_chn_datalen(0);
		if(datalen)
		{
			SDIOTRAN_ERR("channel %d len is =%d!!",0,datalen);
			break;
		}
		else
			SDIOTRAN_ERR("channel %d len is =%d!!",0,datalen);

	}
	
	SDIOTRAN_ERR("ap send data: %d,%d,%d,%d  %d,%d,%d,%d  %d,%d,%d,%d!!!",\
		data_test_ptr[0],\
		data_test_ptr[1],\
		data_test_ptr[2],\
		data_test_ptr[3], \
		data_test_ptr[101], \
		data_test_ptr[102], \
		data_test_ptr[103], \
		data_test_ptr[104], \
		data_test_ptr[251], \
		data_test_ptr[252], \
		data_test_ptr[253], \
		data_test_ptr[254] \
		);

	sdio_dev_write(0,data_test_ptr,datalen);

}


static int gaole_test_thread(void *data)
{
	int i,j;

	struct sched_param param = {.sched_priority = 90};

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);

	data_test_ptr = kmalloc(TEST_BUF_LEN,GFP_KERNEL);
	if (NULL == data_test_ptr)
	{
		SDIOTRAN_ERR("kmalloc send buf err!!!");
		return -1;
	}
	
	SDIOTRAN_DEBUG("kmalloc send buf ok!!!");

	for(i=0,j=0;i<TEST_BUF_LEN;i++,j++)
		data_test_ptr[i] = (j<=255?j:(j=0,j));	

	test_sdio_send_func();


	while(1)
	{
		test_sdio_recv_func();
		
		if(1 == stop_test)
			break;		
	}	
	
	return 0;
}

void gaole_creat_test(void)
{
	struct task_struct * task;

	task = kthread_create(gaole_test_thread, NULL, "GaoleTest");
	if(0 != task)
		wake_up_process(task);
}


#endif//SDIO_TEST_CASE8


#if defined(SDIO_TEST_CASE9)

#define TEST_BUF_LEN 8192 //SDIO_TEST_CASE9

#define TEST_SEND_CHN 0


char *data_test_ptr = NULL;


int active_chn = 0;
int read_datalen = 0;
static int gaole_test_count = 0;

static struct timeval start_time;
static struct timeval stop_time;
static signed long long elapsed_time;

static int last_chn = 0;

void case9_workq_handler(void)
{
	int i;
	
	active_chn = 0;
	read_datalen = 0;

	active_chn = sdio_dev_get_read_chn();
	//SDIOTRAN_ERR("\nap recv: active_chn is 0x%x!!!\n",active_chn);
	
	if(-1 == active_chn)
		return;

	if(last_chn != active_chn)
	{
		last_chn = active_chn;
		gaole_test_count = 0;

	}
	gaole_test_count ++;
	
	

	//SDIOTRAN_ERR("\nap recv: read_datalen is 0x%x!!!\n",read_datalen);

	//for(i=0;i<TEST_BUF_LEN;i++)
	//	data_test_ptr[i] = 0;

	sdio_dev_read(active_chn,data_test_ptr,&read_datalen);

	if(2 == gaole_test_count)
		do_gettimeofday(&start_time);	

	
	if(101 == gaole_test_count)
	{
		do_gettimeofday(&stop_time);
		elapsed_time = timeval_to_ns(&stop_time) - timeval_to_ns(&start_time);
		SDIOTRAN_ERR("Chn %d test elapsed_time is %lld!!!",active_chn,elapsed_time);
		elapsed_time = 0;
	}
	/*
	SDIOTRAN_ERR("\ncase7:ap recv data: %d,%d,%d,%d  %d,%d,%d,%d  %d,%d,%d,%d!!!\n",\
		data_test_ptr[0],\
		data_test_ptr[1],\
		data_test_ptr[2],\
		data_test_ptr[3], \
		data_test_ptr[101], \
		data_test_ptr[102], \
		data_test_ptr[103], \
		data_test_ptr[104], \
		data_test_ptr[251], \
		data_test_ptr[252], \
		data_test_ptr[253], \
		data_test_ptr[254] \
		);*/
}


static void test_sdio_send_func(void)
{

	uint32 datalen;
	
	SDIOTRAN_ERR("entry");

	while(!sdio_dev_chn_idle(TEST_SEND_CHN));	

	while(1)
	{
		datalen = sdio_dev_get_chn_datalen(TEST_SEND_CHN);
		if(datalen)
		{
			SDIOTRAN_ERR("channel %d len is =%d!!",TEST_SEND_CHN,datalen);
			break;
		}
		else
			SDIOTRAN_ERR("channel %d len is =%d!!",TEST_SEND_CHN,datalen);

	}
	
	SDIOTRAN_ERR("ap send data: %d,%d,%d,%d  %d,%d,%d,%d  %d,%d,%d,%d!!!",\
		data_test_ptr[0],\
		data_test_ptr[1],\
		data_test_ptr[2],\
		data_test_ptr[3], \
		data_test_ptr[101], \
		data_test_ptr[102], \
		data_test_ptr[103], \
		data_test_ptr[104], \
		data_test_ptr[251], \
		data_test_ptr[252], \
		data_test_ptr[253], \
		data_test_ptr[254] \
		);

	sdio_dev_write(TEST_SEND_CHN,data_test_ptr,datalen);

}


static int gaole_test_thread(void *data)
{
	int i,j;

	struct sched_param param = {.sched_priority = 90};

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);

	data_test_ptr = kmalloc(TEST_BUF_LEN,GFP_KERNEL);
	if (NULL == data_test_ptr)
	{
		SDIOTRAN_ERR("kmalloc send buf err!!!");
		return -1;
	}
	
	SDIOTRAN_DEBUG("kmalloc send buf ok!!!");

	for(i=0,j=0;i<TEST_BUF_LEN;i++,j++)
		data_test_ptr[i] = (j<=255?j:(j=0,j));	


	while(1)
	{
		msleep(10);
		test_sdio_send_func();
		break;
	}	
	
	return 0;
}

void gaole_creat_test(void)
{
	struct task_struct * task;

	task = kthread_create(gaole_test_thread, NULL, "GaoleTest");
	if(0 != task)
		wake_up_process(task);
}
#endif//SDIO_TEST_CASE9


