#include <vulkan/vulkan.h>

#include "vkr.hpp"

namespace vkr {
	struct impl_VideoContext {
		VkInstance instance;
	};

	VideoContext::VideoContext(const char* app_name, bool enable_validation_layers, u32 extension_count, const char** extensions) {
		handle = new impl_VideoContext();

		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = app_name;
		app_info.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;
		create_info.enabledExtensionCount = extension_count;
		create_info.ppEnabledExtensionNames = extensions;

		VkResult r;
		if ((r = vkCreateInstance(&create_info, null, &handle->instance)) != VK_SUCCESS) {
			error("vkCreateInstance failed with code %d.", r);
			info("vkCreateInstance commonly fails because your hardware doesn't support Vulkan. Check that your driver is up to date.");
			abort_with("Failed to create Vulkan instance.");
		}

		info("Vulkan instance created.");
	}

	VideoContext::~VideoContext() {
		vkDestroyInstance(handle->instance, null);
		delete handle;
	}
};
