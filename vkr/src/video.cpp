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
	}

	VideoContext::~VideoContext() {
		vkDestroySwapchainKHR(handle->device, handle->swapchain, null);
		vkDestroyDevice(handle->device, null);
		vkDestroySurfaceKHR(handle->instance, handle->surface, null);
		vkDestroyInstance(handle->instance, null);

		delete[] handle->swapchain_images;

		delete handle;
	}
};
