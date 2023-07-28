#!/bin/bash

usage() { echo "usage: $0 [-f]" 1>&2; exit 1; }

while getopts "f" o; do
    case "${o}" in
        f)
            f=1
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

ls main \
| sed -n '/^min_/d;s/\.html$//p' \
| while read x; do
  dev="main/$x.html"
  min="main/min_$x.html.gz"
  [ ! "$dev" -nt "$min" ] && [ ! "$0" -nt "$min" ] && [ -z "$f" ] && continue
  sed '''
s/^\s\+//;
/^$/d;
/^<style>$/,/^<\/style>$/{
  s/\s*\([,+<>]\)\s*\(.*{\)/\1\2/g;
  s/\s*\([:{}]\)\s*/\1/g
};
/^<script>$/,/^<\/script>$/{
  s/\s*\([][+-=<>{}()?:;,*&]\+\)\s*/\1/g
}
''' "$dev" \
  | tr -d '\n' \
  | sed 's/>/>\n/' \
  | gzip \
  > "$min"
  echo "$dev ($(stat -c%s "$dev")) -> $min ($(stat -c%s "$min"))"
done
