config = {
    "header": {
        "uuid": "26aca962-0907-423d-99cb-e95b31eca255",
        "type": "SECURITY",
        "vendor": "SAMSUNG",
        "product": "defex_lsm",
        "variant": "defex_lsm",
        "name": "defex_lsm",
    },
    "build": {
        "path": "security/defex_lsm",
        "file": "security_defex_lsm.py",
        "location": [
            {
                "src": "*.c Makefile:update Kconfig:update",
                "dst": "security/samsung/defex_lsm/",
            },
            {
                "src": "catch_engine/*",
                "dst": "security/samsung/defex_lsm/catch_engine/",
            },
            {
                "src": "cert/*",
                "dst": "security/samsung/defex_lsm/cert/",
            },
            {
                "src": "core/*",
                "dst": "security/samsung/defex_lsm/core/",
            },
            {
                "src": "debug/*",
                "dst": "security/samsung/defex_lsm/debug/",
            },
            {
                "src": "feature_immutable/*",
                "dst": "security/samsung/defex_lsm/feature_immutable/",
            },
            {
                "src": "feature_privilege_escalation_detection/*",
                "dst": "security/samsung/defex_lsm/feature_privilege_escalation_detection/",
            },
            {
                "src": "feature_safeplace/*",
                "dst": "security/samsung/defex_lsm/feature_safeplace/",
            },
            {
                "src": "include/*.h",
                "dst": "security/samsung/defex_lsm/include/",
            },
            {
                "src": "include/linux/defex.h",
                "dst": "include/linux/defex.h",
            },
        ],
    },
    "features": [
        {
            "label": "General",
            "type": "boolean",
            "configs": {
                "True": [
                    "# CONFIG_SECURITY_DEFEX is not set"
                ],
                "False": [],
            },
            "value": True,
        }
    ],
    "kunit_test": {
            "build": {
                "location": [
                    {
                        "dst": "security/samsung/defex_lsm/kunit_test/",
                        "src": "kunit_test/*.c kunit_test/Makefile:cp",
                    },
                ],
            },
            "features": [
                {
                    "label": "default",
                    "configs": {
                        "True": [
                            "CONFIG_SECURITY_DEFEX=y",
                        ],
                        "False": [],
                    },
                    "type": "boolean",
                },
            ]
    },
}


def load():
    return config


if __name__ == "__main__":
    print(load())
