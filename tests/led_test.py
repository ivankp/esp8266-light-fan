#!/usr/bin/env python3

import requests
from time import sleep

addr = r'http://192.168.4.1/'

def set_led(led):
    r = requests.get(f'{addr}set?led={led}')
    print(r.text)
    assert r.text == f'{{"led":{led}}}'

def test_led():
    set_led(0)
    for _ in range(2):
        sleep(1)
        set_led(1)
        sleep(1)
        set_led(0)
    for _ in range(10):
        sleep(0.05)
        set_led(1)
        sleep(0.05)
        set_led(0)
