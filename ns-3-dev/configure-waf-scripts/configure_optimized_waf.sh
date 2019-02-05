#!/usr/bin/env bash
CXXFLAGS="-Wall -O3 -g3 -ggdb -std=c++11" ./waf configure --enable-examples --enable-tests
