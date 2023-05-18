#!/usr/bin/env python3

from urllib.parse import quote

with open('favicon-utf-snowflake.svg') as f:
    print(quote(f.read()))
