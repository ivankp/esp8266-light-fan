#!/usr/bin/env python3

import requests

addr = r'http://192.168.4.1/'

with open('../main/index.html.gz', 'rb') as f:
    expected = f.read()

def impl_page():
    resp = requests.get(addr, stream=True)

    content_length = int(resp.headers.get('Content-Length'))
    assert content_length == len(expected)

    data = resp.raw.read(content_length)
    assert data == expected

if __name__ == "__main__":
    import sys

    try:
        n = int(sys.argv[1])
    except:
        n = 100
    for i in range(1, n+1):
        print(i)
        impl_page()

else:
    import pytest

    @pytest.mark.parametrize('repeat', range(10))
    def test_page(repeat):
        impl_page()
