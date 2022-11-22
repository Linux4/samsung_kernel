config = {
    "header": {
        "uuid": "9b4f06c4-b46a-445c-9806-ac6c6ccc808e",
        "type": "FUELGAUGE",
        "vendor": "S.LSI",
        "product": "S2MU106",
        "variant": "S2MU106",
        "name": "S2MU106 fuelgauge",
        "required": [
            [
            	# uuid: S2MU106 mfd
                "8580d4d0-9bd4-4d93-be72-84d4f66f3dc9",
            ]
        ]
    },
    "interfaces": {
        "inports": [
            {
                "type": "gpio",
                "labels": [
                    {
                        "label": "GPIO: FG_ALERT_GPIO",
                        "parent": "${fg_alert_parent}",
                        "properties": [],
                        "symbol": "${fg_alert_gpio}"
                    }
                ]
            },
            {
                "type": "i2c",
                "labels": [
                    {
                        "label": "I2C: FUELGAUGE",
                        "properties": [],
                        "symbol": "${fuelgauge_i2c}"
                    }
                ]
            }
        ]
    },
    "build": {
        "path": "battery/fuelgauge/s2mu106_fuelgauge",
        "file": "s2mu106_fuelgauge.py",
        "location": [
           {
               "dst": "drivers/battery/fuelgauge/s2mu106_fuelgauge/",
               "src": "s2mu106* Kconfig:update Makefile:update"
           },
           {
               "dst": "arch/arm64/boot/dts/samsung/",
               "src": "*.dtsi"
           }
       ]
    },
    "features": [
        {
            "label": "General",
            "type": "boolean",
            "configs": {
                "True": [
                     "CONFIG_FUELGAUGE_S2MU106=y",
                ],
                "False": []
            },
        },
        {
            "label": "USE 5MILLIOHM",
            "type": "boolean",
            "configs": {
                "True": [
                     "CONFIG_FUELGAUGE_S2MU106_USE_5MILLIOHM=y",
                ],
                "False": []
            },
            "value": True
        }
    ]
}


def load():
    return config

if __name__ == '__main__':
  print(load())
