#!/usr/bin/env python3

import sys, re
from urllib.parse import quote
import requests

re_ns   = re.compile(r'\n+\s*')
re_tag  = re.compile(r'<(link|script)\b[^>]*>')
re_attr = re.compile(r'\b(\w+)="([^"]+)"')
# re_script = [ (re.compile(r), s) for r, s in [
#     ( r'//.*\n', '' ),
#     ( r'\n+\s*', '' ),
#     ( r'\s*([][+-=<>{}()?:;,*&|]+)\s*', r'\1' )
# ]]
re_style = [ (re.compile(r), s) for r, s in [
    ( r'\s+([,+<>])\s+', r' \1 ' ),
    ( r'\s*([:{};])\s*', r'\1' )
]]

def read(filename):
    with open(filename) as f:
        return f.read().strip()

def read_terse(filename):
    return re_ns.sub('', read(filename))

html = re.sub(r'>', '>\n', read_terse('index.html'), count=1)

merged = ''
cursor = 0

for t in re_tag.finditer(html):
    tag = html[ t.start() : t.end() ]
    # print(t[1]+':', tag)
    attrs = { a[1]: a[2] for a in re_attr.finditer(tag) }

    merged += html[ cursor : t.start() ]
    if t[1] == 'script':
        script = read(attrs['src'])
        # for r, s in re_script:
        #     script = r.sub(s, script)

        merged += '<script>' + requests.post(
            'https://www.toptal.com/developers/javascript-minifier/api/raw',
            data = { 'input': script }
        ).text

    else:
        rel = attrs['rel']
        if rel == 'stylesheet':
            style = read_terse(attrs['href'])
            for r, s in re_style:
                style = r.sub(s, style)
            merged += f'<style>{style}</style>'

        elif rel == 'icon':
            icon = quote(read_terse(attrs['href']))
            merged += f'<link rel="icon" href="data:{attrs["type"]},{icon}">'

    cursor = t.end()

merged += html[ cursor : ]

with open('min_index.html', 'w') as f:
    f.write(merged)
