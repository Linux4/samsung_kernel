config = {
    "header": {
        "uuid": "4a5e31db-d4ce-41f3-8743-9f02ea50cea1",
        "type": "SECURITY",
        "vendor": "SAMSUNG",
        "product": "mz",
        "variant": "mz",
        "name": "mz",
    },
    "build": {
        "path": "security/mz",
        "file": "security_mz.py",
        "location": [
            {
                "src": "*",
                "dst": "security/samsung/mz/",
            },
            {
                "src": "include/linux/mz.h",
                "dst": "include/linux/mz.h",
            },
        ],
    },
    "features": [
        {
            "label": "BUILD TYPE",
            "configs": {
                "not set": [
                    "# CONFIG_MEMORY_ZEROISATION is not set"
                ],
                "module": [
                    "CONFIG_MEMORY_ZEROISATION=m"
                ],
                "built-in": [
                    "CONFIG_MEMORY_ZEROISATION=y"
                ],
            },
             "list_value": [
                "not set",
                "module",
                "built-in",
            ],
            "type": "list",
            "value": "not set",
        }
    ],
    "kunit_test": {
            "build": {
                "location": [
                    {
                        "dst": "security/samsung/mz/test/",
                        "src": "test/*.c test/*.h test/Makefile:cp",
                    },
                ],
            },
            "features": [
                {
                    "label": "default",
                    "configs": {
                        "True": [
                            "CONFIG_MEMORY_ZEROISATION=y",
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
