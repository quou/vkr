#include <algorithm>
#include <optional>
#include <set>
#include <vector>

#include <string.h>

#include <vulkan/vulkan.h>
#include <stb_image.h>

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
					avail_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
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
		v2i size = app.get_size();

		VkExtent2D extent = {
			(u32)size.x, (u32)size.y
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
					features.samplerAnisotropy &&
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

	static VkCommandBuffer begin_temp_command_buffer(impl_VideoContext* handle) {
		VkCommandBufferAllocateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		info.commandPool = handle->command_pool;
		info.commandBufferCount = 1;

		VkCommandBuffer buffer;
		vkAllocateCommandBuffers(handle->device, &info, &buffer);

		VkCommandBufferBeginInfo begin_info{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(buffer, &begin_info);

		return buffer;
	}

	static void end_temp_command_buffer(impl_VideoContext* handle, VkCommandBuffer buffer) {
		vkEndCommandBuffer(buffer);

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &buffer;

		vkQueueSubmit(handle->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
		vkQueueWaitIdle(handle->graphics_queue);

		vkFreeCommandBuffers(handle->device, handle->command_pool, 1, &buffer);
	}

	/* Copies the VRAM from one buffer to another, similar to how memcpy works on the CPU.
	 *
	 * Waits for the copy to complete before returning. */
	static void copy_buffer(impl_VideoContext* handle, VkBuffer dst, VkBuffer src, VkDeviceSize size) {
		VkCommandBuffer command_buffer = begin_temp_command_buffer(handle);

		VkBufferCopy copy{};
		copy.size = size;
		vkCmdCopyBuffer(command_buffer, src, dst, 1, &copy);

		end_temp_command_buffer(handle, command_buffer);
	}

	template <typename T>
	static void cpu_copy_buffer(T* src, usize src_count, T** dst, usize* dst_count) {
		if (src_count == 0) {
			*dst = null;
			*dst_count = 0;
			return;
		}

		*dst_count = src_count;

		*dst = new T[src_count]();

		for (usize i = 0; i < src_count; i++) {
			(*dst)[i] = src[i];
		}
	}

	static void new_image(impl_VideoContext* handle, v2i size, VkFormat format,
		VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags props,
		VkImage* image, VmaAllocation* image_memory) {

		VkImageCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		create_info.imageType = VK_IMAGE_TYPE_2D;
		create_info.extent.width = (u32)size.x;
		create_info.extent.height = (u32)size.y;
		create_info.extent.depth = 1;
		create_info.mipLevels = 1;
		create_info.arrayLayers = 1;
		create_info.format = format;
		create_info.tiling = tiling;
		create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		create_info.usage = usage;
		create_info.samples = VK_SAMPLE_COUNT_1_BIT;
		create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo alloc_info{};
		alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
		alloc_info.requiredFlags = props;

		if (vmaCreateImage(handle->allocator, &create_info, &alloc_info, image, image_memory, null) != VK_SUCCESS) {
			abort_with("Failed to create image.");
		}
	}

	static void change_image_layout(impl_VideoContext* handle, VkImage image, VkFormat format,
		VkImageLayout src_layout, VkImageLayout dst_layout) {

		auto command_buffer = begin_temp_command_buffer(handle);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = src_layout;
		barrier.newLayout = dst_layout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags src_stage, dst_stage;

		if (src_layout == VK_IMAGE_LAYOUT_UNDEFINED && dst_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		} else if (src_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && dst_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		} else {
			abort_with("Bad layout transition.");
		}

		vkCmdPipelineBarrier(command_buffer, src_stage, dst_stage, 0, 0, null, 0, null, 1, &barrier);

		end_temp_command_buffer(handle, command_buffer);
	}

	static void copy_buffer_to_image(impl_VideoContext* handle, VkBuffer buffer, VkImage image, v2i size) {
		auto command_buffer = begin_temp_command_buffer(handle);

		VkBufferImageCopy region{};
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.layerCount = 1;
		region.imageExtent = { (u32)size.x, (u32)size.y, 1 };

		vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		end_temp_command_buffer(handle, command_buffer);
	}

	static VkImageView new_image_view(impl_VideoContext* handle, VkImage image, VkFormat format, VkImageAspectFlags flags) {
		VkImageViewCreateInfo iv_create_info{};
		iv_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		iv_create_info.image = image;
		iv_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		iv_create_info.format = format;
		iv_create_info.subresourceRange.aspectMask = flags;
		iv_create_info.subresourceRange.baseMipLevel = 0;
		iv_create_info.subresourceRange.levelCount = 1;
		iv_create_info.subresourceRange.baseArrayLayer = 0;
		iv_create_info.subresourceRange.layerCount = 1;

		VkImageView view;
		if (vkCreateImageView(handle->device, &iv_create_info, null, &view) != VK_SUCCESS) {
			abort_with("Failed to create image view.");
		}

		return view;
	}

	static VkFormat find_supported_format(impl_VideoContext* handle, VkFormat* candidates, usize candidate_count, VkImageTiling tiling, VkFormatFeatureFlags features) {
		for (usize i = 0; i < candidate_count; i++) {
			VkFormat format = candidates[i];

			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(handle->pdevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
				return format;
			} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
				return format;
			}
		}

		abort_with("No supported formats.");

		return (VkFormat)0;
	}

	static VkFormat find_depth_format(impl_VideoContext* handle) {
		VkFormat formats[] = {
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT
		};

		return find_supported_format(handle, formats, 3, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}


	static VkFormat fb_format(impl_VideoContext* handle, Framebuffer::Attachment::Format format) {
#define fmt(v_) format == Framebuffer::Attachment::Format::v_

		return
			fmt(depth)   ? find_depth_format(handle) :
			fmt(red8)    ? VK_FORMAT_R8_UNORM :
			fmt(rgb8)    ? VK_FORMAT_R8G8B8_UNORM :
			fmt(rgba8)   ? VK_FORMAT_R8G8B8A8_UNORM :
			fmt(redf32)  ? VK_FORMAT_R32_SFLOAT :
			fmt(rgbf32)  ? VK_FORMAT_R32G32B32_SFLOAT :
			fmt(rgbaf32) ? VK_FORMAT_R32G32B32A32_SFLOAT :
			fmt(redf16)  ? VK_FORMAT_R16_SFLOAT :
			fmt(rgbf16)  ? VK_FORMAT_R16G16B16_SFLOAT :
			fmt(rgbaf16) ? VK_FORMAT_R16G16B16A16_SFLOAT :
			VK_FORMAT_R8G8B8_UNORM;
#undef fmt
	}



	static bool has_stencil_comp(VkFormat format) {
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	static void new_depth_resources(impl_VideoContext* handle, VkImage* image, VkImageView* view, VmaAllocation* memory, v2i size, bool can_sample = false) {
		VkFormat depth_format = find_depth_format(handle);

		i32 usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		if (can_sample) {
			usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
		}

		new_image(handle, size,
			depth_format, VK_IMAGE_TILING_OPTIMAL, usage,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory);
		*view = new_image_view(handle, *image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
	}

	static void new_buffer(impl_VideoContext* handle, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
		VmaAllocationCreateFlags flags, VkBuffer* buffer, VmaAllocation* buffer_memory) {

		VkBufferCreateInfo buffer_info{};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = size;
		buffer_info.usage = usage;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
			flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		}

		VmaAllocationCreateInfo alloc_info{};
		alloc_info.flags = flags;
		alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
		alloc_info.requiredFlags = props;

		if (vmaCreateBuffer(handle->allocator, &buffer_info, &alloc_info, buffer, buffer_memory, null) != VK_SUCCESS) {
			abort_with("Failed to create buffer.");
		}
	}

	VideoContext::VideoContext(const App& app, const char* app_name, bool enable_validation_layers, u32 extension_count, const char** extensions) : current_frame(0), app(app), want_recreate(false) {
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
		device_features.samplerAnisotropy = VK_TRUE;

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

		/* Create the allocator */
		VmaVulkanFunctions vk_functions{};
		vk_functions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
		vk_functions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

		VmaAllocatorCreateInfo allocator_info{};
		allocator_info.vulkanApiVersion = VK_API_VERSION_1_0;
		allocator_info.physicalDevice = handle->pdevice;
		allocator_info.device = handle->device;
		allocator_info.instance = handle->instance;
		allocator_info.pVulkanFunctions = &vk_functions;

		vmaCreateAllocator(&allocator_info, &handle->allocator);

		init_swapchain();

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

		Framebuffer::Attachment attachments[] = {
			{
				.type = Framebuffer::Attachment::Type::color,
				.format = Framebuffer::Attachment::Format::rgb8,
			},
			{
				.type = Framebuffer::Attachment::Type::depth,
				.format = Framebuffer::Attachment::Format::depth,
			}
		};

		/* Create the default framebuffer. */
		default_fb = new Framebuffer(this,
			Framebuffer::Flags::default_fb | Framebuffer::Flags::fit,
			app.get_size(),
			attachments, 2);
	}

	VideoContext::~VideoContext() {
		for (u32 i = 0; i < max_frames_in_flight; i++) {
			vkDestroySemaphore(handle->device, handle->image_avail_semaphores[i], null);
			vkDestroySemaphore(handle->device, handle->render_finish_semaphores[i], null);
			vkDestroyFence(handle->device, handle->in_flight_fences[i], null);
		}

		vkDestroyCommandPool(handle->device, handle->command_pool, null);

		delete default_fb;

		for (u32 i = 0; i < handle->swapchain_image_count; i++) {
			vkDestroyImageView(handle->device, handle->swapchain_image_views[i], null);
		}

		vkDestroySwapchainKHR(handle->device, handle->swapchain, null);

		vmaDestroyAllocator(handle->allocator);

		vkDestroyDevice(handle->device, null);
		vkDestroySurfaceKHR(handle->instance, handle->surface, null);
		vkDestroyInstance(handle->instance, null);

		delete[] handle->swapchain_images;
		delete[] handle->swapchain_image_views;

		delete handle;
	}

	void VideoContext::init_swapchain() {
		auto qfs = get_queue_families(handle->pdevice, handle);

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
			handle->swapchain_image_views[i] = new_image_view(handle, handle->swapchain_images[i],
				handle->swapchain_format, VK_IMAGE_ASPECT_COLOR_BIT);
		}

	}

	void VideoContext::deinit_swapchain() {
		for (u32 i = 0; i < handle->swapchain_image_count; i++) {
			vkDestroyImageView(handle->device, handle->swapchain_image_views[i], null);
		}

		vkDestroySwapchainKHR(handle->device, handle->swapchain, null);

		delete[] handle->swapchain_images;
		delete[] handle->swapchain_image_views;
	}

	void VideoContext::begin() {
		object_count = 0;
		skip_frame = false;

		vkWaitForFences(handle->device, 1, &handle->in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

		auto r = vkAcquireNextImageKHR(handle->device, handle->swapchain, UINT64_MAX,
			handle->image_avail_semaphores[current_frame], VK_NULL_HANDLE, &image_id);
		if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR || want_recreate) {
			skip_frame = true;
			want_recreate = false;
			resize(app.get_size());
			return;
		} else if (r != VK_SUCCESS) {
			abort_with("Failed to acquire swapchain image.");
		}

		vkResetFences(handle->device, 1, &handle->in_flight_fences[current_frame]);

		vkResetCommandBuffer(handle->command_buffers[current_frame], 0);

		VkCommandBufferBeginInfo begin_info{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(handle->command_buffers[current_frame], &begin_info) != VK_SUCCESS) {
			warning("Failed to begin the command buffer.");
			return;
		}
	}

	void VideoContext::end() {
		if (skip_frame) { return; }

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

	void VideoContext::resize(v2i new_size) {
		wait_for_done();

		deinit_swapchain();

		init_swapchain();

		for (auto fb : framebuffers) {
			if (fb->flags & Framebuffer::Flags::fit) {
				fb->resize(new_size);
			}
		}

		for (auto pip : pipelines) {
			pip->recreate();
		}
	}

	Pipeline::Pipeline(VideoContext* video, Flags flags, Shader* shader, usize stride,
			Attribute* attribs, usize attrib_count,
			Framebuffer* framebuffer,
			DescriptorSet* desc_sets, usize desc_set_count,
			PushConstantRange* pcranges, usize pcrange_count, bool is_recreating) : is_recreating(is_recreating),
			video(video), descriptor_set_count(desc_set_count),
			flags(flags), framebuffer(framebuffer), stride(stride) {
		handle = new impl_Pipeline();

		if (!is_recreating) {
			this->shader = shader;
			cpu_copy_buffer(attribs, attrib_count, &this->attribs, &this->attrib_count);
			cpu_copy_buffer(desc_sets, desc_set_count, &this->descriptor_sets, &this->descriptor_set_count);
			cpu_copy_buffer(pcranges, pcrange_count, &this->pcranges, &this->pcrange_count);

			for (usize i = 0; i < desc_set_count; i++) {
				cpu_copy_buffer(
					desc_sets[i].descriptors,              desc_sets[i].count,
					&this->descriptor_sets[i].descriptors, &this->descriptor_sets[i].count);
			}

			video->pipelines.push_back(this);
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

		auto scaled_fb_size = framebuffer->get_scaled_size();

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width  = scaled_fb_size.x;
		viewport.height = scaled_fb_size.y;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = { (u32)scaled_fb_size.x, (u32)scaled_fb_size.y };

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
		rasteriser.cullMode =
			(flags & Flags::cull_back_face)  ? VK_CULL_MODE_BACK_BIT :
			(flags & Flags::cull_front_face) ? VK_CULL_MODE_FRONT_BIT :
			VK_CULL_MODE_NONE;
		rasteriser.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasteriser.depthBiasEnable = VK_FALSE;

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineDepthStencilStateCreateInfo depth_stencil{};
		depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil.depthTestEnable = VK_TRUE;
		depth_stencil.depthWriteEnable = VK_TRUE;
		depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depth_stencil.depthBoundsTestEnable = VK_FALSE;
		depth_stencil.stencilTestEnable = VK_FALSE;

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

		/* Count the descriptors of different types to create a descriptor pool. */
		sampler_count = uniform_count = 0;
		for (usize i = 0; i < desc_set_count; i++) {
			auto set = desc_sets + i;

			for (usize ii = 0; ii < set->count; ii++) {
				auto desc = set->descriptors + ii;

				switch (desc->resource.type) {
					case ResourcePointer::Type::texture:
					case ResourcePointer::Type::framebuffer_output:
						sampler_count++;
						break;
					case ResourcePointer::Type::uniform_buffer:
						uniform_count++;
						break;
					default:
						abort_with("Invalid resource pointer type on descriptor.");
						break;
				}
			}
		}

		/* Create the descriptor pool. */
		VkDescriptorPoolSize pool_sizes[2];
		usize pool_size_count = 0;
		if (uniform_count > 0) {
			auto idx = pool_size_count++;

			pool_sizes[idx].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			pool_sizes[idx].descriptorCount = max_frames_in_flight * (u32)uniform_count;
		}

		if (sampler_count > 0) {
			auto idx = pool_size_count++;

			pool_sizes[idx].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			pool_sizes[idx].descriptorCount = max_frames_in_flight * (u32)sampler_count;
		}

		VkDescriptorPoolCreateInfo pool_info{};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.poolSizeCount = (u32)pool_size_count;
		pool_info.pPoolSizes = pool_sizes;
		pool_info.maxSets =
			(max_frames_in_flight * uniform_count) +
			(max_frames_in_flight * sampler_count);

		if (vkCreateDescriptorPool(video->handle->device, &pool_info, null, &handle->descriptor_pool) != VK_SUCCESS) {
			abort_with("Failed to create the descriptor pool.");
		}

		handle->desc_sets = new impl_DescriptorSet[desc_set_count]();
		handle->uniforms = new impl_UniformBuffer[uniform_count]();
		auto set_layouts = new VkDescriptorSetLayout[desc_set_count]();

		for (usize i = 0; i < desc_set_count; i++) {
			auto set = desc_sets + i;
			auto v_set = handle->desc_sets + i;

			auto layout_bindings = new VkDescriptorSetLayoutBinding[set->count]();

			for (usize ii = 0; ii < set->count; ii++) {
				auto desc = set->descriptors + ii;

				auto lb = layout_bindings + ii;

				VkDescriptorType type;

				switch (desc->resource.type) {
					case ResourcePointer::Type::texture:
					case ResourcePointer::Type::framebuffer_output:
						lb->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						break;
					case ResourcePointer::Type::uniform_buffer:
						lb->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
						break;
					default:
						abort_with("Invalid resource pointer type on descriptor.");
						break;
				}

				lb->binding = desc->binding;
				lb->descriptorCount = 1;
				lb->stageFlags = desc->stage == Stage::vertex ?
					VK_SHADER_STAGE_VERTEX_BIT :
					VK_SHADER_STAGE_FRAGMENT_BIT;
			}

			VkDescriptorSetLayoutCreateInfo layout_info{};
			layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layout_info.bindingCount = (u32)set->count;
			layout_info.pBindings = layout_bindings;

			if (vkCreateDescriptorSetLayout(video->handle->device, &layout_info, null, &v_set->layout) != VK_SUCCESS) {
				abort_with("Failed to create the descriptor set layout.");
			}

			set_layouts[i] = v_set->layout;

			/* Each descriptor set for each frame in flight uses
			 * the same descriptor set layout. */
			VkDescriptorSetLayout layouts[max_frames_in_flight];
			for (u32 ii = 0; ii < max_frames_in_flight; ii++) {
				layouts[ii] = v_set->layout;
			}

			VkDescriptorSetAllocateInfo alloc_info{};
			alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			alloc_info.descriptorPool = handle->descriptor_pool;
			alloc_info.descriptorSetCount = max_frames_in_flight;
			alloc_info.pSetLayouts = layouts;

			if (vkAllocateDescriptorSets(video->handle->device, &alloc_info, v_set->sets) != VK_SUCCESS) {
				abort_with("Failed to allocate descriptor sets.");
			}

			/* Write the descriptor set and create uniform buffers if necessary. */
			for (usize ii = 0, ui = 0; ii < set->count; ii++) {
				VkWriteDescriptorSet desc_writes[max_frames_in_flight] = {};

				usize uniform_index;

				if (set->descriptors[ii].resource.type == ResourcePointer::Type::uniform_buffer) {
					uniform_index = ui++;

					handle->uniforms[uniform_index].ptr  = set->descriptors[ii].resource.uniform.ptr;
					handle->uniforms[uniform_index].size = set->descriptors[ii].resource.uniform.size;
				}

				for (usize j = 0; j < max_frames_in_flight; j++) {
					auto write = desc_writes + j;

					write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					write->dstSet = v_set->sets[j];
					write->dstBinding = set->descriptors[ii].binding;
					write->dstArrayElement = 0;
					write->descriptorCount = 1;

					VkDescriptorImageInfo image_info;
					VkDescriptorBufferInfo buffer_info;

					switch (set->descriptors[ii].resource.type) {
						case ResourcePointer::Type::texture: {
							auto t = set->descriptors[ii].resource.texture.ptr;

							VkDescriptorImageInfo image_info{};
							image_info.imageView   = t->handle->view;
							image_info.sampler     = t->handle->sampler;
							image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

							write->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
							write->pImageInfo = &image_info;
						} break;
						case ResourcePointer::Type::framebuffer_output: {	
							auto fb = set->descriptors[ii].resource.framebuffer.ptr;
							auto attachment = set->descriptors[ii].resource.framebuffer.attachment;

							image_info.imageView   = fb->handle->attachment_map[attachment]->image_views[j];
							image_info.sampler     = fb->handle->sampler;
							image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

							write->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
							write->pImageInfo = &image_info;
						} break;
						case ResourcePointer::Type::uniform_buffer: {
							new_buffer(video->handle, (VkDeviceSize)handle->uniforms[uniform_index].size,
								VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
								VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								VMA_ALLOCATION_CREATE_MAPPED_BIT,
								handle->uniforms[uniform_index].buffers + j,
								handle->uniforms[uniform_index].memories + j);

							buffer_info.buffer = handle->uniforms[uniform_index].buffers[j];
							buffer_info.offset = 0;
							buffer_info.range = handle->uniforms[uniform_index].size;

							write->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
							write->pBufferInfo = &buffer_info;
						} break;
						default:
							abort_with("Invalid resource pointer type on descriptor.");
							break;
					}
				}

				vkUpdateDescriptorSets(video->handle->device, max_frames_in_flight, desc_writes, 0, null);
			}

			delete[] layout_bindings;
		}

		auto pc_ranges = new VkPushConstantRange[pcrange_count]();
		for (usize i = 0; i < pcrange_count; i++) {
			pc_ranges[i].stageFlags = pcranges[i].stage == Stage::vertex ?
				VK_SHADER_STAGE_VERTEX_BIT :
				VK_SHADER_STAGE_FRAGMENT_BIT;
			pc_ranges[i].offset = pcranges[i].start;
			pc_ranges[i].size = pcranges[i].size;
		}

		VkPipelineLayoutCreateInfo pipeline_layout_info{};
		pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_info.setLayoutCount = (u32)desc_set_count;
		pipeline_layout_info.pSetLayouts = set_layouts;
		pipeline_layout_info.pushConstantRangeCount = pcrange_count;
		pipeline_layout_info.pPushConstantRanges = pc_ranges;

		if (vkCreatePipelineLayout(video->handle->device, &pipeline_layout_info, null, &handle->pipeline_layout) != VK_SUCCESS) {
			abort_with("Failed to create pipeline layout.");
		}

		VkGraphicsPipelineCreateInfo pipeline_info{};
		pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_info.stageCount = 2;
		pipeline_info.pStages = stages;
		pipeline_info.pVertexInputState = &vertex_input_info;
		pipeline_info.pInputAssemblyState = &input_assembly;
		pipeline_info.pViewportState = &viewport_state;
		pipeline_info.pRasterizationState = &rasteriser;
		pipeline_info.pMultisampleState = &multisampling;
		pipeline_info.pDepthStencilState = &depth_stencil;
		pipeline_info.pColorBlendState = &color_blending;
		pipeline_info.pDynamicState = null;
		pipeline_info.layout = handle->pipeline_layout;
		pipeline_info.renderPass = framebuffer->handle->render_pass;
		pipeline_info.subpass = 0;

		if (vkCreateGraphicsPipelines(video->handle->device, VK_NULL_HANDLE, 1, &pipeline_info, null, &handle->pipeline) != VK_SUCCESS) {
			abort_with("Failed to create pipeline.");
		}

		delete[] vk_attribs;
		delete[] pc_ranges;
		delete[] set_layouts;
	}

	Pipeline::~Pipeline() {
		video->wait_for_done();

		if (!is_recreating) {
			delete[] attribs;

			for (usize i = 0; i < descriptor_set_count; i++) {
				delete[] descriptor_sets[i].descriptors;
			}

			delete[] descriptor_sets;

			delete[] pcranges;
		}

		for (usize i = 0; i < uniform_count; i++) {
			for (u32 ii = 0; ii < max_frames_in_flight; ii++) {
				vmaDestroyBuffer(video->handle->allocator,
						handle->uniforms[i].buffers[ii],
						handle->uniforms[i].memories[ii]);
			}
		}

		for (usize i = 0; i < descriptor_set_count; i++) {
			auto set = handle->desc_sets + i;

			vkDestroyDescriptorSetLayout(video->handle->device, set->layout, null);
		}

		vkDestroyDescriptorPool(video->handle->device, handle->descriptor_pool, null);

		delete[] handle->desc_sets;
		delete[] handle->uniforms;

		vkDestroyPipeline(video->handle->device, handle->pipeline, null);
		vkDestroyPipelineLayout(video->handle->device, handle->pipeline_layout, null);

		delete handle;
	}

	void Pipeline::begin() {
		if (video->skip_frame) { return; }

		video->pipeline = this;

		/* Update the uniform buffers. */
		for (u32 i = 0; i < uniform_count; i++) {
			auto u = handle->uniforms + i;

			void* uniform_data;
			vmaMapMemory(video->handle->allocator, u->memories[video->current_frame], &uniform_data);
			memcpy(uniform_data, u->ptr, u->size);
			vmaUnmapMemory(video->handle->allocator, u->memories[video->current_frame]);
		}

		framebuffer->begin();

		vkCmdBindPipeline(video->handle->command_buffers[video->current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, handle->pipeline);
	}

	void Pipeline::end() {
		if (video->skip_frame) { return; }

		framebuffer->end();
	}

	void Pipeline::push_constant(Stage stage, const void* ptr, usize size, usize offset) {
		if (video->skip_frame) { return; }

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

	void Pipeline::bind_descriptor_set(usize target, usize index) {
		if (video->skip_frame) { return; }

		vkCmdBindDescriptorSets(video->handle->command_buffers[video->current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS,
			handle->pipeline_layout, target, 1,
			handle->desc_sets[index].sets + video->current_frame, 0, null);
	}

	void Pipeline::recreate() {
		is_recreating = true;

		/* The C++ Gods hate me. The C# Gods hate me even more. */
		this->~Pipeline();
		new(this) Pipeline(video, flags, shader, stride,
			attribs, attrib_count, framebuffer, descriptor_sets, descriptor_set_count,
			pcranges, pcrange_count, true);

		is_recreating = false;
	}

	Framebuffer::Framebuffer(VideoContext* video, Flags flags, v2i size, Attachment* attachments, usize attachment_count, f32 scale, bool is_recreating) :
		is_recreating(is_recreating), video(video), flags(flags), size(size), scale(scale) {

		if (!is_recreating) {
			cpu_copy_buffer(attachments, attachment_count, &this->attachments, &this->attachment_count);

			video->framebuffers.push_back(this);
		}

		handle = new impl_Framebuffer();

		handle->is_headless = flags & Flags::headless;

		if (handle->is_headless) {
			VkSamplerCreateInfo sampler_info{};
			sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sampler_info.magFilter = VK_FILTER_LINEAR;
			sampler_info.minFilter = VK_FILTER_LINEAR;
			sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			sampler_info.anisotropyEnable = VK_FALSE;
			sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			sampler_info.unnormalizedCoordinates = VK_FALSE;
			sampler_info.compareEnable = VK_FALSE;
			sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
			sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

			if (vkCreateSampler(video->handle->device, &sampler_info, null, &handle->sampler) != VK_SUCCESS) {
				abort_with("Failed to create framebuffer sampler.");
			}
		}

		bool use_depth = false;

		VkFormat color_format = video->handle->swapchain_format;

		/* Get the index of the first depth attachment.
		 *
		 * Framebuffers only support a single depth attachment. */
		usize depth_index = (usize)-1;
		for (usize i = 0; i < attachment_count; i++) {
			if (depth_index > attachment_count && attachments[i].type == Attachment::Type::depth) {
				depth_index = i;
				use_depth = true;
				break;
			}
		}

		depth_enable = use_depth;

		/* Create color attachments */
		auto ca_descs = new VkAttachmentDescription[attachment_count]();
		auto ca_refs  = new VkAttachmentReference[attachment_count]();
		auto color_formats = new VkFormat[attachment_count]();
		usize color_attachment_count = 0;
		for (usize i = 0; i < attachment_count; i++) {
			if (attachments[i].type == Attachment::Type::color) {
				auto color_attachment = attachments + i;

				auto idx = color_attachment_count++;

				if (flags & Flags::headless) {
					color_formats[idx] = fb_format(video->handle, color_attachment->format);
				} else {
					color_formats[idx] = color_format;
				}

				ca_descs[idx].format = color_formats[idx];
				ca_descs[idx].samples = VK_SAMPLE_COUNT_1_BIT;
				ca_descs[idx].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				ca_descs[idx].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				ca_descs[idx].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				ca_descs[idx].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				ca_descs[idx].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	
				if (flags & Flags::headless) {
					ca_descs[idx].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				} else {
					ca_descs[idx].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				}

				ca_refs[idx].attachment = i;
				ca_refs[idx].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}
		}

		if (flags & Flags::default_fb) {
			/* Only one colour attachment is supported on the default framebuffer. */
			color_attachment_count = 1;
		} else {
			/* Map the actual indices to attachments. */
			handle->colors = new impl_Attachment[color_attachment_count];
			for (usize i = 0, ci = 0; i < attachment_count; i++, ci++) {
				if (attachments[i].type == Attachment::Type::color) {
					handle->attachment_map[(u32)i] = handle->colors + ci;
				} else {
					handle->attachment_map[(u32)i] = &handle->depth;
				}
			}
		}

		handle->color_count = color_attachment_count;

		VkAttachmentDescription depth_attachment{};
		depth_attachment.format = find_depth_format(video->handle);
		depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depth_attachment_ref{};
		depth_attachment_ref.attachment = depth_index;
		depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = color_attachment_count;
		subpass.pColorAttachments = color_attachment_count > 0 ? ca_refs : null;

		if (use_depth) {
			subpass.pDepthStencilAttachment = &depth_attachment_ref;
		}

		/* Combine the depth and color attachments into a single array suitable
		 * for giving to vkCreateRenderPass. */
		VkAttachmentDescription* v_attachments;
		handle->clear_colors = new VkClearValue[attachment_count]();
		if (use_depth && color_attachment_count > 0) {
			v_attachments = new VkAttachmentDescription[attachment_count]();

			for (usize i = 0; i < depth_index; i++) {
				v_attachments[i] = ca_descs[i];
				handle->clear_colors[i].color = {{ 0.1f, 0.1f, 0.1f, 1.0f }};
			}

			v_attachments[depth_index] = depth_attachment;
			handle->clear_colors[depth_index].depthStencil = { 1.0f, 0 };

			for (usize i = depth_index + 1; i < attachment_count; i++) {
				v_attachments[i] = ca_descs[i - 1];
				handle->clear_colors[i].color = {{ 0.1f, 0.1f, 0.1f, 1.0f }};
			}
			handle->clear_color_count = attachment_count;
		} else if (use_depth && color_attachment_count == 0) {
			v_attachments = new VkAttachmentDescription[1]();
			v_attachments[depth_index] = depth_attachment;
			handle->clear_colors[depth_index].depthStencil = { 1.0f, 0 };
			handle->clear_color_count = 1;
		} else {
			v_attachments = ca_descs;

			for (usize i = 0; i < attachment_count; i++) {
				handle->clear_colors[i].color = {{ 0.1f, 0.1f, 0.1f, 1.0f }};
			}
			handle->clear_color_count = attachment_count;
		}

		VkSubpassDependency deps[2];
		usize dep_count = 0;

		if (color_attachment_count > 0) {
			deps[dep_count].srcSubpass      = VK_SUBPASS_EXTERNAL;
			deps[dep_count].dstSubpass      = 0;
			deps[dep_count].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			deps[dep_count].srcAccessMask   = 0;
			deps[dep_count].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			deps[dep_count].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			deps[dep_count].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dep_count++;
		}

		if (use_depth) {
			deps[dep_count].srcSubpass      = VK_SUBPASS_EXTERNAL;
			deps[dep_count].dstSubpass      = 0;
			deps[dep_count].srcStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			deps[dep_count].srcAccessMask   = 0;
			deps[dep_count].dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			deps[dep_count].dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			deps[dep_count].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dep_count++;
		}

		VkRenderPassCreateInfo render_pass_info{};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = (u32)attachment_count;
		render_pass_info.pAttachments = v_attachments;
		render_pass_info.subpassCount = 1;
		render_pass_info.pSubpasses = &subpass;
		render_pass_info.dependencyCount = dep_count;
		render_pass_info.pDependencies = deps;

		if (vkCreateRenderPass(video->handle->device, &render_pass_info, null, &handle->render_pass)) {
			abort_with("Failed to create render pass.");
		}

		if (flags & Flags::default_fb) {
			/* For the swapchain.. */
			handle->swapchain_framebuffers = new VkFramebuffer[video->handle->swapchain_image_count];
			handle->framebuffers = handle->swapchain_framebuffers;

			/* The default framebuffer can only have two attachments. */
			VkImageView image_attachments[2];

			/* Create the depth buffer. */
			if (use_depth) {
				new_depth_resources(video->handle, &handle->depth_image, &handle->depth_image_view, &handle->depth_memory, size);
				image_attachments[1] = handle->depth_image_view;
			}

			for (u32 i = 0; i < video->handle->swapchain_image_count; i++) {
				image_attachments[0] = video->handle->swapchain_image_views[i];

				VkFramebufferCreateInfo fb_info{};
				fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				fb_info.renderPass = handle->render_pass;
				fb_info.attachmentCount = use_depth ? 2 : 1;
				fb_info.pAttachments = image_attachments;
				fb_info.width  = size.x;
				fb_info.height = size.y;
				fb_info.layers = 1;

				if (vkCreateFramebuffer(video->handle->device, &fb_info, null, handle->framebuffers + i) != VK_SUCCESS) {
					abort_with("Failed to create framebuffer.");
				}
			}
		} else {
			/* Create images and image views for off-screen rendering. */
			handle->framebuffers = handle->offscreen_framebuffers;

			for (usize i = 0; i < color_attachment_count; i++) {
				auto attachment = handle->colors + i;
				attachment->type = Attachment::Type::color;

				auto fmt = color_formats[i];

				for (u32 ii = 0; ii < max_frames_in_flight; ii++) {
					new_image(video->handle, v2i((i32)((f32)size.x * scale), (i32)((f32)size.y * scale)),
						fmt, VK_IMAGE_TILING_OPTIMAL,
						VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
						attachment->images + ii, attachment->image_memories + ii);
					attachment->image_views[ii] = new_image_view(video->handle, attachment->images[ii],
						fmt, VK_IMAGE_ASPECT_COLOR_BIT);
				}
			}

			/* Create the depth buffers. */
			if (use_depth) {
				auto fmt = find_depth_format(video->handle);

				for (u32 i = 0; i < max_frames_in_flight; i++) {
					new_depth_resources(video->handle, &handle->depth.images[i],
						&handle->depth.image_views[i], &handle->depth.image_memories[i], size * scale, true);
					handle->depth.type = Attachment::Type::depth;
				}
			}

			auto image_attachments = new VkImageView[attachment_count];

			/* Create framebuffers, one for each frame in flight. */
			for (u32 i = 0; i < max_frames_in_flight; i++) {
				if (use_depth && color_attachment_count > 0) {
					for (usize ii = 0; ii < depth_index; ii++) {
						image_attachments[ii] = handle->colors[ii].image_views[i];
					}

					image_attachments[depth_index] = handle->depth.image_views[i];

					for (usize ii = depth_index + 1; ii < attachment_count; ii++) {
						image_attachments[ii] = handle->colors[ii - 1].image_views[i];
					}
				} else if (use_depth && color_attachment_count == 0) {
					image_attachments[depth_index] = handle->depth.image_views[i];
				} else {
					for (usize ii = 0; ii < attachment_count; ii++) {
						image_attachments[ii] = handle->colors[ii].image_views[i];
					}
				}

				VkFramebufferCreateInfo fb_info{};
				fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				fb_info.renderPass = handle->render_pass;
				fb_info.attachmentCount = attachment_count;
				fb_info.pAttachments = image_attachments;
				fb_info.width =  (u32)((f32)size.x * scale);
				fb_info.height = (u32)((f32)size.y * scale);
				fb_info.layers = 1;

				if (vkCreateFramebuffer(video->handle->device, &fb_info, null, handle->framebuffers + i) != VK_SUCCESS) {
					abort_with("Failed to create framebuffer.");
				}
			}

			delete[] image_attachments;
		}

		if (use_depth) {
			delete[] v_attachments;
		}

		delete[] color_formats;
		delete[] ca_descs;
		delete[] ca_refs;
	}

	Framebuffer::~Framebuffer() {
		video->wait_for_done();

		if (!is_recreating) {
			delete[] attachments;
		}

		if (flags & Flags::default_fb) {
			for (u32 i = 0; i < video->handle->swapchain_image_count; i++) {
				vkDestroyFramebuffer(video->handle->device, handle->swapchain_framebuffers[i], null);
			}

			if (depth_enable) {
				vkDestroyImageView(video->handle->device, handle->depth_image_view, null);
				vmaDestroyImage(video->handle->allocator, handle->depth_image, handle->depth_memory);
			}

			delete[] handle->swapchain_framebuffers;
		} else if (flags & Flags::headless) {
			vkDestroySampler(video->handle->device, handle->sampler, null);

			if (depth_enable) {
				for (u32 i = 0; i < max_frames_in_flight; i++) {
					vkDestroyImageView(video->handle->device, handle->depth.image_views[i], null);
					vmaDestroyImage(video->handle->allocator, handle->depth.images[i], handle->depth.image_memories[i]);
				}
			}

			for (usize i = 0; i < handle->color_count; i++) {
				auto attachment = handle->colors + i;

				for (u32 ii = 0; ii < max_frames_in_flight; ii++) {
					vmaDestroyImage(video->handle->allocator, attachment->images[ii], attachment->image_memories[ii]);
					vkDestroyImageView(video->handle->device, attachment->image_views[ii], null);
				}
			}

			for (u32 i = 0; i < max_frames_in_flight; i++) {
				vkDestroyFramebuffer(video->handle->device, handle->framebuffers[i], null);
			}

			delete[] handle->colors;
		}

		delete[] handle->clear_colors;

		vkDestroyRenderPass(video->handle->device, handle->render_pass, null);

		delete handle;
	}

	void Framebuffer::resize(v2i new_size) {
		is_recreating = true;

		/* [Evil laugher] */
		this->~Framebuffer();
		new(this) Framebuffer(video, flags, new_size, attachments, attachment_count, scale, true);

		is_recreating = false;
	}

	void Framebuffer::begin() {
		if (flags & Flags::headless) {
			/* Transition the image layouts into layouts for writing to. */

			for (auto& pair : handle->attachment_map) {
				auto attachment = pair.second;

				auto new_layout =
					attachment->type == Attachment::Type::color ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL :
					VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

				VkImageMemoryBarrier barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				barrier.image = attachment->images[video->current_frame];
				barrier.newLayout = new_layout;
				barrier.subresourceRange = { attachment->get_aspect_flags(), 0, 1, 0, 1};
				barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

				vkCmdPipelineBarrier(video->handle->command_buffers[video->current_frame],
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					0, 0, null, 0, null, 1, &barrier);
			}
		}

		VkRenderPassBeginInfo render_pass_info{};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_info.renderPass = handle->render_pass;
		render_pass_info.framebuffer = handle->get_current_framebuffer(video->image_id, video->current_frame);
		render_pass_info.renderArea.offset = { 0, 0 };
		render_pass_info.renderArea.extent = VkExtent2D { (u32)get_scaled_size().x, (u32)get_scaled_size().y };
		render_pass_info.clearValueCount = handle->clear_color_count;
		render_pass_info.pClearValues = handle->clear_colors;

		vkCmdBeginRenderPass(video->handle->command_buffers[video->current_frame], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	}

	void Framebuffer::end() {
		vkCmdEndRenderPass(video->handle->command_buffers[video->current_frame]);

		if (flags & Flags::headless) {
			/* Transition the image layouts into layouts so that they might b
			 * sampled from a shader. */

			for (auto& pair : handle->attachment_map) {
				auto attachment = pair.second;

				auto old_layout =
					attachment->type == Attachment::Type::color ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL :
					VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

				auto new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				VkImageMemoryBarrier barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.oldLayout = old_layout;
				barrier.image = attachment->images[video->current_frame];
				barrier.newLayout = new_layout;
				barrier.subresourceRange = { attachment->get_aspect_flags(), 0, 1, 0, 1};
				barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

				vkCmdPipelineBarrier(video->handle->command_buffers[video->current_frame],
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					0, 0, null, 0, null, 1, &barrier);
			}
		}
	}

	Buffer::Buffer(VideoContext* video) : video(video) {
		handle = new impl_Buffer();
	}

	Buffer::~Buffer() {
		delete handle;
	}

	VertexBuffer::VertexBuffer(VideoContext* video, void* verts, usize size) : Buffer(video) {
		VkBuffer stage_buffer;
		VmaAllocation stage_buffer_memory;

		new_buffer(video->handle, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VMA_ALLOCATION_CREATE_MAPPED_BIT,
			&stage_buffer, &stage_buffer_memory);

		void* data;
		vmaMapMemory(video->handle->allocator, stage_buffer_memory, &data);
		memcpy(data, verts, size);
		vmaUnmapMemory(video->handle->allocator, stage_buffer_memory);

		new_buffer(video->handle, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &handle->buffer, &handle->memory);
		copy_buffer(video->handle, handle->buffer, stage_buffer, size);

		vmaDestroyBuffer(video->handle->allocator, stage_buffer, stage_buffer_memory);
	}

	VertexBuffer::~VertexBuffer() {
		video->wait_for_done();

		vmaDestroyBuffer(video->handle->allocator, handle->buffer, handle->memory);
	}

	void VertexBuffer::bind() {
		if (video->skip_frame) { return; }

		VkBuffer vbs[] = { handle->buffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(video->handle->command_buffers[video->current_frame], 0, 1, vbs, offsets);
	}

	void VertexBuffer::draw(usize count) {
		if (video->skip_frame) { return; }

		vkCmdDraw(video->handle->command_buffers[video->current_frame], count, 1, 0, 0);
	}

	IndexBuffer::IndexBuffer(VideoContext* video, u16* indices, usize count) : Buffer(video), count(count) {
		usize size = sizeof(u16) * count;

		VkBuffer stage_buffer;
		VmaAllocation stage_buffer_memory;

		new_buffer(video->handle, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VMA_ALLOCATION_CREATE_MAPPED_BIT,
			&stage_buffer, &stage_buffer_memory);

		void* data;
		vmaMapMemory(video->handle->allocator, stage_buffer_memory, &data);
		memcpy(data, indices, size);
		vmaUnmapMemory(video->handle->allocator, stage_buffer_memory);

		new_buffer(video->handle, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &handle->buffer, &handle->memory);
		copy_buffer(video->handle, handle->buffer, stage_buffer, size);

		vmaDestroyBuffer(video->handle->allocator, stage_buffer, stage_buffer_memory);
	}

	IndexBuffer::~IndexBuffer() {
		video->wait_for_done();

		vmaDestroyBuffer(video->handle->allocator, handle->buffer, handle->memory);
	}

	void IndexBuffer::draw() {
		if (video->skip_frame) { return; }

		vkCmdBindIndexBuffer(video->handle->command_buffers[video->current_frame], handle->buffer, 0, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(video->handle->command_buffers[video->current_frame], count, 1, 0, 0, 0);

		video->object_count++;
	}

	Texture::Texture(VideoContext* video, const void* data, v2i size, u32 component_count) :
		video(video), size(size), component_count(component_count) {

		handle = new impl_Texture();

		VkDeviceSize image_size = size.x * size.y * component_count;

		VkBuffer stage_buffer;
		VmaAllocation stage_buffer_memory;

		new_buffer(video->handle, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VMA_ALLOCATION_CREATE_MAPPED_BIT,
			&stage_buffer, &stage_buffer_memory);
		
		void* remote_data;
		vmaMapMemory(video->handle->allocator, stage_buffer_memory, &remote_data);
		memcpy(remote_data, data, image_size);
		vmaUnmapMemory(video->handle->allocator, stage_buffer_memory);

		/* TODO: Take this from the component count. */
		auto format = VK_FORMAT_R8G8B8A8_UNORM;

		new_image(video->handle, size, format, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&handle->image, &handle->memory);
		
		change_image_layout(video->handle, handle->image, format,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copy_buffer_to_image(video->handle, stage_buffer, handle->image, size);
		change_image_layout(video->handle, handle->image, format,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	
		vmaDestroyBuffer(video->handle->allocator, stage_buffer, stage_buffer_memory);

		VkImageViewCreateInfo view_info{};
		view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = format;
		view_info.image = handle->image;
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.levelCount = 1;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount = 1;

		/* TODO: use new_image_view for this instead. */
		if (vkCreateImageView(video->handle->device, &view_info, null, &handle->view) != VK_SUCCESS) {
			abort_with("Failed to create image view.");
		}

		/* Used to get the anisotropy level that the hardware supports. */
		VkPhysicalDeviceProperties pprops{};
		vkGetPhysicalDeviceProperties(video->handle->pdevice, &pprops);

		VkSamplerCreateInfo sampler_info{};
		sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_info.magFilter = VK_FILTER_LINEAR;
		sampler_info.minFilter = VK_FILTER_LINEAR;
		sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler_info.anisotropyEnable = VK_TRUE;
		sampler_info.maxAnisotropy = pprops.limits.maxSamplerAnisotropy;
		sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		sampler_info.unnormalizedCoordinates = VK_FALSE;
		sampler_info.compareEnable = VK_FALSE;
		sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
		sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

		if (vkCreateSampler(video->handle->device, &sampler_info, null, &handle->sampler) != VK_SUCCESS) {
			abort_with("Failed to create texture sampler.");
		}
	}

	Texture::~Texture() {
		vkDestroySampler(video->handle->device, handle->sampler, null);
		vkDestroyImageView(video->handle->device, handle->view, null);
		vmaDestroyImage(video->handle->allocator, handle->image, handle->memory);

		delete handle;
	}

	Texture* Texture::from_file(VideoContext* video, const char* file_path) {
		v2i size;
		i32 channels;
		void* data = stbi_load(file_path, &size.x, &size.y, &channels, 4);
		if (!data) {
			error("Failed to load `%s': %s.", file_path, stbi_failure_reason());
			return null;
		}

		Texture* r = new Texture(video, data, size, 4);
		
		stbi_image_free(data);
		
		return r;
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
