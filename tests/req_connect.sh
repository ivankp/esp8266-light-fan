#!/usr/bin/env bash

addr='http://192.168.4.1/'

test_post() {
  local resp="$(curl -sS -d "$2" "${addr}$1")"
  if [ "$resp" == "$3" ]; then
    printf "\033[32mSuccess\033[0m $3\n"
  else
    printf "\033[31mFailed\033[0m $3\n"
    echo "$resp"
  fi
}

test_post 'connect' "$(head -c 200 /dev/zero | tr '\0' '.')" 'Invalid SSID or PASS'
test_post 'connect' "$(head -c 100 /dev/zero | tr '\0' '.')" 'Invalid SSID or PASS'
test_post 'connect' "$(head -c 99 /dev/zero | tr '\0' '.')" 'Invalid SSID or PASS'
test_post 'connect' "$(head -c 99 /dev/zero)" 'Invalid SSID or PASS'

test_post 'connect' "$(head -c 98 /dev/zero | tr '\0' '.')" 'Invalid SSID'
test_post 'connect' "Router" 'Invalid SSID'
test_post 'connect' "R" 'Invalid SSID'
test_post 'connect' "\0" 'Invalid SSID'

test_post 'connect' "Router\0a" 'Invalid PASS'
test_post 'connect' "Router\0aaaaaaaaaaa" 'Invalid PASS'

test_post 'connect' "$(python 'print("."*33,sep="",end="")')" 'Invalid SSID'
test_post 'connect' "$(python 'print("."*33,"\0",sep="",end="")')" 'Invalid SSID'
test_post 'connect' "$(python 'print("."*32,"\0","."*65,"\0",sep="",end="")')" \
  'Invalid PASS'

test_post 'connect' '' 'Would reconnect'

test_post 'connect' "R\0" 'Connecting to R'
test_post 'connect' "R\0\0" 'Connecting to R'
test_post 'connect' "Router\0" 'Connecting to Router'
test_post 'connect' "Router\0\0" 'Connecting to Router'
test_post 'connect' "Router\0\0" 'Connecting to Router'

test_post 'connect' "$(python 'print("."*32,"\0",sep="",end="")')" \
  "$(python 'print("Connecting to ","."*32,sep="",end="")')"
test_post 'connect' "$(python 'print("."*32,"\0\0",sep="",end="")')" \
  "$(python 'print("Connecting to ","."*32,sep="",end="")')"
test_post 'connect' "$(python 'print("."*32,"\0","."*64,"\0",sep="",end="")')" \
  "$(python 'print("Connecting to ","."*32,sep="",end="")')"
