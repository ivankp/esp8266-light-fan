#!/usr/bin/env python3

from test import Test, TestGroup
import requests

addr = r'http://192.168.4.1/'

@Test
def test(data, code, expected):
    resp = requests.post(addr + 'connect', data = data)
    assert resp.status_code == code
    assert resp.text == expected

@TestGroup
def post_connect():
    test('.' * 200, 400, 'Invalid SSID or PASS')
    test('.' * 100, 400, 'Invalid SSID or PASS')
    test('.' *  99, 400, 'Invalid SSID or PASS')
    test('\0' * 99, 400, 'Invalid SSID or PASS')

    test('.' *  98, 400, 'Invalid SSID')
