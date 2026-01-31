#!/usr/bin/env python3

import requests
from time import sleep

addr = r'http://192.168.4.1/'

def set_led(led):
    r = requests.get(f'{addr}set?led={led}')
    print(r.text)
    assert r.text == f'{{"led":{led}}}'

if __name__ == "__main__":
    set_led(0)
    for _ in range(10):
        sleep(1)
        set_led(1)
        sleep(1)
        set_led(0)
