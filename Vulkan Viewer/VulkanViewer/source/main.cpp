#include "Headers.h"
#include "VulkanApplication.h"

std::vector<const char *> instanceExtensionNames = 
{
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
	VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
};

std::vector<const char *> layerNames =
{
	"VK_LAYER_LUNARG_standard_validation"
};

std::vector<const char *> deviceExtensionNames = 
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

int main(int argc, char **argv)
{
	VulkanApplication* appObj = VulkanApplication::GetInstance();
	appObj->Initialize();
	appObj->Prepare();
	bool isWindowOpen = true;
	while (isWindowOpen) 
    {
		appObj->Update();
		isWindowOpen = appObj->Render();
	}
	appObj->DeInitialize();
}
