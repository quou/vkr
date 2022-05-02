#include <string.h>

#include <vulkan/vulkan.h>

#include "vkr.hpp"

namespace vkr {
	static const char* validation_layers[] = {
		"VK_LAYER_KHRONOS_validation"
	};

	struct impl_VideoContext {
		VkInstance instance;
	};

	bool VideoContext::validation_layers_supported() {
		u32 avail_count;
		vkEnumerateInstanceLayerProperties(&avail_count, null);

		VkLayerProperties* avail_layers = new VkLayerProperties[avail_count];

		vkEnumerateInstanceLayerProperties(&avail_count, avail_layers);

		for (u32 i = 0; i < sizeof(validation_layers) / sizeof(*validation_layers); i++) {
			bool found = false;

			const char* layer = validation_layers[i];

			for (u32 ii = 0; ii < avail_count; ii++) {
				if (strcmp(layer, avail_layers[ii].layerName) == 0) {
					found = true;
					break;
				}
			}

			if (!found) {
				delete[] avail_layers;
				return false;
			}
		}

		delete[] avail_layers;

		return true;
	}

	VideoContext::VideoContext(const char* app_name, bool enable_validation_layers, u32 extension_count, const char** extensions) {
		handle = new impl_VideoContext();

		if (enable_validation_layers && !validation_layers_supported()) {
			abort_with("Request for unsupported validation layers.");
		}

		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = app_name;
		app_info.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;
		create_info.enabledExtensionCount = extension_count;
		create_info.ppEnabledExtensionNames = extensions;

		if (enable_validation_layers) {
			create_info.enabledLayerCount = sizeof(validation_layers) / sizeof(*validation_layers);
			create_info.ppEnabledLayerNames = validation_layers;
		}

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
