#!/usr/bin/env python3

import pytest
import requests
from time import sleep

addr = r'http://192.168.4.1/'

def set_led(led):
    r = requests.get(f'{addr}set?led={led}')
    print(r.text)
    assert r.text == f'{{"led":{led}}}'

@pytest.mark.slow
def test_led():
    set_led(0)
    for _ in range(4):
        sleep(1)
        set_led(1)
        sleep(1)
        set_led(0)
    for _ in range(4):
        sleep(0.2)
        set_led(1)
        sleep(0.2)
        set_led(0)
