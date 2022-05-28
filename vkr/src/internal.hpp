#include <unordered_map>
#include <vector>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

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

		/* Command buffer */
		VkCommandPool command_pool;
		VkCommandBuffer command_buffers[max_frames_in_flight];

		/* Synchronisation. */
		VkSemaphore image_avail_semaphores[max_frames_in_flight];   /* Signalled when an image has been acquired from the swapchain. */
		VkSemaphore render_finish_semaphores[max_frames_in_flight]; /* Signalled when the picture has finished rendering. */
		VkFence in_flight_fences[max_frames_in_flight];             /* Waits for the last frame to finish. */

		VkDebugUtilsMessengerEXT messenger;
	};

	struct impl_Buffer {
		VkBuffer buffer;
		VmaAllocation memory;

		/* For dynamic vertex buffers. */
		VkBuffer buffers[max_frames_in_flight];
		VmaAllocation memories[max_frames_in_flight];
		void* datas[max_frames_in_flight];
	};

	struct impl_Texture {
		VkImage image;
		VkImageView view;
		VkSampler sampler;

		VmaAllocation memory;
	};

	struct impl_DescriptorSet {
		VkDescriptorSetLayout layout;
		VkDescriptorSet       sets[max_frames_in_flight];
	};

	struct impl_UniformBuffer {
		VkBuffer buffers[max_frames_in_flight];
		VmaAllocation memories[max_frames_in_flight];

		void* ptr;
		usize size;
	};

	struct impl_Pipeline {
		VkPipelineLayout pipeline_layout;
		VkPipeline pipeline;

		VkDescriptorPool descriptor_pool;

		impl_DescriptorSet* desc_sets;
		impl_UniformBuffer* uniforms;
	};

	struct impl_Attachment {
		Framebuffer::Attachment::Type type;

		VkImage images[max_frames_in_flight];
		VkImageView image_views[max_frames_in_flight];
		VmaAllocation image_memories[max_frames_in_flight];

		VkImageAspectFlags get_aspect_flags() {
			return type == Framebuffer::Attachment::Type::color ? VK_IMAGE_ASPECT_COLOR_BIT :
				VK_IMAGE_ASPECT_DEPTH_BIT;
		}
	};

	struct impl_Sampler {
		VkSampler sampler;
	};

	struct impl_Framebuffer {
		VkRenderPass render_pass;

		/* For drawing to the swapchain. */
		VkFramebuffer* swapchain_framebuffers;

		/* For offscreen rendering. */
		VkFramebuffer offscreen_framebuffers[max_frames_in_flight];

		/* Points to either swapchain_framebuffers if this is
		 * the default framebuffer, or offscreen_framebuffers
		 * if this is a headless framebuffer. */
		VkFramebuffer* framebuffers;

		impl_Attachment* colors;
		usize color_count;
		impl_Attachment depth;

		/* Used if this is the default framebuffer. On the default
		 * framebuffer, the depth buffer can't be sampled and a
		 * separate one won't be needed for each frame in flight. */
		VkImage depth_image;
		VkImageView depth_image_view;
		VmaAllocation depth_memory;

		bool is_headless;

		VkClearValue* clear_colors;
		usize clear_color_count;

		VkFramebuffer get_current_framebuffer(u32 image_id, u32 current_frame) const {
			if (is_headless) {
				return framebuffers[current_frame];
			}

			return swapchain_framebuffers[image_id];
		}

		std::unordered_map<u32, impl_Attachment*> attachment_map;
	};

	struct impl_Shader {
		VkShaderModule v_shader, f_shader;
	};
};
