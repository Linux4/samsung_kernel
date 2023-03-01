config = {
    "header": {
        "uuid": "5d5aa802-3802-45f8-8b43-3e2ca4c59112",
        "type": "CHARGER",
        "vendor": "S.LSI",
        "product": "S2MU106",
        "variant": "S2MU106",
        "name": "S2MU106 charger",
        "required": [
            [
            	# uuid: S2MU106 mfd
                "8580d4d0-9bd4-4d93-be72-84d4f66f3dc9",
                # uuid: S2MU106 Fuelgauge
                "9b4f06c4-b46a-445c-9806-ac6c6ccc808e",
            ]
        ]
    },
    "interfaces": {
        "outports": [
            {
                "labels": [
                    {
                        "label": "s2mu106-charger",
                        "value": "s2mu106-charger"
                    }
                ],
                "type": "charger_name"
            }
        ]
    },
    "build": {
        "path": "battery/charger/s2mu106_charger",
        "file": "s2mu106_charger.py",
        "location": [
             {
                 "dst": "drivers/battery/charger/s2mu106_charger/",
                 "src": "s2mu106* Kconfig:update Makefile:update"
             },
             {
                "dst": "arch/arm64/boot/dts/samsung/",
                "src": "s2mu106_charger.dtsi"
             }
        ]
    },
    "features": [
         {
             "label": "General",
             "type": "boolean",
             "configs": {
                 "True": [
                    "CONFIG_PM_S2MU106=y",
                    "CONFIG_CHARGER_S2MU106=y",
                    "CONFIG_LSI_IFPMIC=y"
                 ],
                 "False": []
             },
         }
    ]
}


def load():
    return config

if __name__ == '__main__':
  print(load())
