#include "VulkanLED.h"
#include "VulkanApplication.h"

VulkanLayerAndExtension::VulkanLayerAndExtension()
{
	_dbgCreateDebugReportCallback	= nullptr;
	_dbgDestroyDebugReportCallback	= nullptr;
	_debugReportCallback            = NULL;
}

VulkanLayerAndExtension::~VulkanLayerAndExtension()
{
	_dbgCreateDebugReportCallback  = nullptr;
	_dbgDestroyDebugReportCallback = nullptr;
	_debugReportCallback           = NULL;
}

VkResult VulkanLayerAndExtension::GetInstanceLayerProperties()
{
	uint32_t						instanceLayerCount;		// Stores number of layers supported by instance
	std::vector<VkLayerProperties>	layerProperties;		// Vector to store layer properties
	VkResult						result;					// Variable to check Vulkan API result status

	// Query all the layers
	do 
    {
		result = vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);

        if (result)
        {
            return result;
        }

        if (instanceLayerCount == 0)
        {
            return VK_INCOMPLETE; // return fail
        }

		layerProperties.resize(instanceLayerCount);
		result = vkEnumerateInstanceLayerProperties(&instanceLayerCount, layerProperties.data());
    } while (result == VK_INCOMPLETE);

	// Query all the extensions for each layer and store it.
	std::cout << "\nInstanced Layers" << std::endl;
	std::cout << "===================" << std::endl;
	for (auto globalLayerProp: layerProperties)
    {
		std::cout <<"\n"<< globalLayerProp.description <<"\n\t|\n\t|---[Layer Name]--> " << globalLayerProp.layerName <<"\n";

		LayerProperties layerProps;
		layerProps._properties = globalLayerProp;

		// Get Instance level extensions for corresponding layer properties
		result = GetExtensionProperties(layerProps);

		if (result)
        {
			continue;
		}

		_layerPropertyList.push_back(layerProps);

		for (auto j : layerProps._extensions)
        {
			std::cout << "\t\t|\n\t\t|---[Layer Extension]--> " << j.extensionName << "\n";
		}
	}
	return result;
}

/*
* Get the device extensions
*/
 
VkResult VulkanLayerAndExtension::GetDeviceExtensionProperties(VkPhysicalDevice* gpu)
{
	VkResult result = {};					// Variable to check Vulkan API result status

	// Query all the extensions for each layer and store it.
	std::cout << "Device extensions" << std::endl;
	std::cout << "===================" << std::endl;
	std::vector<LayerProperties>* instanceLayerProp = &VulkanApplication::GetInstance()->_instanceObj._layerExtension._layerPropertyList;
	for (const auto& globalLayerProp : *instanceLayerProp) 
    {
		LayerProperties layerProps;
		layerProps._properties = globalLayerProp._properties;

        if ((result = GetExtensionProperties(layerProps, gpu)))
        {
            continue;
        }

		std::cout << "\n" << globalLayerProp._properties.description << "\n\t|\n\t|---[Layer Name]--> " << globalLayerProp._properties.layerName << "\n";
		_layerPropertyList.push_back(layerProps);

		if (!layerProps._extensions.empty()) 
        {
			for (auto j : layerProps._extensions)
            {
				std::cout << "\t\t|\n\t\t|---[Device Extesion]--> " << j.extensionName << "\n";
			}
		}
		else 
        {
			std::cout << "\t\t|\n\t\t|---[Device Extesion]--> No extension found \n";
		}
	}
	return result;
}

// This function retrieves extension and its properties at instance 
// and device level. Pass a valid physical device
// pointer to retrieve device level extensions, otherwise
// use NULL to retrieve extension specific to instance level.
VkResult VulkanLayerAndExtension::GetExtensionProperties(LayerProperties &layerProps, VkPhysicalDevice* gpu)
{
	uint32_t	extensionCount;								 // Stores number of extension per layer
	VkResult	result;										 // Variable to check Vulkan API result status
	char*		layerName = layerProps._properties.layerName; // Name of the layer 

	do {
		// Get the total number of extension in this layer
        if (gpu)
        {
            result = vkEnumerateDeviceExtensionProperties(*gpu, layerName, &extensionCount, nullptr);
        }
        else
        {
            result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, nullptr);
        }

        if (result || extensionCount == 0)
        {
            continue;
        }

		layerProps._extensions.resize(extensionCount);

		// Gather all extension properties 
        if (gpu)
        {
            result = vkEnumerateDeviceExtensionProperties(*gpu, layerName, &extensionCount, layerProps._extensions.data());
        }
        else
        {
            result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, layerProps._extensions.data());
        }
	} while (result == VK_INCOMPLETE);

	return result;
}

void VulkanLayerAndExtension::DestroyDebugReportCallback()
{
	VulkanApplication* appObj = VulkanApplication::GetInstance();
	VkInstance& instance	= appObj->_instanceObj._instance;
	_dbgDestroyDebugReportCallback(instance, _debugReportCallback, nullptr);
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanLayerAndExtension::DebugFunction(VkFlags msgFlags, 
                                                                      VkDebugReportObjectTypeEXT objType,
	                                                                  uint64_t srcObject,
                                                                      size_t location, 
                                                                      int32_t msgCode,
	                                                                  const char *layerPrefix,
                                                                      const char *msg, void *userData)
{

	if (msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT) 
    {
		std::cout << "[VK_DEBUG_REPORT] ERROR: [" << layerPrefix << "] Code" << msgCode << ":" << msg << std::endl;
	}
	else if (msgFlags & VK_DEBUG_REPORT_WARNING_BIT_EXT) 
    {
		std::cout << "[VK_DEBUG_REPORT] WARNING: [" << layerPrefix << "] Code" << msgCode << ":" << msg << std::endl;
	}
	else if (msgFlags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) 
    {
		std::cout << "[VK_DEBUG_REPORT] INFORMATION: [" << layerPrefix << "] Code" << msgCode << ":" << msg << std::endl;
	}
	else if (msgFlags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
    {
		std::cout << "[VK_DEBUG_REPORT] PERFORMANCE: [" << layerPrefix << "] Code" << msgCode << ":" << msg << std::endl;
	}
	else if (msgFlags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
    {
		std::cout << "[VK_DEBUG_REPORT] DEBUG: [" << layerPrefix << "] Code" << msgCode << ":" << msg << std::endl;
	}
	else 
    {
		return VK_FALSE;
	}

	fflush(stdout);
	return VK_TRUE;
}

/*
Inspects the incoming layer names against system supported layers, theses layers are not supported
then this function removed it from layerNames allow
*/
VkBool32 VulkanLayerAndExtension::AreLayersSupported(std::vector<const char *> &layerNames)
{
    const auto checkCount = static_cast<uint32_t>(layerNames.size());
    const auto layerCount = static_cast<uint32_t>(_layerPropertyList.size());
	std::vector<const char*> unsupportLayerNames;
	for (uint32_t i = 0; i < checkCount; i++) 
    {
		VkBool32 isSupported = 0;
		for (uint32_t j = 0; j < layerCount; j++) 
        {
			if (!strcmp(layerNames[i], _layerPropertyList[j]._properties.layerName)) 
            {
				isSupported = 1;
			}
		}

		if (!isSupported) 
        {
			std::cout << "No Layer support found, removed from layer: " << layerNames[i] << std::endl;
			unsupportLayerNames.push_back(layerNames[i]);
		}
		else
        {
			std::cout << "Layer supported: " << layerNames[i] << std::endl;
		}
	}

	for (auto i : unsupportLayerNames) 
    {
        const auto it = std::find(layerNames.begin(), layerNames.end(), i);
        if (it != layerNames.end())
        {
            layerNames.erase(it);
        }
	}

	return true;
}



VkResult VulkanLayerAndExtension::CreateDebugReportCallback()
{
    VulkanApplication* appObj	= VulkanApplication::GetInstance();
	VkInstance* instance		= &appObj->_instanceObj._instance;

	// Get vkCreateDebugReportCallbackEXT API
	_dbgCreateDebugReportCallback = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(*instance, "vkCreateDebugReportCallbackEXT"));
	if (!_dbgCreateDebugReportCallback) 
    {
		std::cout << "Error: GetInstanceProcAddr unable to locate vkCreateDebugReportCallbackEXT function." << std::endl;
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	std::cout << "GetInstanceProcAddr loaded dbgCreateDebugReportCallback function\n";

	// Get vkDestroyDebugReportCallbackEXT API
	_dbgDestroyDebugReportCallback = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(*instance, "vkDestroyDebugReportCallbackEXT"));
	if (!_dbgDestroyDebugReportCallback) 
    {
		std::cout << "Error: GetInstanceProcAddr unable to locate vkDestroyDebugReportCallbackEXT function." << std::endl;
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	std::cout << "GetInstanceProcAddr loaded dbgDestroyDebugReportCallback function\n";

	// Define the debug report control structure, provide the reference of 'debugFunction'
	// , this function prints the debug information on the console.
	_dbgReportCreateInfo.sType		 = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
	_dbgReportCreateInfo.pfnCallback = DebugFunction;
	_dbgReportCreateInfo.pUserData	 = nullptr;
	_dbgReportCreateInfo.pNext		 = nullptr;
	_dbgReportCreateInfo.flags		 = VK_DEBUG_REPORT_WARNING_BIT_EXT |
									   VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
									   VK_DEBUG_REPORT_ERROR_BIT_EXT |
									   VK_DEBUG_REPORT_DEBUG_BIT_EXT;

	// Create the debug report callback and store the handle into 'debugReportCallback'
    const VkResult result = _dbgCreateDebugReportCallback(*instance, &_dbgReportCreateInfo, nullptr, &_debugReportCallback);
	if (result == VK_SUCCESS) 
    {
		std::cout << "Debug report callback object created successfully\n";
	}
	return result;
}
