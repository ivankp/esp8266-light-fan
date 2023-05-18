#!/bin/bash

ls \
| sed -n 's/\.dev\.html$//p' \
| while read x; do
  < $x.dev.html \
  sed 's/^\s\+//; /^$/d; /^<style>$/,/^<\/style>$/{s/\s\+//g}' \
  | tr -d '\n' \
  | sed 's/>/>\n/' \
  > $x.html
done
