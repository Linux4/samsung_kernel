#include <linux/sched.h>
#include "gsp_drv_common.h"
#include "gsp_kcfg.h"
#include "gsp_config_if.h"
#include "gsp_work_queue.h"


void print_cfg_info(struct gsp_cfg_info *cfg)
{
    if (NULL == cfg) {
        return;
    }

    GSP_DEBUG("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& \n");
    GSP_DEBUG( "misc: ahb_clock %d|gsp_clock %d|dithering_en %d|gsp_gap %d\n",
            cfg->misc_info.ahb_clock,
            cfg->misc_info.gsp_clock,
            cfg->misc_info.dithering_en,
            cfg->misc_info.gsp_gap);

    GSP_DEBUG( "L0: format:%d,pitch:%d,clip(x:%d,y:%d,w:%d,h:%d) =rot:%d=> des(x:%d,y:%d,w:%d,h:%d)\n",
            cfg->layer0_info.img_format,
            cfg->layer0_info.pitch,
            cfg->layer0_info.clip_rect.st_x,
            cfg->layer0_info.clip_rect.st_y,
            cfg->layer0_info.clip_rect.rect_w,
            cfg->layer0_info.clip_rect.rect_h,
            cfg->layer0_info.rot_angle,
            cfg->layer0_info.des_rect.st_x,
            cfg->layer0_info.des_rect.st_y,
            cfg->layer0_info.des_rect.rect_w,
            cfg->layer0_info.des_rect.rect_h);
    GSP_DEBUG( "L0: alpha:%d,colorkey_en:%d,pallet_en:%d,scaling_en:%d,layer_en:%d,pmargb_en:%d,pmargb_mod:%d\n",
            cfg->layer0_info.alpha,
            cfg->layer0_info.colorkey_en,
            cfg->layer0_info.pallet_en,
            cfg->layer0_info.scaling_en,
            cfg->layer0_info.layer_en,
            cfg->layer0_info.pmargb_en,
            cfg->layer0_info.pmargb_mod);
    GSP_DEBUG( "L0: col_tap_mode:%d,row_tap_mode:%d,a_swap_mode:%d,rgb_swap_mode:%d,va_word_endn:%d,va_lng_wrd_endn:%d,uv_word_endn:%d,uv_lng_wrd_endn:%d,y_word_endn:%d,y_lng_wrd_endn:%d\n",
            cfg->layer0_info.col_tap_mode,
            cfg->layer0_info.row_tap_mode,
            cfg->layer0_info.endian_mode.a_swap_mode,
            cfg->layer0_info.endian_mode.rgb_swap_mode,
            cfg->layer0_info.endian_mode.va_word_endn,
            cfg->layer0_info.endian_mode.va_lng_wrd_endn,
            cfg->layer0_info.endian_mode.uv_word_endn,
            cfg->layer0_info.endian_mode.uv_lng_wrd_endn,
            cfg->layer0_info.endian_mode.y_word_endn,
            cfg->layer0_info.endian_mode.y_lng_wrd_endn);
    GSP_DEBUG( "L0: addr_y:0x%08x,addr_uv:0x%08x,addr_v:0x%08x,(grey r:%d,g:%d,b:%d,a:%d),(colorkey r:%d,g:%d,b:%d,a:%d)\n",
            cfg->layer0_info.src_addr.addr_y,
            cfg->layer0_info.src_addr.addr_uv,
            cfg->layer0_info.src_addr.addr_v,
            cfg->layer0_info.grey.r_val,
            cfg->layer0_info.grey.g_val,
            cfg->layer0_info.grey.b_val,
            cfg->layer0_info.grey.a_val,
            cfg->layer0_info.colorkey.r_val,
            cfg->layer0_info.colorkey.g_val,
            cfg->layer0_info.colorkey.b_val,
            cfg->layer0_info.colorkey.a_val);


    GSP_DEBUG( "L1: format:%d,pitch:%d,clip(x:%d,y:%d,w:%d,h:%d) =rot:%d=> des(x:%d,y:%d)\n",
            cfg->layer1_info.img_format,
            cfg->layer1_info.pitch,
            cfg->layer1_info.clip_rect.st_x,
            cfg->layer1_info.clip_rect.st_y,
            cfg->layer1_info.clip_rect.rect_w,
            cfg->layer1_info.clip_rect.rect_h,
            cfg->layer1_info.rot_angle,
            cfg->layer1_info.des_pos.pos_pt_x,
            cfg->layer1_info.des_pos.pos_pt_y);
    GSP_DEBUG( "L1: alpha:%d,colorkey_en:%d,pallet_en:%d,layer_en:%d,pmargb_en:%d,pmargb_mod:%d\n",
            cfg->layer1_info.alpha,
            cfg->layer1_info.colorkey_en,
            cfg->layer1_info.pallet_en,
            cfg->layer1_info.layer_en,
            cfg->layer1_info.pmargb_en,
            cfg->layer1_info.pmargb_mod);

    GSP_DEBUG( "L1: col_tap_mode:%d,row_tap_mode:%d,a_swap_mode:%d,rgb_swap_mode:%d,va_word_endn:%d,va_lng_wrd_endn:%d,uv_word_endn:%d,uv_lng_wrd_endn:%d,y_word_endn:%d,y_lng_wrd_endn:%d\n",
            cfg->layer1_info.col_tap_mode,
            cfg->layer1_info.row_tap_mode,
            cfg->layer1_info.endian_mode.a_swap_mode,
            cfg->layer1_info.endian_mode.rgb_swap_mode,
            cfg->layer1_info.endian_mode.va_word_endn,
            cfg->layer1_info.endian_mode.va_lng_wrd_endn,
            cfg->layer1_info.endian_mode.uv_word_endn,
            cfg->layer1_info.endian_mode.uv_lng_wrd_endn,
            cfg->layer1_info.endian_mode.y_word_endn,
            cfg->layer1_info.endian_mode.y_lng_wrd_endn);
    GSP_DEBUG( "L1: addr_y:0x%08x,addr_uv:0x%08x,addr_v:0x%08x,(grey r:%d,g:%d,b:%d,a:%d),(colorkey r:%d,g:%d,b:%d,a:%d)\n",
            cfg->layer1_info.src_addr.addr_y,
            cfg->layer1_info.src_addr.addr_uv,
            cfg->layer1_info.src_addr.addr_v,
            cfg->layer1_info.grey.r_val,
            cfg->layer1_info.grey.g_val,
            cfg->layer1_info.grey.b_val,
            cfg->layer1_info.grey.a_val,
            cfg->layer1_info.colorkey.r_val,
            cfg->layer1_info.colorkey.g_val,
            cfg->layer1_info.colorkey.b_val,
            cfg->layer1_info.colorkey.a_val);


    GSP_DEBUG( "Ld cfg:fmt:%d|pitch %04d|cmpr8 %d\n",
            cfg->layer_des_info.img_format,
            cfg->layer_des_info.pitch,
            cfg->layer_des_info.compress_r8_en);
    GSP_DEBUG( "Ld Yaddr 0x%08x|Uaddr 0x%08x|Vaddr 0x%08x\n",
            cfg->layer_des_info.src_addr.addr_y,
            cfg->layer_des_info.src_addr.addr_uv,
            cfg->layer_des_info.src_addr.addr_v);
    GSP_DEBUG( "Ld:a_swap_mode:%d,rgb_swap_mode:%d,va_word_endn:%d,va_lng_wrd_endn:%d,uv_word_endn:%d,uv_lng_wrd_endn:%d,y_word_endn:%d,y_lng_wrd_endn:%d\n",
            cfg->layer_des_info.endian_mode.a_swap_mode,
            cfg->layer_des_info.endian_mode.rgb_swap_mode,
            cfg->layer_des_info.endian_mode.va_word_endn,
            cfg->layer_des_info.endian_mode.va_lng_wrd_endn,
            cfg->layer_des_info.endian_mode.uv_word_endn,
            cfg->layer_des_info.endian_mode.uv_lng_wrd_endn,
            cfg->layer_des_info.endian_mode.y_word_endn,
            cfg->layer_des_info.endian_mode.y_lng_wrd_endn);
    GSP_DEBUG("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& \n");
}


void print_gsp_reg(ulong reg_addr)
{
    GSP_REG_T *g_gsp_reg = (GSP_REG_T *)reg_addr;
    uint32_t i = 0;

    GSP_DEBUG("********************************************* \n");
    GSP_DEBUG("GSP_EN			0x%08x: 0x%08x; 0x%08x: 0x%08x; 0x%08x: 0x%08x;\n",
           (0X20D00000),GSP_REG_READ(GSP_MOD_EN),
           (0X20D01000),GSP_REG_READ(GSP_MOD_EN+0x1000),
           (0X20D02000),GSP_REG_READ(GSP_MOD_EN+0x1000));
    GSP_DEBUG("GSP_RESET		0x%08x: 0x%08x; 0x%08x: 0x%08x; 0x%08x: 0x%08x;\n",
           (0X20D00004),GSP_REG_READ(GSP_SOFT_RESET),
           (0X20D01004),GSP_REG_READ(GSP_SOFT_RESET+0x1000),
           (0X20D02004),GSP_REG_READ(GSP_SOFT_RESET+0x1000));
    GSP_DEBUG("AUTO_GATE		0x%08x: 0x%08x; 0x%08x: 0x%08x; 0x%08x: 0x%08x;\n",
           (0X20D00040),GSP_REG_READ(GSP_AUTO_GATE_ENABLE_BASE),
           (0X20D01040),GSP_REG_READ(GSP_AUTO_GATE_ENABLE_BASE+0x1000),
           (0X20D02040),GSP_REG_READ(GSP_AUTO_GATE_ENABLE_BASE+0x1000));
    GSP_DEBUG("GSP_CLOCK		0x%08x: 0x%08x \n",(0X71200028),GSP_REG_READ(GSP_CLOCK_BASE));
    GSP_DEBUG("AHB_CLOCK		0x%08x: 0x%08x \n",(0X71200020),GSP_REG_READ(GSP_AHB_CLOCK_BASE));
    GSP_DEBUG("EMC_EN			0x%08x: 0x%08x \n",(0X402E0004),GSP_REG_READ(GSP_EMC_MATRIX_BASE));

    while(i < 0x198) {
        GSP_DEBUG("0x%08x: 0x%08x \n",(0x20a00000+i),GSP_REG_READ(reg_addr+i));
        i += 4;
    }
    GSP_DEBUG( "misc: run %d|busy %d|errflag %d|errcode %02d|dither %d|pmmod0 %d|pmmod1 %d|pmen %d|scale %d|reserv2 %d|scl_stat_clr %d|l0en %d|l1en %d|rb %d\n",
            g_gsp_reg->gsp_cfg_u.mBits.gsp_run,
            g_gsp_reg->gsp_cfg_u.mBits.gsp_busy,
            g_gsp_reg->gsp_cfg_u.mBits.error_flag,
            g_gsp_reg->gsp_cfg_u.mBits.error_code,
            g_gsp_reg->gsp_cfg_u.mBits.dither_en,
            g_gsp_reg->gsp_cfg_u.mBits.pmargb_mod0,
            g_gsp_reg->gsp_cfg_u.mBits.pmargb_mod1,
            g_gsp_reg->gsp_cfg_u.mBits.pmargb_en,
            g_gsp_reg->gsp_cfg_u.mBits.scale_en,
            g_gsp_reg->gsp_cfg_u.mBits.y2r_opt,
            g_gsp_reg->gsp_cfg_u.mBits.scale_status_clr,
            g_gsp_reg->gsp_cfg_u.mBits.l0_en,
            g_gsp_reg->gsp_cfg_u.mBits.l1_en,
            g_gsp_reg->gsp_cfg_u.mBits.dist_rb);
    GSP_DEBUG( "misc: inten %d|intmod %d|intclr %d\n",
            g_gsp_reg->gsp_int_cfg_u.mBits.int_en,
            g_gsp_reg->gsp_int_cfg_u.mBits.int_mod,
            g_gsp_reg->gsp_int_cfg_u.mBits.int_clr);


    GSP_DEBUG( "L0 cfg:fmt %d|rot %d|ck %d|pallet %d|rowtap %d|coltap %d\n",
            g_gsp_reg->gsp_layer0_cfg_u.mBits.img_format_l0,
            g_gsp_reg->gsp_layer0_cfg_u.mBits.rot_mod_l0,
            g_gsp_reg->gsp_layer0_cfg_u.mBits.ck_en_l0,
            g_gsp_reg->gsp_layer0_cfg_u.mBits.pallet_en_l0,
            g_gsp_reg->gsp_layer0_cfg_u.mBits.row_tap_mod,
            g_gsp_reg->gsp_layer0_cfg_u.mBits.col_tap_mod);
    GSP_DEBUG( "L0 blockalpha %03d, pitch %04d,(%04d,%04d)%04dx%04d => (%04d,%04d)%04dx%04d\n",
            g_gsp_reg->gsp_layer0_alpha_u.mBits.alpha_l0,
            g_gsp_reg->gsp_layer0_pitch_u.mBits.pitch0,
            g_gsp_reg->gsp_layer0_clip_start_u.mBits.clip_start_x_l0,
            g_gsp_reg->gsp_layer0_clip_start_u.mBits.clip_start_y_l0,
            g_gsp_reg->gsp_layer0_clip_size_u.mBits.clip_size_x_l0,
            g_gsp_reg->gsp_layer0_clip_size_u.mBits.clip_size_y_l0,
            g_gsp_reg->gsp_layer0_des_start_u.mBits.des_start_x_l0,
            g_gsp_reg->gsp_layer0_des_start_u.mBits.des_start_y_l0,
            g_gsp_reg->gsp_layer0_des_size_u.mBits.des_size_x_l0,
            g_gsp_reg->gsp_layer0_des_size_u.mBits.des_size_y_l0);
    GSP_DEBUG( "L0 Yaddr 0x%08x|Uaddr 0x%08x|Vaddr 0x%08x\n",
            g_gsp_reg->gsp_layer0_y_addr_u.dwValue,
            g_gsp_reg->gsp_layer0_uv_addr_u.dwValue,
            g_gsp_reg->gsp_layer0_va_addr_u.dwValue);
    GSP_DEBUG( "L0 grey(%03d,%03d,%03d) colorkey(%03d,%03d,%03d)\n",
            g_gsp_reg->gsp_layer0_grey_rgb_u.mBits.layer0_grey_r,
            g_gsp_reg->gsp_layer0_grey_rgb_u.mBits.layer0_grey_g,
            g_gsp_reg->gsp_layer0_grey_rgb_u.mBits.layer0_grey_b,
            g_gsp_reg->gsp_layer0_ck_u.mBits.ck_r_l0,
            g_gsp_reg->gsp_layer0_ck_u.mBits.ck_g_l0,
            g_gsp_reg->gsp_layer0_ck_u.mBits.ck_b_l0);
    GSP_DEBUG( "L0 endian: y %d|u %d|v %d|rgb %d|alpha %d\n",
            g_gsp_reg->gsp_layer0_endian_u.mBits.y_endian_mod_l0,
            g_gsp_reg->gsp_layer0_endian_u.mBits.uv_endian_mod_l0,
            g_gsp_reg->gsp_layer0_endian_u.mBits.va_endian_mod_l0,
            g_gsp_reg->gsp_layer0_endian_u.mBits.rgb_swap_mod_l0,
            g_gsp_reg->gsp_layer0_endian_u.mBits.a_swap_mod_l0);


    GSP_DEBUG( "L1 cfg:fmt %d|rot %d|ck %d|pallet %d\n",
            g_gsp_reg->gsp_layer1_cfg_u.mBits.img_format_l1,
            g_gsp_reg->gsp_layer1_cfg_u.mBits.rot_mod_l1,
            g_gsp_reg->gsp_layer1_cfg_u.mBits.ck_en_l1,
            g_gsp_reg->gsp_layer1_cfg_u.mBits.pallet_en_l1);
    GSP_DEBUG( "L1 blockalpha %03d, pitch %04d,(%04d,%04d)%04dx%04d => (%04d,%04d)\n",
            g_gsp_reg->gsp_layer1_alpha_u.mBits.alpha_l1,
            g_gsp_reg->gsp_layer1_pitch_u.mBits.pitch1,
            g_gsp_reg->gsp_layer1_clip_start_u.mBits.clip_start_x_l1,
            g_gsp_reg->gsp_layer1_clip_start_u.mBits.clip_start_y_l1,
            g_gsp_reg->gsp_layer1_clip_size_u.mBits.clip_size_x_l1,
            g_gsp_reg->gsp_layer1_clip_size_u.mBits.clip_size_y_l1,
            g_gsp_reg->gsp_layer1_des_start_u.mBits.des_start_x_l1,
            g_gsp_reg->gsp_layer1_des_start_u.mBits.des_start_y_l1);
    GSP_DEBUG( "L1 Yaddr 0x%08x|Uaddr 0x%08x|Vaddr 0x%08x\n",
            g_gsp_reg->gsp_layer1_y_addr_u.dwValue,
            g_gsp_reg->gsp_layer1_uv_addr_u.dwValue,
            g_gsp_reg->gsp_layer1_va_addr_u.dwValue);
    GSP_DEBUG( "L1 grey(%03d,%03d,%03d) colorkey(%03d,%03d,%03d)\n",
            g_gsp_reg->gsp_layer1_grey_rgb_u.mBits.grey_r_l1,
            g_gsp_reg->gsp_layer1_grey_rgb_u.mBits.grey_g_l1,
            g_gsp_reg->gsp_layer1_grey_rgb_u.mBits.grey_b_l1,
            g_gsp_reg->gsp_layer1_ck_u.mBits.ck_r_l1,
            g_gsp_reg->gsp_layer1_ck_u.mBits.ck_g_l1,
            g_gsp_reg->gsp_layer1_ck_u.mBits.ck_b_l1);
    GSP_DEBUG( "L1 endian: y %d|u %d|v %d|rgb %d|alpha %d\n",
            g_gsp_reg->gsp_layer1_endian_u.mBits.y_endian_mod_l1,
            g_gsp_reg->gsp_layer1_endian_u.mBits.uv_endian_mod_l1,
            g_gsp_reg->gsp_layer1_endian_u.mBits.va_endian_mod_l1,
            g_gsp_reg->gsp_layer1_endian_u.mBits.rgb_swap_mod_l1,
            g_gsp_reg->gsp_layer1_endian_u.mBits.a_swap_mod_l1);


    GSP_DEBUG( "Ld cfg:fmt %d|cmpr8 %d|pitch %04d\n",
            g_gsp_reg->gsp_des_data_cfg_u.mBits.des_img_format,
            g_gsp_reg->gsp_des_data_cfg_u.mBits.compress_r8,
            g_gsp_reg->gsp_des_pitch_u.mBits.des_pitch);
    GSP_DEBUG( "Ld Yaddr 0x%08x|Uaddr 0x%08x|Vaddr 0x%08x\n",
            g_gsp_reg->gsp_des_y_addr_u.dwValue,
            g_gsp_reg->gsp_des_uv_addr_u.dwValue,
            g_gsp_reg->gsp_des_v_addr_u.dwValue);
    GSP_DEBUG( "Ld endian: y %d|u %d|v %d|rgb %d|alpha %d\n",
            g_gsp_reg->gsp_des_data_endian_u.mBits.y_endian_mod,
            g_gsp_reg->gsp_des_data_endian_u.mBits.uv_endian_mod,
            g_gsp_reg->gsp_des_data_endian_u.mBits.v_endian_mod,
            g_gsp_reg->gsp_des_data_endian_u.mBits.rgb_swap_mod,
            g_gsp_reg->gsp_des_data_endian_u.mBits.a_swap_mod);
    GSP_DEBUG( "********************************************* \n");
}

void print_work_queue_status(struct gsp_work_queue *wq)
{
	int empty_cnt = -1;
	int fill_cnt = -1;
	int sep_cnt = -1;

	if (wq != NULL) {
		empty_cnt = wq->empty_cnt;
		fill_cnt = wq->fill_cnt;
		sep_cnt = wq->sep_cnt;
		GSP_DEBUG("empty count: %d, fill count: %d"
				"sep count: %d\n", empty_cnt, fill_cnt, sep_cnt);
	} else
		GSP_DEBUG("can't print null workqueue status\n");
}

/*
 * The task state array is a strange "bitmap" of
 * reasons to sleep. Thus "running" is zero, and
 * you can test for combinations of others with
 * simple bit tests.
 */
static const char * const task_state_array[] = {
	"R (running)",		/*   0 */
	"S (sleeping)",		/*   1 */
	"D (disk sleep)",	/*   2 */
	"T (stopped)",		/*   4 */
	"t (tracing stop)",	/*   8 */
	"Z (zombie)",		/*  16 */
	"X (dead)",		/*  32 */
	"x (dead)",		/*  64 */
	"K (wakekill)",		/* 128 */
	"W (waking)",		/* 256 */
	"P (parked)",		/* 512 */
};

static inline const char *get_task_state(struct task_struct *tsk)
{
	unsigned int state = (tsk->state & TASK_REPORT) | tsk->exit_state;
	const char * const *p = &task_state_array[0];

	BUILD_BUG_ON(1 + ilog2(TASK_STATE_MAX) != ARRAY_SIZE(task_state_array));

	while (state) {
		p++;
		state >>= 1;
	}
	return *p;
}

void gsp_work_thread_status_print(struct gsp_context *ctx)
{
	if (NULL == ctx) {
		GSP_ERR("can't print work thread status with null context\n");
		return;
	}

	GSP_DEBUG("work thread state: %s\n", get_task_state(ctx->work_thread));
}

void gsp_timeout_print(struct gsp_context *ctx)
{
	struct gsp_kcfg_info *kcfg = NULL;
	if (NULL == ctx) {
		GSP_ERR("can't print timeout info with null context\n");
		return;
	}

	kcfg = gsp_current_kcfg_get(ctx);
	if (kcfg == NULL) {
		GSP_DEBUG("gsp current kcfg is null\n");	
		return;
	}
	gsp_work_thread_status_print(ctx);
    GSP_DEBUG("%s%d:GSP_EN:0x%08x,EMC_MATRIX:0x%08x,IOMMU:0x%08x,GSP_GAP:0x%08x,"
			"GSP_CLOCK:0x%08x,GSP_AUTO_GATE:0x%08x\n",
			__func__, __LINE__,
           (uint32_t)(GSP_REG_READ(GSP_MOD_EN)&GSP_MOD_EN_BIT),
           (uint32_t)(GSP_REG_READ(GSP_EMC_MATRIX_BASE)&GSP_EMC_MATRIX_BIT),
           GSP_REG_READ(ctx->mmu_reg_addr),
           ((volatile GSP_REG_T*)ctx->gsp_reg_addr)->gsp_cfg_u.mBits.dist_rb,
           GSP_REG_READ(GSP_CLOCK_BASE)&0x3,
           (uint32_t)(GSP_REG_READ(GSP_AUTO_GATE_ENABLE_BASE)&GSP_AUTO_GATE_ENABLE_BIT));
    print_cfg_info(&kcfg->cfg);
    print_gsp_reg(ctx->gsp_reg_addr);
}

