
enum {
	CAM_PW_DOMAIN_DCAM = 0,
	CAM_PW_DOMAIN_ISP,
	CAM_PW_DOMAIN_JPG,
	CAM_PW_DOMAIN_COUNT_MAX
};
enum {
	CAM_PW_DOMAIN_OFF = 0,
	CAM_PW_DOMAIN_ON,
};
struct client_info_t {
	u8 pw_state;
	u8 pw_count;
};

struct cam_pw_domain_info_t {
	struct client_info_t pw_cam_info[CAM_PW_DOMAIN_COUNT_MAX];
	struct mutex client_lock;
	u8 cam_pw_state;
};

int cam_pw_on(u8 client);
int cam_pw_off(u8 client);
