vk_sdk_path     = "$(VULKAN_SDK)"
vk_include_path = string.format("%s/Include", vk_sdk_path)
vk_lib_path     = string.format("%s/Lib",     vk_sdk_path)

workspace "vkr"
	configurations { "debug", "release" }

	startproject "sbox"

include "packer"

include "vkr"
include "sbox"
