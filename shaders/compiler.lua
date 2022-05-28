#!/usr/bin/lua

-- This Lua script does some pre-processing on the shaders
-- and invokes a call to glslc for each shader.
--
-- Usage:
-- 	compiler.lua file intdir outdir
--
-- intdir is where the intermediate .vert and .frag files are
-- created.
--
-- outdir is where the final .spv files are created.

local line_number = 1
local vertex = ""
local fragment = ""
for line in io.lines(arg[1]) do
	if version_string == nil and #line > 0 and string.sub(line, 1, 8) == "#version" then
		version_string = line
	elseif version_string == nil then
		print(string.format("%s:1: error: First line must be #version.", arg[1]))
		break
	end

	if line == "#begin VERTEX" then
		if #vertex == 0 then
			vertex = version_string .. "\n"
		end

		vertex = vertex .. "#line " .. tostring(line_number + 1) .. " \"" .. arg[1] .. "\"\n"

		adding_to = "vertex"
		goto continue
	end

	if line == "#begin FRAGMENT" then
		if #fragment == 0 then
			fragment = version_string .. "\n"
		end

		fragment = fragment .. "#line " .. tostring(line_number + 1) .. " \"" .. arg[1] .. "\"\n"

		adding_to = "fragment"
		goto continue
	end

	if string.sub(line, 1, 4) == "#end" then
		adding_to = nil
		goto continue
	end

	if adding_to == "vertex" then
		vertex = vertex .. line .. "\n"
	elseif adding_to == "fragment" then
		fragment = fragment .. line .. "\n"
	end

::continue::
	line_number = line_number + 1
end

local intdir = arg[2]
local outdir = arg[3]

if string.sub(intdir, #intdir - 1, #intdir) == "/" then
	intdir = intdir .. "/"
end

if string.sub(outdir, #outdir - 1, #outdir) == "/" then
	outdir = outdir .. "/"
end

local intpath_v = intdir .. string.gsub(arg[1], "/", "_") .. ".cint.vert"
local intpath_f = intdir .. string.gsub(arg[1], "/", "_") .. ".cint.frag"

local out_i_v_f = io.open(intpath_v, "w")
io.output(out_i_v_f)
io.write(vertex)
io.close(out_i_v_f)

local out_i_f_f = io.open(intpath_f, "w")
io.output(out_i_f_f)
io.write(fragment)
io.close(out_i_f_f)

local out_name_v = string.gsub(string.match(arg[1], ".*/(.*)"), ".glsl", ".vert")
local out_name_f = string.gsub(string.match(arg[1], ".*/(.*)"), ".glsl", ".frag")

io.popen("glslc " .. intpath_v .. " -o " .. outdir .. out_name_v .. ".spv")
io.popen("glslc " .. intpath_f .. " -o " .. outdir .. out_name_f .. ".spv")
