#include "Wrappers.h"
#include "VulkanApplication.h"

void CommandBufferMgr::AllocCommandBuffer(const VkDevice* device, const VkCommandPool cmdPool, VkCommandBuffer* cmdBuf, const VkCommandBufferAllocateInfo* commandBufferInfo)
{
	// Dependency on the intialize SwapChain Extensions and initialize CommandPool
	VkResult result;

	// If command information is available use it as it is.
	if (commandBufferInfo) 
    {
		result = vkAllocateCommandBuffers(*device, commandBufferInfo, cmdBuf);
		assert(!result);
		return;
	}

	// Default implementation, create the command buffer
	// allocation info and use the supplied parameter into it
	VkCommandBufferAllocateInfo cmdInfo;
	cmdInfo.sType		= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdInfo.pNext		= nullptr;
	cmdInfo.commandPool = cmdPool;
	cmdInfo.level		= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdInfo.commandBufferCount = static_cast<uint32_t>(sizeof(cmdBuf)) / sizeof(VkCommandBuffer);;

	result = vkAllocateCommandBuffers(*device, &cmdInfo, cmdBuf);
	assert(!result);
}

void CommandBufferMgr::BeginCommandBuffer(VkCommandBuffer cmdBuf, VkCommandBufferBeginInfo* inCmdBufInfo)
{
	// Dependency on  the initialieCommandBuffer()
	VkResult  result;
	// If the user has specified the custom command buffer use it
	if (inCmdBufInfo) 
    {
		result = vkBeginCommandBuffer(cmdBuf, inCmdBufInfo);
		assert(result == VK_SUCCESS);
		return;
	}

	// Otherwise, use the default implementation.
	VkCommandBufferInheritanceInfo cmdBufInheritInfo;
	cmdBufInheritInfo.sType					= VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	cmdBufInheritInfo.pNext					= nullptr;
	cmdBufInheritInfo.renderPass			= VK_NULL_HANDLE;
	cmdBufInheritInfo.subpass				= 0;
	cmdBufInheritInfo.framebuffer			= VK_NULL_HANDLE;
	cmdBufInheritInfo.occlusionQueryEnable	= VK_FALSE;
	cmdBufInheritInfo.queryFlags			= 0;
	cmdBufInheritInfo.pipelineStatistics	= 0;
	
	VkCommandBufferBeginInfo cmdBufInfo;
	cmdBufInfo.sType				= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.pNext				= nullptr;
	cmdBufInfo.flags				= 0;
	cmdBufInfo.pInheritanceInfo		= &cmdBufInheritInfo;

	result = vkBeginCommandBuffer(cmdBuf, &cmdBufInfo);

	assert(result == VK_SUCCESS);
}

void CommandBufferMgr::EndCommandBuffer(VkCommandBuffer commandBuffer)
{
    const VkResult result = vkEndCommandBuffer(commandBuffer);
	assert(result == VK_SUCCESS);
}

void CommandBufferMgr::SubmitCommandBuffer(const VkQueue& queue, const VkCommandBuffer* commandBuffer, const VkSubmitInfo* inSubmitInfo, const VkFence& fence)
{
	VkResult result;
	
	// If Subimt information is avialable use it as it is, this assumes that 
	// the commands are already specified in the structure, hence ignore command buffer 
	if (inSubmitInfo)
    {
		result = vkQueueSubmit(queue, 1, inSubmitInfo, fence);
		assert(!result);

		result = vkQueueWaitIdle(queue);
		assert(!result);
		return;
	}

	// Otherwise, create the submit information with specified buffer commands
	VkSubmitInfo submitInfo;
	submitInfo.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext				= nullptr;
	submitInfo.waitSemaphoreCount	= 0;
	submitInfo.pWaitSemaphores		= nullptr;
	submitInfo.pWaitDstStageMask	= nullptr;
	submitInfo.commandBufferCount	= static_cast<uint32_t>(sizeof(commandBuffer))/sizeof(VkCommandBuffer);
	submitInfo.pCommandBuffers		= commandBuffer;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores	= nullptr;

	result = vkQueueSubmit(queue, 1, &submitInfo, fence);
	assert(!result);

	result = vkQueueWaitIdle(queue);
	assert(!result);
}

// PPM parser implementation
PpmParser::PpmParser(): _tex2D(nullptr)
{
    _isValid = false;
    _imageWidth = 0;
    _imageHeight = 0;
    _ppmFile = "invalid file name";
    _dataPosition = 0;
}

PpmParser::~PpmParser()
{

}

int32_t PpmParser::GetImageWidth()
{
	return _imageWidth;
}

int32_t PpmParser::GetImageHeight()
{
	return _imageHeight;
}

bool PpmParser::GetHeaderInfo(const char *filename)
{
	_tex2D = new gli::texture2D(gli::load(filename));
	_imageHeight = static_cast<uint32_t>(_tex2D[0].dimensions().x);
	_imageWidth  = static_cast<uint32_t>(_tex2D[0].dimensions().y);
	return true;
}

bool PpmParser::LoadImageData(int rowPitch, uint8_t *data)
{
    auto* dataTemp = static_cast<uint8_t*>(_tex2D->data());
	for (int y = 0; y < _imageHeight; y++)
	{
	    const size_t imageSize = _imageWidth * 4;
		memcpy(data, dataTemp, imageSize);
		dataTemp += imageSize;

		// Advance row by row pitch information
		data += rowPitch;
	}

	return true;
}

void* ReadFile(const char *spvFileName, size_t *fileSize)
{

	FILE *fp = fopen(spvFileName, "rb");
	if (!fp) 
    {
		return nullptr;
	}

    fseek(fp, 0L, SEEK_END);
    const long int size = ftell(fp);

	fseek(fp, 0L, SEEK_SET);

	void* spvShader = malloc(size+1); // Plus for NULL character '\0'
	memset(spvShader, 0, size+1);

    const size_t retval = fread(spvShader, size, 1, fp);
	assert(retval == 1);

	*fileSize = size;
	fclose(fp);
	return spvShader;
}

