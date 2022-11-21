config = {
    "build": {
        "file": "nfc_nfc_logger.py",
        "location": [
            {
                "dst": "drivers/nfc/nfc_logger/",
                "src": "*.c *.h Makefile Kconfig",
            }
        ],
        "path": "nfc/nfc_logger",
    },
    "features": [
        {
            "configs": {
                "False": [],
                "True": [
                    "CONFIG_SEC_NFC_LOGGER=y"
                ],
            },
            "label": "General",
            "type": "boolean",
        }
    ],
    "header": {
        "name": "nfc_logger",
        "product": "nfc_logger",
        "type": "NFC",
        "uuid": "47a3aed4-832f-443e-80f2-1cacca7bc719",
        "variant": "nfc_logger",
        "vendor": "S.Mobile",
    },
}


def load():
    return config


if __name__ == "__main__":
    print(load())
