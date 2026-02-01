#!/usr/bin/env python3

from test import Test, TestGroup
import requests

addr = r'http://192.168.4.1/'

MAX_SSID_LEN = 32
MAX_PASSPHRASE_LEN = 64
MAX_TOTAL_LEN = MAX_SSID_LEN + 1 + MAX_PASSPHRASE_LEN + 1

@Test
def test(data, code, expected):
    resp = requests.post(addr + 'connect', data = data)
    # assert resp.status_code == code, f'{resp.status_code} != {code}'
    assert resp.text == expected, f'"{resp.text}" != "{expected}"'

@TestGroup
def post_connect():
    # too long
    test(b'.'  * (MAX_TOTAL_LEN + 100), 400, 'Invalid SSID or PASS')
    test(b'.'  * (MAX_TOTAL_LEN + 200), 400, 'Invalid SSID or PASS')
    test(b'.'  * (MAX_TOTAL_LEN +   1), 400, 'Invalid SSID or PASS')
    test(b'\0' * (MAX_TOTAL_LEN +   1), 400, 'Invalid SSID or PASS')

    # SSID not null terminated
    test(b'.' * MAX_TOTAL_LEN, 400, 'Invalid SSID')
    test(b'.' * (MAX_TOTAL_LEN - 1), 400, 'Invalid SSID')
    test(b'Network', 400, 'Invalid SSID')
    test(b'N', 400, 'Invalid SSID')

    # SSID too short
    test(b'\0', 400, 'Invalid SSID')
    test(b'\0\0', 400, 'Invalid SSID')
    test(b'\0\0\0', 400, 'Invalid SSID')

    # SSID too long
    test(b'.' * (MAX_SSID_LEN + 1) + b'\0', 400, 'Invalid SSID')

    # PASS not null terminated
    test(b'ValidSSID\0a', 400, 'Invalid PASS')
    test(b'ValidSSID\0aaaaaaaaaaa', 400, 'Invalid PASS')

    # PASS too long
    test(b'Network\0' + b'-' * (MAX_PASSPHRASE_LEN + 1) + b'\0', 400, 'Invalid PASS')

    # Trailing bytes
    test(b'N\0\0\0', 400, 'Invalid PASS')
    test(b'N\0\0.', 400, 'Invalid PASS')
    test(b'Network\0\0\0', 400, 'Invalid PASS')
    test(b'Network\0\0.', 400, 'Invalid PASS')

    # Valid requests
    test(b'', 400, 'No saved SSID') # will change

    test(b'A\0', 200, 'Connecting to A')
    test(b'B\0\0', 200, 'Connecting to B')
    test(b'Network\0', 200, 'Connecting to Network')
    test(b'Network\0\0', 200, 'Connecting to Network')

    for ssid in [
        '\x80', 'Сеть', '🙂', '.' * MAX_SSID_LEN
    ]:
        for z in [ '\0', '\0\0', '\0' + '.' * MAX_PASSPHRASE_LEN + '\0' ]:
            test(ssid + z, 200, f'Connecting to {ssid}')
            test(ssid + z, 200, f'Connecting to {ssid}')
