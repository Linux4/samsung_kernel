
config = {
    "header": {
        "uuid": "111883b2-b27a-41d6-b77d-997bb9c25bcd",
        "type": "SECURITY",
        "vendor": "SAMSUNG",
        "product": "icdrv",
        "variant": "icdrv",
        "name": "icdrv"
    },
    "build": {
        "path": "security/icdrv",
        "file": "security_icdrv.py",
        "location": [
           {
               "src": "*",
               "dst": "drivers/security/samsung/icdrv/"
           },
       ]
    },
    "features": [
       {
           "label": "General",
           "type": "boolean",
           "configs": {
               "True": ["CONFIG_ICD=y"],
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
