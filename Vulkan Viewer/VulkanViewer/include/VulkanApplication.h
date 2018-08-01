#pragma once
#include "VulkanInstance.h"
#include "VulkanDevice.h"
#include "VulkanRenderer.h"

class VulkanApplication
{
public:
	// DTOR
	~VulkanApplication();

    static VulkanApplication* GetInstance();

    // Simple program life cycle
    void Initialize();				// Initialize and allocate resources
    void Prepare();					// Prepare resource
    void Update();					// Update data
    void Resize();					// Resize window
    bool Render();					// Render primitives
    void DeInitialize();			// Release resources

    VulkanInstance  _instanceObj;	// Vulkan Instance object
    VulkanDevice*   _deviceObj;
    VulkanRenderer* _rendererObj;
    bool            _isPrepared;
    bool            _isResizing;

private:
    // CTOR: Application constructor responsible for layer enumeration.
    VulkanApplication();

	// Variable for Single Ton implementation
    static std::unique_ptr<VulkanApplication> _instance;
    static std::once_flag _onlyOnce;

	// Create the Vulkan instance object
	VkResult CreateVulkanInstance(std::vector<const char *>& layers, std::vector<const char *>& extensionNames,  const char* applicationName);
	VkResult HandShakeWithDevice(VkPhysicalDevice* gpu, std::vector<const char *>& layers, std::vector<const char *>& extensions);
	VkResult EnumeratePhysicalDevices(std::vector<VkPhysicalDevice>& gpuList);

	bool                          _debugFlag;
	std::vector<VkPhysicalDevice> _gpuList;
};