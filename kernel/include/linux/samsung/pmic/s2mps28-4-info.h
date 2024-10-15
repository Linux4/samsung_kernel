
#ifndef __LINUX_MFD_S2MPS28_4_REG_H
#define __LINUX_MFD_S2MPS28_4_REG_H

/* S2MPS28 sub4 Regulator ids */
enum s2mps28_regulators {
	S2MPS28_BUCK1,
	S2MPS28_BUCK2,
	S2MPS28_BUCK3,
	S2MPS28_BUCK4,
	S2MPS28_BUCK5,
	S2MPS28_BUCK_SR1,
	S2MPS28_LDO1,
	S2MPS28_LDO2,
	//S2MPS28_LDO3,
	S2MPS28_LDO4,
	S2MPS28_LDO5,
	S2MPS28_REG_MAX,
};

/* S2MPS28 shared i2c API function */
extern int s2mps28_4_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *dest);
extern int s2mps28_4_bulk_read(struct i2c_client *i2c, uint8_t reg, int count,
			     uint8_t *buf);
extern int s2mps28_4_write_reg(struct i2c_client *i2c, uint8_t reg, uint8_t value);
extern int s2mps28_4_bulk_write(struct i2c_client *i2c, uint8_t reg, int count,
			      uint8_t *buf);
extern int s2mps28_4_update_reg(struct i2c_client *i2c, uint8_t reg, uint8_t val, uint8_t mask);

extern int s2mps28_4_notifier_init(struct s2mps28_dev *s2mps28);
extern void s2mps28_4_notifier_deinit(struct s2mps28_dev *s2mps28);
#endif /* __LINUX_MFD_S2MPS28_4_REG_H */
