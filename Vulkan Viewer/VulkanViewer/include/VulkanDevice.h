#pragma once

#include "Headers.h"
#include "VulkanLED.h"

// Vulkan exposes one or more devices, each of which exposes one or more queues which may process 
// work asynchronously to one another.The queues supported by a device are divided into families, 
// each of which supports one or more types of functionality and may contain multiple queues with 
// similar characteristics.Queues within a single family are considered compatible with one another, 
// and work produced for a family of queues can be executed on any queue within that family
class VulkanDevice
{
public:
	VulkanDevice(VkPhysicalDevice* gpu);
	~VulkanDevice();

	VkResult CreateDevice(std::vector<const char *>& layers, std::vector<const char *>& extensions);
	void DestroyDevice();

	bool MemoryTypeFromProperties(uint32_t typeBits, VkFlags requirementsMask, uint32_t *typeIndex);
	
	// Get the avaialbe queues exposed by the physical devices
	void GetPhysicalDeviceQueuesAndProperties();

	// Query physical device to retrive queue properties
	uint32_t GetGraphicsQueueHandle();

	// Queue related member functions.
	void GetDeviceQueue();

	VkDevice							_device;	// Logical device
	VkPhysicalDevice*					_gpu;		// Physical device
	VkPhysicalDeviceProperties			_gpuProps;	// Physical device attributes
	VkPhysicalDeviceMemoryProperties	_memoryProperties;

	// Queue
	VkQueue									_queue;							// Vulkan Queues object
	std::vector<VkQueueFamilyProperties>	_queueFamilyProps;				// Store all queue families exposed by the physical device. attributes
	uint32_t								_graphicsQueueIndex;			// Stores graphics queue index
	uint32_t								_graphicsQueueWithPresentIndex;  // Number of queue family exposed by device
	uint32_t								_queueFamilyCount;				// Device specificc layer and extensions
	
	// Layer and extensions
	VulkanLayerAndExtension		_layerExtension;
	VkPhysicalDeviceFeatures	_deviceFeatures;
};