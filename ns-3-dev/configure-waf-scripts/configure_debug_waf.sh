#!/usr/bin/env bash
CXXFLAGS="-Wall -g3 -ggdb -O0 -std=c++17" ./waf configure --enable-examples --enable-tests 
