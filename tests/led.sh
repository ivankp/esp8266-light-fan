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

test_get 'set?led=0' '{"led":0}'
for i in {1..10}; do
  sleep 1
  test_get 'set?led=1' '{"led":1}'
  sleep 1
  test_get 'set?led=0' '{"led":0}'
done
