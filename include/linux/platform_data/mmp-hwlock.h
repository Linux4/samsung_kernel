#ifndef __MMP_HWLOCK_H
#define __MMP_HWLOCK_H

void mmp_hwlock_lock(struct i2c_adapter *);
void mmp_hwlock_unlock(struct i2c_adapter *);
int mmp_hwlock_trylock(struct i2c_adapter *);

/* used for telephony to get access the ripc */
spinlock_t lock_for_ripc;
EXPORT_SYMBOL(lock_for_ripc);

extern bool mmp_hwlock_get_status(void);

#endif
