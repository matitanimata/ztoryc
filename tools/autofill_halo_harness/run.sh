#!/bin/sh
# Extracts the real code, builds the harness and runs it.
set -e
cd "$(dirname "$0")"
python3 extract.py
c++ -std=c++17 -O1 -w -o harness harness.cpp
./harness
