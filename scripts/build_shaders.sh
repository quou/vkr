#!/bin/bash

mkdir -p shaders/obj
mkdir -p res/shaders

./shaders/compiler.lua shaders/src/lit.glsl shaders/obj/ res/shaders/
