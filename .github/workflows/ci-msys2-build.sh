#!/bin/bash
env
qmake ../RetroShare -spec win32-g++
mingw32-make -j3
