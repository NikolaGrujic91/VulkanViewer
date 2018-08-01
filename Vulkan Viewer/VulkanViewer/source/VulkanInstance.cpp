#include "VulkanInstance.h"

VkResult VulkanInstance::CreateInstance(std::vector<const char *>& layers, std::vector<const char *>& extensionNames, char const*const appName)
{
	_layerExtension._appRequestedExtensionNames	= extensionNames;
	_layerExtension._appRequestedLayerNames		= layers;
	
	// Define the Vulkan application structure 
	VkApplicationInfo appInfo	= {};
	appInfo.sType				= VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext				= nullptr;
	appInfo.pApplicationName	= appName;
	appInfo.applicationVersion	= 1;
	appInfo.pEngineName			= appName;
	appInfo.engineVersion		= 1;
	// VK_API_VERSION is now deprecated, use VK_MAKE_VERSION instead.
	appInfo.apiVersion			= VK_MAKE_VERSION(1, 0, 0);

	// Define the Vulkan instance create info structure 
	VkInstanceCreateInfo instInfo	= {};
	instInfo.sType					= VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instInfo.pNext					= &_layerExtension._dbgReportCreateInfo;
	instInfo.flags					= 0;
	instInfo.pApplicationInfo		= &appInfo;

	// Specify the list of layer name to be enabled.
	instInfo.enabledLayerCount		= static_cast<uint32_t>(layers.size());
	instInfo.ppEnabledLayerNames	= !layers.empty() ? layers.data() : nullptr;

	// Specify the list of extensions to be used in the application.
	instInfo.enabledExtensionCount	= static_cast<uint32_t>(extensionNames.size());
	instInfo.ppEnabledExtensionNames = !extensionNames.empty() ? extensionNames.data() : nullptr;

    const VkResult result = vkCreateInstance(&instInfo, nullptr, &_instance);
	assert(result == VK_SUCCESS);

	return result;
}

void VulkanInstance::DestroyInstance()
{
	vkDestroyInstance(_instance, nullptr);
}
