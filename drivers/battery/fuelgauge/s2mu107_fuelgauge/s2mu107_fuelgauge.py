config = {
    "build": {
        "file": "s2mu107_fuelgauge.py",
        "location": [
            {
                "dst": "drivers/battery/fuelgauge/s2mu107_fuelgauge/",
                "src": "s2mu107* Kconfig:update Makefile:update"
            },
            {
                "dst": "arch/arm64/boot/dts/samsung/",
                "src": "*.dtsi"
            }
        ],
        "path": "battery/fuelgauge/s2mu107_fuelgauge"
    },
    "features": [
        {
            "configs": {
                "False": [],
                "True": [
                    "CONFIG_FUELGAUGE_S2MU107=y",
                    "CONFIG_FUELGAUGE_S2MU107_BATCAP_LRN=y",
                    "CONFIG_FUELGAUGE_S2MU107_TEMP_COMPEN=y"
                ]
            },
            "label": "General",
            "type": "boolean"
        },
        {
            "label": "DT_COPY",
            "type": "boolean",
            "value": True
        }
    ],
    "header": {
        "name": "S2MU107 Fuelgauge",
        "product": "S2MU107",
        "type": "FUELGAUGE",
        "uuid": "d3eaf79a-a724-457b-873f-74690f120d24",
        "required": [
            [
              	# uuid: S2MU107 mfd
                "36bc1216-3f8a-4aad-9e46-b04229d3ec2f",
            ]
        ],
        "variant": "S2MU107",
        "vendor": "S.LSI"
    },
    "interfaces": {
        "inports": [
            {
                "labels": [
                    {
                        "label": "GPIO: FG_ALERT_GPIO",
                        "parent": "${fg_alert_parent}",
                        "properties": [],
                        "symbol": "${fg_alert_gpio}"
                    }
                ],
                "type": "gpio"
            },
            {
                "labels": [
                    {
                        "label": "I2C: FUELGAUGE",
                        "properties": [],
                        "symbol": "${fuelgauge_i2c}"
                    }
                ],
                "type": "i2c"
            }
        ]
    }
}


def load():

    return config

if __name__ == '__main__':
    print(load())
