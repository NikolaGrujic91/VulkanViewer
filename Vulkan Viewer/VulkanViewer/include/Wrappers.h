#pragma once
#include "Headers.h"

/***************COMMAND BUFFER WRAPPERS***************/
class CommandBufferMgr
{
public:
	static void AllocCommandBuffer(const VkDevice* device, const VkCommandPool cmdPool, VkCommandBuffer* cmdBuf, const VkCommandBufferAllocateInfo* commandBufferInfo = nullptr);
	static void BeginCommandBuffer(VkCommandBuffer cmdBuf, VkCommandBufferBeginInfo* inCmdBufInfo = nullptr);
	static void EndCommandBuffer(VkCommandBuffer commandBuffer);
	static void SubmitCommandBuffer(const VkQueue& queue, const VkCommandBuffer* commandBuffer, const VkSubmitInfo* inSubmitInfo = nullptr, const VkFence& fence = VK_NULL_HANDLE);
};

void* ReadFile(const char *spvFileName, size_t *fileSize);

/***************TEXTURE WRAPPERS***************/
struct TextureData
{
	VkSampler				_sampler;
	VkImage					_image;
	VkImageLayout			_imageLayout;
	VkMemoryAllocateInfo	_memoryAlloc;
	VkDeviceMemory			_mem;
	VkImageView				_view;
	uint32_t				_mipMapLevels;
	uint32_t				_layerCount;
	uint32_t				_textureWidth, _textureHeight;
	VkDescriptorImageInfo	_descsImgInfo;
};

/***************PPM PARSER CLASS***************/

class PpmParser
{
public:
	PpmParser();
	~PpmParser();
	bool GetHeaderInfo(const char *filename);
	bool LoadImageData(int rowPitch, uint8_t * data);
	int32_t GetImageWidth();
	int32_t GetImageHeight();
	const char* Filename() { return _ppmFile.c_str(); }

private:
	bool _isValid;
	int32_t _imageWidth;
	int32_t _imageHeight;
	int32_t _dataPosition;
	std::string _ppmFile;
	gli::texture2D* _tex2D;
};