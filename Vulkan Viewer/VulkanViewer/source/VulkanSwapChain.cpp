/*
* Learning Vulkan - ISBN: 9781786469809
*
* Author: Parminder Singh, parminder.vulkan@gmail.com
* Linkedin: https://www.linkedin.com/in/parmindersingh18
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#include "VulkanSwapChain.h"

#include "VulkanDevice.h"
#include "VulkanInstance.h"
#include "VulkanRenderer.h"
#include "VulkanApplication.h"

#define INSTANCE_FUNC_PTR(instance, entrypoint){											\
    fp##entrypoint = (PFN_vk##entrypoint) vkGetInstanceProcAddr(instance, "vk"#entrypoint); \
    if (fp##entrypoint == NULL) {															\
        std::cout << "Unable to locate the vkGetDeviceProcAddr: vk"#entrypoint;				\
        exit(-1);																			\
    }																						\
}

#define DEVICE_FUNC_PTR(dev, entrypoint){													\
    fp##entrypoint = (PFN_vk##entrypoint) vkGetDeviceProcAddr(dev, "vk"#entrypoint);		\
    if (fp##entrypoint == NULL) {															\
        std::cout << "Unable to locate the vkGetDeviceProcAddr: vk"#entrypoint;				\
        exit(-1);																			\
    }																						\
}

VulkanSwapChain::VulkanSwapChain(VulkanRenderer* renderer)
{
	_rendererObj = renderer;
	_appObj		= VulkanApplication::GetInstance();
	_scPublicVars._swapChain = VK_NULL_HANDLE;
}

VulkanSwapChain::~VulkanSwapChain()
{
	_scPrivateVars._swapchainImages.clear();
	_scPrivateVars._surfFormats.clear();
	_scPrivateVars._presentModes.clear();
}

VkResult VulkanSwapChain::CreateSwapChainExtensions()
{
	// Dependency on createPresentationWindow()
	VkInstance& instance	= _appObj->_instanceObj._instance;
	VkDevice& device		= _appObj->_deviceObj->_device;

	// Get Instance based swap chain extension function pointer
	INSTANCE_FUNC_PTR(instance, GetPhysicalDeviceSurfaceSupportKHR);
	INSTANCE_FUNC_PTR(instance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
	INSTANCE_FUNC_PTR(instance, GetPhysicalDeviceSurfaceFormatsKHR);
	INSTANCE_FUNC_PTR(instance, GetPhysicalDeviceSurfacePresentModesKHR);
	INSTANCE_FUNC_PTR(instance, DestroySurfaceKHR);

	// Get Device based swap chain extension function pointer
	DEVICE_FUNC_PTR(device, CreateSwapchainKHR);
	DEVICE_FUNC_PTR(device, DestroySwapchainKHR);
	DEVICE_FUNC_PTR(device, GetSwapchainImagesKHR);
	DEVICE_FUNC_PTR(device, AcquireNextImageKHR);
	DEVICE_FUNC_PTR(device, QueuePresentKHR);

	return VK_SUCCESS;
}

VkResult VulkanSwapChain::CreateSurface()
{
	VkResult  result;
	// Depends on createPresentationWindow(), need an empty window handle
	VkInstance& instance = _appObj->_instanceObj._instance;

	// Construct the surface description:
#ifdef _WIN32
	VkWin32SurfaceCreateInfoKHR createInfo = {};
	createInfo.sType		= VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.pNext		= nullptr;
	createInfo.hinstance	= _rendererObj->_connection;
	createInfo.hwnd			= _rendererObj->_window;

	result = vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &_scPublicVars._surface);

#else  // _WIN32

	VkXcbSurfaceCreateInfoKHR createInfo = {};
	createInfo.sType		= VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	createInfo.pNext		= NULL;
	createInfo.connection	= rendererObj->connection;
	createInfo.window		= rendererObj->window;

	result = vkCreateXcbSurfaceKHR(instance, &createInfo, NULL, &surface);
#endif // _WIN32
	
	assert(result == VK_SUCCESS);
	return result;
}

uint32_t VulkanSwapChain::GetGraphicsQueueWithPresentationSupport()
{
	VulkanDevice* device	      = _appObj->_deviceObj;
    const uint32_t queueCount     = device->_queueFamilyCount;
    VkPhysicalDevice gpu	      = *device->_gpu;
	std::vector<VkQueueFamilyProperties>& queueProps = device->_queueFamilyProps;

	// Iterate over each queue and get presentation status for each.
    auto* supportsPresent = static_cast<VkBool32 *>(malloc(queueCount * sizeof(VkBool32)));
	for (uint32_t i = 0; i < queueCount; i++) 
    {
		fpGetPhysicalDeviceSurfaceSupportKHR(gpu, i, _scPublicVars._surface, &supportsPresent[i]);
	}

	 // Search for a graphics queue and a present queue in the array of queue
	 // families, try to find one that supports both
	uint32_t graphicsQueueNodeIndex = UINT32_MAX;
	uint32_t presentQueueNodeIndex = UINT32_MAX;
	for (uint32_t i = 0; i < queueCount; i++) 
    {
		if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
        {
			if (graphicsQueueNodeIndex == UINT32_MAX)
            {
				graphicsQueueNodeIndex = i;
			}

			if (supportsPresent[i] == VK_TRUE)
            {
				graphicsQueueNodeIndex = i;
				presentQueueNodeIndex = i;
				break;
			}
		}
	}
	
	if (presentQueueNodeIndex == UINT32_MAX)
    {
		// If didn't find a queue that supports both graphics and present, then
		// find a separate present queue.
		for (uint32_t i = 0; i < queueCount; ++i) 
        {
			if (supportsPresent[i] == VK_TRUE) 
            {
				presentQueueNodeIndex = i;
				break;
			}
		}
	}

	free(supportsPresent);

	// Generate error if could not find both a graphics and a present queue
	if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX)
    {
		return  UINT32_MAX;
	}

	return graphicsQueueNodeIndex;
}

void VulkanSwapChain::GetSupportedFormats()
{
    VkPhysicalDevice gpu = *_rendererObj->GetDevice()->_gpu;

    // Get the list of VkFormats that are supported:
	uint32_t formatCount;
	VkResult result = fpGetPhysicalDeviceSurfaceFormatsKHR(gpu, _scPublicVars._surface, &formatCount, nullptr);
	assert(result == VK_SUCCESS);
	_scPrivateVars._surfFormats.clear();
	_scPrivateVars._surfFormats.resize(formatCount);

	// Get VkFormats in allocated objects
	result = fpGetPhysicalDeviceSurfaceFormatsKHR(gpu, _scPublicVars._surface, &formatCount, &_scPrivateVars._surfFormats[0]);
	assert(result == VK_SUCCESS);

	// In case it’s a VK_FORMAT_UNDEFINED, then surface has no 
	// preferred format. We use BGRA 32 bit format
	if (formatCount == 1 && _scPrivateVars._surfFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		_scPublicVars._format = VK_FORMAT_B8G8R8A8_UNORM;
	}
	else
	{
		assert(formatCount >= 1);
		_scPublicVars._format = _scPrivateVars._surfFormats[0].format;
	}
}

void VulkanSwapChain::IntializeSwapChain()
{
	// Querying swapchain extensions
	CreateSwapChainExtensions();

	// Create surface and associate with Windowing
	CreateSurface();

	// Getting a graphics queue with presentation support
    const uint32_t index = GetGraphicsQueueWithPresentationSupport();
	if (index == UINT32_MAX)
	{
		std::cout << "Could not find a graphics and a present queue\nCould not find a graphics and a present queue\n";
		exit(-1);
	}
	_rendererObj->GetDevice()->_graphicsQueueWithPresentIndex = index;

	// Get the list of formats that are supported
	GetSupportedFormats();
}

void VulkanSwapChain::CreateSwapChain(const VkCommandBuffer& cmd)
{
	// use extensions and get the surface capabilities, present mode
	GetSurfaceCapabilitiesAndPresentMode();

	// Gather necessary information for present mode.
	ManagePresentMode();

	// Create the swap chain images
	CreateSwapChainColorImages();

	// Get the create color image drawing surfaces
	CreateColorImageView(cmd);
}

void VulkanSwapChain::GetSurfaceCapabilitiesAndPresentMode()
{
    VkPhysicalDevice gpu = *_appObj->_deviceObj->_gpu;
	VkResult result = fpGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, _scPublicVars._surface, &_scPrivateVars._surfCapabilities);
	assert(result == VK_SUCCESS);

	result = fpGetPhysicalDeviceSurfacePresentModesKHR(gpu, _scPublicVars._surface, &_scPrivateVars._presentModeCount, nullptr);
	assert(result == VK_SUCCESS);

	_scPrivateVars._presentModes.clear();
	_scPrivateVars._presentModes.resize(_scPrivateVars._presentModeCount);
	assert(!_scPrivateVars._presentModes.empty());

	result = fpGetPhysicalDeviceSurfacePresentModesKHR(gpu, _scPublicVars._surface, &_scPrivateVars._presentModeCount, &_scPrivateVars._presentModes[0]);
	assert(result == VK_SUCCESS);

	if (_scPrivateVars._surfCapabilities.currentExtent.width == static_cast<uint32_t>(-1))
	{
		// If the surface width and height is not defined, the set the equal to image size.
		_scPrivateVars._swapChainExtent.width = _rendererObj->_width;
		_scPrivateVars._swapChainExtent.height = _rendererObj->_height;
	}
	else
	{
		// If the surface size is defined, then it must match the swap chain size
		_scPrivateVars._swapChainExtent = _scPrivateVars._surfCapabilities.currentExtent;
	}
}

void VulkanSwapChain::ManagePresentMode()
{
	// If mailbox mode is available, use it, as is the lowest-latency non-
	// tearing mode.  If not, try IMMEDIATE which will usually be available,
	// and is fastest (though it tears).  If not, fall back to FIFO which is
	// always available.
	_scPrivateVars._swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (size_t i = 0; i < _scPrivateVars._presentModeCount; i++) 
    {
		if (_scPrivateVars._presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
			_scPrivateVars._swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
		if ((_scPrivateVars._swapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) &&
			(_scPrivateVars._presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)) 
        {
			_scPrivateVars._swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		}
	}

	// Determine the number of VkImage's to use in the swap chain (we desire to
	// own only 1 image at a time, besides the images being displayed and
	// queued for display):
	_scPrivateVars._desiredNumberOfSwapChainImages = _scPrivateVars._surfCapabilities.minImageCount + 1;
	if ((_scPrivateVars._surfCapabilities.maxImageCount > 0) &&
		(_scPrivateVars._desiredNumberOfSwapChainImages > _scPrivateVars._surfCapabilities.maxImageCount))
	{
		// Application must settle for fewer images than desired:
		_scPrivateVars._desiredNumberOfSwapChainImages = _scPrivateVars._surfCapabilities.maxImageCount;
	}

	if (_scPrivateVars._surfCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) 
    {
		_scPrivateVars._preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else 
    {
		_scPrivateVars._preTransform = _scPrivateVars._surfCapabilities.currentTransform;
	}
}

void VulkanSwapChain::CreateSwapChainColorImages()
{
    const VkSwapchainKHR oldSwapchain = _scPublicVars._swapChain;

	VkSwapchainCreateInfoKHR swapChainInfo = {};
	swapChainInfo.sType					= VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainInfo.pNext					= nullptr;
	swapChainInfo.surface				= _scPublicVars._surface;
	swapChainInfo.minImageCount			= _scPrivateVars._desiredNumberOfSwapChainImages;
	swapChainInfo.imageFormat			= _scPublicVars._format;
	swapChainInfo.imageExtent.width		= _scPrivateVars._swapChainExtent.width;
	swapChainInfo.imageExtent.height	= _scPrivateVars._swapChainExtent.height;
	swapChainInfo.preTransform			= _scPrivateVars._preTransform;
	swapChainInfo.compositeAlpha		= VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainInfo.imageArrayLayers		= 1;
	swapChainInfo.presentMode			= _scPrivateVars._swapchainPresentMode;
	swapChainInfo.oldSwapchain			= oldSwapchain;
	swapChainInfo.clipped				= true;
	swapChainInfo.imageColorSpace		= VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	swapChainInfo.imageUsage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	swapChainInfo.imageSharingMode		= VK_SHARING_MODE_EXCLUSIVE;
	swapChainInfo.queueFamilyIndexCount = 0;
	swapChainInfo.pQueueFamilyIndices	= nullptr;

	VkResult result = fpCreateSwapchainKHR(_rendererObj->GetDevice()->_device, &swapChainInfo, nullptr, &_scPublicVars._swapChain);
	assert(result == VK_SUCCESS);

	// Create the swapchain object
	result = fpGetSwapchainImagesKHR(_rendererObj->GetDevice()->_device, _scPublicVars._swapChain, &_scPublicVars._swapchainImageCount, nullptr);
	assert(result == VK_SUCCESS);

	_scPrivateVars._swapchainImages.clear();
	// Get the number of images the swapchain has
	_scPrivateVars._swapchainImages.resize(_scPublicVars._swapchainImageCount);
	assert(!_scPrivateVars._swapchainImages.empty());

	// Retrieve the swapchain image surfaces 
	result = fpGetSwapchainImagesKHR(_rendererObj->GetDevice()->_device, _scPublicVars._swapChain, &_scPublicVars._swapchainImageCount, &_scPrivateVars._swapchainImages[0]);
	assert(result == VK_SUCCESS);

	if (oldSwapchain != VK_NULL_HANDLE) 
    {
		fpDestroySwapchainKHR(_rendererObj->GetDevice()->_device, oldSwapchain, nullptr);
	}
}

void VulkanSwapChain::CreateColorImageView(const VkCommandBuffer& cmd)
{
    _scPublicVars._colorBuffer.clear();
	for (uint32_t i = 0; i < _scPublicVars._swapchainImageCount; i++) 
    {
		SwapChainBuffer scBuffer{};

		VkImageViewCreateInfo imgViewInfo = {};
		imgViewInfo.sType			= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imgViewInfo.pNext			= nullptr;
		imgViewInfo.format			= _scPublicVars._format;
		imgViewInfo.components		= { VK_COMPONENT_SWIZZLE_IDENTITY };
		imgViewInfo.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
		imgViewInfo.subresourceRange.baseMipLevel	= 0;
		imgViewInfo.subresourceRange.levelCount		= 1;
		imgViewInfo.subresourceRange.baseArrayLayer	= 0;
		imgViewInfo.subresourceRange.layerCount		= 1;
		imgViewInfo.viewType						= VK_IMAGE_VIEW_TYPE_2D;
		imgViewInfo.flags							= 0;

		scBuffer._image = _scPrivateVars._swapchainImages[i];

		// Since the swapchain is not owned by us we cannot set the image layout
		// upon setting the implementation may give error, the images layout were
		// create by the WSI implementation not by us. 

		imgViewInfo.image = scBuffer._image;

        const VkResult result = vkCreateImageView(_rendererObj->GetDevice()->_device, &imgViewInfo, nullptr, &scBuffer._view);
		_scPublicVars._colorBuffer.push_back(scBuffer);
		assert(result == VK_SUCCESS);
	}
	_scPublicVars._currentColorBuffer = 0;
}

void VulkanSwapChain::DestroySwapChain()
{
	VulkanDevice* deviceObj = _appObj->_deviceObj;

	for (uint32_t i = 0; i < _scPublicVars._swapchainImageCount; i++) 
    {
		vkDestroyImageView(deviceObj->_device, _scPublicVars._colorBuffer[i]._view, nullptr);
	}
	
	if (!_appObj->_isResizing)
    {
		// This piece code will only executes at application shutdown.
        // During resize the old swapchain image is delete in createSwapChainColorImages()
		fpDestroySwapchainKHR(deviceObj->_device, _scPublicVars._swapChain, nullptr);
		vkDestroySurfaceKHR(_appObj->_instanceObj._instance, _scPublicVars._surface, nullptr);
	}
}

void VulkanSwapChain::SetSwapChainExtent(uint32_t swapChainWidth, uint32_t swapChainHeight)
{
	_scPrivateVars._swapChainExtent.width = swapChainWidth;
	_scPrivateVars._swapChainExtent.height = swapChainHeight;
}
