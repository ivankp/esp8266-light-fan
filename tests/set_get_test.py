#!/usr/bin/env python3

from test import Test, TestGroup
import requests

addr = r'http://192.168.4.1/'

@Test
def test(url, expected):
    resp = requests.get(addr + url)
    assert resp.status_code == 200
    assert resp.text == expected

@TestGroup
def test_set_get():
    test('set?light=0', '{"light":0}')
    test('set?fan=0', '{"fan":0}')
    test('get', '{"light":0,"fan":0}')

    test('set?light=1', '{"light":1}')
    test('get', '{"light":1,"fan":0}')

    test('set?light=0', '{"light":0}')
    test('get', '{"light":0,"fan":0}')

    test('set?fan=1', '{"fan":1}')
    test('get', '{"light":0,"fan":1}')

    test('set?light=1', '{"light":1}')
    test('get', '{"light":1,"fan":1}')

    test('set?light=0&fan=0', '{"light":0,"fan":0}')
    test('get', '{"light":0,"fan":0}')

@TestGroup
def test_set_get_bad():
    test('set?light=0&fan=0', '{"light":0,"fan":0}')
    test('get', '{"light":0,"fan":0}')

    test('set?light=0', '{"light":0}')
    test('get', '{"light":0,"fan":0}')
    test('set?light=1', '{"light":1}')
    test('get', '{"light":1,"fan":0}')
    test('set?light=2', '{}')
    test('get', '{"light":1,"fan":0}')
    test('set?light=3', '{}')
    test('get', '{"light":1,"fan":0}')
    test('set?light=31', '{}')
    test('get', '{"light":1,"fan":0}')
    test('set?light=', '{}')
    test('get', '{"light":1,"fan":0}')
    test('set?light=a', '{}')
    test('get', '{"light":1,"fan":0}')

    test('set?light=0&fan=0', '{"light":0,"fan":0}')
    test('get', '{"light":0,"fan":0}')

    test('set?fan=0', '{"fan":0}')
    test('get', '{"light":0,"fan":0}')
    test('set?fan=1', '{"fan":1}')
    test('get', '{"light":0,"fan":1}')
    test('set?fan=2', '{}')
    test('get', '{"light":0,"fan":1}')
    test('set?fan=3', '{}')
    test('get', '{"light":0,"fan":1}')
    test('set?fan=31', '{}')
    test('get', '{"light":0,"fan":1}')
    test('set?fan=', '{}')
    test('get', '{"light":0,"fan":1}')
    test('set?fan=a', '{}')
    test('get', '{"light":0,"fan":1}')

    test('set?light=0&fan=0', '{"light":0,"fan":0}')
    test('get', '{"light":0,"fan":0}')

    test('set?led=1', '{"led":1}')
    test('set?led=0', '{"led":0}')
    test('set?led=2', '{}')
    test('set?led=3', '{}')
    test('set?led=31', '{}')
    test('set?led=', '{}')
    test('set?led=a', '{}')
    test('set?led=0', '{"led":0}')

    test('set?light=0&fan=0', '{"light":0,"fan":0}')
    test('get', '{"light":0,"fan":0}')
