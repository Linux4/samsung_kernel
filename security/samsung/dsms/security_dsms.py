
config = {
    "header": {
        "uuid": "49a14baf-5930-467b-8df9-bec056adacea",
        "type": "SECURITY",
        "vendor": "SAMSUNG",
        "product": "dsms",
        "variant": "dsms",
        "name": "dsms"
    },
    "build": {
        "path": "security/dsms",
        "file": "security_dsms.py",
        "location": [
           {
               "src": "*",
               "dst": "security/samsung/dsms/"
           },
           {
               "src": "include/linux/dsms.h",
               "dst": "include/linux/dsms.h"
           }
       ]
    },
    "features": [
       {
           "label": "General",
           "type": "boolean",
           "configs": {
               "True": ["# CONFIG_SECURITY_DSMS is not set"],
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
