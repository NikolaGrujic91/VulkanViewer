#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <fstream>
#include <algorithm>
#include <vector>
#include <cstring>
#include <set>
#include <array>
#include <glm/glm.hpp>

const int WIDTH = 800;
const int HEIGHT = 600;

struct QueueFamilyIndices {
	int graphicsFamily = -1;
	int presentFamily = -1;

	bool isComplete() {
		return graphicsFamily >= 0 && presentFamily >= 0;
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 2> attributeDesriptions = {};
		attributeDesriptions[0].binding = 0;
		attributeDesriptions[0].location = 0;
		attributeDesriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDesriptions[0].offset = offsetof(Vertex, pos);
		attributeDesriptions[1].binding = 0;
		attributeDesriptions[1].location = 1;
		attributeDesriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDesriptions[1].offset = offsetof(Vertex, color);
		return attributeDesriptions;
	}
};

//List of validation layers to enable
const std::vector<const char*> validationLayers = { "VK_LAYER_LUNARG_standard_validation" };
//List of swapchain device extensions to enable
const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
//Array of vertex data for vertex shader
const std::vector<Vertex> vertices = {
	{{0.0f, -0.5},{1.0f, 0.0f, 0.0f}},
	{{0.5f, 0.5f},{0.0f, 1.0f, 0.0f}},
	{{-0.5f, 0.5f},{0.0f, 0.0f, 1.0f}}
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if (func != nullptr)
		return func(instance, pCreateInfo, pAllocator, pCallback);
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if (func != nullptr)
		func(instance, callback, pAllocator);
}

static std::vector<char> readShaderFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
		throw std::runtime_error("failed to open shader file!");

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

#pragma region Resource Management

template <typename T>
class VDeleter {
public:
	VDeleter() : VDeleter([](T, VkAllocationCallbacks*) {}) {}

#pragma region Non-default constructors

	/// <summary>
	/// vkDestroyXXX(object, callbacks):Only the object itself need to be passed to the cleanup function,
	/// so we can simply construct a VDeleter with just the function as argument.
	/// </summary>
	/// <param name="T">object</param>
	/// <param name="VkAllocationCallbacks">callbacks</param>
	VDeleter(std::function<void(T, VkAllocationCallbacks*)> deletef) {
		this->deleter = [=](T obj) { deletef(obj, nullptr); };
	}

	/// <summary>
	/// vkDestroyXXX(instance, object, callbacks):A VkInstance also needs to be passed to the cleanup function,
	/// so we use the VDeleter constructor that takes the VkInstance reference and cleanup function as parameters.
	/// </summary>
	/// <param name="VkInstance">instance</param>
	/// <param name="T">object</param>
	/// <param name="VkAllocationCallbacks">callbacks</param>
	VDeleter(const VDeleter<VkInstance>& instance, std::function<void(VkInstance, T, VkAllocationCallbacks*)> deletef) {
		this->deleter = [&instance, deletef](T obj) { deletef(instance, obj, nullptr); };
	}

	/// <summary>
	/// vkDestroyXXX(device, object, callbacks):A VkDevice also needs to be passed to the cleanup function,
	/// so we use the VDeleter constructor that takes the VkDevice reference and cleanup function as parameters.
	/// </summary>
	/// <param name="VkDevice">device</param>
	/// <param name="T">object</param>
	/// <param name="VkAllocationCallbacks">callbacks</param>
	VDeleter(const VDeleter<VkDevice>& device, std::function<void(VkDevice, T, VkAllocationCallbacks*)> deletef) {
		this->deleter = [&device, deletef](T obj) { deletef(device, obj, nullptr); };
	}

#pragma endregion

	~VDeleter() { cleanup(); }

	const T* operator &() const { return &object; }

	T* replace() {
		cleanup();
		return &object;
	}

	operator T() const { return object; }

	void operator=(T rhs) {
		if (rhs != object) {
			cleanup();
			object = rhs;
		}
	}

	template<typename V>
	bool operator==(V rhs) { return object == T(rhs); }

private:
	T object{ VK_NULL_HANDLE };
	std::function<void(T)> deleter;

	void cleanup() {
		if (object != VK_NULL_HANDLE)
			deleter(object);

		object = VK_NULL_HANDLE;
	}
};

#pragma endregion

class VulkanInitializer {
public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
	}

private:
	GLFWwindow* window;

	VDeleter<VkInstance> instance{ vkDestroyInstance };
	VDeleter<VkDebugReportCallbackEXT> callback{ instance, DestroyDebugReportCallbackEXT };
	VDeleter<VkSurfaceKHR> surface{ instance, vkDestroySurfaceKHR };

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VDeleter<VkDevice> device{ vkDestroyDevice };

	VkQueue graphicsQueue;
	VkQueue presentQueue;

	VDeleter<VkSwapchainKHR> swapChain{ device, vkDestroySwapchainKHR };
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	std::vector<VDeleter<VkImageView>> swapChainImageViews;
	std::vector<VDeleter<VkFramebuffer>> swapChainFramebuffers;

	VDeleter<VkRenderPass> renderPass{ device, vkDestroyRenderPass };
	VDeleter<VkPipelineLayout> pipelineLayout{ device, vkDestroyPipelineLayout };
	VDeleter<VkPipeline> graphicsPipeline{ device, vkDestroyPipeline };

	VDeleter<VkCommandPool> commandPool{ device, vkDestroyCommandPool };
	std::vector<VkCommandBuffer> commandBuffers;

	VDeleter<VkSemaphore> imageAvailableSemaphore{ device, vkDestroySemaphore };//signal that an image has been acquired and is ready for rendering
	VDeleter<VkSemaphore> renderFinishedSemaphore{ device, vkDestroySemaphore };//signal that rendering has finished and presentation can happen

	VDeleter<VkBuffer> vertexBuffer{ device, vkDestroyBuffer };

	///<summary>Initialize GLFW and create window</summary>
	void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}

#pragma region Init Vulkan

	void initVulkan() {
		createInstance();
		setupDebugCallback();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createVertexBuffer();
		createCommandBuffers();
		createSemaphores();
	}

#pragma region Instance and Validation Layers

	void createInstance() {
		//Check validation layers support
		if (enableValidationLayers && !checkValidationLayerSupport())
			throw std::runtime_error("validation layers requested, but not available!");

		//Optional struct: provide some info for driver about graphics engine being used
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		//Mandatory struct: tells Vulkan driver which global extensions and validation layers to use
		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.data();

		if (enableValidationLayers) {
			createInfo.enabledLayerCount = validationLayers.size();
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
			createInfo.enabledLayerCount = 0;

		//Check if instance of Vulkan library was created successfully
		if (vkCreateInstance(&createInfo, nullptr, instance.replace()) != VK_SUCCESS)
			throw std::runtime_error("failed to create instance!");
	}

	bool checkValidationLayerSupport() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers)
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}

			if (!layerFound)
				return false;
		}

		return true;
	}

	void checkForExtensionSupport() {
		uint32_t extensionsCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, nullptr);

		std::vector<VkExtensionProperties> extensions(extensionsCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, extensions.data());

		std::cout << "Available extensions:" << std::endl;
		for (const auto& extension : extensions)
			std::cout << "\t" << extension.extensionName << "\t" << extension.specVersion << std::endl;
	}

#pragma endregion

#pragma region Debug Message callback

	///<summary> function that will return the required list of extensions based on whether validation layers are enabled or not</summary>
	std::vector<const char*> getRequiredExtensions() {
		std::vector<const char*> extensions;
		unsigned int glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		for (unsigned int i = 0; i < glfwExtensionCount; i++)
			extensions.push_back(glfwExtensions[i]);

		if (enableValidationLayers)
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

		return extensions;
	}

	///<summary>Debug callback function</summary>
	///<param name="flags">specifies the type of message</param>
	///<param name="objType">specifies the type of object that is the subject of the message</param>
	///<param name="masg">contains the pointer to the message itself.</param>
	///<param name="userData"> pass your own data to the callback</param>
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData) {
		std::cerr << "validation layer: " << msg << std::endl;
		return VK_FALSE;
	}

	void setupDebugCallback() {
		if (!enableValidationLayers) return;

		VkDebugReportCallbackCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;// filter which types of messages you would like to receive
		createInfo.pfnCallback = debugCallback;//assign pointer to the callback function

		if (CreateDebugReportCallbackEXT(instance, &createInfo, nullptr, callback.replace()) != VK_SUCCESS)
			throw std::runtime_error("failed to set up debug callback!");
	}

#pragma endregion

#pragma region Physical device

	void pickPhysicalDevice() {
		//Get number of GPUs with Vulkan support
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

		//Check if there are 0 GPUs with Vulkan support
		if (deviceCount == 0)
			throw std::runtime_error("failed to find GPUs with Vulkan support!");

		//Hold all GPUs with Vulkan support
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		//Check if any of the GPUs meet the requirements
		for (const auto& device : devices) {
			if (isDeviceSuitable(device)) {
				physicalDevice = device;
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE)
			throw std::runtime_error("failed to find a suitable GPU!");
	}

	///<summary>Function that evaluate GPU</summary>
	bool isDeviceSuitable(VkPhysicalDevice device) {
		QueueFamilyIndices indices = findQueueFamilies(device);

		bool extensionsSupported = checkDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indices.isComplete() && extensionsSupported && swapChainAdequate;
	}

	bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
#pragma region Enumerate device extensions
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::cout << "Available device extensions:" << std::endl;
		for (const auto& extension : availableExtensions)
			std::cout << "\t" << extension.extensionName << "\t" << extension.specVersion << std::endl;
#pragma endregion

#pragma region Check if device have all of the required extensions
		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions)
			requiredExtensions.erase(extension.extensionName);

		return requiredExtensions.empty();
#pragma endregion
	}

#pragma endregion

#pragma region Queue Families

	///<summary>Return indices of queue families that satisfy certain desired properties</summary>
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			//if VK_QUEUE_GRAPHICS_BIT is set, then the queues in this queue family support graphics operations.
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				indices.graphicsFamily = i;

			// look for a queue family that has the capability of presenting to our window surface
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

			if (queueFamily.queueCount > 0 && presentSupport)
				indices.presentFamily = i;

			if (indices.isComplete())
				break;

			i++;
		}

		return indices;
	}

#pragma endregion

#pragma region Logical device

	void createLogicalDevice() {
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<int> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };

		float queuePriority = 1.0f;
		for (int queueFamily : uniqueQueueFamilies) {
			//This structure describes the number of queues we want for a single queue family
			VkDeviceQueueCreateInfo queueCreateInfo = {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		//This structure specify the set of device features to be used
		VkPhysicalDeviceFeatures deviceFeatures = {};

		//Creating the logical device
		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();

		createInfo.pEnabledFeatures = &deviceFeatures;

		createInfo.enabledExtensionCount = deviceExtensions.size();
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();

		if (enableValidationLayers) {
			createInfo.enabledLayerCount = validationLayers.size();
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
			createInfo.enabledLayerCount = 0;

		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, device.replace()) != VK_SUCCESS)
			throw std::runtime_error("failed to create logical device!");

		//Retrieve queue handles
		vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
		vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
	}

#pragma endregion

#pragma region Window Surface

	void createSurface() {
		if (glfwCreateWindowSurface(instance, window, nullptr, surface.replace()) != VK_SUCCESS)
			throw std::runtime_error("failed to create window surface!");
	}

#pragma endregion

#pragma region Swap Chain

	///<summary>Querying details of swap chain support</summary>
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
		std::cout << "Available swap chain capabilities:" << std::endl;
		std::cout << "\t current Extent:\t" << &details.capabilities.currentExtent.height << " " << &details.capabilities.currentExtent.width << std::endl;
		std::cout << "\t current Transform:\t" << &details.capabilities.currentTransform << std::endl;
		std::cout << "\t max Image Array Layers:\t" << &details.capabilities.maxImageArrayLayers << std::endl;
		std::cout << "\t max Image Count:\t" << &details.capabilities.maxImageCount << std::endl;
		std::cout << "\t max Image Extent:\t" << &details.capabilities.maxImageExtent.height << " " << &details.capabilities.maxImageExtent.width << std::endl;
		std::cout << "\t min Image Count:\t" << &details.capabilities.minImageCount << std::endl;
		std::cout << "\t min Image Extent:\t" << &details.capabilities.minImageExtent.height << " " << &details.capabilities.minImageExtent.width << std::endl;
		std::cout << "\t supported Composite Alpha:\t" << &details.capabilities.supportedCompositeAlpha << std::endl;
		std::cout << "\t supported Transforms:\t" << &details.capabilities.supportedTransforms << std::endl;
		std::cout << "\t supported Usage Flags:\t" << &details.capabilities.supportedUsageFlags << std::endl;

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		std::cout << "Available swap chain formats:" << std::endl;
		for (const auto& format : details.formats)
			std::cout << "\t" << format.format << "\t" << format.colorSpace << std::endl;

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		std::cout << "Available swap chain present modes:" << std::endl;
		for (const auto& presentMode : details.presentModes)
			std::cout << "\t" << presentMode << std::endl;

		return details;
	}

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
		//If surface has no prefered format, set desired format
		if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
			return{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

		//Else check if desired format combination is available
		for (const auto& availableFormat : availableFormats)
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return availableFormat;

		//if desired format combination is not available, return first format specified
		return availableFormats[0];
	}

	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
		for (const auto& availablePresentMode : availablePresentModes)
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
				return availablePresentMode;

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
			return capabilities.currentExtent;
		else {
			VkExtent2D actualExtent = { WIDTH, HEIGHT };

			actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

			return actualExtent;
		}
	}

	void createSwapChain() {
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		//Set queue length
		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
			imageCount = swapChainSupport.capabilities.maxImageCount;

		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;//specifying which surface the swap chain should be tied to

									 //details of the swap chain images
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		//specify how to handle swap chain images that will be used across multiple queue families
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentFamily };

		if (indices.graphicsFamily != indices.presentFamily) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

		//specify that a certain transform should be applied to images in the swap chain
		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		//specifies if the alpha channel should be used for blending with other windows in the window system
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;

		createInfo.oldSwapchain = VK_NULL_HANDLE;

		// creating the swap chain
		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, swapChain.replace()) != VK_SUCCESS)
			throw std::runtime_error("failed to create swap chain!");

		// Retrieving the swap chain images
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;
	}

#pragma endregion

#pragma region Image Views

	void createImageViews() {
		swapChainImageViews.resize(swapChainImages.size(), VDeleter<VkImageView>{device, vkDestroyImageView});

		for (uint32_t i = 0; i < swapChainImages.size(); i++) {
			VkImageViewCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = swapChainImageFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(device, &createInfo, nullptr, swapChainImageViews[i].replace()) != VK_SUCCESS)
				throw std::runtime_error("failed to create image views!");
		}
	}

#pragma endregion

#pragma region Render Passes

	void createRenderPass() {
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(device, &renderPassInfo, nullptr, renderPass.replace()) != VK_SUCCESS)
			throw std::runtime_error("failed to create render pass!");
	}

#pragma endregion

#pragma region  Graphics Pipeline

	void createGraphicsPipeline() {
		auto vertShaderCode = readShaderFile("vert.spv");
		auto fragShaderCode = readShaderFile("frag.spv");

		VDeleter<VkShaderModule> vertShaderModule{ device, vkDestroyShaderModule };
		VDeleter<VkShaderModule> fragShaderModule{ device, vkDestroyShaderModule };
		createShaderModule(vertShaderCode, vertShaderModule);
		createShaderModule(fragShaderCode, fragShaderModule);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapChainExtent.width;
		viewport.height = (float)swapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtent;

		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;

		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pushConstantRangeCount = 0;

		if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, pipelineLayout.replace()) != VK_SUCCESS)
			throw std::runtime_error("failed to create pipeline layout!");

		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, graphicsPipeline.replace()) != VK_SUCCESS)
			throw std::runtime_error("failed to create graphics pipeline!");
	}

	void createShaderModule(const std::vector<char>& code, VDeleter<VkShaderModule>& shaderModule) {
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = (uint32_t*)code.data();

		if (vkCreateShaderModule(device, &createInfo, nullptr, shaderModule.replace()) != VK_SUCCESS)
			throw std::runtime_error("failed to create shader module!");
	}

#pragma endregion

#pragma region Framebuffers

	void createFramebuffers() {
		swapChainFramebuffers.resize(swapChainImageViews.size(), VDeleter<VkFramebuffer>{device, vkDestroyFramebuffer});

		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			VkImageView attachments[] = { swapChainImageViews[i] };

			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = swapChainExtent.width;
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, swapChainFramebuffers[i].replace()) != VK_SUCCESS)
				throw std::runtime_error("failed to create framebuffer!");
		}
	}

#pragma endregion

#pragma region Command pools

	void createCommandPool() {
		QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

		if (vkCreateCommandPool(device, &poolInfo, nullptr, commandPool.replace()) != VK_SUCCESS)
			throw std::runtime_error("failed to create command pool!");
	}

#pragma endregion

#pragma region Vertex buffer

	void createVertexBuffer() {
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = sizeof(vertices[0]) * vertices.size();
		bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(device, &bufferInfo, nullptr, vertexBuffer.replace()) != VK_SUCCESS)
			throw std::runtime_error("failed to create vertex buffer!");

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);
	}

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
			if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
				return i;

		throw std::runtime_error("failed to find suitable memory type!");
	}

#pragma endregion

#pragma region Command buffers

	void createCommandBuffers() {
		commandBuffers.resize(swapChainFramebuffers.size());

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

		if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate command buffers!");

		for (size_t i = 0; i < commandBuffers.size(); i++) {
			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

			vkBeginCommandBuffer(commandBuffers[i], &beginInfo);

			VkRenderPassBeginInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = renderPass;
			renderPassInfo.framebuffer = swapChainFramebuffers[i];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = swapChainExtent;

			VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;

			vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

			vkCmdEndRenderPass(commandBuffers[i]);

			if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
				throw std::runtime_error("failed to record command buffer!");
		}
	}

#pragma endregion

#pragma region Semaphores

	void createSemaphores() {
		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, imageAvailableSemaphore.replace()) != VK_SUCCESS || vkCreateSemaphore(device, &semaphoreInfo, nullptr, renderFinishedSemaphore.replace()) != VK_SUCCESS)
			throw std::runtime_error("failed to create semaphores!");
	}

#pragma endregion

#pragma endregion

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			drawFrame();
		}

		vkDeviceWaitIdle(device);

		glfwDestroyWindow(window);
	}

	void drawFrame() {
		uint32_t imageIndex;
		vkAcquireNextImageKHR(device, swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

		VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = &imageIndex;

		vkQueuePresentKHR(presentQueue, &presentInfo);
	}
};

int main() {
	VulkanInitializer app;

	try {
		app.run();
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}