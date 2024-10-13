#include <linux/types.h>
enum{
	DISP_PW_DOMAIN_DISPC = 0,
	DISP_PW_DOMAIN_GSP,
	DISP_PW_DOMAIN_VPP,
	DISP_PW_DOMAIN_COUNT_MAX,
};
enum{
	DISP_PW_DOMAIN_OFF = 0,
	DISP_PW_DOMAIN_ON,
};
struct client_info{
	u8 pw_state;
};

struct pw_domain_info{
	struct client_info pw_dispc_info;
	struct client_info pw_gsp_info;
	struct client_info pw_vpp_info;
	struct mutex client_lock;
	u8 pw_count;
};

void disp_pw_on(u8 client);
int disp_pw_off(u8 client);
