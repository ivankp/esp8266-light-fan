#!/bin/sh

< connect.dev.html sed 's/^\s\+//; /^$/d; /^<style>$/,/^<\/style>$/{s/\s\+//g}' | tr -d '\n' | sed 's/>/>\n/' > connect.html
