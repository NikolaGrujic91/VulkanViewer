#pragma once

#include "Headers.h"
class VulkanDevice;

/*
* Keep each of our swap chain buffers' image, command buffer and view in one spot
*/
struct SwapChainBuffer
{
	VkImage _image;
	VkImageView _view;
};

struct SwapChainPrivateVariables
{
	// Store the image surface capabilities
	VkSurfaceCapabilitiesKHR	_surfCapabilities;				

	// Stores the number of present mode support by the implementation
	uint32_t					_presentModeCount;

	// Arrays for retrived present modes
	std::vector<VkPresentModeKHR> _presentModes;

	// Size of the swap chain color images
	VkExtent2D					_swapChainExtent;

	// Number of color images supported by the implementation
	uint32_t					_desiredNumberOfSwapChainImages;

	VkSurfaceTransformFlagBitsKHR _preTransform;

	// Stores present mode bitwise flag for the creation of swap chain
	VkPresentModeKHR			_swapchainPresentMode;

	// The retrived drawing color swap chain images
	std::vector<VkImage>		_swapchainImages;

	std::vector<VkSurfaceFormatKHR> _surfFormats;
};

struct SwapChainPublicVariables
{
	// The logical platform dependent surface object
	VkSurfaceKHR _surface;

	// Number of buffer image used for swap chain
	uint32_t _swapchainImageCount;

	// Swap chain object
	VkSwapchainKHR _swapChain;

	// List of color swap chain images
	std::vector<SwapChainBuffer> _colorBuffer;

	// Semaphore for sync purpose
	VkSemaphore _presentCompleteSemaphore;

	// Current drawing surface index in use
	uint32_t _currentColorBuffer;

	// Format of the image 
	VkFormat _format;
};

class VulkanSwapChain
{
public:
	VulkanSwapChain(VkInstance* instance,
		            VkDevice* device,
	                VulkanDevice* deviceObj,
	                VkPhysicalDevice* gpu,
	                HINSTANCE* connection,
	                HWND* window,
	                int* width,
	                int* height,
		            bool* isResizing);
	~VulkanSwapChain();
	void IntializeSwapChain();
	void CreateSwapChain(const VkCommandBuffer& cmd);
	void DestroySwapChain();
	void SetSwapChainExtent(uint32_t swapChainWidth, uint32_t swapChainHeight);

    // User define structure containing public variables used 
    // by the swap chain private and public functions.
    SwapChainPublicVariables	_scPublicVars;
    PFN_vkQueuePresentKHR		fpQueuePresentKHR;
    PFN_vkAcquireNextImageKHR	fpAcquireNextImageKHR;

private:
	VkResult CreateSwapChainExtensions();
	void GetSupportedFormats();
	VkResult CreateSurface();
	uint32_t GetGraphicsQueueWithPresentationSupport();
	void GetSurfaceCapabilitiesAndPresentMode();
	void ManagePresentMode();
	void CreateSwapChainColorImages();
	void CreateColorImageView(const VkCommandBuffer& cmd);

	PFN_vkGetPhysicalDeviceSurfaceSupportKHR		fpGetPhysicalDeviceSurfaceSupportKHR;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR	fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR		fpGetPhysicalDeviceSurfaceFormatsKHR;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR	fpGetPhysicalDeviceSurfacePresentModesKHR;
	PFN_vkDestroySurfaceKHR							fpDestroySurfaceKHR;

	// Layer Extensions Debugging
	PFN_vkCreateSwapchainKHR	fpCreateSwapchainKHR;
	PFN_vkDestroySwapchainKHR	fpDestroySwapchainKHR;
	PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;

	// User define structure containing private variables used 
	// by the swap chain private and public functions.
	SwapChainPrivateVariables	_scPrivateVars;

	VkInstance*       _instance;
	VkDevice*         _device;
	VulkanDevice*     _deviceObj;
	VkPhysicalDevice* _gpu;
	HINSTANCE*        _connection;
	HWND*             _window;
	int*              _width;
	int*              _height;
	bool*             _isResizing;
};
