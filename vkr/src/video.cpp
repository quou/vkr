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

	static bool has_stencil_comp(VkFormat format) {
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	static void new_depth_resources(impl_VideoContext* handle, VkImage* image, VkImageView* view, VmaAllocation* memory) {
		VkFormat depth_format = find_depth_format(handle);

		new_image(handle, v2i((i32)handle->swapchain_extent.width, (i32)handle->swapchain_extent.height),
			depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
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

		/* Create the default framebuffer. */
		default_fb = new Framebuffer(this,
			Framebuffer::Flags::default_fb |
			Framebuffer::Flags::depth_test,
			v2i((i32)extent.width, (i32)extent.height));
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

	Pipeline::Pipeline(VideoContext* video, Flags flags, Shader* shader, usize stride,
			Attribute* attribs, usize attrib_count,
			Framebuffer* framebuffer,
			UniformBuffer* uniforms, usize uniform_count,
			SamplerBinding* sampler_bindings, usize sampler_binding_count,
			PushConstantRange* pcranges, usize pcrange_count) :
			video(video), uniform_count(uniform_count), sampler_binding_count(sampler_binding_count),
			flags(flags), framebuffer(framebuffer) {
		handle = new impl_Pipeline();

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
		viewport.width  = framebuffer->get_size().x;
		viewport.height = framebuffer->get_size().y;
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

		/* Set up uniform buffers and descriptor sets. */
		handle->uniforms = new impl_UniformBuffer[uniform_count];
		handle->sampler_bindings = new impl_SamplerBinding[sampler_binding_count];
		handle->temp_sets = new VkDescriptorSet[sampler_binding_count + 1];

		auto layout_bindings = new VkDescriptorSetLayoutBinding[uniform_count];
		for (usize i = 0; i < uniform_count; i++) {
			memset(layout_bindings + i, 0, sizeof(*layout_bindings));

			layout_bindings[i].binding = uniforms[i].binding;
			layout_bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			layout_bindings[i].descriptorCount = 1;
			layout_bindings[i].stageFlags = uniforms[i].stage == Stage::vertex ?
				VK_SHADER_STAGE_VERTEX_BIT :
				VK_SHADER_STAGE_FRAGMENT_BIT;
			layout_bindings[i].pImmutableSamplers = null;
		}

		auto s_layout_bindings = new VkDescriptorSetLayoutBinding[sampler_binding_count];
		u32 sampler_count = 0;
		for (usize i = 0; i < sampler_binding_count; i++)  {
			memset(s_layout_bindings + i, 0, sizeof(*s_layout_bindings));

			bool dup = false;
			for (u32 ii = 0; ii < sampler_count; ii++) {
				if (sampler_bindings[i].binding == s_layout_bindings[ii].binding) {
					dup = true;
					break;
				}
			}
			if (dup) { continue; }

			u32 idx = sampler_count++;
			s_layout_bindings[idx].binding = sampler_bindings[idx].binding;
			s_layout_bindings[idx].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			s_layout_bindings[idx].descriptorCount = 1;
			s_layout_bindings[idx].stageFlags = sampler_bindings[idx].stage == Stage::vertex ?
				VK_SHADER_STAGE_VERTEX_BIT :
				VK_SHADER_STAGE_FRAGMENT_BIT;
			s_layout_bindings[idx].pImmutableSamplers = null;
		}

		VkDescriptorSetLayoutCreateInfo layout_info{};
		layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layout_info.bindingCount = uniform_count;
		layout_info.pBindings = layout_bindings;

		if (vkCreateDescriptorSetLayout(video->handle->device, &layout_info, null, &handle->descriptor_set_layout) != VK_SUCCESS) {
			abort_with("Failed to create descriptor set layout.");
		}

		VkDescriptorSetLayoutCreateInfo s_layout_info{};
		s_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		s_layout_info.bindingCount = sampler_count;
		s_layout_info.pBindings = s_layout_bindings;

		if (vkCreateDescriptorSetLayout(video->handle->device, &s_layout_info, null, &handle->sampler_desc_set_layout) != VK_SUCCESS) {
			abort_with("Failed to create descriptor set layout.");
		}

		/* Create descriptor pool. */
		VkDescriptorPoolSize pool_size{};
		pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool_size.descriptorCount = max_frames_in_flight * uniform_count;

		VkDescriptorPoolSize pool_sizes[] = {
			{
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = max_frames_in_flight * (u32)uniform_count
			},
			{
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = max_frames_in_flight * (u32)sampler_binding_count
			}
		};

		VkDescriptorPoolCreateInfo pool_info{};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.poolSizeCount = 2;
		pool_info.pPoolSizes = pool_sizes;
		pool_info.maxSets = 
			(max_frames_in_flight * uniform_count) +
			(max_frames_in_flight * sampler_binding_count);

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

		for (u32 ii = 0; ii < max_frames_in_flight; ii++) {
			layouts[ii] = handle->sampler_desc_set_layout;
		}

		/* Create sampler descriptor sets. */
		for (usize i = 0; i < sampler_binding_count; i++) {
			VkDescriptorSetAllocateInfo set_alloc_info{};
			set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			set_alloc_info.descriptorPool = handle->descriptor_pool;
			set_alloc_info.descriptorSetCount = max_frames_in_flight;
			set_alloc_info.pSetLayouts = layouts;

			if (vkAllocateDescriptorSets(video->handle->device, &set_alloc_info, handle->sampler_bindings[i].descriptor_sets) != VK_SUCCESS) {
				abort_with("Failed to allocate descriptor sets.");
			}
		}

		for (usize i = 0; i < uniform_count; i++) {
			handle->uniforms[i].ptr = uniforms[i].ptr;
			handle->uniforms[i].size = uniforms[i].size;

			VkDeviceSize buffer_size = handle->uniforms[i].size;

			/* Create uniform buffers. */
			for (u32 ii = 0; ii < max_frames_in_flight; ii++) {
				new_buffer(video->handle, buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					VMA_ALLOCATION_CREATE_MAPPED_BIT,
					handle->uniforms[i].uniform_buffers + ii,
					handle->uniforms[i].uniform_buffer_memories + ii);
			}

			/* Write the descriptor sets */
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

		/* Write the sampler descriptor sets. */
		for (usize i = 0; i < sampler_binding_count; i++) {
			for (u32 ii = 0; ii < max_frames_in_flight; ii++) {
				VkDescriptorImageInfo image_info{};
				image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				image_info.imageView = sampler_bindings[i].texture->handle->view;
				image_info.sampler = sampler_bindings[i].texture->handle->sampler;

				VkWriteDescriptorSet desc_write{};
				desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				desc_write.dstSet = handle->sampler_bindings[i].descriptor_sets[ii];
				desc_write.dstBinding = sampler_bindings[i].binding;
				desc_write.dstArrayElement = 0;
				desc_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				desc_write.descriptorCount = 1;
				desc_write.pImageInfo = &image_info;

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

		VkDescriptorSetLayout set_layouts[] = {
			handle->descriptor_set_layout,
			handle->sampler_desc_set_layout
		};

		VkPipelineLayoutCreateInfo pipeline_layout_info{};
		pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_info.setLayoutCount = 2;
		pipeline_layout_info.pSetLayouts = set_layouts;
		pipeline_layout_info.pushConstantRangeCount = pcrange_count;
		pipeline_layout_info.pPushConstantRanges = pc_ranges;

		if (vkCreatePipelineLayout(video->handle->device, &pipeline_layout_info, null, &handle->pipeline_layout) != VK_SUCCESS) {
			abort_with("Failed to create pipeline layout.");
		}

		delete[] pc_ranges;
		delete[] layout_bindings;
		delete[] s_layout_bindings;

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
	}

	Pipeline::~Pipeline() {
		video->wait_for_done();

		for (u32 i = 0; i < uniform_count; i++) {
			for (u32 ii = 0; ii < max_frames_in_flight; ii++) {
				vmaDestroyBuffer(video->handle->allocator,
						handle->uniforms[i].uniform_buffers[ii],
						handle->uniforms[i].uniform_buffer_memories[ii]);
			}
		}

		vkDestroyDescriptorPool(video->handle->device, handle->descriptor_pool, null);
		vkDestroyDescriptorSetLayout(video->handle->device, handle->descriptor_set_layout, null);
		vkDestroyDescriptorSetLayout(video->handle->device, handle->sampler_desc_set_layout, null);

		delete[] handle->uniforms;
		delete[] handle->sampler_bindings;
		delete[] handle->temp_sets;

		vkDestroyPipeline(video->handle->device, handle->pipeline, null);
		vkDestroyPipelineLayout(video->handle->device, handle->pipeline_layout, null);

		delete handle;
	}

	void Pipeline::begin() {
		video->pipeline = this;

		/* Update the uniform buffers. */
		for (u32 i = 0; i < uniform_count; i++) {
			void* uniform_data;
			vmaMapMemory(video->handle->allocator, handle->uniforms[i].uniform_buffer_memories[video->current_frame],
				&uniform_data);
			memcpy(uniform_data, handle->uniforms[i].ptr, handle->uniforms[i].size);
			vmaUnmapMemory(video->handle->allocator, handle->uniforms[i].uniform_buffer_memories[video->current_frame]);
		}

		VkRenderPassBeginInfo render_pass_info{};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_info.renderPass = framebuffer->handle->render_pass;
		render_pass_info.framebuffer = framebuffer->handle->get_current_framebuffer(video->image_id, video->current_frame);
		render_pass_info.renderArea.offset = { 0, 0 };
		render_pass_info.renderArea.extent = VkExtent2D { (u32)framebuffer->get_size().x, (u32)framebuffer->get_size().y };

		VkClearValue clear_colors[2];
		clear_colors[0].color = {{ 0.01f, 0.01f, 0.01f, 1.0f }};
		clear_colors[1].depthStencil = { 1.0f, 0 };
		render_pass_info.clearValueCount = 2;
		render_pass_info.pClearValues = clear_colors;

		vkCmdBeginRenderPass(video->handle->command_buffers[video->current_frame], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(video->handle->command_buffers[video->current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, handle->pipeline);

		vkCmdBindDescriptorSets(video->handle->command_buffers[video->current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS,
			handle->pipeline_layout, 0, 1,
			&handle->descriptor_sets[video->current_frame], 0, null);
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

	void Pipeline::bind_samplers(u32* indices, usize index_count) {
		for (usize i = 0; i < index_count; i++) {
			handle->temp_sets[i] = handle->sampler_bindings[indices[i]].descriptor_sets[video->current_frame];
		}

		vkCmdBindDescriptorSets(video->handle->command_buffers[video->current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS,
			handle->pipeline_layout, 1, index_count,
			handle->temp_sets, 0, null);
	}

	Framebuffer::Framebuffer(VideoContext* video, Flags flags, v2i size) : video(video), flags(flags), size(size) {
		handle = new impl_Framebuffer();

		handle->is_headless = flags & Flags::headless;

		bool use_depth = flags & Flags::depth_test;

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
		depth_attachment_ref.attachment = 1;
		depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

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

		if (use_depth) {
			subpass.pDepthStencilAttachment = &depth_attachment_ref;
		}

		VkSubpassDependency dep{};
		dep.srcSubpass = VK_SUBPASS_EXTERNAL;
		dep.dstSubpass = 0;
		dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dep.srcAccessMask = 0;
		dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		u32 attachment_count = 0;

		VkAttachmentDescription attachments[max_attachments];
		attachments[attachment_count++] = color_attachment;

		if (use_depth) {
			attachments[attachment_count++] = depth_attachment;
		}

		VkRenderPassCreateInfo render_pass_info{};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = attachment_count;
		render_pass_info.pAttachments = attachments;
		render_pass_info.subpassCount = 1;
		render_pass_info.pSubpasses = &subpass;
		render_pass_info.dependencyCount = 1;
		render_pass_info.pDependencies = &dep;

		if (vkCreateRenderPass(video->handle->device, &render_pass_info, null, &handle->render_pass)) {
			abort_with("Failed to create render pass.");
		}

		VkImageView image_attachments[max_attachments];

		/* Create the depth buffer */
		if (use_depth) {
			new_depth_resources(video->handle, &handle->depth_image, &handle->depth_image_view, &handle->depth_memory);
			image_attachments[1] = handle->depth_image_view;
		}

		if (flags & Flags::default_fb) {
			/* For the swapchain.. */
			handle->swapchain_framebuffers = new VkFramebuffer[video->handle->swapchain_image_count];
			handle->framebuffers = handle->swapchain_framebuffers;
			for (u32 i = 0; i < video->handle->swapchain_image_count; i++) {
				image_attachments[0] = video->handle->swapchain_image_views[i];

				VkFramebufferCreateInfo fb_info{};
				fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				fb_info.renderPass = handle->render_pass;
				fb_info.attachmentCount = attachment_count;
				fb_info.pAttachments = image_attachments;
				fb_info.width =  size.x;
				fb_info.height = size.y;
				fb_info.layers = 1;

				if (vkCreateFramebuffer(video->handle->device, &fb_info, null, handle->framebuffers + i) != VK_SUCCESS) {
					abort_with("Failed to create framebuffer.");
				}
			}
		} else {
			/* Create images and image views for off-screen rendering. */
			handle->framebuffers = handle->offscreen_framebuffers;
			for (u32 i = 0; i < max_frames_in_flight; i++) {
				/* TODO: Read this from the flags. */
				auto format = VK_FORMAT_R8G8B8_SRGB;

				new_image(video->handle, size, format, VK_IMAGE_TILING_OPTIMAL,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					handle->images + i, handle->image_memories + i);
				new_image_view(video->handle, handle->images[i], format, VK_IMAGE_ASPECT_COLOR_BIT);

				image_attachments[0] = handle->image_views[i];

				VkFramebufferCreateInfo fb_info{};
				fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				fb_info.renderPass = handle->render_pass;
				fb_info.attachmentCount = attachment_count;
				fb_info.pAttachments = image_attachments;
				fb_info.width =  size.x;
				fb_info.height = size.y;
				fb_info.layers = 1;

				if (vkCreateFramebuffer(video->handle->device, &fb_info, null, handle->framebuffers + i) != VK_SUCCESS) {
					abort_with("Failed to create framebuffer.");
				}
			}
		}
	}

	Framebuffer::~Framebuffer() {
		if (flags & Flags::default_fb) {
			for (u32 i = 0; i < video->handle->swapchain_image_count; i++) {
				vkDestroyFramebuffer(video->handle->device, handle->swapchain_framebuffers[i], null);
			}
		}

		if (flags & Flags::depth_test) {
			vkDestroyImageView(video->handle->device, handle->depth_image_view, null);
			vmaDestroyImage(video->handle->allocator, handle->depth_image, handle->depth_memory);
		}

		vkDestroyRenderPass(video->handle->device, handle->render_pass, null);

		delete[] handle->swapchain_framebuffers;

		delete handle;
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
		VkBuffer vbs[] = { handle->buffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(video->handle->command_buffers[video->current_frame], 0, 1, vbs, offsets);
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
		vkCmdBindIndexBuffer(video->handle->command_buffers[video->current_frame], handle->buffer, 0, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(video->handle->command_buffers[video->current_frame], count, 1, 0, 0, 0);

		video->object_count++;
	}

	Texture::Texture(VideoContext* video, void* data, v2i size, u32 component_count) :
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

		auto format = VK_FORMAT_R8G8B8A8_SRGB;

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
		view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
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
