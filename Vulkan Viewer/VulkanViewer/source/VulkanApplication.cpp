#include "VulkanApplication.h"
#include "VulkanDrawable.h"

std::unique_ptr<VulkanApplication> VulkanApplication::_instance;
std::once_flag VulkanApplication::_onlyOnce;

extern std::vector<const char *> instanceExtensionNames;
extern std::vector<const char *> layerNames;
extern std::vector<const char *> deviceExtensionNames;


// Application constructor responsible for layer enumeration.
VulkanApplication::VulkanApplication() 
{
	// At application start up, enumerate instance layers
	_instanceObj._layerExtension.GetInstanceLayerProperties();

	_deviceObj = nullptr;
	_debugFlag = false;
	_rendererObj = nullptr;
	_isPrepared = false;
	_isResizing = false;
}

VulkanApplication::~VulkanApplication()
{
	delete _rendererObj;
	_rendererObj = nullptr;
}

// Returns the Single ton object of VulkanApplication
VulkanApplication* VulkanApplication::GetInstance()
{
    std::call_once(_onlyOnce, [](){_instance.reset(new VulkanApplication()); });
    return _instance.get();
}

VkResult VulkanApplication::CreateVulkanInstance( std::vector<const char *>& layers, std::vector<const char *>& extensionNames, const char* applicationName)
{
	return _instanceObj.CreateInstance(layers, extensionNames, applicationName);
}

// This function is responsible for creating the logical device.
// The process of logical device creation requires the following steps:-
// 1. Get the physical device specific layer and corresponding extensions [Optional]
// 2. Create user define VulkanDevice object
// 3. Provide the list of layer and extension that needs to enabled in this physical device
// 4. Get the physical device or GPU properties
// 5. Get the memory properties from the physical device or GPU
// 6. Query the physical device exposed queues and related properties
// 7. Get the handle of graphics queue
// 8. Create the logical device, connect it to the graphics queue.

// High level function for creating device and queues
VkResult VulkanApplication::HandShakeWithDevice(VkPhysicalDevice* gpu, std::vector<const char *>& layers, std::vector<const char *>& extensions )
{

	// The user define Vulkan Device object this will manage the
	// Physical and logical device and their queue and properties
	_deviceObj = new VulkanDevice(gpu);
	if (!_deviceObj)
    {
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	}
	
	// Print the devices available layer and their extension 
	_deviceObj->_layerExtension.GetDeviceExtensionProperties(gpu);

	// Get the physical device or GPU properties
	vkGetPhysicalDeviceProperties(*gpu, &_deviceObj->_gpuProps);

	// Get the memory properties from the physical device or GPU.
	vkGetPhysicalDeviceMemoryProperties(*gpu, &_deviceObj->_memoryProperties);

	// Query the availabe queues on the physical device and their properties.
	_deviceObj->GetPhysicalDeviceQueuesAndProperties();

	// Retrive the queue which support graphics pipeline.
	_deviceObj->GetGraphicsQueueHandle();

	// Create Logical Device, ensure that this device is connecte to graphics queue
	return _deviceObj->CreateDevice(layers, extensions);
}

VkResult VulkanApplication::EnumeratePhysicalDevices(std::vector<VkPhysicalDevice>& gpuList)
{
	uint32_t gpuDeviceCount;

	VkResult result = vkEnumeratePhysicalDevices(_instanceObj._instance, &gpuDeviceCount, nullptr);
	assert(result == VK_SUCCESS);

	assert(gpuDeviceCount);

	gpuList.resize(gpuDeviceCount);

	result = vkEnumeratePhysicalDevices(_instanceObj._instance, &gpuDeviceCount, gpuList.data());
	assert(result == VK_SUCCESS);

	return result;
}

void VulkanApplication::Initialize()
{
	char title[] = "Hello World!!!";

	// Check if the supplied layer are support or not
	_instanceObj._layerExtension.AreLayersSupported(layerNames);

	// Create the Vulkan instance with specified layer and extension names.
	CreateVulkanInstance(layerNames, instanceExtensionNames, title);

	// Create the debugging report if debugging is enabled
	if (_debugFlag)
    {
		_instanceObj._layerExtension.CreateDebugReportCallback();
	}

	// Get the list of physical devices on the system
	EnumeratePhysicalDevices(_gpuList);

	// This example use only one device which is available first.
	if (!_gpuList.empty()) 
    {
		HandShakeWithDevice(&_gpuList[0], layerNames, deviceExtensionNames);
	}

	if (!_rendererObj)
    {
		_rendererObj = new VulkanRenderer(this, _deviceObj);
		// Create an empy window 500x500
		_rendererObj->CreatePresentationWindow(500, 500);
		// Initialize swapchain
		_rendererObj->GetSwapChain()->IntializeSwapChain();
	}
	_rendererObj->Initialize();
}

void VulkanApplication::Resize()
{
	// If prepared then only proceed for 
	if (!_isPrepared) 
    {
		return;
	}
	
	_isResizing = true;

	vkDeviceWaitIdle(_deviceObj->_device);
	_rendererObj->DestroyFramebuffers();
	_rendererObj->DestroyCommandPool();
	_rendererObj->DestroyPipeline();
	_rendererObj->GetPipelineObject()->DestroyPipelineCache();
	for (VulkanDrawable* drawableObj : *_rendererObj->GetDrawingItems())
	{
		drawableObj->DestroyDescriptor();
	}
	_rendererObj->DestroyRenderpass();
	_rendererObj->GetSwapChain()->DestroySwapChain();
	_rendererObj->DestroyDrawableVertexBuffer();
	_rendererObj->DestroyDrawableUniformBuffer();
	_rendererObj->DestroyTextureResource();
	_rendererObj->DestroyDepthBuffer();
	_rendererObj->Initialize();
	Prepare();

	_isResizing = false;
}

void VulkanApplication::DeInitialize()
{
	// Destroy all the pipeline objects
	_rendererObj->DestroyPipeline();

	// Destroy the associate pipeline cache
	_rendererObj->GetPipelineObject()->DestroyPipelineCache();

	for (VulkanDrawable* drawableObj : *_rendererObj->GetDrawingItems())
	{
		drawableObj->DestroyDescriptor();
	}

	_rendererObj->GetShader()->DestroyShaders();
	_rendererObj->DestroyFramebuffers();
	_rendererObj->DestroyRenderpass();
	_rendererObj->DestroyDrawableVertexBuffer();
	_rendererObj->DestroyDrawableUniformBuffer();

	_rendererObj->DestroyDrawableCommandBuffer();
	_rendererObj->DestroyDepthBuffer();
	_rendererObj->GetSwapChain()->DestroySwapChain();
	_rendererObj->DestroyCommandBuffer();
	_rendererObj->DestroyDrawableSynchronizationObjects();
	_rendererObj->DestroyCommandPool();
	_rendererObj->DestroyPresentationWindow();
	_rendererObj->DestroyTextureResource();
	_deviceObj->DestroyDevice();
	if (_debugFlag) 
    {
		_instanceObj._layerExtension.DestroyDebugReportCallback();
	}
	_instanceObj.DestroyInstance();
}

void VulkanApplication::Prepare()
{
	_isPrepared = false;
	_rendererObj->Prepare();
	_isPrepared = true;
}

void VulkanApplication::Update()
{
	_rendererObj->Update();
}

bool VulkanApplication::Render() 
{
    if (!_isPrepared)
    {
        return false;
    }

	return _rendererObj->Render();
}
