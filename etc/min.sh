#!/bin/bash

ls main \
| sed -n '/^min_/d;s/\.html$//p' \
| while read x; do
  dev="main/$x.html"
  min="main/min_$x.html"
  [ ! "$dev" -nt "$min" ] && continue
  sed '''
s/^\s\+//;
/^$/d;
/^<style>$/,/^<\/style>$/{ s/\s\+//g }
/^<script>$/,/^<\/script>$/{ s/\s*\([+-=<>{}()?:;,*]\+\)\s*/\1/g }
''' "$dev" \
  | tr -d '\n' \
  | sed 's/>/>\n/' \
  > "$min"
  echo "$dev ($(stat -c%s "$dev")) -> $min ($(stat -c%s "$min"))"
done
