#pragma once

#include "VulkanLED.h"

//Define class here
class VulkanInstance
{
public:
    VulkanInstance(){}
    ~VulkanInstance(){}

    // VulkanInstance public functions
    VkResult CreateInstance(std::vector<const char *>& layers, std::vector<const char *>& extensionNames, const char* appName);
    void DestroyInstance();

    // VulkanInstance member variables
    VkInstance _instance;

	// Vulkan instance specific layer and extensions
	VulkanLayerAndExtension _layerExtension;


};
