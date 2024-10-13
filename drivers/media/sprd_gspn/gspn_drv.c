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

GSPN_CONTEXT_T *g_gspnCtx = NULL;// used to print log and debug when kernel crash, not used to pass parameters

/*
gspn_put_cmds_fence
description:if the CMDS is sync, do nothing; else put the acq fences of all these CMDS
*/
static void gspn_put_cmds_fence(GSPN_CMD_INFO_T *pCMD,uint32_t n)
{
    GSPN_CMD_INFO_T CMD;
    int ret = 0;

    while(n>0) {
        ret = copy_from_user((void*)&CMD, (void*)pCMD, sizeof(GSPN_CMD_INFO_T));
        if(ret) {
            GSPN_LOGE("copy from user err!ret=%d. pCMD:%p\n",ret,pCMD);
            ret = GSPN_K_COPY_FROM_USER_ERR;
            return;
        } else {
            if(CMD.misc_info.async_flag) {
                GSPN_PUT_CMD_ACQ_FENCE_BY_FD(&CMD);
            }
        }
        pCMD++;
        n--;
    }
}

void *gsp_memcpy(void *k_dst, void *k_src, size_t n)
{
	const char *src = k_src;
	char *dst = k_dst;

	/* Simple, byte oriented memcpy. */
	while (n--)
		*dst++ = *src++;

	return k_dst;
}
/*
gspn_kcmd_fence_pre_process
description: create release fence for every layer of CMD, copy this release fence back to user
                 collect all acq fence together
warning: function should be protect by gspnCtx->fence_create_sema
*/
static GSPN_ERR_CODE_E gspn_kcmd_fence_pre_process(GSPN_CONTEXT_T *gspnCtx, GSPN_KCMD_INFO_T *pKCMD)
{
    GSPN_CMD_INFO_T *pCMD = &pKCMD->src_cmd;
    GSPN_ERR_CODE_E ret = GSPN_NO_ERR;

#define GSPN_CREATE_REL_FENCE(fd,gspnCtx)\
            fd = sprd_fence_create(gspnCtx->timeline, "gspn_rel_fence", gspnCtx->remain_async_cmd_cnt+1);\
            if(fd < 0) {\
                GSPN_LOGE("create fence failed.\n");\
                ret = GSPN_K_CREATE_FENCE_ERR;\
                goto error;\
            }
#define GSPN_COLLECT_ACQ_FENCE(fd,pKCMD)\
            if((fd) >= 0) {\
                pKCMD->acq_fence[pKCMD->acq_fence_cnt] = sync_fence_fdget(fd);\
                pKCMD->acq_fence_cnt++;\
            } else {\
                GSPN_LOGI("invalid fence fd.\n");\
            }
#define GSPN_PUT_REL_FENCE_TO_USER(fd,ptr)\
            ret = put_user((fd), ptr);\
            if(ret) {\
                GSPN_LOGE("put release fence to user failed,ret:%d.\n",ret);\
                ret = GSPN_K_PUT_FENCE_TO_USER_ERR;\
                goto error;\
            }
#define GSPN_LAYER_FENCE_PROCESS(layer_info)\
            if(pCMD->layer_info.layer_en) {\
                GSPN_CREATE_REL_FENCE(pCMD->layer_info.rls_fen_fd,gspnCtx);\
                GSPN_COLLECT_ACQ_FENCE(pCMD->layer_info.acq_fen_fd,pKCMD);\
                GSPN_PUT_REL_FENCE_TO_USER(pCMD->layer_info.rls_fen_fd, &pKCMD->ucmd->layer_info.rls_fen_fd);\
            }

    pKCMD->acq_fence_cnt = 0;
    if(pCMD->misc_info.async_flag) {
        GSPN_LAYER_FENCE_PROCESS(l0_info);
        GSPN_LAYER_FENCE_PROCESS(l1_info);
        GSPN_LAYER_FENCE_PROCESS(l2_info);
        GSPN_LAYER_FENCE_PROCESS(l3_info);
        GSPN_LAYER_FENCE_PROCESS(des1_info);
        GSPN_LAYER_FENCE_PROCESS(des2_info);
        gspnCtx->remain_async_cmd_cnt++;
        GSPN_LOGI("after create rel fence, remain_cnt:%d, time_line:%d\n",
                  gspnCtx->remain_async_cmd_cnt,gspnCtx->timeline->timeline->value);
    }
    return GSPN_NO_ERR;

error:
    return ret;
}


/*
gspn_kcmd_list_fence_pre_process
description: create release fence for every layer of each KCMD in KCMD_list
                 collect all acq fence fd of KCMD together
                 copy release fd back to user space
*/
static GSPN_ERR_CODE_E gspn_kcmd_list_fence_pre_process(GSPN_CONTEXT_T *gspnCtx, struct list_head *KCMD_list)
{
    GSPN_KCMD_INFO_T *pKCMD = container_of(KCMD_list->next, GSPN_KCMD_INFO_T, list);
    GSPN_ERR_CODE_E ret = GSPN_NO_ERR;

    if(pKCMD->src_cmd.misc_info.async_flag) {
        down(&gspnCtx->fence_create_sema);
        list_for_each_entry(pKCMD, KCMD_list, list) {
            ret = gspn_kcmd_fence_pre_process(gspnCtx, pKCMD);
            if(ret) {
                /*destory all these release fence, that just create before*/
                GSPN_LOGE("fence_pre_process failed!\n");
                break;
            }
        }
        up(&gspnCtx->fence_create_sema);
    }
    return ret;
}

/*
gspn_kcmd_list_fence_destory
description: for every layer of each KCMD in KCMD_list,
                      if release fence created, destory it.
                      put all acq fence.
*/
static void gspn_kcmd_list_fence_destory(GSPN_CONTEXT_T *gspnCtx, struct list_head *KCMD_list)
{
#define GSPN_DESTORY_REL_FENCE(fd,gspnCtx)\
            if((fd)>=0) {\
                sprd_fence_destroy(gspnCtx->timeline, (fd));\
                (fd) = -1;\
            }
#define GSPN_DESTORY_CMD_REL_FENCE(pCMD,gspnCtx)\
            GSPN_DESTORY_REL_FENCE((pCMD)->l0_info.rls_fen_fd,gspnCtx);\
            GSPN_DESTORY_REL_FENCE((pCMD)->l1_info.rls_fen_fd,gspnCtx);\
            GSPN_DESTORY_REL_FENCE((pCMD)->l2_info.rls_fen_fd,gspnCtx);\
            GSPN_DESTORY_REL_FENCE((pCMD)->l3_info.rls_fen_fd,gspnCtx);\
            GSPN_DESTORY_REL_FENCE((pCMD)->des1_info.rls_fen_fd,gspnCtx);\
            GSPN_DESTORY_REL_FENCE((pCMD)->des2_info.rls_fen_fd,gspnCtx)

    GSPN_KCMD_INFO_T *pKCMD = container_of(KCMD_list->next, GSPN_KCMD_INFO_T, list);

    if(pKCMD->src_cmd.misc_info.async_flag) {
        down(&gspnCtx->fence_create_sema);
        /*if release fence created, destory it*/
        list_for_each_entry(pKCMD, KCMD_list, list) {
            GSPN_DESTORY_CMD_REL_FENCE(&pKCMD->src_cmd,gspnCtx);
            GSPN_PUT_CMD_ACQ_FENCE_BY_FD(&pKCMD->src_cmd);
            gspnCtx->remain_async_cmd_cnt--;
        }
        up(&gspnCtx->fence_create_sema);
    }
}


/*
gspn_get_kcmd_from_empty_list
description: get n KCMD from empty list, clean them, and link them together as a frame
*/
static GSPN_ERR_CODE_E gspn_get_kcmd_from_empty_list(GSPN_CONTEXT_T *gspnCtx,struct list_head *KCMD_list,uint32_t n)
{
    GSPN_KCMD_INFO_T *pKCMD = NULL;
    struct list_head *pos = NULL;
    struct list_head frame_list;
    int32_t i = 0;

    INIT_LIST_HEAD(KCMD_list);
    INIT_LIST_HEAD(&frame_list);

    //get the really count of empty_list
    i = 0;
    list_for_each(pos, &gspnCtx->empty_list) {
        pKCMD = list_first_entry(pos, GSPN_KCMD_INFO_T, list);
		GSPN_LOGE("pos = %p, i= %d, kcmd = %p\n", pos, i, pKCMD);
        i++;
    }
    if(i != gspnCtx->empty_list_cnt) {
        GSPN_LOGW("empty_list_cnt:%d, real_cnt:%d!\n",gspnCtx->empty_list_cnt,i);
    }
    if(i < n) {
        GSPN_LOGE("not enough empty kcmd, need:%d/real_cnt:%d!\n",n,i);
        return GSPN_K_NOT_ENOUGH_EMPTY_KCMD_ERR;
    }

    i = 0;
    while(i < n) {
        pKCMD = list_first_entry(&gspnCtx->empty_list, GSPN_KCMD_INFO_T, list);
		GSPN_LOGE("Pkcmd = %p, n= %d, gspnCtx->empty_list = %p\n", pKCMD, n, gspnCtx->empty_list);
        list_del(&pKCMD->list);
        memset(pKCMD,0,sizeof(GSPN_KCMD_INFO_T));  //need to be dong 0417
        pKCMD->src_cmd.l0_info.acq_fen_fd = -1;
        pKCMD->src_cmd.l0_info.rls_fen_fd = -1;
        pKCMD->src_cmd.l1_info.acq_fen_fd = -1;
        pKCMD->src_cmd.l1_info.rls_fen_fd = -1;
        pKCMD->src_cmd.l2_info.acq_fen_fd = -1;
        pKCMD->src_cmd.l2_info.rls_fen_fd = -1;
        pKCMD->src_cmd.l3_info.acq_fen_fd = -1;
        pKCMD->src_cmd.l3_info.rls_fen_fd = -1;
        pKCMD->src_cmd.des1_info.acq_fen_fd = -1;
        pKCMD->src_cmd.des1_info.rls_fen_fd = -1;
        pKCMD->src_cmd.des2_info.acq_fen_fd = -1;
        pKCMD->src_cmd.des2_info.rls_fen_fd = -1;
        list_add_tail(&pKCMD->list_frame, &frame_list);
        list_add_tail(&pKCMD->list, KCMD_list);
        gspnCtx->empty_list_cnt --;
        i++;
    }
    list_del(&frame_list);
    return GSPN_NO_ERR;
}

/*
gspn_put_list_to_empty_list
description: put KCMD list to fill list
*/
GSPN_ERR_CODE_E gspn_put_list_to_empty_list(GSPN_CONTEXT_T *gspnCtx,struct list_head *new_list)
{
    struct list_head *pos = NULL;
    struct list_head *next = NULL;
    int32_t cnt = 0;


	GSPN_LOGE("111111, enter\n");

    down(&gspnCtx->empty_list_sema);
    //get the really count of empty_list
    cnt = 0;
    list_for_each(pos, &gspnCtx->empty_list) {
        cnt++;
    }
	GSPN_LOGE("111111, enter, cnt = %d\n", cnt);
    if(cnt != gspnCtx->empty_list_cnt) {
        GSPN_LOGW("empty_list_cnt:%d,cnt:%d!\n",gspnCtx->empty_list_cnt,cnt);
    }
	GSPN_LOGE("111111, enter, cnt = %d\n", cnt);
    list_for_each_safe(pos, next, new_list) {
		GSPN_LOGE("111111, enter, new_list = %p, pos = %p, next = %p, cnt = %d\n", new_list, pos, next, cnt);
        list_del(pos);
		 GSPN_LOGE("1111_add_empty_list, pos = %p", pos);
        list_add_tail(pos, &gspnCtx->empty_list);
        gspnCtx->empty_list_cnt++;
    }
    up(&gspnCtx->empty_list_sema);
    wake_up_interruptible_all(&(gspnCtx->empty_list_wq));
    return GSPN_NO_ERR;
}

/*
gspn_put_list_to_fill_list
description: put KCMD list to fill list
*/
static GSPN_ERR_CODE_E gspn_put_list_to_fill_list(GSPN_CONTEXT_T *gspnCtx,struct list_head *new_list)
{
    struct list_head *pos = NULL;
    struct list_head *next = NULL;
    int32_t cnt = 0;

    down(&gspnCtx->fill_list_sema);
    //get the really count of fill_list
    cnt = 0;
    list_for_each(pos, &gspnCtx->fill_list) {
        cnt++;
    }
    if(cnt != gspnCtx->fill_list_cnt) {
        GSPN_LOGW("fill_list_cnt:%d,cnt:%d!\n",gspnCtx->fill_list_cnt,cnt);
    }

    list_for_each_safe(pos, next, new_list) {
        list_del(pos);
        list_add_tail(pos, &gspnCtx->fill_list);
        gspnCtx->fill_list_cnt++;
    }
    up(&gspnCtx->fill_list_sema);

    //add to fill list success, wake work thread to process
    up(&gspnCtx->wake_work_thread_sema);
    return GSPN_NO_ERR;
}

/*
func:gspn_enable_set
desc:config des-layer enable and scale enable according to the run mode and scl_seq.
*/
static GSPN_ERR_CODE_E gspn_enable_set(GSPN_KCMD_INFO_T *pKCMD)
{
    pKCMD->src_cmd.misc_info.scale_en = 0;
    if(pKCMD->src_cmd.misc_info.run_mod == 0) {
        pKCMD->src_cmd.des2_info.layer_en = 0;
        if(pKCMD->src_cmd.l0_info.layer_en == 1
           ||pKCMD->src_cmd.l1_info.layer_en == 1
           ||pKCMD->src_cmd.l2_info.layer_en == 1
           ||pKCMD->src_cmd.l3_info.layer_en == 1) {
            pKCMD->src_cmd.des1_info.layer_en = 1;
        } else {
            GSPN_LOGE("all src layer are not enable!\n");
            return GSPN_K_PARAM_CHK_ERR;
        }

        if(pKCMD->src_cmd.misc_info.scale_seq == 0) {
            if(pKCMD->src_cmd.l0_info.clip_size.w  != pKCMD->src_cmd.des1_info.scale_out_size.w
               ||pKCMD->src_cmd.l0_info.clip_size.h  != pKCMD->src_cmd.des1_info.scale_out_size.h) {
                pKCMD->src_cmd.misc_info.scale_en = 1;
            }
        } else {
            if(pKCMD->src_cmd.des1_info.work_src_size.w != pKCMD->src_cmd.des1_info.scale_out_size.w
               ||pKCMD->src_cmd.des1_info.work_src_size.h != pKCMD->src_cmd.des1_info.scale_out_size.h) {
                pKCMD->src_cmd.misc_info.scale_en = 1;
            }
        }
    } else {
        if(pKCMD->src_cmd.l1_info.layer_en == 1
           ||pKCMD->src_cmd.l2_info.layer_en == 1
           ||pKCMD->src_cmd.l3_info.layer_en == 1) {
            pKCMD->src_cmd.des1_info.layer_en = 1;
        } else {
            pKCMD->src_cmd.des1_info.layer_en = 0;
        }

        if(pKCMD->src_cmd.l0_info.layer_en == 1) {
            pKCMD->src_cmd.des2_info.layer_en = 1;
            if(pKCMD->src_cmd.l0_info.clip_size.w  != pKCMD->src_cmd.des2_info.scale_out_size.w
               ||pKCMD->src_cmd.l0_info.clip_size.h  != pKCMD->src_cmd.des2_info.scale_out_size.h) {
                pKCMD->src_cmd.misc_info.scale_en = 1;
            }
        } else {
            pKCMD->src_cmd.des2_info.layer_en = 0;
        }
    }
    return GSPN_NO_ERR;
}

/*
func: gspn_get_dmabuf
description:translate every layer's buffer-fd to DMA-buffer-pointer,
            for the fd is only valid in user process context.
*/
static GSPN_ERR_CODE_E gspn_get_dmabuf(GSPN_KCMD_INFO_T *pKCMD)
{


#define GSPN_Lx_GET_DMABUF(lx_info)\
    if(lx_info.layer_en == 1 && lx_info.addr_info.share_fd > 0) { \
        lx_info.addr_info.dmabuf = (void *)sprd_ion_gsp_get_dmabuf(lx_info.addr_info.share_fd); \
        if(0 == lx_info.addr_info.dmabuf) { \
            GSPN_LOGE("get dma buffer failed!lx_info.addr_info.share_fd = %d\n", lx_info.addr_info.share_fd); \
            return GSPN_K_GET_DMABUF_BY_FD_ERR; \
        } \
        GSPN_LOGI("fd=%d, dmabuf=%p\n",lx_info.addr_info.share_fd,lx_info.addr_info.dmabuf); \
    }
    GSPN_Lx_GET_DMABUF(pKCMD->src_cmd.des1_info);
    GSPN_Lx_GET_DMABUF(pKCMD->src_cmd.des2_info);
    GSPN_Lx_GET_DMABUF(pKCMD->src_cmd.l0_info);
    GSPN_Lx_GET_DMABUF(pKCMD->src_cmd.l1_info);
    GSPN_Lx_GET_DMABUF(pKCMD->src_cmd.l2_info);
    GSPN_Lx_GET_DMABUF(pKCMD->src_cmd.l3_info);


    return GSPN_NO_ERR;
}

/*
gspn_fill_kcmd
description: copy CMD to KCMD
*/
static GSPN_ERR_CODE_E gspn_fill_kcmd(struct list_head *KCMD_list,GSPN_CMD_INFO_T *pCMD,uint32_t *done_cnt)
{
    GSPN_KCMD_INFO_T *pKCMD = NULL;
    int32_t ret = GSPN_NO_ERR;

    list_for_each_entry(pKCMD, KCMD_list, list) {
        ret = copy_from_user((void*)&pKCMD->src_cmd, (void __user *)pCMD, sizeof(GSPN_CMD_INFO_T));
        if(ret) {
            GSPN_LOGE("copy from user err!ret=%d. pCMD:%p\n",ret,pCMD);
            ret = GSPN_K_COPY_FROM_USER_ERR;
            return ret;
        }
        GSPN_LOGI("11111, %s, %d, pKcmd_addr = 0x%x, pcmd_addr = 0x%x",
					__func__, __LINE__, pKCMD, pCMD);
        pKCMD->ucmd = (GSPN_CMD_INFO_T __user *)pCMD;
        pKCMD->pid = current->pid;
        pKCMD->done_cnt = done_cnt;

        /*config destnation layer enable, scale enable*/
        ret = gspn_enable_set(pKCMD);
        if(ret) {
            return ret;
        }

        /*translate every layer's buffer-fd to DMA-buffer-pointer,
        for the fd is only valid in user process context*/
        ret = gspn_get_dmabuf(pKCMD);
        if(ret) {
            return ret;
        }

        pCMD++;
    }
    return ret;
}

#if 0
uint32_t  __attribute__((weak)) sci_get_chip_id(void)
{
    if (NULL == g_gspnCtx) {
        return -1;
    }

    GSPN_LOGI("GSPN local read chip id, *(%p) == %x \n",
              (void*)(g_gspnCtx->core_mng.chip_id_addr),REG_GET(g_gspnCtx->core_mng.chip_id_addr+0xFC));
    return REG_GET(g_gspnCtx->core_mng.chip_id_addr);
}

static uint32_t  gspn_get_chip_id(void)
{
    uint32_t adie_chip_id = sci_get_chip_id();

    if (NULL == g_gspnCtx) {
        return -1;
    }

    if((adie_chip_id & 0xffff0000) < 0x50000000) {
        GSPN_LOGW("chip id 0x%08x is invalidate, try to get it by reading reg directly!\n", adie_chip_id);
        adie_chip_id = REG_GET(g_gspnCtx->core_mng.chip_id_addr);
        if((adie_chip_id & 0xffff0000) < 0x50000000) {
            GSPN_LOGW("chip id 0x%08x from reg is invalidate too!\n", adie_chip_id);
        }
    }
    GSPN_LOGI("return chip id 0x%08x \n", adie_chip_id);
    return adie_chip_id;
}
#else
uint32_t  __attribute__((weak)) sci_get_chip_id(void)
{
    GSPN_LOGI("GSPN local read chip id, *(%p) == %x \n",
              (void*)(SPRD_AONAPB_BASE+0xFC),REG_GET(SPRD_AONAPB_BASE+0xFC));
    return REG_GET(SPRD_AONAPB_BASE+0xFC);
}

static uint32_t  gspn_get_chip_id(void)
{
    uint32_t adie_chip_id = sci_get_chip_id();
    if((adie_chip_id & 0xffff0000) < 0x50000000) {
        GSPN_LOGW("chip id 0x%08x is invalidate, try to get it by reading reg directly!\n", adie_chip_id);
        adie_chip_id = REG_GET(SPRD_AONAPB_BASE+0xFC);
        if((adie_chip_id & 0xffff0000) < 0x50000000) {
            GSPN_LOGW("chip id 0x%08x from reg is invalidate too!\n", adie_chip_id);
        }
    }
    GSPN_LOGI("return chip id 0x%08x \n", adie_chip_id);
    return adie_chip_id;
}
#endif
static GSPN_CAPABILITY_T* gspn_cfg_capability(GSPN_CONTEXT_T *gspnCtx)
{
    uint32_t adie_chip_id = 0;

    if(gspnCtx->capability.magic != GSPN_CAPABILITY_MAGIC_NUMBER) {  // not initialized
        memset((void*)&gspnCtx->capability,0,sizeof(gspnCtx->capability));
        gspnCtx->capability.max_layer_cnt = 4;
        gspnCtx->capability.max_yuvLayer_cnt = 1;
        gspnCtx->capability.max_scaleLayer_cnt = 1;
        gspnCtx->capability.seq0_scale_range_up=256;// 1/4~4
        gspnCtx->capability.seq0_scale_range_down=1;// 1/16~4
        gspnCtx->capability.seq1_scale_range_up=64;// 1/4~4
        gspnCtx->capability.seq1_scale_range_down=4;// 1/16~4
        gspnCtx->capability.scale_updown_sametime=1;
        gspnCtx->capability.src_yuv_xywh_even_limit=1;
        gspnCtx->capability.max_video_size=2;
        gspnCtx->capability.addr_type_support=GSPN_ADDR_TYPE_IOVIRTUAL;
        gspnCtx->capability.std_support_in = GSPN_STD_BT601_FULL | GSPN_STD_BT601_NARROW;
        gspnCtx->capability.std_support_out = GSPN_STD_BT601_FULL | GSPN_STD_BT601_NARROW;
        adie_chip_id = gspn_get_chip_id();
        switch(adie_chip_id&0xFFFF0000) {
            case 0x96310000:/*sharkLT8*/
                gspnCtx->capability.version = 0x10;
                gspnCtx->capability.block_alpha_limit=1;
                gspnCtx->capability.max_video_size=1;
                /*on sharkLT8, the max gspn clock 256MHz,
                blending throughput one pixel one clock on output side.
                scaling throughput is 0.8 pixel one clock on output side.*/
                gspnCtx->capability.max_throughput=256;
                break;
            default:
                gspnCtx->capability.version = 0x10;
                gspnCtx->capability.block_alpha_limit=1;
                gspnCtx->capability.max_video_size=1;
                /*on sharkLT8, the max gspn clock 256MHz,
                blending throughput one pixel one clock on output side.
                scaling throughput is 0.8 pixel one clock on output side.*/
                gspnCtx->capability.max_throughput=256;
                GSPN_LOGI("info:a new chip id, be treated as sharkLT8!\n");
               // return &gspnCtx->capability;
                break;
        }

#ifndef CONFIG_SPRD_IOMMU
        gspnCtx->capability.buf_type_support = GSPN_ADDR_TYPE_PHYSICAL;
#endif
        gspnCtx->capability.crop_min.w=gspnCtx->capability.crop_min.h=4;
        gspnCtx->capability.out_min.w=gspnCtx->capability.out_min.h=4;
        gspnCtx->capability.crop_max.w=gspnCtx->capability.crop_max.h=8191;
        gspnCtx->capability.out_max.w=gspnCtx->capability.out_max.h=8191;
        gspnCtx->capability.magic = GSPN_CAPABILITY_MAGIC_NUMBER;
    }
    return &gspnCtx->capability;
}



static long gspn_drv_ioctl(struct file *pfile,
                           uint32_t cmd,
                           unsigned long arg)
{
    int32_t ret = -GSPN_NO_ERR;
    uint32_t param_size = _IOC_SIZE(cmd);
    uint32_t ctl_code = _IOC_NR(cmd);
    uint32_t n = (_IOC_TYPE(cmd)&GSPN_IO_CNT_MASK);// CMD[n]
    uint32_t async_flag = (_IOC_TYPE(cmd)&GSPN_IO_ASYNC_MASK)?1:0;
    uint32_t done_cnt = 0;// used for sync call
    GSPN_CONTEXT_T *gspnCtx = (GSPN_CONTEXT_T*)pfile->private_data;
    struct list_head KCMD_list;

    GSPN_LOGI("<<<pid:0x%08x,cmd:0x%08x, io code:0x%x, param_size:%d, type:0x%02x, arg:%ld, async_flag = %d\n",
              current->pid, cmd, _IOC_NR(cmd), param_size, _IOC_TYPE(cmd), arg, async_flag);
    if(gspnCtx == NULL||arg == 0) {
        GSPN_LOGE("ioctl failed!gspnCtx:%p,arg:%ld\n",gspnCtx,arg);
        return -EINVAL;
    }

    switch (ctl_code) {
        case GSPN_CTL_CODE_GET_CAPABILITY:
            if (param_size>=sizeof(GSPN_CAPABILITY_T)) {
                GSPN_CAPABILITY_T *cap=gspn_cfg_capability(gspnCtx);
                if(arg & MEM_OPS_ADDR_ALIGN_MASK || (ulong)cap & MEM_OPS_ADDR_ALIGN_MASK) {
                    GSPN_LOGW("copy_to_user use none 8B alignment address!");
                }
                ret=copy_to_user((void __user *)arg,(const void*)cap,sizeof(GSPN_CAPABILITY_T));
                if(ret) {
                    GSPN_LOGE("get gspn capability failed in copy_to_user !\n");
                    ret = GSPN_K_COPY_TO_USER_ERR;
                    break;
                }
                GSPN_LOGI("get gspn capability success in copy_to_user \n");
            } else {
                GSPN_LOGE("get gsp capability, buffer is too small,come:%u,need:%zu!",
                          param_size,sizeof(GSPN_CAPABILITY_T));
                ret = GSPN_K_COPY_TO_USER_ERR;
                break;
            }
            break;

        case GSPN_CTL_CODE_SET_PARAM: {
            if(n>GSPN_CMD_ARRAY_MAX || param_size<sizeof(GSPN_CMD_INFO_T)) {
                //if this happen, we don't destory acq fence , because we don't know the number of the array
                GSPN_LOGE("ioctl failed!%d>%d or %d<%zu!\n",
                          n,GSPN_CMD_ARRAY_MAX,param_size,sizeof(GSPN_CMD_INFO_T));
                return -EINVAL;
            }

            ret = wait_event_interruptible_timeout(gspnCtx->empty_list_wq,
                                                   (gspnCtx->empty_list_cnt>=n ||gspnCtx->suspend_flag),
                                                   msecs_to_jiffies(500));

            if (ret<=0) { // ret<0:interrupted by singal ; ret==0: timeout;
                GSPN_LOGE("wait condition failed!ret:%d,n:%d,empty:%d,suspend:%d\n",
                          ret,n,gspnCtx->empty_list_cnt,gspnCtx->suspend_flag);
                gspn_put_cmds_fence((GSPN_CMD_INFO_T*)arg,n);
            } else {
                GSPN_LOGI("wait condition success!ret:%d,n:%d,empty:%d,suspend:%d, gspnCtx = 0x%x\n",
                          ret,n,gspnCtx->empty_list_cnt,gspnCtx->suspend_flag, gspnCtx);

                down(&gspnCtx->empty_list_sema);
                if(gspnCtx->suspend_flag || gspnCtx->empty_list_cnt<n) {
                    up(&gspnCtx->empty_list_sema);
                	 GSPN_LOGI("11111, %s, %d",__func__, __LINE__);
                    gspn_put_cmds_fence((GSPN_CMD_INFO_T*)arg,n);
                	 GSPN_LOGI("11111, %s, %d",__func__, __LINE__);
                } else {
                	 GSPN_LOGI("11111, %s, %d",__func__, __LINE__);
                    /*get n KCMD from empty list*/
                    ret = gspn_get_kcmd_from_empty_list(gspnCtx,&KCMD_list,n);
                    up(&gspnCtx->empty_list_sema);
                    if(!ret) {
                	     GSPN_LOGI("11111, %s, %d",__func__, __LINE__);
                        /*cfg KCMD,copy from user CMD*/
                        ret = gspn_fill_kcmd(&KCMD_list,(GSPN_CMD_INFO_T*)arg,&done_cnt);
                        if(!ret) {
                            /*create release fence for every layer of each KCMD in KCMD_list*/
                            if(async_flag) {
                                ret = gspn_kcmd_list_fence_pre_process(gspnCtx, &KCMD_list);
                                if(ret) {
                                    goto cfg_exit2;
                                }
                            }

                            /*put KCMD_list to fill list*/
                            ret = gspn_put_list_to_fill_list(gspnCtx,&KCMD_list);
                            if(!ret) {
                                /*if it's sync cmd, wait it done*/
                                if(async_flag==0) {
                                    ret = wait_event_interruptible_timeout(gspnCtx->sync_done_wq,
                                                                           (done_cnt>=n ||gspnCtx->suspend_flag),
                                                                           msecs_to_jiffies(1000));
                                    if (ret<=0) { // ret<0:interrupted by singal ; ret==0: timeout;
                                        GSPN_LOGE("pid:0x%08x wait frame done failed! ret:%d,n:%d,done_cnt:%d,suspend:%d\n",current->pid,
                                                  ret,n,done_cnt,gspnCtx->suspend_flag);
                                    } else {
                                        GSPN_LOGI("pid:0x%08x wait frame done success! ret:%d,n:%d,done_cnt:%d,suspend:%d\n",current->pid,
                                                  ret,n,done_cnt,gspnCtx->suspend_flag);
											ret = 0;
                                    }
                                }
                                break;
                            } else {
                                goto cfg_exit2;
                            }
                        } else {
                            gspn_put_cmds_fence((GSPN_CMD_INFO_T*)arg,n);
                            goto cfg_exit1;
                        }
                    } else {
                        gspn_put_cmds_fence((GSPN_CMD_INFO_T*)arg,n);
                        /*rel fence have not been created yet, just return*/
                        break;
                    }
                cfg_exit2:
                    gspn_kcmd_list_fence_destory(gspnCtx, &KCMD_list);
                cfg_exit1:
                    gspn_put_list_to_empty_list(gspnCtx, &KCMD_list);
                }
            }
        }
        break;
        default:
            ret = GSPN_K_CTL_CODE_ERR;
            GSPN_LOGE("unknown cmd!\n");
            break;
    }


    GSPN_LOGI("pid:0x%08x,cmd:0x%08x, io number:0x%x, param_size:%d,type:0x%02x,arg:%ld>>>\n",
              current->pid, cmd, _IOC_NR(cmd), param_size,_IOC_TYPE(cmd),arg);
    return ret;
}

static int32_t gspn_drv_open(struct inode *node, struct file *pfile)
{
    struct miscdevice *miscdev = NULL;
    GSPN_CONTEXT_T *gspnCtx = NULL;
    GSPN_LOGI("enter. pid:0x%08x\n",current->pid);

    miscdev = pfile->private_data;
    if (NULL == miscdev) {
        GSPN_LOGE("open failed!miscdev == NULL!\n");
        return -EINVAL;
    }
    gspnCtx = container_of(miscdev, GSPN_CONTEXT_T, dev);
    if (NULL == gspnCtx) {
        GSPN_LOGE("open failed!gspnCtx == NULL!\n");
        return -EINVAL;
    }
    pfile->private_data = (void*)gspnCtx;
    return GSPN_NO_ERR;
}

static int32_t gspn_drv_release(struct inode *node, struct file *pfile)
{
    GSPN_LOGI("enter. pid:0x%08x\n",current->pid);
    pfile->private_data = NULL;
    return GSPN_NO_ERR;
}

static int atoi(const char *str)
{
    int value = 0;
    while(*str>='0' && *str<='9') {
        value *= 10;
        value += *str - '0';
        str++;
    }
    return value;
}
static ssize_t gspn_drv_write(struct file *pfile, const char __user * u_data, size_t cnt, loff_t *cnt_ret)
{
    GSPN_CONTEXT_T *gspnCtx = (GSPN_CONTEXT_T*)pfile->private_data;
    uint32_t i = 0;
    if(gspnCtx == NULL) {
        GSPN_LOGE("gspnCtx is NULL! pid:0x%08x\n",current->pid);
        return -ENODEV;
    }

    i = atoi(u_data);
    GSPN_PRINTK("enter. u_data:%s == %d\n", u_data, i);
    if(i<255) {
        gspnCtx->log_level = i;
    }
    return 1;
}

static ssize_t gspn_drv_read(struct file *pfile, char __user *u_data, size_t cnt, loff_t *cnt_ret)
{
    char rt_word[32]= {0};
    GSPN_CONTEXT_T *gspnCtx = (GSPN_CONTEXT_T*)pfile->private_data;

    if(gspnCtx == NULL) {
        GSPN_LOGE("gspnCtx is NULL! pid:0x%08x\n",current->pid);
        return -ENODEV;
    }

    *cnt_ret = 0;
    *cnt_ret += sprintf(rt_word + *cnt_ret, "log level:%d.\n",gspnCtx->log_level);
    return copy_to_user(u_data, (void*)rt_word, (ulong)*cnt_ret);
}

static struct file_operations gspn_drv_fops = {
    .owner          = THIS_MODULE,
    .open           = gspn_drv_open,
    .write          = gspn_drv_write,
    .read           = gspn_drv_read,
    .unlocked_ioctl = gspn_drv_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = gspn_drv_ioctl,
#endif
    .release        = gspn_drv_release
};

static int gspn_parse_dt_reg(struct device_node *np,uint32_t idx,uint32_t** reg_base,const char *name)
{
    struct resource res;
    int32_t ret = 0;

	printk("11111, , np->name = %s\n", np->name);
    ret = of_address_to_resource(np, idx, &res);
    if (ret) {
        GSPN_LOGE("failed to parse %s reg base!\n",name);
        return -1;
    }
	printk("11111, res.start = 0x%x, res.end = 0x%x, res.name = %s, size = 0x%x\n", res.start, res.end, res.name, resource_size(&res));
    *reg_base = (uint32_t*)ioremap_nocache(res.start, resource_size(&res));
    if(*reg_base == NULL) {
        GSPN_LOGE("failed to ioremap %s reg base!\n",name);
        return -1;
    }
    GSPN_LOGI("%s pa:%08x va:%p\n", name, (uint32_t)res.start,*reg_base);
    return 0;
}

static int gspn_parse_dt_clk(struct device_node *np,struct clk **ppclk,const char *name)
{
    *ppclk = of_clk_get_by_name(np,name);
    if (IS_ERR(*ppclk)) {
        GSPN_LOGE("failed to get %s!\n",name);
        return -EINVAL;
    }
    //GSPN_LOGI("get %s success!\n",name);
    return 0;
}

static int gspn_parse_dt_number(struct device_node *np,uint32_t *pNumber,const char *name)
{
    uint32_t data = 0;
    int32_t ret = 0;

    /* get and check core id */
    ret = of_property_read_u32(np, name, &data);
    if (ret) {
        GSPN_LOGE("get %s err!\n",name);
    } else {
        *pNumber = data;
       // GSPN_LOGI("%s : 0x%08x\n", name, data);
    }
    return ret;
}

/*
gspn_parse_dt_init_core
description: parse dt structure and initialize core structure
*/
static int gspn_parse_dt_init_core(GSPN_CONTEXT_T *gspnCtx)
{
    struct device_node *np = NULL, *nchd = NULL;
    GSPN_CORE_T *core = NULL;
    uint32_t nr = 0;
    int32_t ret = 0, i = 0;

    /* param check */
    if(gspnCtx == NULL ||gspnCtx->gspn_of_dev == NULL||gspnCtx->gspn_of_dev->of_node == NULL) {
        GSPN_LOGE("parameter err!\n");
        return -EINVAL;
    }
    np = gspnCtx->gspn_of_dev->of_node;
    nr = of_get_child_count(np);
    if (nr == 0 || nr > GSPN_CORE_MAX) {
        GSPN_LOGE("get dt child count err!\n");
        return (!nr ? -ENODEV : -EINVAL);
    }
    //GSPN_LOGI("get dt child count %d.\n",nr);

    /* alloc core structure */
    //core = kzalloc(nr*sizeof(GSPN_CORE_T), GFP_KERNEL);
    core = vzalloc(nr*sizeof(GSPN_CORE_T));
    if (core == NULL) {
        GSPN_LOGE("failed to alloc gspn core structure!\n");
        return -ENOMEM;
    }

    ret = gspn_parse_dt_reg(np,0,&gspnCtx->core_mng.int_reg_base,"glb_int_base");/* parse ap interrupt reg base */
    ret |= gspn_parse_dt_reg(np,1,&gspnCtx->core_mng.enable_reg_base,"gspn_en_base");/* parse gspn clock enable ctl reg base */
    ret |= gspn_parse_dt_reg(np,2,&gspnCtx->core_mng.reset_reg_base,"gspn_rst_base");/* parse gspn soft reset ctl reg base */
    ret |= gspn_parse_dt_reg(np,3,&gspnCtx->core_mng.module_sel_reg_base,"module_sel_base");/* parse gspn soft reset ctl reg base */
    ret |= gspn_parse_dt_reg(np,4,&gspnCtx->core_mng.chip_id_addr,"ap_chipid_addr");/* parse gspn soft reset ctl reg base */
    if (ret) {
        GSPN_LOGE(" core%d parse int/en/rst reg err!\n",i);
        goto error;
    }

    /* parse each core params */
    i = 0;
    for_each_child_of_node(np, nchd) {
        /* get and check core id */
        ret = gspn_parse_dt_number(nchd,&core[i].core_id,"core_id");
        if (ret || i != core[i].core_id) {
            GSPN_LOGE("core_id order err, i %d, data %d.\n",i,core[i].core_id);
            goto error;
        }
        //GSPN_TRACE("%s[%d] core_id = %d\n",__func__,__LINE__,i);

        /* alloc CMDQ reg structure,
        this memory must be phy, if it's virtual, it's hard to map it to iommu*/
        core[i].cmdq = kzalloc(GSPN_CMDQ_ARRAY_MAX*sizeof(GSPN_CMD_REG_T), GFP_KERNEL);
        if (core[i].cmdq == NULL) {
            GSPN_LOGE("core%d, failed to alloc CMDQ array!\n",i);
            goto error;
        }

        ret = gspn_parse_dt_number(nchd,&core[i].gspn_en_rst_bit,"gspn_en_rst_bit");
        ret |= gspn_parse_dt_number(nchd,&core[i].mmu_en_rst_bit,"mmu_en_rst_bit");
        ret |= gspn_parse_dt_number(nchd,&core[i].auto_gate_bit,"auto_gate_bit");
        ret |= gspn_parse_dt_number(nchd,&core[i].force_gate_bit,"force_gate_bit");
        ret |= gspn_parse_dt_number(nchd,&core[i].emc_en_bit,"emc_en_bit");
        if (ret) {
            GSPN_LOGE("core%d parse bit number err!\n",i);
            goto error;
        }
#if 0
        ret = gspn_parse_dt_reg(nchd,0,(uint32_t**)&core[i].ctl_reg_base,"gspn_ctl_reg");/* parse core[n] module reg base */
        ret |= gspn_parse_dt_reg(nchd,1,&core[i].iommu_ctl_reg_base,"mmu_ctl_reg");/* parse core[n] iommu ctl reg base */
        ret |= gspn_parse_dt_reg(nchd,2,&core[i].clk_select_reg_base,"gspn_clk_sel");/* parse core[n] clock source select ctl reg base */
        ret |= gspn_parse_dt_reg(nchd,3,&core[i].auto_gate_reg_base,"gspn_auto_gate");/* parse core[n] clock auto-gate reg base */
        ret |= gspn_parse_dt_reg(nchd,4,&core[i].emc_reg_base,"gspn_emc_reg");/* parse core[n] emc clock reg base */
        if (ret) {
            GSPN_LOGE("core%d failed to get reg base!\n",i);
            goto error;
        } 
#else
	 core[i].ctl_reg_base = (uint32_t*)ioremap_nocache(0x20a00000, 0x1000);
     core[i].iommu_ctl_reg_base =  (uint32_t*)ioremap_nocache(0x20b08000, 0x1000);
     core[i].clk_select_reg_base =  (uint32_t*)ioremap_nocache(0x215000a0, 0x1000);
     core[i].auto_gate_reg_base =  (uint32_t*)ioremap_nocache(0x20e00010, 0x1000);
     core[i].emc_reg_base =  (uint32_t*)ioremap_nocache(0x20e00000, 0x1000);
#endif
        /* parse core[n] irq number */
        core[i].interrupt_id = irq_of_parse_and_map(nchd, 0);
        if (core[i].interrupt_id == 0) {
            GSPN_LOGE("core%d failed to parse irq number!\n",i);
            ret = -EINVAL;
            goto error;
        }
        GSPN_LOGI("core%d, irq:%d,\n",i,core[i].interrupt_id);

        ret = gspn_parse_dt_clk(nchd,&core[i].emc_clk_parent,GSPN_EMC_CLOCK_PARENT_NAME);/* parse core[n] emc clock parent */
        ret |= gspn_parse_dt_clk(nchd,&core[i].emc_clk,GSPN_EMC_CLOCK_NAME);/* parse core[n] emc clock */
        ret |= gspn_parse_dt_clk(nchd,&core[i].gspn_clk_parent,GSPN_CLOCK_PARENT3);/* parse core[n] gspn clock parent */
        ret |= gspn_parse_dt_clk(nchd,&core[i].gspn_clk,GSPN_CLOCK_NAME);/* parse core[n] gspn clock */
        if(ret) {
            GSPN_LOGE("core%d failed to get clock!\n",i);
            goto error;
        }

        //core[i].status = GSPN_CORE_UNINITIALIZED; //hl changed
        core[i].status = GSPN_CORE_FREE; 
        core[i].current_kcmd = NULL;
        core[i].current_kcmd_sub_cmd_index = -1;
        i++;
    }
    gspnCtx->core_mng.core_cnt = nr;
    gspnCtx->core_mng.free_cnt = nr;
    gspnCtx->core_mng.cores = core;
    GSPN_LOGI("parse success, exit.\n");
    return 0;

error:
    if(core) {
        i = 0;
        while(i < nr) {
            if(core[i].cmdq) {
                kfree(core[i].cmdq);
                core[i].cmdq = NULL;
            }
        }
        vfree(core);
        core = NULL;
    }

    return ret;
}


/*
gspn_clock_init
description: only set parents, clock enable implement in gspn_module_enable()
*/
static int32_t gspn_clock_init(GSPN_CONTEXT_T *gspnCtx)
{
    int ret = 0, i = 0;

    while(i<gspnCtx->core_mng.core_cnt) {
        ret = clk_set_parent(gspnCtx->core_mng.cores[i].emc_clk, gspnCtx->core_mng.cores[i].emc_clk_parent);
        if(ret) {
            GSPN_LOGE("core%d failed to set emc clk parent!\n",i);
            return ret;
        } else {
            GSPN_LOGI("core%d set emc clk parent success!\n",i);
        }

        ret = clk_set_parent(gspnCtx->core_mng.cores[i].gspn_clk, gspnCtx->core_mng.cores[i].gspn_clk_parent);
        if(ret) {
            GSPN_LOGE("core%d failed to set gspn clk parent!\n",i);
            return ret;
        } else {
            GSPN_LOGI("core%d set gspn clk parent success!\n",i);
        }
        i++;
    }
    GSPN_AUTO_GATE_ENABLE(&gspnCtx->core_mng);
    return 0;
}

static int32_t gspn_suspend_common(GSPN_CONTEXT_T *gspnCtx)
{
    int32_t ret = 0;
    int32_t i = 0;
    gspnCtx->suspend_flag = 1;
    up(&gspnCtx->wake_work_thread_sema);
    wake_up_interruptible_all(&(gspnCtx->empty_list_wq));
    wake_up_interruptible_all(&(gspnCtx->sync_done_wq));

    //in case of GSPN is processing now, wait it finish
    ret = down_timeout(&gspnCtx->suspend_clean_done_sema,msecs_to_jiffies(500));
    if(ret) {
        GSPN_LOGE("wait suspend clean done sema failed, ret: %d\n",ret);
        while(i<gspnCtx->core_mng.core_cnt) {
            GSPN_DumpReg(&gspnCtx->core_mng, i);
            i++;
        }
    } else {
        GSPN_LOGI("wait suspend clean done sema success. \n");
    }
    return ret;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void gspn_early_suspend(struct early_suspend* es)
{
    GSPN_CONTEXT_T *gspnCtx = NULL;

    GSPN_PRINTK("enter.\n");

    gspnCtx = container_of(es, GSPN_CONTEXT_T, earlysuspend);
    if (NULL == gspnCtx) {
        GSPN_LOGE("suspend failed! gspnCtx == NULL!\n");
        return;
    }
    gspn_suspend_common(gspnCtx);
}

static void gspn_late_resume(struct early_suspend* es)
{
    GSPN_CONTEXT_T *gspnCtx = NULL;

    GSPN_PRINTK("enter.\n");
    gspnCtx = container_of(es, GSPN_CONTEXT_T, earlysuspend);
    if (NULL == gspnCtx) {
        GSPN_LOGE("resume failed!gspnCtx == NULL!\n");
        return;
    }

    gspnCtx->gsp_coef_force_calc = 1;
    GSPN_AUTO_GATE_ENABLE(&gspnCtx->core_mng);
    gspnCtx->suspend_flag = 0;
}

#else
static int32_t gspn_suspend(struct platform_device *pdev,pm_message_t state)
{
    GSPN_CONTEXT_T *gspnCtx = NULL;
    int32_t ret = 0;

    GSPN_PRINTK("enter.\n");
    gspnCtx = platform_get_drvdata(pdev);
    if (NULL == gspnCtx) {
        GSPN_LOGE("suspend failed! gspnCtx == NULL!\n");
        return -EINVAL;
    }
    ret = gspn_suspend_common(gspnCtx);
    return ret;
}

static int32_t gspn_resume(struct platform_device *pdev)
{
    GSPN_CONTEXT_T *gspnCtx = NULL;
    GSPN_PRINTK("enter\n");

    gspnCtx = (GSPN_CONTEXT_T *)platform_get_drvdata(pdev);
    if (NULL == gspnCtx) {
        GSPN_LOGE("resume failed!gspnCtx == NULL!\n");
        return -EINVAL;
    }

    gspnCtx->gsp_coef_force_calc = 1;
    GSPN_AUTO_GATE_ENABLE(&gspnCtx->core_mng);
    gspnCtx->suspend_flag = 0;
    return 0;
}

#endif


/*
func:gspn_async_init
desc:init list, create work thread, prepare for async feature
*/
static int32_t gspn_async_init(GSPN_CONTEXT_T *gspnCtx)
{
    int32_t i = 0;
    /*cmd list initialize*/
    INIT_LIST_HEAD(&gspnCtx->empty_list);

    while(i < GSPN_KCMD_MAX) {
        list_add_tail(&gspnCtx->kcmds[i].list, &gspnCtx->empty_list);
        //GSPN_LOGE("gspnCtx->kcmds[%d].list_addr = %p, gspnCtx->kcmds[%d] = %p\n ",  i, &(gspnCtx->kcmds[i].list), i, &(gspnCtx->kcmds[i]));
        i++;
    }
    INIT_LIST_HEAD(&gspnCtx->fill_list);
    gspnCtx->empty_list_cnt = GSPN_KCMD_MAX;
    gspnCtx->fill_list_cnt = 0;
    sema_init(&gspnCtx->empty_list_sema, 1);
    sema_init(&gspnCtx->fill_list_sema, 1);
    init_waitqueue_head(&gspnCtx->empty_list_wq);
    init_waitqueue_head(&gspnCtx->sync_done_wq);

    sema_init(&gspnCtx->wake_work_thread_sema,0);
    sema_init(&gspnCtx->suspend_clean_done_sema,0);
    sema_init(&gspnCtx->fence_create_sema,1);

    if(open_sprd_sync_timeline("gspn", &gspnCtx->timeline)) {
        GSPN_LOGE("open timeline failed!\n");
        return -1;
    }
    //GSPN_LOGI("open timeline success. (%p)\n", gspnCtx->timeline);

    gspnCtx->work_thread = kthread_run(gspn_work_thread, (void *)gspnCtx, "gspn_work_thread");
    if (IS_ERR(gspnCtx->work_thread)) {
        GSPN_LOGE("create gspn work thread failed! %ld\n",PTR_ERR(gspnCtx->work_thread));
        gspnCtx->work_thread = NULL;
        goto error;
    }
   // GSPN_LOGI("create gspn work thread success. %ld\n",PTR_ERR(gspnCtx->work_thread));
    return 0;

error:
    close_sprd_sync_timeline(gspnCtx->timeline);
    return -1;
}



static int32_t gspn_drv_probe(struct platform_device *pdev)
{
    GSPN_CONTEXT_T *gspnCtx = NULL;
    int32_t ret = 0;
    int32_t i = 0;
    //struct resource r;

   // printk("%s[%d] enter.\n",__func__,__LINE__);

    /*params check*/
    if(pdev == NULL) {
        GSPN_LOGE("pdev is null!!\n");
        return -EINVAL;
    }

    /*alloc ctx*/
    //gspnCtx =kzalloc(sizeof(GSPN_CONTEXT_T), GFP_KERNEL);
    gspnCtx =vzalloc(sizeof(GSPN_CONTEXT_T));
    if (NULL == gspnCtx) {
        GSPN_LOGE("alloc GSPN_CONTEXT_T failed!\n");
        return -ENOMEM;
    }
    g_gspnCtx = gspnCtx;

    /*ctx initialize*/
    gspnCtx->log_level = 0xFF;
    gspnCtx->gspn_of_dev = &(pdev->dev);
    ret = gspn_parse_dt_init_core(gspnCtx);
    if (ret) {
        goto exit;
    }

    /*chip id check*/
    gspn_cfg_capability(gspnCtx);
    if(gspnCtx->capability.magic != GSPN_CAPABILITY_MAGIC_NUMBER) {
        GSPN_LOGE("chip id not match, maybe there is only GSP not GSPN!\n");
        goto exit;
    }

    /*only on sharkLT8, gsp/gspn select*/
    if(gspnCtx->core_mng.module_sel_reg_base && gspnCtx->capability.version == 0x10) {
        REG_SET(gspnCtx->core_mng.module_sel_reg_base, 1);
    }

    ret = gspn_clock_init(gspnCtx);
    if (ret) {
        goto exit;
    }

     INIT_LIST_HEAD(&gspnCtx->core_mng.dissociation_list);

    /*thread create*/
    ret = gspn_async_init(gspnCtx);
    if (ret) {
        goto exit;
    }

    /*misc dev regist*/
    gspnCtx->dev.minor = MISC_DYNAMIC_MINOR;
    gspnCtx->dev.name = "sprd_gspn";
    gspnCtx->dev.fops = &gspn_drv_fops;
    ret = misc_register(&gspnCtx->dev);
    if (ret) {
        GSPN_LOGE("gspn cannot register miscdev (%d)\n",ret);
        goto exit;
    }

    /*request irq*/
    i = 0;
    while(i<gspnCtx->core_mng.core_cnt) {
        ret = request_irq(gspnCtx->core_mng.cores[i].interrupt_id,
                          gspn_irq_handler,
                          0,//IRQF_SHARED
                          "GSPN",
                          gspnCtx);
        if (ret) {
            GSPN_LOGE("core%d could not request irq %d!\n",i,gspnCtx->core_mng.cores[i].interrupt_id);
            goto exit1;
        }
        i++;
    }


#ifdef CONFIG_HAS_EARLYSUSPEND
    gspnCtx->earlysuspend.suspend = gspn_early_suspend;
    gspnCtx->earlysuspend.resume  = gspn_late_resume;
    gspnCtx->earlysuspend.level   = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
    register_early_suspend(&gspnCtx->earlysuspend);
#endif

    platform_set_drvdata(pdev, gspnCtx);

    GSPN_LOGI("gspn probe success.\n");
    return ret;

exit1:
    misc_deregister(&gspnCtx->dev);

exit:
    if(gspnCtx->work_thread) {
        kthread_stop(gspnCtx->work_thread);
        gspnCtx->work_thread = NULL;
    }

    if (NULL != gspnCtx) {
        if (NULL != gspnCtx->core_mng.cores) {
            i = 0;
            while(i < gspnCtx->core_mng.core_cnt) {
                if(gspnCtx->core_mng.cores[i].cmdq) {
                    kfree(gspnCtx->core_mng.cores[i].cmdq);
                    gspnCtx->core_mng.cores[i].cmdq = NULL;
                }
            }
            vfree(gspnCtx->core_mng.cores);
            gspnCtx->core_mng.cores = NULL;
        }
        vfree(gspnCtx);
        gspnCtx = NULL;
    }

    // TODO: should set to 0 after probe to close log
    //gspnCtx->log_level = 0;

    return ret;
}

static int32_t gspn_drv_remove(struct platform_device *pdev)
{
    int i = 0;
    GSPN_CONTEXT_T *gspnCtx = NULL;
    pr_info("%s[%d] enter.\n",__func__,__LINE__);

    gspnCtx = platform_get_drvdata(pdev);
    if (NULL == gspnCtx) {
        return -EINVAL;
    }

    if(gspnCtx->work_thread) {
        kthread_stop(gspnCtx->work_thread);
        gspnCtx->work_thread = NULL;
    }

    i = 0;
    while(i<gspnCtx->core_mng.core_cnt) {
        free_irq(gspnCtx->core_mng.cores[i].interrupt_id, gspn_irq_handler);
        i++;
    }

    misc_deregister(&gspnCtx->dev);
    if(gspnCtx->core_mng.cores) {
        i = 0;
        while(i < gspnCtx->core_mng.core_cnt) {
            if(gspnCtx->core_mng.cores[i].cmdq) {
                kfree(gspnCtx->core_mng.cores[i].cmdq);
                gspnCtx->core_mng.cores[i].cmdq = NULL;
            }
        }
        vfree(gspnCtx->core_mng.cores);
        gspnCtx->core_mng.cores = NULL;
    }

    vfree(gspnCtx);
    gspnCtx = NULL;
    platform_set_drvdata(pdev, gspnCtx);
    pr_info("%s[%d] exit.\n",__func__,__LINE__);
    return 0;
}

static const struct of_device_id sprdgspn_dt_ids[] = {
    { .compatible = "sprd,gspn", },
    {}
};

static struct platform_driver gspn_driver = {
    .probe = gspn_drv_probe,
    .remove = gspn_drv_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend = gspn_suspend,
    .resume = gspn_resume,
#endif
    .driver =
    {
        .owner = THIS_MODULE,
        .name = "sprd_gspn",
        .of_match_table = of_match_ptr(sprdgspn_dt_ids),
    }
};

static int32_t __init gspn_drv_init(void)
{
    pr_info("%s[%d] enter! \n",__func__,__LINE__);

    if (platform_driver_register(&gspn_driver) != 0) {
        pr_info("%s[%d] gspn platform driver register Failed!\n",__func__,__LINE__);
        return -1;
    } else {
        pr_info("%s[%d] gspn platform driver registered successful!\n",__func__,__LINE__);
    }
    return 0;
}

static void gspn_drv_exit(void)
{
    platform_driver_unregister(&gspn_driver);
    pr_info("%s[%d] gspn platform driver unregistered!\n",__func__,__LINE__);
}

module_init(gspn_drv_init);
module_exit(gspn_drv_exit);

MODULE_DESCRIPTION("GSPN Driver");
MODULE_LICENSE("GPL");


