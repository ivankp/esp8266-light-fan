#!/bin/bash

ls main \
| sed -n 's/\.dev\.html$//p' \
| while read x; do
  < main/$x.dev.html \
  sed 's/^\s\+//; /^$/d; /^<style>$/,/^<\/style>$/{s/\s\+//g}' \
  | tr -d '\n' \
  | sed 's/>/>\n/' \
  > main/$x.html
done