#pragma once
#include "Headers.h"

struct LayerProperties
{
	VkLayerProperties                  _properties;
	std::vector<VkExtensionProperties> _extensions;
};

class VulkanLayerAndExtension{
public:
	VulkanLayerAndExtension();
	~VulkanLayerAndExtension();

	/******* LAYER AND EXTENSION MEMBER FUNCTION AND VARAIBLES *******/

	// Instance/global layer
	VkResult GetInstanceLayerProperties();

	// Global extensions
	VkResult GetExtensionProperties(LayerProperties &layerProps, VkPhysicalDevice* gpu = NULL);

	// Device based extensions
	VkResult GetDeviceExtensionProperties(VkPhysicalDevice* gpu);

	// List of layer names requested by the application.
	std::vector<const char *>			_appRequestedLayerNames;
	
	// List of extension names requested by the application.
	std::vector<const char *>			_appRequestedExtensionNames;
	
	// Layers and corresponding extension list
	std::vector<LayerProperties>		_layerPropertyList;

	/******* VULKAN DEBUGGING MEMBER FUNCTION AND VARAIBLES *******/

	VkBool32 AreLayersSupported(std::vector<const char *> &layerNames);
	VkResult CreateDebugReportCallback();
	void	 DestroyDebugReportCallback();
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugFunction(VkFlags msgFlags, 
														VkDebugReportObjectTypeEXT objType,
														uint64_t srcObject, 
														size_t location, 
														int32_t msgCode,
														const char *layerPrefix,
														const char *msg, 
														void *userData);

	VkDebugReportCallbackCreateInfoEXT _dbgReportCreateInfo = {};

private:
	PFN_vkCreateDebugReportCallbackEXT  _dbgCreateDebugReportCallback;
	PFN_vkDestroyDebugReportCallbackEXT _dbgDestroyDebugReportCallback;
	VkDebugReportCallbackEXT            _debugReportCallback;
	
};