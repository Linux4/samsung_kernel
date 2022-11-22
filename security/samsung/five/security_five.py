
config = {
    "header": {
        "uuid": "bb0496e5-b4ec-4467-b49d-944423738954",
        "type": "SECURITY",
        "vendor": "SAMSUNG",
        "product": "five",
        "variant": "five",
        "name": "five"
    },
    "build": {
        "path": "security/five",
        "file": "security_five.py",
        "location": [
           {
               "src": "*",
               "dst": "security/samsung/five/"
           },
       ]
    },
    "features": [
       {
           "label": "General",
           "type": "boolean",
           "configs": {
               "True": ["# CONFIG_FIVE is not set"],
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
