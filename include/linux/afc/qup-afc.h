#ifndef _QUP_AFC_H
#define _QUP_AFC_H
#include <linux/qcom-geni-se.h>

#define AFC_FW_PORT_NUMBER	6
#define AFC_CORE2X_VOTE		(960)

/* M_CMD OP codes for AFC */
#define AFC_WRITE		(0x1)
#define AFC_READ		(0x2)
#define AFC_WRITE_READ		(0x3)

#define RX_STOP_BIT		(0x1)

enum {
	CHECK_AFC	= 0,
	SET_VOLTAGE 	= 1,
};

enum {
	AFC_INIT,
	NOT_AFC,
	AFC_FAIL,
	AFC_DISABLE,
	NON_AFC_MAX
};

/* AFC detected */
enum {
	AFC_5V = NON_AFC_MAX,
	AFC_9V,
	AFC_12V,
};

enum {
	SET_5V	= 5,
	SET_9V	= 9,
};

struct geni_afc_dev {
	struct device *dev;
	void __iomem *base;
	unsigned int tx_wm;
	int irq;
	struct se_geni_rsc afc_rsc;
	struct device *wrapper_dev;
	unsigned int afc_tx_buf[1];		//master sends a single byte over the wire 
	unsigned int afc_rx_buf[16];     	//max 16 bytes can be received
	unsigned int vol;
	unsigned int purpose;
	unsigned int afc_cnt;			//AFC communcation count
	unsigned int afc_error;
	bool	afc_fw;
	bool	afc_disable;
	struct work_struct afc_work;
	struct work_struct start_afc_work;
	struct mutex afc_mutex;
};
#endif /* _QUP_AFC_H */
