#include <algorithm>
#include <optional>
#include <set>
#include <vector>

#include <string.h>

#include <vulkan/vulkan.h>

#include "vkr.hpp"
#include "internal.hpp"

/* The vulkan spec only requires 128 byte
 * push constants, so that's the maximum that
 * this renderer will use. */
#define max_push_const_size 128

namespace vkr {
	static const char* validation_layers[] = {
		"VK_LAYER_KHRONOS_validation"
	};

	static const char* device_extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	/* Lists the different types of queues that a device has to offer. */
	struct QueueFamilies {
		std::optional<u32> graphics;
		std::optional<u32> present;
	};

	struct SwapChainCapabilities {
		VkSurfaceCapabilitiesKHR capabilities;
		u32 format_count;       VkSurfaceFormatKHR* formats;
		u32 present_mode_count; VkPresentModeKHR* present_modes;

		void free() {
			delete[] formats;
			delete[] present_modes;
		};
	};

	static SwapChainCapabilities get_swap_chain_capabilities(impl_VideoContext* handle, VkPhysicalDevice device) {
		SwapChainCapabilities r{};

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, handle->surface, &r.capabilities);

		vkGetPhysicalDeviceSurfaceFormatsKHR(device, handle->surface, &r.format_count, null);
		if (r.format_count > 0) {
			r.formats = new VkSurfaceFormatKHR[r.format_count];
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, handle->surface, &r.format_count, r.formats);
		}

		vkGetPhysicalDeviceSurfacePresentModesKHR(device, handle->surface, &r.present_mode_count, null);
		if (r.present_mode_count > 0) {
			r.present_modes = new VkPresentModeKHR[r.present_mode_count];
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, handle->surface,
					&r.present_mode_count, r.present_modes);
		}

		return r;
	}

	/* Chooses the first format that uses SRGB and non-linear colourspace. On failure, it just chooses the
	 * first available format. */
	static VkSurfaceFormatKHR choose_swap_surface_format(u32 avail_format_count, VkSurfaceFormatKHR* avail_formats) {
		for (u32 i = 0; i < avail_format_count; i++) {
			if (
					avail_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
					avail_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return avail_formats[i];
			}
		}

		warning("Failed to find a surface that supports an SRGB non-linear colorspace.");

		return avail_formats[0];
	}

	/* If available, VK_PRESENT_MODE_MAILBOX_KHR is used. Otherwise, it will default to VK_PRESENT_MODE_FIFO_KHR.
	 *
	 * This is basically just what kind of VSync to use. */
	static VkPresentModeKHR choose_swap_present_mode(u32 avail_present_mode_count, VkPresentModeKHR* avail_present_modes) {
		for (u32 i = 0; i < avail_present_mode_count; i++) {
			if (avail_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
				return avail_present_modes[i];
			}
		}

		warning("VK_PRESENT_MODE_MAILBOX_KHR is not supported.");

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	static VkExtent2D choose_swap_extent(const App& app, const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != UINT32_MAX && capabilities.currentExtent.height != UINT32_MAX) {
			return capabilities.currentExtent;
		}

		v2i size = app.get_size();

		VkExtent2D extent = {
			(u32)(size.x), (u32)size.y
		};

		extent.width  = std::clamp(extent.width,  capabilities.minImageExtent.width,  capabilities.maxImageExtent.width);
		extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return extent;
	}

	static QueueFamilies get_queue_families(VkPhysicalDevice device, impl_VideoContext* handle) {
		QueueFamilies r;

		u32 family_count = 0;

		vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, null);

		auto families = new VkQueueFamilyProperties[family_count];

		vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, families);

		for (u32 i = 0; i < family_count; i++) {
			if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				r.graphics = i;
			}

			VkBool32 supports_presentation = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, handle->surface, &supports_presentation);
			if (supports_presentation) {
				r.present = i;
			}
		}

		delete[] families;

		return r;
	}

	static bool device_supports_extensions(VkPhysicalDevice device) {
		u32 avail_ext_count;
		vkEnumerateDeviceExtensionProperties(device, null, &avail_ext_count, null);

		auto avail_exts = new VkExtensionProperties[avail_ext_count];

		vkEnumerateDeviceExtensionProperties(device, null, &avail_ext_count, avail_exts);

		for (u32 i = 0; i < sizeof(device_extensions) / sizeof(*device_extensions); i++) {
			bool found = false;

			for (u32 ii = 0; ii < avail_ext_count; ii++) {
				if (strcmp(device_extensions[i], avail_exts[ii].extensionName) == 0) {
					found = true;
					break;
				}
			}

			if (!found) {
				delete[] avail_exts;
				return false;
			}
		}

		delete[] avail_exts;

		return true;
	}

	static VkPhysicalDevice first_suitable_device(VkPhysicalDevice* devices, u32 device_count, impl_VideoContext* handle) {
		for (u32 i = 0; i < device_count; i++) {
			auto device = devices[i];

			VkPhysicalDeviceProperties props;
			VkPhysicalDeviceFeatures features;
			vkGetPhysicalDeviceProperties(device, &props);
			vkGetPhysicalDeviceFeatures(device, &features);

			auto qfs = get_queue_families(device, handle);

			bool swap_chain_good = false;
			bool extensions_good = device_supports_extensions(device);
			if (extensions_good) {
				SwapChainCapabilities scc = get_swap_chain_capabilities(handle, device);
				swap_chain_good = scc.format_count > 0 && scc.present_mode_count > 0;
				scc.free();
			}

			/* For a graphics device to be suitable, it must be a GPU and
			 * have a queue capable of executing graphical commands. */
			if (
					(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
					props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) &&
					extensions_good && swap_chain_good &&
					qfs.graphics.has_value() && qfs.present.has_value()) {
				info("Selected physical device: %s.", props.deviceName);
				return device;
			}
		}

		return VK_NULL_HANDLE;
	}

	bool VideoContext::validation_layers_supported() {
		u32 avail_count;
		vkEnumerateInstanceLayerProperties(&avail_count, null);

		auto avail_layers = new VkLayerProperties[avail_count];

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

	static VkShaderModule new_shader_module(VkDevice device, const u8* code, usize code_size) {
		VkShaderModuleCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		info.codeSize = code_size;
		info.pCode = (u32*)code;

		VkShaderModule m;
		if (vkCreateShaderModule(device, &info, null, &m) != VK_SUCCESS) {
			abort_with("Failed to create shader module.");
		}

		return m;
	}

	/* Converts an array of Pipeline::Attributes into an array of
	 * VkVertexInputInputAttributeDescriptions and a VkVertexInputBindingDescription */
	static void render_pass_attributes_to_vk_attributes(
		Pipeline::Attribute* attribs,
		usize attrib_count,
		usize stride,
		VkVertexInputBindingDescription* vk_desc,
		VkVertexInputAttributeDescription* vk_attribs) {

		memset(vk_desc, 0, sizeof(VkVertexInputBindingDescription));

		vk_desc->binding = 0;
		vk_desc->stride = stride;
		vk_desc->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		for (usize i = 0; i < attrib_count; i++) {
			Pipeline::Attribute* attrib = attribs + i;
			VkVertexInputAttributeDescription* vk_attrib = vk_attribs + i;

			memset(vk_attrib, 0, sizeof(VkVertexInputAttributeDescription));

			vk_attrib->binding = 0;
			vk_attrib->location = attrib->location;
			vk_attrib->offset = attrib->offset;

			switch (attrib->type) {
			case Pipeline::Attribute::Type::float1:
				vk_attrib->format = VK_FORMAT_R32_SFLOAT;
				break;
			case Pipeline::Attribute::Type::float2:
				vk_attrib->format = VK_FORMAT_R32G32_SFLOAT;
				break;
			case Pipeline::Attribute::Type::float3:
				vk_attrib->format = VK_FORMAT_R32G32B32_SFLOAT;
				break;
			case Pipeline::Attribute::Type::float4:
				vk_attrib->format = VK_FORMAT_R32G32B32A32_SFLOAT;
				break;
			default: break;
			}
		}
	}
	
	/* Copies the VRAM from one buffer to another, similar to how memcpy works on the CPU.
	 *
	 * Waits for the copy to complete before returning. */
	static void copy_buffer(impl_VideoContext* handle, VkBuffer dst, VkBuffer src, VkDeviceSize size) {
		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandPool = handle->command_pool;
		alloc_info.commandBufferCount = 1;

		VkCommandBuffer command_buffer;
		vkAllocateCommandBuffers(handle->device, &alloc_info, &command_buffer);

		VkCommandBufferBeginInfo begin_info{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(command_buffer, &begin_info);
		
		VkBufferCopy copy{};
		copy.size = size;
		vkCmdCopyBuffer(command_buffer, src, dst, 1, &copy);

		vkEndCommandBuffer(command_buffer);

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffer;

		vkQueueSubmit(handle->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
		vkQueueWaitIdle(handle->graphics_queue);

		vkFreeCommandBuffers(handle->device, handle->command_pool, 1, &command_buffer);
	}

	static u32 find_memory_type(impl_VideoContext* handle, u32 filter, VkMemoryPropertyFlags flags) {
		VkPhysicalDeviceMemoryProperties mem_props;
		vkGetPhysicalDeviceMemoryProperties(handle->pdevice, &mem_props);

		for (u32 i = 0; i < mem_props.memoryTypeCount; i++) {
			if ((filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & flags) == flags) {
				return i;
			}
		}

		abort_with("Failed to find a suitable type of memory.");

		return 0;
	}

	static void new_buffer(impl_VideoContext* handle, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
		VkBuffer* buffer, VkDeviceMemory* buffer_memory) {

		VkBufferCreateInfo buffer_info{};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = size;
		buffer_info.usage = usage;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(handle->device, &buffer_info, null, buffer) != VK_SUCCESS) {
			abort_with("Failed to create buffer.");
		}

		VkMemoryRequirements mem_req;
		vkGetBufferMemoryRequirements(handle->device, *buffer, &mem_req);

		VkMemoryAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = find_memory_type(handle, mem_req.memoryTypeBits, props);

		if (vkAllocateMemory(handle->device, &alloc_info, null, buffer_memory) != VK_SUCCESS) {
			abort_with("Failed to allocate memory for a buffer.");
		}

		vkBindBufferMemory(handle->device, *buffer, *buffer_memory, 0);
	}

	VideoContext::VideoContext(const App& app, const char* app_name, bool enable_validation_layers, u32 extension_count, const char** extensions) : current_frame(0) {
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
			abort_with("Failed to create Vulkan instance.");
		}

		info("Vulkan instance created.");

		/* Create the window surface */
		if (!app.create_window_surface(*this)) {
			abort_with("Failed to create a window surface.");
		}

		/* Create the device. */
		u32 device_count = 0;
		vkEnumeratePhysicalDevices(handle->instance, &device_count, null);

		if (device_count == 0) {
			abort_with("No Vulkan-capable graphics hardware is installed in this machine.\n");
		}

		auto devices = new VkPhysicalDevice[device_count];

		vkEnumeratePhysicalDevices(handle->instance, &device_count, devices);

		handle->pdevice = first_suitable_device(devices, device_count, handle);
		if (handle->pdevice == VK_NULL_HANDLE) {
			error("first_suitable_device() failed.");
			info("Vulkan-capable hardware exists, but it does not support the required features.");
			abort_with("Failed to find a suitable graphics device.");
		}

		auto qfs = get_queue_families(handle->pdevice, handle);

		std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
		std::set<u32> unique_queue_families = { qfs.graphics.value(), qfs.present.value() };

		f32 queue_priority = 1.0f;
		for (u32 f : unique_queue_families) {
			VkDeviceQueueCreateInfo queue_create_info{};
			queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_create_info.queueFamilyIndex = f;
			queue_create_info.queueCount = 1;
			queue_create_info.pQueuePriorities = &queue_priority;
			queue_create_infos.push_back(queue_create_info);
		}

		VkPhysicalDeviceFeatures device_features{};

		VkDeviceCreateInfo device_create_info{};
		device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_create_info.pQueueCreateInfos = &queue_create_infos[0];
		device_create_info.queueCreateInfoCount = (u32)queue_create_infos.size();
		device_create_info.pEnabledFeatures = &device_features;
		device_create_info.enabledExtensionCount = sizeof(device_extensions) / sizeof(*device_extensions);
		device_create_info.ppEnabledExtensionNames = device_extensions;

		if (vkCreateDevice(handle->pdevice, &device_create_info, null, &handle->device) != VK_SUCCESS) {
			abort_with("Failed to create a Vulkan device.");
		}

		vkGetDeviceQueue(handle->device, qfs.graphics.value(), 0, &handle->graphics_queue);
		vkGetDeviceQueue(handle->device, qfs.present.value(), 0, &handle->present_queue);

		delete[] devices;

		/* Create the swap chain. */
		SwapChainCapabilities scc = get_swap_chain_capabilities(handle, handle->pdevice);
		VkSurfaceFormatKHR surface_format = choose_swap_surface_format(scc.format_count, scc.formats);
		VkPresentModeKHR present_mode = choose_swap_present_mode(scc.present_mode_count, scc.present_modes);
		VkExtent2D extent = choose_swap_extent(app, scc.capabilities);

		handle->swapchain_format = surface_format.format;
		handle->swapchain_extent = extent;

		/* Acquire one more image than the minimum if possible so that
		 * we don't end up waiting for the driver to give us another image. */
		u32 image_count = scc.capabilities.minImageCount;
		if (image_count < scc.capabilities.maxImageCount) {
			image_count++;
		}

		VkSwapchainCreateInfoKHR swap_create_info{};
		swap_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swap_create_info.surface = handle->surface;
		swap_create_info.minImageCount = image_count;
		swap_create_info.imageFormat = surface_format.format;
		swap_create_info.imageColorSpace = surface_format.colorSpace;
		swap_create_info.imageExtent = extent;
		swap_create_info.imageArrayLayers = 1;
		swap_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		if (qfs.graphics != qfs.present) {
			u32 queue_family_indices[] = { qfs.graphics.value(), qfs.present.value() };

			swap_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swap_create_info.queueFamilyIndexCount = 2;
			swap_create_info.pQueueFamilyIndices = queue_family_indices;
		} else {
			swap_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			swap_create_info.queueFamilyIndexCount = 0;
			swap_create_info.pQueueFamilyIndices = null;
		}

		swap_create_info.preTransform = scc.capabilities.currentTransform;

		scc.free();

		swap_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		swap_create_info.presentMode = present_mode;
		swap_create_info.clipped = VK_TRUE;

		swap_create_info.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(handle->device, &swap_create_info, null, &handle->swapchain) != VK_SUCCESS) {
			abort_with("Failed to create swapchain.");
		}

		/* Acquire handles to the swapchain images. */
		vkGetSwapchainImagesKHR(handle->device, handle->swapchain, &handle->swapchain_image_count, null);
		handle->swapchain_images = new VkImage[handle->swapchain_image_count];
		vkGetSwapchainImagesKHR(handle->device, handle->swapchain, &handle->swapchain_image_count, handle->swapchain_images);

		handle->swapchain_image_views = new VkImageView[handle->swapchain_image_count];

		/* Create image views. */
		for (u32 i = 0; i < handle->swapchain_image_count; i++) {
			VkImageViewCreateInfo iv_create_info{};
			iv_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			iv_create_info.image = handle->swapchain_images[i];

			iv_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			iv_create_info.format = handle->swapchain_format;
			iv_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			iv_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			iv_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			iv_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			iv_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			iv_create_info.subresourceRange.baseMipLevel = 0;
			iv_create_info.subresourceRange.levelCount = 1;
			iv_create_info.subresourceRange.baseArrayLayer = 0;
			iv_create_info.subresourceRange.layerCount = 1;

			if (vkCreateImageView(handle->device, &iv_create_info, null, &handle->swapchain_image_views[i]) != VK_SUCCESS) {
				abort_with("Failed to create image view.");
			}
		}

		/* Create the command pool. */
		VkCommandPoolCreateInfo pool_info{};
		pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		pool_info.queueFamilyIndex = qfs.graphics.value();

		if (vkCreateCommandPool(handle->device, &pool_info, null, &handle->command_pool) != VK_SUCCESS) {
			abort_with("Failed to create command pool.");
		}

		/* Create the command buffers. */
		VkCommandBufferAllocateInfo cb_alloc_info{};
		cb_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cb_alloc_info.commandPool = handle->command_pool;
		cb_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cb_alloc_info.commandBufferCount = max_frames_in_flight;

		if (vkAllocateCommandBuffers(handle->device, &cb_alloc_info, handle->command_buffers) != VK_SUCCESS) {
			abort_with("Failed to allocate command buffers.");
		}

		/* Create the synchronisation objects. */
		VkSemaphoreCreateInfo semaphore_info{};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fence_info{};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (u32 i = 0; i < max_frames_in_flight; i++) {
			if (vkCreateSemaphore(handle->device, &semaphore_info, null, &handle->image_avail_semaphores[i])   != VK_SUCCESS ||
				vkCreateSemaphore(handle->device, &semaphore_info, null, &handle->render_finish_semaphores[i]) != VK_SUCCESS ||
				vkCreateFence(handle->device, &fence_info, null, &handle->in_flight_fences[i])) {
				abort_with("Failed to create synchronisation objects.");
			}
		}
	}

	VideoContext::~VideoContext() {
		for (u32 i = 0; i < max_frames_in_flight; i++) {
			vkDestroySemaphore(handle->device, handle->image_avail_semaphores[i], null);
			vkDestroySemaphore(handle->device, handle->render_finish_semaphores[i], null);
			vkDestroyFence(handle->device, handle->in_flight_fences[i], null);
		}

		vkDestroyCommandPool(handle->device, handle->command_pool, null);

		for (u32 i = 0; i < handle->swapchain_image_count; i++) {
			vkDestroyFramebuffer(handle->device, handle->swapchain_framebuffers[i], null);
		}

		for (u32 i = 0; i < handle->swapchain_image_count; i++) {
			vkDestroyImageView(handle->device, handle->swapchain_image_views[i], null);
		}

		vkDestroySwapchainKHR(handle->device, handle->swapchain, null);

		vkDestroyDevice(handle->device, null);
		vkDestroySurfaceKHR(handle->instance, handle->surface, null);
		vkDestroyInstance(handle->instance, null);

		delete[] handle->swapchain_images;
		delete[] handle->swapchain_image_views;
		delete[] handle->swapchain_framebuffers;

		delete handle;
	}

	void VideoContext::begin() {
		object_count = 0;

		vkWaitForFences(handle->device, 1, &handle->in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);
		vkResetFences(handle->device, 1, &handle->in_flight_fences[current_frame]);

		vkAcquireNextImageKHR(handle->device, handle->swapchain, UINT64_MAX,
			handle->image_avail_semaphores[current_frame], VK_NULL_HANDLE, &image_id);

		vkResetCommandBuffer(handle->command_buffers[current_frame], 0);

		VkCommandBufferBeginInfo begin_info{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(handle->command_buffers[current_frame], &begin_info) != VK_SUCCESS) {
			warning("Failed to begin the command buffer.");
			return;
		}
	}

	void VideoContext::end() {
		if (vkEndCommandBuffer(handle->command_buffers[current_frame]) != VK_SUCCESS) {
			warning("Failed to end the command buffer");
			return;
		}

		VkSemaphore wait_semaphores[] = { handle->image_avail_semaphores[current_frame] };
		VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSemaphore signal_semaphores[] = { handle->render_finish_semaphores[current_frame] };

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_semaphores;
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &handle->command_buffers[current_frame];
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signal_semaphores;

		if (vkQueueSubmit(handle->graphics_queue, 1, &submit_info, handle->in_flight_fences[current_frame]) != VK_SUCCESS) {
			warning("Failed to submit draw command buffer.");
			return;
		}

		VkSwapchainKHR swapchains[] = { handle->swapchain };

		VkPresentInfoKHR present_info{};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = signal_semaphores;
		present_info.swapchainCount = 1;
		present_info.pSwapchains = swapchains;
		present_info.pImageIndices = &image_id;
		present_info.pResults = null;

		vkQueuePresentKHR(handle->present_queue, &present_info);

		current_frame = (current_frame + 1) % max_frames_in_flight;
	}

	void VideoContext::wait_for_done() const {
		vkDeviceWaitIdle(handle->device);
	}

	Pipeline::Pipeline(VideoContext* video, Shader* shader, usize stride,
			Attribute* attribs, usize attrib_count,
			UniformBuffer* uniforms, usize uniform_count,
			PushConstantRange* pcranges, usize pcrange_count) :
			video(video), uniform_count(uniform_count) {
		handle = new impl_Pipeline();

		VkAttachmentDescription color_attachment{};
		color_attachment.format = video->handle->swapchain_format;
		color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference color_attachment_ref{};
		color_attachment_ref.attachment = 0;
		color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_attachment_ref;

		VkSubpassDependency dep{};
		dep.srcSubpass = VK_SUBPASS_EXTERNAL;
		dep.dstSubpass = 0;
		dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.srcAccessMask = 0;
		dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo render_pass_info{};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = 1;
		render_pass_info.pAttachments = &color_attachment;
		render_pass_info.subpassCount = 1;
		render_pass_info.pSubpasses = &subpass;
		render_pass_info.dependencyCount = 1;
		render_pass_info.pDependencies = &dep;

		if (vkCreateRenderPass(video->handle->device, &render_pass_info, null, &handle->render_pass)) {
			abort_with("Failed to create render pass.");
		}

		VkPipelineShaderStageCreateInfo v_stage_info{};
		v_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		v_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
		v_stage_info.module = shader->handle->v_shader;
		v_stage_info.pName = "main";

		VkPipelineShaderStageCreateInfo f_stage_info{};
		f_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		f_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		f_stage_info.module = shader->handle->f_shader;
		f_stage_info.pName = "main";

		VkPipelineShaderStageCreateInfo stages[] = { v_stage_info, f_stage_info };

		VkVertexInputBindingDescription bind_desc;
		VkVertexInputAttributeDescription* vk_attribs = new VkVertexInputAttributeDescription[attrib_count];
		render_pass_attributes_to_vk_attributes(attribs, attrib_count, stride, &bind_desc, vk_attribs);

		VkPipelineVertexInputStateCreateInfo vertex_input_info{};
		vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_info.vertexBindingDescriptionCount = 1;
		vertex_input_info.pVertexBindingDescriptions = &bind_desc;
		vertex_input_info.vertexAttributeDescriptionCount = attrib_count;
		vertex_input_info.pVertexAttributeDescriptions = vk_attribs;

		VkPipelineInputAssemblyStateCreateInfo input_assembly{};
		input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		input_assembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width  = (f32)video->handle->swapchain_extent.width;
		viewport.height = (f32)video->handle->swapchain_extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = video->handle->swapchain_extent;

		VkPipelineViewportStateCreateInfo viewport_state{};
		viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_state.viewportCount = 1;
		viewport_state.pViewports = &viewport;
		viewport_state.scissorCount = 1;
		viewport_state.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasteriser{};
		rasteriser.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasteriser.depthClampEnable = VK_FALSE;
		rasteriser.rasterizerDiscardEnable = VK_FALSE;
		rasteriser.polygonMode = VK_POLYGON_MODE_FILL;
		rasteriser.lineWidth = 1.0f;
		rasteriser.cullMode = VK_CULL_MODE_BACK_BIT;
		rasteriser.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasteriser.depthBiasEnable = VK_FALSE;

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineColorBlendAttachmentState color_blend_attachment{};
		color_blend_attachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;
		color_blend_attachment.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo color_blending{};
		color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blending.logicOpEnable = VK_FALSE;
		color_blending.logicOp = VK_LOGIC_OP_COPY;
		color_blending.attachmentCount = 1;
		color_blending.pAttachments = &color_blend_attachment;

		/* Set up uniform buffers and descriptor sets. */
		handle->uniforms = new impl_UniformBuffer[uniform_count];
		auto layout_bindings = new VkDescriptorSetLayoutBinding[uniform_count];
		for (usize i = 0; i < uniform_count; i++) {
			VkDescriptorSetLayoutBinding layout_binding{};
			layout_bindings[i].binding = uniforms[i].binding;
			layout_bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			layout_bindings[i].descriptorCount = 1;
			layout_bindings[i].stageFlags = uniforms[i].stage == Stage::vertex ?
				VK_SHADER_STAGE_VERTEX_BIT :
				VK_SHADER_STAGE_FRAGMENT_BIT;
			layout_bindings[i].pImmutableSamplers = null;
		}

		VkDescriptorSetLayoutCreateInfo layout_info{};
		layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layout_info.bindingCount = uniform_count;
		layout_info.pBindings = layout_bindings;

		if (vkCreateDescriptorSetLayout(video->handle->device, &layout_info, null, &handle->descriptor_set_layout) != VK_SUCCESS) {
			abort_with("Failed to create descriptor set layout.");
		}

		/* Create descriptor pool. */
		VkDescriptorPoolSize pool_size{};
		pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool_size.descriptorCount = max_frames_in_flight * uniform_count;

		VkDescriptorPoolCreateInfo pool_info{};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.poolSizeCount = 1;
		pool_info.pPoolSizes = &pool_size;
		pool_info.maxSets = max_frames_in_flight * uniform_count;

		if (vkCreateDescriptorPool(video->handle->device, &pool_info, null, &handle->descriptor_pool) != VK_SUCCESS) {
			abort_with("Failed to create descriptor pool.");
		}

		/* Create descriptor set. */
		VkDescriptorSetLayout layouts[max_frames_in_flight];
		for (u32 ii = 0; ii < max_frames_in_flight; ii++) {
			layouts[ii] = handle->descriptor_set_layout;
		}

		VkDescriptorSetAllocateInfo desc_set_alloc_info{};
		desc_set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		desc_set_alloc_info.descriptorPool = handle->descriptor_pool;
		desc_set_alloc_info.descriptorSetCount = max_frames_in_flight;
		desc_set_alloc_info.pSetLayouts = layouts;

		if (vkAllocateDescriptorSets(video->handle->device, &desc_set_alloc_info, handle->descriptor_sets) != VK_SUCCESS) {
			abort_with("Failed to allocate descriptor sets.");
		}

		for (usize i = 0; i < uniform_count; i++) {
			handle->uniforms[i].ptr = uniforms[i].ptr;
			handle->uniforms[i].size = uniforms[i].size;

			VkDeviceSize buffer_size = handle->uniforms[i].size;

			/* Create uniform buffers. */
			for (u32 ii = 0; ii < max_frames_in_flight; ii++) {
				new_buffer(video->handle, buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					handle->uniforms[i].uniform_buffers + ii,
					handle->uniforms[i].uniform_buffer_memories + ii);
			}

			for (u32 ii = 0; ii < max_frames_in_flight; ii++) {
				VkDescriptorBufferInfo buffer_info{};
				buffer_info.buffer = handle->uniforms[i].uniform_buffers[ii];
				buffer_info.offset = 0;
				buffer_info.range = uniforms[i].size;

				VkWriteDescriptorSet desc_write{};
				desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				desc_write.dstSet = handle->descriptor_sets[ii];
				desc_write.dstBinding = uniforms[i].binding;
				desc_write.dstArrayElement = 0;
				desc_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				desc_write.descriptorCount = 1;
				desc_write.pBufferInfo = &buffer_info;

				vkUpdateDescriptorSets(video->handle->device, 1, &desc_write, 0, null);
			}
		}

		auto pc_ranges = new VkPushConstantRange[pcrange_count];
		for (usize i = 0; i < pcrange_count; i++) {
			memset(pc_ranges + i, 0, sizeof(*pc_ranges));
			pc_ranges[i].stageFlags = pcranges[i].stage ==Stage::vertex ?
				VK_SHADER_STAGE_VERTEX_BIT :
				VK_SHADER_STAGE_FRAGMENT_BIT;
			pc_ranges[i].offset = pcranges[i].start;
			pc_ranges[i].size = pcranges[i].size;
		}

		VkPipelineLayoutCreateInfo pipeline_layout_info{};
		pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_info.setLayoutCount = 1;
		pipeline_layout_info.pSetLayouts = &handle->descriptor_set_layout;
		pipeline_layout_info.pushConstantRangeCount = pcrange_count;
		pipeline_layout_info.pPushConstantRanges = pc_ranges;

		if (vkCreatePipelineLayout(video->handle->device, &pipeline_layout_info, null, &handle->pipeline_layout) != VK_SUCCESS) {
			abort_with("Failed to create pipeline layout.");
		}

		delete[] pc_ranges;
		delete[] layout_bindings;

		VkGraphicsPipelineCreateInfo pipeline_info{};
		pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_info.stageCount = 2;
		pipeline_info.pStages = stages;
		pipeline_info.pVertexInputState = &vertex_input_info;
		pipeline_info.pInputAssemblyState = &input_assembly;
		pipeline_info.pViewportState = &viewport_state;
		pipeline_info.pRasterizationState = &rasteriser;
		pipeline_info.pMultisampleState = &multisampling;
		pipeline_info.pDepthStencilState = null;
		pipeline_info.pColorBlendState = &color_blending;
		pipeline_info.pDynamicState = null;
		pipeline_info.layout = handle->pipeline_layout;
		pipeline_info.renderPass = handle->render_pass;
		pipeline_info.subpass = 0;

		if (vkCreateGraphicsPipelines(video->handle->device, VK_NULL_HANDLE, 1, &pipeline_info, null, &handle->pipeline) != VK_SUCCESS) {
			abort_with("Failed to create pipeline.");
		}

		delete[] vk_attribs;
	}

	Pipeline::~Pipeline() {
		video->wait_for_done();

		for (u32 i = 0; i < uniform_count; i++) {
			for (u32 ii = 0; ii < max_frames_in_flight; ii++) {
				vkDestroyBuffer(video->handle->device, handle->uniforms[i].uniform_buffers[ii], null);
				vkFreeMemory(video->handle->device, handle->uniforms[i].uniform_buffer_memories[ii], null);
			}
		}

		vkDestroyDescriptorPool(video->handle->device, handle->descriptor_pool, null);
		vkDestroyDescriptorSetLayout(video->handle->device, handle->descriptor_set_layout, null);

		delete[] handle->uniforms;

		vkDestroyPipeline(video->handle->device, handle->pipeline, null);
		vkDestroyPipelineLayout(video->handle->device, handle->pipeline_layout, null);
		vkDestroyRenderPass(video->handle->device, handle->render_pass, null);
	
		delete handle;
	}

	void Pipeline::make_default() {
		/* Create framebuffers for the swapchain. */
		video->handle->swapchain_framebuffers = new VkFramebuffer[video->handle->swapchain_image_count];
		for (u32 i = 0; i < video->handle->swapchain_image_count; i++) {
			VkImageView attachments[] = {
				video->handle->swapchain_image_views[i]
			};

			VkFramebufferCreateInfo fb_info{};
			fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fb_info.renderPass = handle->render_pass;
			fb_info.attachmentCount = 1;
			fb_info.pAttachments = attachments;
			fb_info.width =  video->handle->swapchain_extent.width;
			fb_info.height = video->handle->swapchain_extent.height;
			fb_info.layers = 1;

			if (vkCreateFramebuffer(video->handle->device, &fb_info, null, &video->handle->swapchain_framebuffers[i]) != VK_SUCCESS) {
				abort_with("Failed to create framebuffer.");
			}
		}
	}

	void Pipeline::begin() {
		video->pipeline = this;

		/* TODO: pass a framebuffer into here? */

		/* Update the uniform buffers. */
		for (u32 i = 0; i < uniform_count; i++) {
			void* uniform_data;
			vkMapMemory(video->handle->device, handle->uniforms[i].uniform_buffer_memories[video->current_frame],
				0, handle->uniforms[i].size, 0, &uniform_data);
			memcpy(uniform_data, handle->uniforms[i].ptr, handle->uniforms[i].size);
			vkUnmapMemory(video->handle->device, handle->uniforms[i].uniform_buffer_memories[video->current_frame]);
		}

		VkRenderPassBeginInfo render_pass_info{};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_info.renderPass = handle->render_pass;
		render_pass_info.framebuffer = video->handle->swapchain_framebuffers[video->image_id];
		render_pass_info.renderArea.offset = { 0, 0 };
		render_pass_info.renderArea.extent = video->handle->swapchain_extent;

		VkClearValue clear_color = { { { 0.0f, 0.0f, 0.0f, 1.0f } } };
		render_pass_info.clearValueCount = 1;
		render_pass_info.pClearValues = &clear_color;

		vkCmdBeginRenderPass(video->handle->command_buffers[video->current_frame], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(video->handle->command_buffers[video->current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, handle->pipeline);
	}

	void Pipeline::end() {
		vkCmdEndRenderPass(video->handle->command_buffers[video->current_frame]);
	}

	void Pipeline::push_constant(Stage stage, const void* ptr, usize size, usize offset) {
#ifdef DEBUG
		if (size > max_push_const_size) {
			abort_with("Push constant too big. Use a uniform buffer instead.");
		}
#endif
		vkCmdPushConstants(video->handle->command_buffers[video->current_frame], handle->pipeline_layout,
		stage == Stage::vertex ?
			VK_SHADER_STAGE_VERTEX_BIT :
			VK_SHADER_STAGE_FRAGMENT_BIT,
		offset, size, ptr);
	}

	Buffer::Buffer(VideoContext* video) : video(video) {
		handle = new impl_Buffer();
	}

	Buffer::~Buffer() {
		delete handle;
	}

	VertexBuffer::VertexBuffer(VideoContext* video, void* verts, usize size) : Buffer(video) {
		VkBuffer stage_buffer;
		VkDeviceMemory stage_buffer_memory;

		new_buffer(video->handle, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&stage_buffer, &stage_buffer_memory);

		void* data;
		vkMapMemory(video->handle->device, stage_buffer_memory, 0, size, 0, &data);
		memcpy(data, verts, size);
		vkUnmapMemory(video->handle->device, stage_buffer_memory);

		new_buffer(video->handle, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &handle->buffer, &handle->memory);
		copy_buffer(video->handle, handle->buffer, stage_buffer, size);

		vkDestroyBuffer(video->handle->device, stage_buffer, null);
		vkFreeMemory(video->handle->device, stage_buffer_memory, null);
	}

	VertexBuffer::~VertexBuffer() {
		video->wait_for_done();

		vkDestroyBuffer(video->handle->device, handle->buffer, null);
		vkFreeMemory(video->handle->device, handle->memory, null);
	}

	void VertexBuffer::bind() {
		VkBuffer vbs[] = { handle->buffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(video->handle->command_buffers[video->current_frame], 0, 1, vbs, offsets);
	}

	IndexBuffer::IndexBuffer(VideoContext* video, u16* indices, usize count) : Buffer(video), count(count) {
		usize size = sizeof(u16) * count;

		VkBuffer stage_buffer;
		VkDeviceMemory stage_buffer_memory;

		new_buffer(video->handle, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&stage_buffer, &stage_buffer_memory);

		void* data;
		vkMapMemory(video->handle->device, stage_buffer_memory, 0, size, 0, &data);
		memcpy(data, indices, size);
		vkUnmapMemory(video->handle->device, stage_buffer_memory);

		new_buffer(video->handle, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &handle->buffer, &handle->memory);
		copy_buffer(video->handle, handle->buffer, stage_buffer, size);

		vkDestroyBuffer(video->handle->device, stage_buffer, null);
		vkFreeMemory(video->handle->device, stage_buffer_memory, null);
	}

	IndexBuffer::~IndexBuffer() {
		video->wait_for_done();

		vkDestroyBuffer(video->handle->device, handle->buffer, null);
		vkFreeMemory(video->handle->device, handle->memory, null);
	}

	void IndexBuffer::draw() {
		for (usize i = 0; i < video->pipeline->uniform_count; i++) {
			vkCmdBindDescriptorSets(video->handle->command_buffers[video->current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS,
				video->pipeline->handle->pipeline_layout, 0, 1,
				&video->pipeline->handle->descriptor_sets[video->current_frame], 0, null);
		}

		vkCmdBindIndexBuffer(video->handle->command_buffers[video->current_frame], handle->buffer, 0, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(video->handle->command_buffers[video->current_frame], count, 1, 0, 0, 0);

		video->object_count++;
	}

	Shader::Shader(VideoContext* video, const u8* v_buf, const u8* f_buf, usize v_size, usize f_size) : video(video) {
		handle = new impl_Shader();

		handle->v_shader = new_shader_module(video->handle->device, v_buf, v_size);
		handle->f_shader = new_shader_module(video->handle->device, f_buf, f_size);

		delete[] v_buf;
		delete[] f_buf;
	}

	Shader* Shader::from_file(VideoContext* video, const char* vert_path, const char* frag_path) {
		u8* v_buf; usize v_size;
		u8* f_buf; usize f_size;

		read_raw(vert_path, &v_buf, &v_size);
		read_raw(frag_path, &f_buf, &f_size);

		return new Shader(video, v_buf, f_buf, v_size, f_size);
	}

	Shader::~Shader() {
		vkDestroyShaderModule(video->handle->device, handle->v_shader, null);
		vkDestroyShaderModule(video->handle->device, handle->f_shader, null);

		delete handle;
	}
};
