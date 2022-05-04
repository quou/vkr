#include <algorithm>
#include <optional>
#include <set>
#include <vector>

#include <string.h>

#include <vulkan/vulkan.h>

#include "vkr.hpp"
#include "internal.hpp"

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

	static VkShaderModule new_shader_module(VkDevice device, u8* code, usize code_size) {
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

	VideoContext::VideoContext(const App& app, const char* app_name, bool enable_validation_layers, u32 extension_count, const char** extensions) {
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

		/* Create a render pass.
		 *
		 * TODO: Abstract this separately. */
		VkAttachmentDescription color_attachment{};
		color_attachment.format = handle->swapchain_format;
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

		VkRenderPassCreateInfo render_pass_info{};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = 1;
		render_pass_info.pAttachments = &color_attachment;
		render_pass_info.subpassCount = 1;
		render_pass_info.pSubpasses = &subpass;

		if (vkCreateRenderPass(handle->device, &render_pass_info, null, &handle->render_pass)) {
			abort_with("Failed to create render pass.");
		}

		/* Create shaders and pipeline.
		 *
		 * TODO: Abstract this separately. */
		u8* v_buf; usize v_size;
		u8* f_buf; usize f_size;

		read_raw("res/shaders/simple.vert.spv", &v_buf, &v_size);
		read_raw("res/shaders/simple.frag.spv", &f_buf, &f_size);

		VkShaderModule v_shader = new_shader_module(handle->device, v_buf, v_size);
		VkShaderModule f_shader = new_shader_module(handle->device, f_buf, f_size);

		delete[] v_buf;
		delete[] f_buf;

		VkPipelineShaderStageCreateInfo v_stage_info{};
		v_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		v_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
		v_stage_info.module = v_shader;
		v_stage_info.pName = "main";

		VkPipelineShaderStageCreateInfo f_stage_info{};
		f_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		f_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		f_stage_info.module = f_shader;
		f_stage_info.pName = "main";

		VkPipelineShaderStageCreateInfo stages[] = { v_stage_info, f_stage_info };

		VkPipelineVertexInputStateCreateInfo vertex_input_info{};
		vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_info.vertexBindingDescriptionCount = 0;
		vertex_input_info.pVertexBindingDescriptions = null;
		vertex_input_info.vertexAttributeDescriptionCount = 0;
		vertex_input_info.pVertexAttributeDescriptions = null;

		VkPipelineInputAssemblyStateCreateInfo input_assembly{};
		input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		input_assembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width  = (f32)extent.width;
		viewport.height = (f32)extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = extent;

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

		VkPipelineLayoutCreateInfo pipeline_layout_info{};
		pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

		if (vkCreatePipelineLayout(handle->device, &pipeline_layout_info, null, &handle->pipeline_layout) != VK_SUCCESS) {
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
		pipeline_info.pDepthStencilState = null;
		pipeline_info.pColorBlendState = &color_blending;
		pipeline_info.pDynamicState = null;
		pipeline_info.layout = handle->pipeline_layout;
		pipeline_info.renderPass = handle->render_pass;
		pipeline_info.subpass = 0;

		if (vkCreateGraphicsPipelines(handle->device, VK_NULL_HANDLE, 1, &pipeline_info, null, &handle->pipeline) != VK_SUCCESS) {
			abort_with("Failed to create pipeline.");
		}

		vkDestroyShaderModule(handle->device, v_shader, null);
		vkDestroyShaderModule(handle->device, f_shader, null);

		/* Create framebuffers for the swapchain. */
		handle->swapchain_framebuffers = new VkFramebuffer[handle->swapchain_image_count];
		for (u32 i = 0; i < handle->swapchain_image_count; i++) {
			VkImageView attachments[] = {
				handle->swapchain_image_views[i]
			};

			VkFramebufferCreateInfo fb_info{};
			fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fb_info.renderPass = handle->render_pass;
			fb_info.attachmentCount = 1;
			fb_info.pAttachments = attachments;
			fb_info.width = extent.width;
			fb_info.height = extent.height;
			fb_info.layers = 1;

			if (vkCreateFramebuffer(handle->device, &fb_info, null, &handle->swapchain_framebuffers[i]) != VK_SUCCESS) {
				abort_with("Failed to create framebuffer.");
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

		/* Create the command buffer. */
		VkCommandBufferAllocateInfo cb_alloc_info{};
		cb_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cb_alloc_info.commandPool = handle->command_pool;
		cb_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cb_alloc_info.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(handle->device, &cb_alloc_info, &handle->command_buffer) != VK_SUCCESS) {
			abort_with("Failed to allocate command buffers.");
		}
	}

	VideoContext::~VideoContext() {
		vkDestroyCommandPool(handle->device, handle->command_pool, null);

		for (u32 i = 0; i < handle->swapchain_image_count; i++) {
			vkDestroyFramebuffer(handle->device, handle->swapchain_framebuffers[i], null);
		}

		vkDestroyPipeline(handle->device, handle->pipeline, null);
		vkDestroyPipelineLayout(handle->device, handle->pipeline_layout, null);
		vkDestroyRenderPass(handle->device, handle->render_pass, null);

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
};
