#!/usr/bin/env python3

import requests

addr = r'http://192.168.4.1/'

def req(url, expected):
    r = requests.get(addr + url)
    assert r.text == expected

def test_set_get():
    req('set?light=0', '{"light":0}')
    req('set?fan=0', '{"fan":0}')
    req('get', '{"light":0,"fan":0}')

    req('set?light=1', '{"light":1}')
    req('get', '{"light":1,"fan":0}')

    req('set?light=0', '{"light":0}')
    req('get', '{"light":0,"fan":0}')

    req('set?fan=1', '{"fan":1}')
    req('get', '{"light":0,"fan":1}')

    req('set?light=1', '{"light":1}')
    req('get', '{"light":1,"fan":1}')

    req('set?light=0&fan=0', '{"light":0,"fan":0}')
    req('get', '{"light":0,"fan":0}')

def test_set_get_other_values():
    req('set?light=0&fan=0', '{"light":0,"fan":0}')
    req('get', '{"light":0,"fan":0}')

    req('set?light=0', '{"light":0}')
    req('get', '{"light":0,"fan":0}')
    req('set?light=1', '{"light":1}')
    req('get', '{"light":1,"fan":0}')
    req('set?light=2', '{}')
    req('get', '{"light":1,"fan":0}')
    req('set?light=3', '{}')
    req('get', '{"light":1,"fan":0}')
    req('set?light=31', '{}')
    req('get', '{"light":1,"fan":0}')
    req('set?light=', '{}')
    req('get', '{"light":1,"fan":0}')
    req('set?light=a', '{}')
    req('get', '{"light":1,"fan":0}')

    req('set?light=0&fan=0', '{"light":0,"fan":0}')
    req('get', '{"light":0,"fan":0}')

    req('set?fan=0', '{"fan":0}')
    req('get', '{"light":0,"fan":0}')
    req('set?fan=1', '{"fan":1}')
    req('get', '{"light":0,"fan":1}')
    req('set?fan=2', '{}')
    req('get', '{"light":0,"fan":1}')
    req('set?fan=3', '{}')
    req('get', '{"light":0,"fan":1}')
    req('set?fan=31', '{}')
    req('get', '{"light":0,"fan":1}')
    req('set?fan=', '{}')
    req('get', '{"light":0,"fan":1}')
    req('set?fan=a', '{}')
    req('get', '{"light":0,"fan":1}')

    req('set?light=0&fan=0', '{"light":0,"fan":0}')
    req('get', '{"light":0,"fan":0}')

    req('set?led=1', '{"led":1}')
    req('set?led=0', '{"led":0}')
    req('set?led=2', '{}')
    req('set?led=3', '{}')
    req('set?led=31', '{}')
    req('set?led=', '{}')
    req('set?led=a', '{}')
    req('set?led=0', '{"led":0}')

    req('set?light=0&fan=0', '{"light":0,"fan":0}')
    req('get', '{"light":0,"fan":0}')
