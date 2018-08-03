#include "VulkanDevice.h"
#include "VulkanInstance.h"

VulkanDevice::VulkanDevice(VkPhysicalDevice* physicalDevice) :
	_device(nullptr),
	_gpuProps(),
	_memoryProperties(),
	_queue(nullptr),
	_graphicsQueueIndex(0),
	_graphicsQueueWithPresentIndex(0),
	_queueFamilyCount(0),
	_deviceFeatures()
{
	_gpu = physicalDevice;
}

VulkanDevice::~VulkanDevice() 
{
}

// Note: This function requires queue object to be in existence before
VkResult VulkanDevice::CreateDevice(std::vector<const char *>& layers, std::vector<const char *>& extensions)
{
	_layerExtension.appRequestedLayerNames		= layers;
	_layerExtension.appRequestedExtensionNames	= extensions;

	// Create Device with available queue information.
	float queuePriorities[1]			= { 0.0 };
	VkDeviceQueueCreateInfo queueInfo	= {};
	queueInfo.queueFamilyIndex			= _graphicsQueueIndex;  
	queueInfo.sType						= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.pNext						= nullptr;
	queueInfo.queueCount				= 1;
	queueInfo.pQueuePriorities			= queuePriorities;


	vkGetPhysicalDeviceFeatures(*_gpu, &_deviceFeatures);

	VkPhysicalDeviceFeatures setEnabledFeatures = {VK_FALSE};
	setEnabledFeatures.samplerAnisotropy = _deviceFeatures.samplerAnisotropy;

	VkDeviceCreateInfo deviceInfo		= {};
	deviceInfo.sType					= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext					= nullptr;
	deviceInfo.queueCreateInfoCount		= 1;
	deviceInfo.pQueueCreateInfos		= &queueInfo;
	deviceInfo.enabledLayerCount		= 0;
	deviceInfo.ppEnabledLayerNames		= nullptr;	// Device layers are deprecated
	deviceInfo.enabledExtensionCount	= static_cast<uint32_t>(extensions.size());
	deviceInfo.ppEnabledExtensionNames	= !extensions.empty() ? extensions.data() : nullptr;
	deviceInfo.pEnabledFeatures			= &setEnabledFeatures;

	const VkResult result = vkCreateDevice(*_gpu, &deviceInfo, nullptr, &_device);
	assert(result == VK_SUCCESS);

	return result;
}

bool VulkanDevice::MemoryTypeFromProperties(uint32_t typeBits, VkFlags requirementsMask, uint32_t *typeIndex)
{
	// Search memtypes to find first index with those properties
	for (uint32_t i = 0; i < 32; i++) 
	{
		if ((typeBits & 1) == 1) 
		{
			// Type is available, does it match user properties?
			if ((_memoryProperties.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask) 
			{
				*typeIndex = i;
				return true;
			}
		}
		typeBits >>= 1;
	}
	// No memory types matched, return failure
	return false;
}

void VulkanDevice::GetPhysicalDeviceQueuesAndProperties()
{
	// Query queue families count with pass NULL as second parameter.
	vkGetPhysicalDeviceQueueFamilyProperties(*_gpu, &_queueFamilyCount, nullptr);
	
	// Allocate space to accomodate Queue properties.
	_queueFamilyProps.resize(_queueFamilyCount);

	// Get queue family properties
	vkGetPhysicalDeviceQueueFamilyProperties(*_gpu, &_queueFamilyCount, _queueFamilyProps.data());
}

uint32_t VulkanDevice::GetGraphicsQueueHandle()
{
	//	1. Get the number of Queues supported by the Physical device
	//	2. Get the properties each Queue type or Queue Family
	//			There could be 4 Queue type or Queue families supported by physical device - 
	//			Graphics Queue	- VK_QUEUE_GRAPHICS_BIT 
	//			Compute Queue	- VK_QUEUE_COMPUTE_BIT
	//			DMA				- VK_QUEUE_TRANSFER_BIT
	//			Sparse memory	- VK_QUEUE_SPARSE_BINDING_BIT
	//	3. Get the index ID for the required Queue family, this ID will act like a handle index to queue.

	bool found = false;
	// 1. Iterate number of Queues supported by the Physical device
	for (unsigned int i = 0; i < _queueFamilyCount; i++)
	{
		// 2. Get the Graphics Queue type
		//		There could be 4 Queue type or Queue families supported by physical device - 
		//		Graphics Queue		- VK_QUEUE_GRAPHICS_BIT 
		//		Compute Queue		- VK_QUEUE_COMPUTE_BIT
		//		DMA/Transfer Queue	- VK_QUEUE_TRANSFER_BIT
		//		Sparse memory		- VK_QUEUE_SPARSE_BINDING_BIT

		if (_queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			// 3. Get the handle/index ID of graphics queue family.
			found				= true;
			_graphicsQueueIndex	= i;
			break;
		}
	}

	// Assert if there is no queue found.
	assert(found);

	return 0;
}

void VulkanDevice::DestroyDevice()
{
	vkDestroyDevice(_device, nullptr);
}


//Queue related functions
void VulkanDevice::GetDeviceQueue()
{
	// Parminder: this depends on intialiing the SwapChain to 
	// get the graphics queue with presentation support
	vkGetDeviceQueue(_device, _graphicsQueueWithPresentIndex, 0, &_queue);
}
