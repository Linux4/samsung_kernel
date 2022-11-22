config = {
    "build": {
        "file": "s2mu107_charger.py",
        "location": [
            {
                "dst": "drivers/battery/charger/s2mu107_charger/",
                "src": "s2mu107* Kconfig:update Makefile:update"
            },
            {
                "dst": "arch/arm64/boot/dts/samsung/",
                "src": "s2mu107_charger.dtsi"
            }
        ],
        "path": "battery/charger/s2mu107_charger"
    },
    "interfaces": {
        "outports": [
            {
                "labels": [
                    {
                        "label": "s2mu107-switching-charger",
                        "value": "s2mu107-switching-charger"
                    },
                    {
                        "label": "s2mu107-direct-charger",
                        "value": "s2mu107-direct-charger"
                    }                    
                ],
                "type": "charger_name"
            }
        ]
    },
    "features": [
        {
            "configs": {
                "False": [],
                "True": [
                    "CONFIG_PM_S2MU107=y",
                    "CONFIG_CHARGER_S2MU107=y",
                    "CONFIG_CHARGER_S2MU107_DIRECT=y",
                    "CONFIG_LSI_IFPMIC=y"
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
        "name": "S2MU107 Charger",
        "product": "S2MU107",
        "type": "CHARGER",
        "uuid": "0825f61a-351b-4f24-be1f-1cb228e7f564",
        "required": [
            [
            	# uuid: S2MU107 mfd
                "36bc1216-3f8a-4aad-9e46-b04229d3ec2f",
                # uuid: S2MU107 Fuelgauge
                "d3eaf79a-a724-457b-873f-74690f120d24",
            ]
        ],
        "variant": "S2MU107",
        "vendor": "S.LSI"
    }
}


def load():

    return config

if __name__ == '__main__':
    print(load())
