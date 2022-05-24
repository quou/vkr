vk_sdk_path     = "C:/VulkanSDK/1.2.148.1"
vk_include_path = string.format("%s/Include", vk_sdk_path)
vk_lib_path     = string.format("%s/Lib",     vk_sdk_path)

workspace "vkr"
	configurations { "debug", "release" }

	startproject "sbox"

include "vkr"
include "sbox"
