#!/usr/bin/env bash

addr='http://192.168.4.1/'

test_get() {
  local resp="$(curl -sS "${addr}$1")"
  if [ "$resp" == "$2" ]; then
    printf "\033[32mSuccess\033[0m $1\n"
  else
    printf "\033[31mFailed\033[0m $1\n"
    echo "$resp"
  fi
}

test_get 'set?light=0' '{"light":0}'
test_get 'set?fan=0' '{"fan":0}'
test_get 'get' '{"light":0,"fan":0}'

test_get 'set?light=1' '{"light":1}'
test_get 'get' '{"light":1,"fan":0}'

test_get 'set?light=0' '{"light":0}'
test_get 'get' '{"light":0,"fan":0}'

test_get 'set?fan=1' '{"fan":1}'
test_get 'get' '{"light":0,"fan":1}'

test_get 'set?light=1' '{"light":1}'
test_get 'get' '{"light":1,"fan":1}'

test_get 'set?light=0&fan=0' '{"light":0,"fan":0}'
test_get 'get' '{"light":0,"fan":0}'
