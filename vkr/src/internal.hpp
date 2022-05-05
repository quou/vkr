#include <vector>

#include <vulkan/vulkan.h>

#include "vkr.hpp"

#define max_frames_in_flight 2

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

		/* Framebuffers */
		VkFramebuffer* swapchain_framebuffers;

		/* Pipeline */
		VkRenderPass render_pass;
		VkPipelineLayout pipeline_layout;
		VkPipeline pipeline;

		/* Command buffer */
		VkCommandPool command_pool;
		VkCommandBuffer command_buffers[max_frames_in_flight];

		/* Synchronisation. */
		VkSemaphore image_avail_semaphores[max_frames_in_flight];   /* Signalled when an image has been acquired from the swapchain. */
		VkSemaphore render_finish_semaphores[max_frames_in_flight]; /* Signalled when the picture has finished rendering. */
		VkFence in_flight_fences[max_frames_in_flight];             /* Waits for the last frame to finish. */

		VkBuffer vb;
		VkDeviceMemory vb_memory;
	};

	struct impl_Pipeline {

	};
};
