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

			/* For a graphics device to be suitable, it must be a GPU and
			 * have a queue capable of executing graphical commands. */
			if (
					(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
					props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) &&
					device_supports_extensions(device) &&
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
	}

	VideoContext::~VideoContext() {
		vkDestroyDevice(handle->device, null);
		vkDestroySurfaceKHR(handle->instance, handle->surface, null);
		vkDestroyInstance(handle->instance, null);
		delete handle;
	}
};
