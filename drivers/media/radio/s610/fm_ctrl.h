void fm_pwron(void);
void fm_pwroff(void);
void fmspeedy_wakeup(void);
void fm_speedy_m_int_enable(void);
void fm_speedy_m_int_disable(void);
void fm_en_speedy_m_int(void);
void fm_dis_speedy_m_int(void);
void fm_speedy_m_int_stat_clear(void);
u32 fmspeedy_get_reg(u32 addr);
void fmspeedy_set_reg(u32 addr, u32 data);
u32 fmspeedy_get_reg_field(u32 addr, u32 shift, u32 mask);
void fmspeedy_set_reg_field(u32 addr, u32 shift, u32 mask, u32 data);
u32 fmspeedy_get_reg_int(u32 addr);
void fmspeedy_set_reg_int(u32 addr, u32 data);
void fm_audio_control(struct s610_radio *radio, bool audio_out, bool lr_switch,
		u32 req_time, u32 audio_addr);

