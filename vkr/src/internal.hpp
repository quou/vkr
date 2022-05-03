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

		/* Swapchain */
		VkSwapchainKHR swapchain;
		
		u32 swapchain_image_count;
		VkImage* swapchain_images;
		VkImageView* swapchain_image_views;
		
		VkFormat swapchain_format;
		VkExtent2D swapchain_extent;
	};
};
