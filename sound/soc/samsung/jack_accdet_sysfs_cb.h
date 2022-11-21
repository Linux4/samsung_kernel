#ifndef _JACK_ACCDET_SYSFS_CB_H
#define _JACK_ACCDET_SYSFS_CB_H

struct accdet_data {
	int jack_state;
	int key_state;
	int mic_adc;
};

void register_accdet_jack_cb(struct accdet_data *accdet_pdata);
#endif /* _JACK_ACCDET_SYSFS_CB_H */
