#ifndef _BAFFINE_MODEM_H_
#define _BAFFINE_MODEM_H_



extern void do_loopback(void);
extern int _pld_write_register(u16 addr, u8 *data, u32 len);
extern int _pld_read_register(u16 addr, u8 *data, u32 len);
extern void pld_loopback_test(struct work_struct *work);
extern u32 SPI_IPC_Txd(u16 addr, u8 *data, u32 data_len);

extern u32 SPI_IPC_Rxd(u16 addr, u8 *data, u32 data_len);

extern void pld_control_via_off(u32 value);
extern void pld_control_via_reset(u32 value);
extern void pld_control_cs_n(u32 value);


#define M_32_SWAP(a) {					\
		  u32 _tmp;					\
		  _tmp = a;					\
		  ((u8 *)&a)[0] = ((u8 *)&_tmp)[3]; \
		  ((u8 *)&a)[1] = ((u8 *)&_tmp)[2]; \
		  ((u8 *)&a)[2] = ((u8 *)&_tmp)[1]; \
		  ((u8 *)&a)[3] = ((u8 *)&_tmp)[0]; \
		}

#define M_32_SWAP_2(a) {					\
		  u32 _tmp;					\
		  _tmp = a;					\
		  ((u8 *)&a)[0] = ((u8 *)&_tmp)[1]; \
		  ((u8 *)&a)[1] = ((u8 *)&_tmp)[0]; \
		  ((u8 *)&a)[2] = ((u8 *)&_tmp)[3]; \
		  ((u8 *)&a)[3] = ((u8 *)&_tmp)[2]; \
		}

#define M_16_SWAP(a) {					\
		 u16 _tmp;					\
		 _tmp = (u16)a;				\
		 ((u8 *)&a)[0] = ((u8 *)&_tmp)[1];  \
		 ((u8 *)&a)[1] = ((u8 *)&_tmp)[0];  \
		}

#endif /* _BAFFINE_MODEM_H_ */

