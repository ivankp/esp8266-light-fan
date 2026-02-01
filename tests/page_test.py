#!/usr/bin/env python3

from test import Test, TestGroup
import sys, requests

addr = r'http://192.168.4.1/'

with open('../main/index.html.gz', 'rb') as f:
    expected = f.read()

@Test
def test_fetch_page():
    resp = requests.get(addr, stream=True)

    content_length = int(resp.headers.get('Content-Length'))
    assert content_length == len(expected)

    data = resp.raw.read(content_length)
    assert data == expected

@TestGroup
def fetch_page():
    try:
        n = int(sys.argv[1])
    except:
        n = 10

    for i in range(1, n+1):
        test_fetch_page()
