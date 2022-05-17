#!/bin/bash

mkdir -p shaders/obj
mkdir -p res/shaders

./shaders/compiler.lua shaders/src/lit.glsl shaders/obj/ res/shaders/
./shaders/compiler.lua shaders/src/shadowmap.glsl shaders/obj/ res/shaders/
./shaders/compiler.lua shaders/src/tonemap.glsl shaders/obj/ res/shaders/
