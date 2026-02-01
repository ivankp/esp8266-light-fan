#!/usr/bin/env python3

from test import Test, TestGroup
import requests
from time import sleep

addr = r'http://192.168.4.1/'

@Test
def set_led(led):
    r = requests.get(f'{addr}set?led={led}')
    assert r.text == f'{{"led":{led}}}'

@TestGroup
def test_led():
    set_led(0)
    for _ in range(10):
        sleep(1)
        set_led(1)
        sleep(1)
        set_led(0)
