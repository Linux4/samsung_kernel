#ifndef _JACK_FACTORY_H
#define _JACK_FACTORY_H

struct jack_platform_det {
    bool jack_det;
    bool mic_det;
    bool button_det;
    unsigned int button_code;
    int privious_button_state;
    int adc_val;
};

struct jack_buttons_zone {
    unsigned int code;
    unsigned int adc_low;
    unsigned int adc_high;
};

struct jack_device_priv {
    struct jack_platform_det jack;
    struct switch_dev sdev;
    //struct jack_buttons_zone jack_buttons_zones[4];
    struct jack_buttons_zone *jack_buttons_zones;
    int mic_adc_range;
};

void create_jack_factory_devices(struct jack_device_priv *pPriv_dat);

#endif /* _JACK_FACTORY_H */