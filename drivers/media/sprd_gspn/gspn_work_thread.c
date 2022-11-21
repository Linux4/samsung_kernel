/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "gspn_drv.h"

/*
gspn_kcmd_leak_check
description: in ioctl, if user process killed after get kcmd from empty but before put to fill,
                 the kcmd maybe dissociate from managed list, maybe there is the other scenario exsit,
                 that cause kcmd leak, this function check this case and recover it.
*/
static void gspn_kcmd_leak_check(GSPN_CONTEXT_T *gspnCtx)
{
    int32_t i = 0;
    /*if empty-list-cnt not equal the total-kcmd-cnt, reinitialize the kcmd list*/
    if(gspnCtx->empty_list_cnt != GSPN_KCMD_MAX) {
        GSPN_LOGW("empty cnt %d != total cnt %d\n", gspnCtx->empty_list_cnt, GSPN_KCMD_MAX);

        /*cmd list initialize*/
        INIT_LIST_HEAD(&gspnCtx->empty_list);
        i = 0;
        while(i < GSPN_KCMD_MAX) {
            list_add_tail(&gspnCtx->kcmds[i].list, &gspnCtx->empty_list);
            i++;
        }
        INIT_LIST_HEAD(&gspnCtx->fill_list);
        gspnCtx->empty_list_cnt = GSPN_KCMD_MAX;
        gspnCtx->fill_list_cnt = 0;
        sema_init(&gspnCtx->empty_list_sema, 1);
        sema_init(&gspnCtx->fill_list_sema, 1);
        init_waitqueue_head(&gspnCtx->empty_list_wq);
        init_waitqueue_head(&gspnCtx->sync_done_wq);
        INIT_LIST_HEAD(&gspnCtx->core_mng.dissociation_list);
    }
}

static void gspn_suspend_process(GSPN_CONTEXT_T *gspnCtx)
{
    GSPN_KCMD_INFO_T *pKCMD = NULL;
    struct list_head temp_list;
    uint32_t record_cnt = 0;
    uint32_t real_cnt = 0;
    int32_t i = 0;

    if(gspnCtx->suspend_flag) {
        /*temp_list = fill_list*/
        down(&gspnCtx->fill_list_sema);
        list_add_tail(&temp_list, &gspnCtx->fill_list);
        list_del(&gspnCtx->fill_list);
        INIT_LIST_HEAD(&gspnCtx->fill_list);
        record_cnt = gspnCtx->fill_list_cnt;
        gspnCtx->fill_list_cnt = 0;
        up(&gspnCtx->fill_list_sema);

        GSPN_LOGI("get list from fill-list ok.\n");

        list_for_each_entry(pKCMD, &temp_list, list) {
            if(pKCMD->src_cmd.misc_info.async_flag) {
                /*put all acq fence */
                while(i < pKCMD->acq_fence_cnt) {
                    sync_fence_put(pKCMD->acq_fence[i]);
                    i++;
                }
                /*signal release fence*/
                down(&gspnCtx->fence_create_sema);
                gspnCtx->remain_async_cmd_cnt--;
                sprd_fence_signal(gspnCtx->timeline);
                up(&gspnCtx->fence_create_sema);
            } else {
                (*(pKCMD->done_cnt))++;
                wake_up_interruptible_all(&(gspnCtx->sync_done_wq));
            }
            real_cnt++;
        }
        GSPN_LOGI("fill-list fence process ok.\n");

        if(real_cnt != record_cnt) {
            GSPN_LOGW("real cnt %d != %d!\n",real_cnt,record_cnt);
        }

        /*add temp_list to empty_list*/
        gspn_put_list_to_empty_list(gspnCtx,&temp_list);
        GSPN_LOGI("put fill-list to empty-list ok.\n");

        /*if all cores done, notify suspend thread*/
        if(GSPN_ALL_CORE_FREE(gspnCtx)) {
            /*if all core free, and dissociation list is not empty, move it to empty list*/
            if(gspnCtx->core_mng.dissociation_list.next != &gspnCtx->core_mng.dissociation_list) {
                gspn_put_list_to_empty_list(gspnCtx,&gspnCtx->core_mng.dissociation_list);
                INIT_LIST_HEAD(&gspnCtx->core_mng.dissociation_list);
            }

            gspn_kcmd_leak_check(gspnCtx);
            GSPN_PRINTK("suspend done.\n");

            up(&gspnCtx->suspend_clean_done_sema);
        }
    }
}


static void gspn_recalculate_timeout(GSPN_CONTEXT_T *gspnCtx)
{
    struct timespec startTime = {-1UL,-1UL};// the earliest start core
    struct timespec currentTime = {-1UL,-1UL};
    long long elapse = 0;
    int32_t i = 0;

    if(gspnCtx->fill_list_cnt == 0) {
        if(GSPN_ALL_CORE_FREE(gspnCtx)) {
            gspnCtx->timeout = 0xffffffff;// set to max to wait cmd come
            GSPN_LOGI("fill-list empty and all cores free, set time to infinite.\n");
            return;
        }
    } else {
        if(gspnCtx->core_mng.free_cnt > 0) {
            gspnCtx->timeout = 1;
            GSPN_LOGI("fill-list not empty and have core free, wake work thread to process.\n");
            up(&gspnCtx->wake_work_thread_sema);
            return;
        }
    }

    /*get the earliest start time*/
    i = 0;
    while(i<gspnCtx->core_mng.core_cnt) {
        if((gspnCtx->core_mng.cores[i].status == GSPN_CORE_BUSY)
           &&(gspnCtx->core_mng.cores[i].start_time.tv_sec < startTime.tv_sec
              ||(gspnCtx->core_mng.cores[i].start_time.tv_sec == startTime.tv_sec
                 &&gspnCtx->core_mng.cores[i].start_time.tv_nsec < startTime.tv_nsec))) {
            startTime.tv_sec = gspnCtx->core_mng.cores[i].start_time.tv_sec;
            startTime.tv_nsec = gspnCtx->core_mng.cores[i].start_time.tv_nsec;
			 GSPN_LOGE("2222, status busy enter!gspnCtx->core_mng.cores[i].status = %d", gspnCtx->core_mng.cores[i].status);
        }
        i++;
    }
	i = 0;
	while(i<gspnCtx->core_mng.core_cnt) {
        if(gspnCtx->core_mng.cores[i].status == GSPN_CORE_BUSY) {
	    get_monotonic_boottime(&currentTime);
	    /*re-calculate the earliest timeout*/
	    elapse = (currentTime.tv_sec - startTime.tv_sec)*1000000000 + currentTime.tv_nsec - startTime.tv_nsec;
		GSPN_LOGE("elapse = %d, currentTime.tv_nsec = %d, startTime.tv_nsec = %d, currentTime.tv_sec = %d, startTime.tv_sec = %d", 
			elapse,currentTime.tv_nsec, startTime.tv_nsec, currentTime.tv_sec, startTime.tv_sec);    
		if(elapse < MS_TO_NS(GSPN_CMD_EXE_TIMEOUT)) {
	        gspnCtx->timeout = NS_TO_US(MS_TO_NS(GSPN_CMD_EXE_TIMEOUT) - elapse);
	        GSPN_LOGI("set timeout %d us .\n", gspnCtx->timeout);
	    } else { // time is up
	        gspnCtx->timeout = 0;
	        //GSPN_TryToRecover(i,gspnCtx->core_mng.cores[i].ctl_reg_base);//recovery core i
	        //up(gspnCtx->wake_work_thread_sema);
	        GSPN_LOGI("time up! set 0 to leave this case to timeout_process().\n");
		}
    }
	i++;
  }
}


static GSPN_ERR_CODE_E gspn_iommu_map(GSPN_CONTEXT_T *gspnCtx,GSPN_KCMD_INFO_T *kcmd)
{
    struct ion_addr_data data;

#define GSPN_LAYER_MAP(lx_info)\
    if(lx_info.layer_en == 1 && lx_info.addr.plane_y == 0 && lx_info.addr_info.share_fd) {\
        data.dmabuf = lx_info.addr_info.dmabuf;\
        if(sprd_ion_get_gsp_addr(&data)) {\
            GSPN_LOGE("map failed!\n");\
            return GSPN_K_IOMMU_MAP_ERR;\
        }\
        if(data.iova_enabled) {\
            lx_info.addr.plane_y = data.iova_addr;\
        } else {\
            lx_info.addr.plane_y = data.phys_addr;\
        }\
        lx_info.addr.plane_u= lx_info.addr.plane_y + lx_info.addr_info.uv_offset;\
        lx_info.addr.plane_v= lx_info.addr.plane_y + lx_info.addr_info.v_offset;\
    }

    GSPN_LAYER_MAP(kcmd->src_cmd.l0_info);
    GSPN_LAYER_MAP(kcmd->src_cmd.l1_info);
    GSPN_LAYER_MAP(kcmd->src_cmd.l2_info);
    GSPN_LAYER_MAP(kcmd->src_cmd.l3_info);
    GSPN_LAYER_MAP(kcmd->src_cmd.des1_info);
    GSPN_LAYER_MAP(kcmd->src_cmd.des2_info);

    return GSPN_NO_ERR;
}

static int gspn_iommu_unmap(GSPN_CONTEXT_T *gspnCtx,GSPN_KCMD_INFO_T *kcmd)
{
#define GSPN_LAYER_UNMAP(lx_info)\
    if(lx_info.addr_info.share_fd) {\
        sprd_ion_free_gsp_addr(lx_info.addr_info.share_fd);\
    }
    GSPN_LAYER_UNMAP(kcmd->src_cmd.l0_info);
    GSPN_LAYER_UNMAP(kcmd->src_cmd.l1_info);
    GSPN_LAYER_UNMAP(kcmd->src_cmd.l2_info);
    GSPN_LAYER_UNMAP(kcmd->src_cmd.l3_info);
    GSPN_LAYER_UNMAP(kcmd->src_cmd.des1_info);
    GSPN_LAYER_UNMAP(kcmd->src_cmd.des2_info);
    return 0;
}


/*
gspn_frame_post_process
description:if frame list done, move these kcmds from dissociation list to empty list
*/
static int32_t gspn_frame_post_process(GSPN_CONTEXT_T *gspnCtx, GSPN_KCMD_INFO_T *kcmd)
{
    int32_t ret = 1;
    struct list_head frame_list;
    if(gspnCtx->suspend_flag == 0) {
        list_add_tail(&frame_list, &kcmd->list_frame);
        list_for_each_entry(kcmd, &frame_list, list_frame) {
            if(kcmd->done_flag == 0) {
                ret = 0;
                break;
            }
        }

        if(ret == 1) { // all done
            GSPN_LOGI("frame done.\n");
            list_for_each_entry(kcmd, &frame_list, list_frame) {
				 GSPN_LOGE("kcmd->list = %p, kcmd = %p\n", &kcmd->list, kcmd);
                list_del(&kcmd->list);//break from dissociation_list
			     list_add_tail(&kcmd->list, &gspnCtx->empty_list);
				 gspnCtx->empty_list_cnt++;
				 up(&gspnCtx->empty_list_sema);
                wake_up_interruptible_all(&(gspnCtx->empty_list_wq));

        		// list_add_tail(&kcmd->list, &frame_list);
            }

            //gspn_put_list_to_empty_list(gspnCtx,&frame_list); // hl chagned 0420
        }
        list_del(&frame_list);
    } else {
        INIT_LIST_HEAD(&frame_list);
        list_del(&kcmd->list);//break from dissociation_list
        list_add_tail(&kcmd->list, &frame_list);
        gspn_put_list_to_empty_list(gspnCtx,&frame_list);
    }
    return ret;
}

/*
func:gspn_calc_exe_time
desc:calc and return the execute time of core, in us unit.
*/
static uint32_t gspn_calc_exe_time(GSPN_CORE_T *cores)
{
    struct timespec startTime = {-1UL,-1UL};// the earliest start core
    struct timespec currentTime = {-1UL,-1UL};
    long long elapse = 0;

    get_monotonic_boottime(&currentTime);
    startTime.tv_sec = cores->start_time.tv_sec;
    startTime.tv_nsec = cores->start_time.tv_nsec;
    elapse = (currentTime.tv_sec - startTime.tv_sec)*1000000000 + currentTime.tv_nsec - startTime.tv_nsec;
    return NS_TO_US(elapse);
}

static void gspn_kcmd_post_process(GSPN_CONTEXT_T *gspnCtx)
{
    GSPN_KCMD_INFO_T *kcmd = NULL;
    GSPN_CTL_REG_T *base = NULL;
    int32_t index = -1;
    int32_t i = 0;
    GSPN_LOGI("1111enter.gspnCtx->core_mng.core_cnt = %d\n", gspnCtx->core_mng.core_cnt);

    while(i<gspnCtx->core_mng.core_cnt) {
        kcmd = gspnCtx->core_mng.cores[i].current_kcmd;
        base = gspnCtx->core_mng.cores[i].ctl_reg_base;
        index = gspnCtx->core_mng.cores[i].current_kcmd_sub_cmd_index;
        if(kcmd != NULL && !GSPN_MOD1_BUSY_GET(base) && !GSPN_MOD2_BUSY_GET(base)) {
            GSPN_LOGI("core %d done.\n", i);

            /*unbind kcmd and core*/
            gspnCtx->core_mng.cores[i].current_kcmd = NULL;
            gspnCtx->core_mng.cores[i].status = GSPN_CORE_FREE;
            gspnCtx->core_mng.free_cnt++;
            if(-1 < index && index < GSPN_SPLIT_PARTS_MAX) { // split case
                /*unbind sub_cmd and core*/
                kcmd->sub_cmd_occupied_core[index] = NULL;
                gspnCtx->core_mng.cores[i].current_kcmd_sub_cmd_index = -1;

                /*set kcmd part done, and check all parts done*/
                kcmd->sub_cmd_done_cnt++;
                GSPN_LOGI("%d/%d done.\n", kcmd->sub_cmd_done_cnt, kcmd->sub_cmd_total);
                if(kcmd->sub_cmd_done_cnt < kcmd->sub_cmd_total) {
                    i++;
                    continue;//only part of kcmd done, so kcmd can't put to empty
                }
            } else { // whole kcmd done
                GSPN_LOGI("whole kcmd done.\n");
                GSPN_LOGP("cost %d us.\n", gspn_calc_exe_time(&gspnCtx->core_mng.cores[i]));
                kcmd->occupied_core = NULL;
            }

            kcmd->done_flag = 1;
            gspn_iommu_unmap(gspnCtx,kcmd);// must before gspn disable
            GSPN_CoreDisable(&gspnCtx->core_mng.cores[i]);
            /*sync process*/
            if(kcmd->src_cmd.misc_info.async_flag) {
                GSPN_LOGI("bf signal fence,remain_cnt:%d, time_line:%d\n",
                          gspnCtx->remain_async_cmd_cnt,gspnCtx->timeline->timeline->value);
                /*signal release fence*/
                down(&gspnCtx->fence_create_sema);
                gspnCtx->remain_async_cmd_cnt--;
                sprd_fence_signal(gspnCtx->timeline);
                up(&gspnCtx->fence_create_sema);

            } else {
                GSPN_LOGI("wake up sync ioctl process, done_cnt:%d.\n", *(kcmd->done_cnt));
                (*(kcmd->done_cnt))++;
                wake_up_interruptible_all(&(gspnCtx->sync_done_wq));
            }

            /*if frame list done, move these kcmds from dissociation list to empty list*/
            gspn_frame_post_process(gspnCtx,kcmd);
        }

        i++;
    }

}

static GSPN_ERR_CODE_E gspn_multi_core_process(GSPN_CONTEXT_T *gspnCtx)
{
    GSPN_LOGE("enter.\n");
    return GSPN_K_PARAM_CHK_ERR;
}

/*
gspn_core_bind
description: occupy a free core and bind it with a kcmd
*/
static GSPN_ERR_CODE_E gspn_core_bind(GSPN_CONTEXT_T *gspnCtx,GSPN_KCMD_INFO_T *kcmd)
{
    GSPN_ERR_CODE_E ret = GSPN_NO_ERR;
    GSPN_CORE_T *core = NULL;
    int32_t i = 0;
    if(gspnCtx->core_mng.free_cnt > 0) {
        while(i < gspnCtx->core_mng.core_cnt) {
            if(gspnCtx->core_mng.cores[i].status == GSPN_CORE_FREE) {
                core = &gspnCtx->core_mng.cores[i];
                break;
            }
            i++;
        }

        if(core != NULL) {
            GSPN_LOGI("core %d is free, bind it with kcmd, free_cnt:%d.\n",
                      i, gspnCtx->core_mng.free_cnt);

            /*bind core and kcmd*/
            core->status = GSPN_CORE_OCCUPIED;
            gspnCtx->core_mng.free_cnt--;
            core->current_kcmd = kcmd;
            core->current_kcmd_sub_cmd_index = -1;
            kcmd->occupied_core = core;
            kcmd->sub_cmd_done_cnt = 0;
            kcmd->sub_cmd_total = 0;
        } else {
            GSPN_LOGE("can't get a free core! free cnt: %d\n",gspnCtx->core_mng.free_cnt);
            ret = GSPN_K_GET_FREE_CORE_ERR;
        }
    } else {
        GSPN_LOGW("can't get a free core! free cnt: %d\n",gspnCtx->core_mng.free_cnt);
        ret = GSPN_K_GET_FREE_CORE_ERR;
    }
    return ret;
}




static void gspn_coef_gen_and_cfg(GSPN_CONTEXT_T *gspnCtx,GSPN_KCMD_INFO_T *kcmd)
{
    uint32_t src_w = 0, src_h = 0;
    uint32_t dst_w = 0,dst_h = 0;
    uint32_t order_v = 0, order_h = 0;
    uint32_t ver_tap, hor_tap;
    uint32_t *ret_coef = NULL;
		
	//GSPN_LOGI("11111, kcmd_addr = 0x%x\n", kcmd);
    if(kcmd->src_cmd.misc_info.scale_en == 1) {
        src_w = kcmd->src_cmd.l0_info.clip_size.w;
        src_h = kcmd->src_cmd.l0_info.clip_size.h;
        if(kcmd->src_cmd.misc_info.run_mod == 0) {
            dst_w = kcmd->src_cmd.des1_info.scale_out_size.w;
            dst_h = kcmd->src_cmd.des1_info.scale_out_size.h;
            if(kcmd->src_cmd.misc_info.scale_seq) {
                src_w = kcmd->src_cmd.des1_info.work_src_size.w;
                src_h = kcmd->src_cmd.des1_info.work_src_size.h;
            }
        } else {
            dst_w = kcmd->src_cmd.des2_info.scale_out_size.w;
            dst_h = kcmd->src_cmd.des2_info.scale_out_size.h;
        }

	    //GSPN_LOGI("11111, kcmd_addr = 0x%x\n", kcmd);
        //printk("GSP coef, f:%d,%dx%d->%dx%d\n",s_gsp_cfg.layer0_info.img_format,coef_in_w,coef_in_h,coef_out_w,coef_out_h);
        if(GSPN_LAYER0_FMT_RGB888 < kcmd->src_cmd.l0_info.fmt
           && kcmd->src_cmd.l0_info.fmt < GSPN_LAYER0_FMT_RGB565
           && (src_w>dst_w||src_h>dst_h)/*video scaling down*/) {
            if(src_h*3 <= src_w*2/*width is larger than 1.5*height*/) {
                kcmd->src_cmd.misc_info.htap4_en = 1;
            }
            //printk("GSP, for video scaling down, we change tap to 2.\n");
        }

        if (kcmd->src_cmd.misc_info.htap4_en) {
            hor_tap = 4;
        } else {
            hor_tap = 8;
        }
        ver_tap = 4;

	    //GSPN_LOGI("11111, kcmd_addr = 0x%x\n", kcmd);
        if(dst_h*4 < src_h && src_h <= dst_h*8) {
            order_v = 1;
        } else if(dst_h*8 < src_h && src_h <= dst_h*16) {
            order_v = 2;
        }
        if(dst_w*4 < src_w && src_w <= dst_w*8) {
            order_h= 1;
        } else if(dst_w*8 < src_w && src_w <= dst_w*16) {
            order_h = 2;
        }
	    //GSPN_LOGI("11111, kcmd_addr = 0x%x\n", kcmd);
        src_w = src_w >> order_h;
        src_h = src_h >> order_v;
        ret_coef = gspn_gen_block_scaler_coef(gspnCtx,
                                              src_w, src_h,
                                              kcmd->src_cmd.des1_info.scale_out_size.w,
                                              kcmd->src_cmd.des1_info.scale_out_size.h,
                                              hor_tap, ver_tap);
	GSPN_LOGI("11111, kcmd_addr = 0x%x\n", kcmd);
        /*config the coef to register of the bind core of kcmd*/
        if(kcmd->occupied_core) {
            if((ulong)ret_coef & MEM_OPS_ADDR_ALIGN_MASK) {
                GSPN_LOGW("memcpy use none 8B alignment address!");
            }
			GSPN_LOGW("ctl_reg_base = %p, coef_tab = %p, kcmd->occupied_core->ctl_reg_base = %p, kcmd->ctl_reg_base->coef_tab = %p\n",
				gspnCtx->core_mng.cores->ctl_reg_base, &gspnCtx->core_mng.cores->ctl_reg_base->coef_tab,
				kcmd->occupied_core->ctl_reg_base, &kcmd->occupied_core->ctl_reg_base->coef_tab);
			gsp_memcpy((void*)&(kcmd->occupied_core->ctl_reg_base->coef_tab),(void*)ret_coef,48*4);
        } else {
            GSPN_LOGE("kcmd->occupied_core is NULL, can't get coef register addr!");
        }
    }
}

/*
gspn_reg_cfg
description: only support single core
*/
static GSPN_ERR_CODE_E gspn_reg_cfg(GSPN_CORE_MNG_T *core_mng, GSPN_KCMD_INFO_T *kcmd, GSPN_CAPABILITY_T *capability)
{
    int32_t ret = 0;
    ret = GSPN_InfoCfg(kcmd,-1);
	//GSPN_DumpReg(core_mng, kcmd->occupied_core->core_id);  //hl add for test
    if(ret==3 && capability->version == 0x10) {
        /*on sharklT8, scaling up and down on different direction will rise err flag,
        but case can be executed successfully.*/
    } else if(ret) {
        GSPN_DumpReg(core_mng, kcmd->occupied_core->core_id);
        return (ret<0)?GSPN_K_PARAM_CHK_ERR:(GSPN_ERR_CODE_E)ret;
    }
    if(kcmd->occupied_core) {
        kcmd->occupied_core->status = GSPN_CORE_CONFIGURED;
    }
    return GSPN_NO_ERR;
}


static int32_t gspn_wait_fence(GSPN_KCMD_INFO_T *kcmd)
{
    uint32_t i = 0;
    int32_t ret = 0;
    long timeout = GSPN_WAIT_FENCE_TIMEOUT;
    GSPN_LOGI("enter.\n");
    for(i=0; i<kcmd->acq_fence_cnt; i++) {
        ret = sync_fence_wait(kcmd->acq_fence[i],timeout);
        GSPN_LOGI("Wait Fence %d/%d %s.\n",i,kcmd->acq_fence_cnt,ret?"failed":"success");
        if(ret) {
            timeout = 1;//timeout occurs, decrease the next timeout
        }
        sync_fence_put(kcmd->acq_fence[i]);
    }
    GSPN_LOGI("exit.\n");
    return ret;
}


static GSPN_ERR_CODE_E gspn_kcmd_pre_process(GSPN_CONTEXT_T *gspnCtx)
{
    GSPN_ERR_CODE_E ret = GSPN_NO_ERR;
    GSPN_KCMD_INFO_T *kcmd = NULL;

	//INIT_LIST_HEAD(&gspnCtx->core_mng.dissociation_list);

    if(gspnCtx->fill_list_cnt > 0) {
        if(gspnCtx->core_mng.free_cnt > 1) {
            ret = gspn_multi_core_process(gspnCtx);
        } else if(gspnCtx->core_mng.free_cnt == 1) {
            GSPN_LOGI("fetch a kcmd from fill-list.\n");

            /*fetch a kcmd from fill list head, and add it to dissociation list tail*/
            down(&gspnCtx->fill_list_sema);
			// GSPN_LOGI("11111,  gspnCtx = 0x%x, kcmd_addr = 0x%x\n", kcmd, gspnCtx);
            kcmd = list_first_entry(&gspnCtx->fill_list, GSPN_KCMD_INFO_T, list);
			 //GSPN_LOGI("11111, &kcmd->list->next = 0x%x\n", kcmd->list.next);
			 //GSPN_LOGI("11111, &kcmd->list->prev = 0x%x\n", kcmd->list.prev);
			 //GSPN_LOGI("11111, kcmd_addr = 0x%x\n", kcmd);
            //list_add_tail(&kcmd->list, &gspnCtx->core_mng.dissociation_list); //hl change
            list_del(&kcmd->list);
			 //GSPN_LOGI("11111, &kcmd->list->next = 0x%x\n", kcmd->list.next);
			 //GSPN_LOGI("11111, &kcmd->list->prev = 0x%x\n", kcmd->list.prev);
			 //GSPN_LOGI("11111, kcmd_addr = 0x%x\n", kcmd);
            gspnCtx->fill_list_cnt--;
			 //GSPN_LOGI("11111, kcmd_addr = 0x%x\n", kcmd);
            up(&gspnCtx->fill_list_sema);
			 //GSPN_LOGI("11111, kcmd_addr = 0x%x\n", kcmd);
			 //GSPN_LOGI("11111, &kcmd->list = 0x%x\n", &kcmd->list);
			 //GSPN_LOGI("11111, &kcmd->list->next = 0x%x\n", kcmd->list.next);
			 //GSPN_LOGI("11111, &kcmd->list->prev = 0x%x\n", kcmd->list.prev);
			 //GSPN_LOGI("11111, &gspnCtx->core_mng.dissociation_list = 0x%x\n",  &gspnCtx->core_mng.dissociation_list);
			 //GSPN_LOGI("11111, gspnCtx->core_mng.dissociation_list.next = 0x%x\n",  gspnCtx->core_mng.dissociation_list.next);
			 //GSPN_LOGI("11111, gspnCtx->core_mng.dissociation_list.pre = 0x%x\n",  gspnCtx->core_mng.dissociation_list.prev);
            list_add_tail(&kcmd->list, &gspnCtx->core_mng.dissociation_list); //hl change
			 //GSPN_LOGI("11111, kcmd_addr = 0x%x\n", kcmd);
            ret = gspn_core_bind(gspnCtx,kcmd);
            if(ret) {
                GSPN_LOGE("bind core failed!");
                return ret;
            }
			 //GSPN_LOGI("11111, kcmd_addr = 0x%x\n", kcmd);
            ret = GSPN_CoreEnable(gspnCtx,kcmd->occupied_core->core_id);
            if(ret) {
                GSPN_LOGE("core enable failed!");
                return ret;
            }
			 //GSPN_LOGI("11111, kcmd_addr = 0x%x\n", kcmd);
            ret = gspn_iommu_map(gspnCtx,kcmd);
            if(ret) {
                GSPN_LOGE("iommu map failed!");
                return ret;
            }
			 //GSPN_LOGI("11111, kcmd_addr = 0x%x\n", kcmd);
            gspn_coef_gen_and_cfg(gspnCtx,kcmd);
			// GSPN_LOGI("11111, kcmd_addr = 0x%x\n", kcmd);
            ret = gspn_reg_cfg(&gspnCtx->core_mng,kcmd,&gspnCtx->capability);
            if(ret) {
                GSPN_LOGE("reg cfg failed!");
                return ret;
            }
			 //GSPN_LOGI("11111, kcmd_addr = 0x%x\n", kcmd);
            if(kcmd->src_cmd.misc_info.async_flag == 1) {
                ret = gspn_wait_fence(kcmd);
                if(ret) {
                    GSPN_LOGE("wait fence failed!");
                    return ret;
                }
            }
			 //GSPN_LOGI("11111, kcmd_addr = 0x%x\n", kcmd);
            ret = GSPN_Trigger(&gspnCtx->core_mng,kcmd->occupied_core->core_id);
            if(ret) {
                GSPN_LOGE("trigger failed!");
                return ret;
            } else {
                GSPN_LOGE("trigger success!");
                get_monotonic_boottime(&kcmd->occupied_core->start_time);
                kcmd->occupied_core->status = GSPN_CORE_BUSY;
                kcmd->occupied_core->current_kcmd_sub_cmd_index = -1;
            }
        }
    }
    return ret;
}


static void gspn_timeout_process(GSPN_CONTEXT_T *gspnCtx)
{
    struct timespec startTime = {-1UL,-1UL};// the earliest start core
    struct timespec currentTime = {-1UL,-1UL};
    long long elapse = 0;
    int32_t i = 0;

   // GSPN_LOGE("enter.\n");
    //if(!GSPN_ALL_CORE_FREE(gspnCtx))
    //{
    get_monotonic_boottime(&currentTime);
    i = 0;
    while(i<gspnCtx->core_mng.core_cnt) {
        if((gspnCtx->core_mng.cores[i].current_kcmd != NULL/*means module enabled, or register read only get zero*/)
           &&(GSPN_MOD1_BUSY_GET(gspnCtx->core_mng.cores[i].ctl_reg_base)//(gspnCtx->core_mng.cores[i].status == GSPN_CORE_BUSY)
              ||GSPN_MOD2_BUSY_GET(gspnCtx->core_mng.cores[i].ctl_reg_base))) {
            startTime.tv_sec = gspnCtx->core_mng.cores[i].start_time.tv_sec;
            startTime.tv_nsec = gspnCtx->core_mng.cores[i].start_time.tv_nsec;

            elapse = (currentTime.tv_sec - startTime.tv_sec)*1000000000 + currentTime.tv_nsec - startTime.tv_nsec;
            if(elapse < 0 || elapse >= MS_TO_NS(GSPN_CMD_EXE_TIMEOUT)) {
                GSPN_LOGE("core%d timeout, try to recovery.\n",i);
                GSPN_DumpReg(&gspnCtx->core_mng, i);
                /*recovery this core*/
                GSPN_TryToRecover(gspnCtx, i);
            }
        }
        i++;
    }
    //}
}

static void gspn_coef_cache_init(GSPN_CONTEXT_T *gspnCtx)
{
    uint32_t i = 0;

    GSPN_LOGI("enter!\n");
    if(gspnCtx->cache_coef_init_flag == 0) {
        i = 0;
        INIT_LIST_HEAD(&gspnCtx->coef_list);
        while(i < GSPN_COEF_CACHE_MAX) {
            list_add_tail(&gspnCtx->coef_cache[i].list, &gspnCtx->coef_list);
            i++;
        }
        gspnCtx->cache_coef_init_flag = 1;
    }
}

int gspn_work_thread(void *data)
{
    GSPN_CONTEXT_T *gspnCtx = (GSPN_CONTEXT_T *)data;
    int32_t ret = 0;

    if(gspnCtx == NULL) {
        GSPN_LOGE("gspnCtx == NULL, return!\n");
        return GSPN_K_CREATE_THREAD_ERR;
    }
    gspn_coef_cache_init(gspnCtx);

    while(1) {
        if(gspnCtx->suspend_flag == 1) {
            gspn_suspend_process(gspnCtx);
        }
        gspn_recalculate_timeout(gspnCtx);
        //ret = down_timeout(&gspnCtx->wake_work_thread_sema, US_TO_JIFFIES(gspnCtx->timeout));
        ret = down_timeout(&gspnCtx->wake_work_thread_sema, US_TO_JIFFIES(0xffffffff));
        sema_init(&gspnCtx->wake_work_thread_sema,0);
        if(ret == -ETIME) { // time out
            gspn_timeout_process(gspnCtx);
        }
        gspn_kcmd_post_process(gspnCtx); // free cores before assign job to them
        if(gspnCtx->suspend_flag == 0) {
            ret = gspn_kcmd_pre_process(gspnCtx); // get kcmds from filled list and assign them to cores
            if(ret) {
                GSPN_LOGE("pre process err!\n");
                up(&gspnCtx->wake_work_thread_sema);// leave the err kcmd to post process
            }
        }
    }
    return 0;
}



