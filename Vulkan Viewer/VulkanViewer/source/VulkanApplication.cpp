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
	_instanceObj.layerExtension.getInstanceLayerProperties();

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
	return _instanceObj.createInstance(layers, extensionNames, applicationName);
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
	_deviceObj->layerExtension.getDeviceExtensionProperties(gpu);

	// Get the physical device or GPU properties
	vkGetPhysicalDeviceProperties(*gpu, &_deviceObj->gpuProps);

	// Get the memory properties from the physical device or GPU.
	vkGetPhysicalDeviceMemoryProperties(*gpu, &_deviceObj->memoryProperties);

	// Query the availabe queues on the physical device and their properties.
	_deviceObj->getPhysicalDeviceQueuesAndProperties();

	// Retrive the queue which support graphics pipeline.
	_deviceObj->getGraphicsQueueHandle();

	// Create Logical Device, ensure that this device is connecte to graphics queue
	return _deviceObj->createDevice(layers, extensions);
}

VkResult VulkanApplication::EnumeratePhysicalDevices(std::vector<VkPhysicalDevice>& gpuList) const
{
	uint32_t gpuDeviceCount;

	VkResult result = vkEnumeratePhysicalDevices(_instanceObj.instance, &gpuDeviceCount, nullptr);
	assert(result == VK_SUCCESS);

	assert(gpuDeviceCount);

	gpuList.resize(gpuDeviceCount);

	result = vkEnumeratePhysicalDevices(_instanceObj.instance, &gpuDeviceCount, gpuList.data());
	assert(result == VK_SUCCESS);

	return result;
}

void VulkanApplication::Initialize()
{
	char title[] = "Hello World!!!";

	// Check if the supplied layer are support or not
	_instanceObj.layerExtension.areLayersSupported(layerNames);

	// Create the Vulkan instance with specified layer and extension names.
	CreateVulkanInstance(layerNames, instanceExtensionNames, title);

	// Create the debugging report if debugging is enabled
	if (_debugFlag)
	{
		_instanceObj.layerExtension.createDebugReportCallback();
	}

	// Get the list of physical devices on the system
	const VkResult result = EnumeratePhysicalDevices(_gpuList);
	assert(result == VK_SUCCESS);

	// This example use only one device which is available first.
	if (!_gpuList.empty()) 
	{
		HandShakeWithDevice(&_gpuList[0], layerNames, deviceExtensionNames);
	}

	if (!_rendererObj) 
	{
		_rendererObj = new VulkanRenderer(this, _deviceObj);
		// Create an empy window 500x500
		_rendererObj->createPresentationWindow(500, 500);
		// Initialize swapchain
		_rendererObj->getSwapChain()->intializeSwapChain();
	}
	_rendererObj->initialize();
}

void VulkanApplication::Resize()
{
	// If prepared then only proceed for 
	if (!_isPrepared) 
	{
		return;
	}
	
	_isResizing = true;

	vkDeviceWaitIdle(_deviceObj->device);
	_rendererObj->destroyFramebuffers();
	_rendererObj->destroyCommandPool();
	_rendererObj->destroyPipeline();
	_rendererObj->getPipelineObject()->destroyPipelineCache();
	for (auto drawableObj : *_rendererObj->getDrawingItems())
	{
		drawableObj->DestroyDescriptor();
	}
	_rendererObj->destroyRenderpass();
	_rendererObj->getSwapChain()->destroySwapChain();
	_rendererObj->destroyDrawableVertexBuffer();
	_rendererObj->destroyDrawableUniformBuffer();
	_rendererObj->destroyTextureResource();
	_rendererObj->destroyDepthBuffer();
	_rendererObj->initialize();
	Prepare();

	_isResizing = false;
}

void VulkanApplication::DeInitialize()
{
	// Destroy all the pipeline objects
	_rendererObj->destroyPipeline();

	// Destroy the associate pipeline cache
	_rendererObj->getPipelineObject()->destroyPipelineCache();

	for (VulkanDrawable* drawableObj : *_rendererObj->getDrawingItems())
	{
		drawableObj->DestroyDescriptor();
	}

	_rendererObj->getShader()->destroyShaders();
	_rendererObj->destroyFramebuffers();
	_rendererObj->destroyRenderpass();
	_rendererObj->destroyDrawableVertexBuffer();
	_rendererObj->destroyDrawableUniformBuffer();

	_rendererObj->destroyDrawableCommandBuffer();
	_rendererObj->destroyDepthBuffer();
	_rendererObj->getSwapChain()->destroySwapChain();
	_rendererObj->destroyCommandBuffer();
	_rendererObj->destroyDrawableSynchronizationObjects();
	_rendererObj->destroyCommandPool();
	_rendererObj->destroyPresentationWindow();
	_rendererObj->destroyTextureResource();
	_deviceObj->destroyDevice();
	if (_debugFlag) 
	{
		_instanceObj.layerExtension.destroyDebugReportCallback();
	}
	_instanceObj.destroyInstance();
}

void VulkanApplication::Prepare()
{
	_isPrepared = false;
	_rendererObj->prepare();
	_isPrepared = true;
}

void VulkanApplication::Update() const
{
	_rendererObj->update();
}

bool VulkanApplication::Render() const
{
	if (!_isPrepared)
	{
		return false;
	}

	return _rendererObj->render();
}
