#ifndef __SPRD_WLAN_FIFO_H_
#define __SPRD_WLAN_FIFO_H_

#include "wlan_common.h"

#define TX_MSG_HEAD_FILED(hdr)         ((sizeof(t_msg_hdr_t) + ((2 == ((hdr)->type)) ? 34 : 0)))
#define TX_MSG_UNIT_LEN(hdr)           ((ALIGN_4BYTE(TX_MSG_HEAD_FILED(hdr) + ((hdr)->len))))
#define TX_MSG_NEXT_MSG(hdr)           (((t_msg_hdr_t *)((unsigned char *)(hdr) + TX_MSG_UNIT_LEN(hdr))))

#define TX_FIFO_FULL                   (-2)
#define TX_FIFO_EMPTY                  (-3)
#define HW_WRITE_ERROR                 (-4)
#define HW_READ_ERROR                  (-6)

#define RX_FIFO_BLOCK_NUM              (8)
#define RX_FIFO_BLOCK_SIZE             HW_RX_SIZE

typedef int (*P_FUNC_1) (unsigned short, unsigned char *, unsigned int);
typedef int (*P_FUNC_2) (unsigned short, unsigned char *, unsigned int *);
typedef int (*P_FUNC_3) (unsigned char *, unsigned int);

typedef struct {
	unsigned char *data;
	unsigned short len;
} dataInfo;

typedef struct {
	unsigned char type:7;	/* CMD/RSP/USER DATA */
	unsigned char mode:1;	/* message dest mode: 1 STA, 2 softAP, 3 P2P */
	unsigned char subtype;	/* SEARCH/ ATTACH/DETACH */
	unsigned short len;	/* Length not include common header */
} t_msg_hdr_t;

typedef struct {
	unsigned char type:7;	/* CMD/RSP/USER DATA */
	unsigned char mode:1;	/* message dest mode: 1 STA, 2 softAP, 3 P2P */
	unsigned char subtype;	/* SEARCH/ ATTACH/DETACH */
	unsigned short len;	/* Length not include common header */
} r_msg_hdr_t;

typedef struct {
	unsigned char mode;
	unsigned char msg_num;
	unsigned short len;
	t_msg_hdr_t msg[14];
	unsigned int tx_cnt;
} tx_big_hdr_t;

typedef struct {
	t_msg_hdr_t hdr;
	dataInfo slice[2];
	void *p;
} tx_msg_t;

typedef struct {
	unsigned int WT;
	unsigned int RD;
	int LASTWT;
	unsigned char *mem;
	unsigned int size;
	unsigned int cp2_txRam;
	unsigned short max_msg_num;
	spinlock_t lock;
	unsigned int wt_cnt;
	unsigned int rd_cnt;
	unsigned int seq1;
	unsigned int seq2;
} txfifo_t;

typedef struct {
	unsigned int size;
	unsigned int cp2_txRam;
	unsigned short max_msg_num;
} txfifo_conf_t;

typedef struct {
	unsigned int WT;
	unsigned int RD;
	unsigned int block_size;
	unsigned int block_num;
	unsigned char *mem[RX_FIFO_BLOCK_NUM];
} rxfifo_t;
extern int tx_fifo_alloc(txfifo_t *tx_fifo,  txfifo_conf_t *conf);
extern int tx_fifo_free(txfifo_t *tx_fifo);
extern int tx_fifo_in(txfifo_t *fifo,  tx_msg_t *msg);
extern int tx_fifo_out(const unsigned char netif_id, const unsigned chn,
		       txfifo_t *fifo, P_FUNC_1 pfunc, unsigned short *count);
extern int rx_fifo_in(const unsigned char chn, rxfifo_t *fifo, P_FUNC_2 pfunc);
extern int rx_fifo_out(rxfifo_t *fifo, P_FUNC_3 pfunc);
extern int rx_fifo_used(rxfifo_t *fifo);
extern unsigned int tx_fifo_used(txfifo_t *fifo);
extern int rx_fifo_alloc(rxfifo_t *fifo);
extern int rx_fifo_free(rxfifo_t *fifo);
extern unsigned int tx_fifo_in_pkt(txfifo_t *tx_fifo);
#endif

