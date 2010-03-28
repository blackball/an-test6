#!/bin/sh
sqlite3 matcher.sqlite < matcher.sql
python add_random.py
