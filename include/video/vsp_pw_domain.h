
enum {
	VSP_PW_DOMAIN_VSP = 0,
	VSP_PW_DOMAIN_VPP,
	VSP_PW_DOMAIN_COUNT_MAX
};
enum {
	VSP_PW_DOMAIN_OFF = 0,
	VSP_PW_DOMAIN_ON,
};
struct client_info_t {
	u8 pw_state;
	u8 pw_count;
};

struct vsp_pw_domain_info_t {
	struct client_info_t pw_vsp_info[VSP_PW_DOMAIN_COUNT_MAX];
	struct mutex client_lock;
	u8 vsp_pw_state;
};

int vsp_pw_on(u8 client);
int vsp_pw_off(u8 client);
