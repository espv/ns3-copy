#!/usr/bin/env bash
CXXFLAGS="-Wall -O3 -g3 -ggdb -std=c++17" ./waf configure --enable-examples --enable-tests
