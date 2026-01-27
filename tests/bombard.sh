#!/usr/bin/env bash

addr='http://192.168.4.1/'

test_get() {
  local resp="$(curl -sS "${addr}" | wc -c)"
  if [ "$resp" == "$2" ]; then
    printf "\033[32mSuccess\033[0m $1\n"
  else
    printf "\033[31mFailed\033[0m $1\n"
    echo "$resp"
    exit 1
  fi
}

for i in {1..100}; do
  test_get "$i" '2585'
done
