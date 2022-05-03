#include <vector>

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

		VkSwapchainKHR swapchain;
		VkImage* swapchain_images;
		u32 swapchain_image_count;

		VkFormat swapchain_format;
		VkExtent2D swapchain_extent;
	};
};
