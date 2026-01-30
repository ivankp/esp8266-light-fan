#!/usr/bin/env bash

addr='http://192.168.4.1/'

test_get() {
  if cmp -s ../main/index.html.gz <(curl -sS "${addr}" | head -c 2584); then
    printf "\033[32mSuccess\033[0m $1\n"
  else
    printf "\033[31mFailed\033[0m $1\n"
    echo "$resp"
    exit 1
  fi
}

for i in {1..25}; do
  test_get "$i"
done
