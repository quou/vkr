#include <vulkan/vulkan.h>

#include "vkr.hpp"

namespace vkr {
	struct impl_VideoContext {
		VkInstance instance;
		VkPhysicalDevice pdevice;
		VkDevice device;

		VkQueue graphics_queue;
		VkQueue present_queue;

		VkSurfaceKHR surface;
	};
};
