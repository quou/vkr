#include <vector>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "vkr.hpp"

#define max_frames_in_flight 3

namespace vkr {
	struct impl_VideoContext {
		VkInstance instance;
		VkPhysicalDevice pdevice;
		VkDevice device;

		VmaAllocator allocator;

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

		/* Depth buffer. */
		VkImage depth_image;
		VmaAllocation depth_memory;
		VkImageView depth_image_view;

		/* Command buffer */
		VkCommandPool command_pool;
		VkCommandBuffer command_buffers[max_frames_in_flight];

		/* Synchronisation. */
		VkSemaphore image_avail_semaphores[max_frames_in_flight];   /* Signalled when an image has been acquired from the swapchain. */
		VkSemaphore render_finish_semaphores[max_frames_in_flight]; /* Signalled when the picture has finished rendering. */
		VkFence in_flight_fences[max_frames_in_flight];             /* Waits for the last frame to finish. */
	};

	struct impl_Buffer {
		VkBuffer buffer;

		VmaAllocation memory;
	};

	struct impl_UniformBuffer {
		VkBuffer uniform_buffers[max_frames_in_flight];
		VmaAllocation uniform_buffer_memories[max_frames_in_flight];
		void* ptr;
		usize size;
	};

	struct impl_Pipeline {
		VkRenderPass render_pass;
		VkPipelineLayout pipeline_layout;
		VkPipeline pipeline;

		VkDescriptorPool descriptor_pool;

		VkDescriptorSetLayout descriptor_set_layout;
		impl_UniformBuffer* uniforms;
		VkDescriptorSet descriptor_sets[max_frames_in_flight];
	};

	struct impl_Shader {
		VkShaderModule v_shader, f_shader;
	};
};
