config = {
    "header": {
        "uuid": "5ce3efce-80ea-430b-8dca-733cfaa3afa8",
        "type": "CHARGER",
        "vendor": "NXP",
        "product": "PCA9468",
        "variant": "PCA9468",
        "name": "PCA9468 Charger"
    },
    "interfaces": {
        "inports": [
            {
                "labels": [
                    {
                        "label": "I2C: DIRECT_CHARGER",
                        "properties": [],
                        "symbol": "${direct_charger_i2c}"
                    }
                ],
                "type": "i2c"
            },
            {
                "labels": [
                    {
                        "label": "GPIO: DC_IRQ",
                        "parent": "${dc_irq_parent}",
                        "properties": [],
                        "symbol": "${dc_irq_gpio}"
                    },
                    {
                        "label": "GPIO: DC_EN",
                        "parent": "${dc_en_parent}",
                        "properties": [],
                        "symbol": "${dc_en_gpio}"
                    }
                ],
                "type": "gpio"
            }
        ],
        "outports": [
            {
                "labels": [
                    {
                        "label": "pca9468-charger",
                        "value": "pca9468-charger"
                    }
                ],
                "type": "charger_name"
            }
        ]
    },
    "build": {
        "path": "battery/charger/pca9468_charger",
        "file": "pca9468_charger.py",
        "location": [
            {
                "dst": "drivers/battery/charger/pca9468_charger/",
                "src": "pca9468* Kconfig:update Makefile:update"
            },
            {
                "dst": "arch/arm64/boot/dts/samsung/",
                "src": "pca9468_charger.dtsi"
            }
       ]
    },
    "features": [
        {
            "configs": {
                "False": [],
                "True": [
                    "CONFIG_CHARGER_PCA9468=y",
                ]
            },
            "label": "General",
            "type": "boolean"
        },
        {
            "configs": {
                "False": [],
                "True": [
                    "CONFIG_SEND_PDMSG_IN_PPS_REQUEST_WORK=y"
                ]
            },
            "label": "SEND_PDMSG_IN_PPS_REQUEST_WORK",
            "type": "boolean",
            "value": True
        }
    ]
}


def load():
    return config

if __name__ == '__main__':
  print(load())
