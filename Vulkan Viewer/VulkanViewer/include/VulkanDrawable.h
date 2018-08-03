#pragma once
#include "Headers.h"
#include "VulkanDescriptor.h"
#include "Wrappers.h"

class VulkanRenderer;

class VulkanDrawable : public VulkanDescriptor
{
public:
	VulkanDrawable(VulkanRenderer* parent = nullptr);
	~VulkanDrawable();

	void CreateVertexBuffer(const void *vertexData, uint32_t dataSize, uint32_t dataStride, bool useTexture);
	void Prepare();
	void Render();
	void Update();

	void SetPipeline(VkPipeline* vulkanPipeline) { _pipeline = vulkanPipeline; }
	VkPipeline* GetPipeline() { return _pipeline; }

	void CreateUniformBuffer();
	void CreateDescriptorPool(bool useTexture) override;
	void CreateDescriptorResources() override;
	void CreateDescriptorSet(bool useTexture) override;
	void CreateDescriptorSetLayout(bool useTexture) override;
	void CreatePipelineLayout() override;

	void InitViewports(VkCommandBuffer* cmd);
	void InitScissors(VkCommandBuffer* cmd);

	void DestroyVertexBuffer();
	void DestroyCommandBuffer();
	void DestroySynchronizationObjects();
	void DestroyUniformBuffer();

	void SetTextures(TextureData* tex);

	struct 
	{
		VkBuffer						_buffer;			// Buffer resource object
		VkDeviceMemory					_memory;			// Buffer resourece object's allocated device memory
		VkDescriptorBufferInfo			_bufferInfo;		// Buffer info that need to supplied into write descriptor set (VkWriteDescriptorSet)
		VkMemoryRequirements			_memRqrmnt;		// Store the queried memory requirement of the uniform buffer
		std::vector<VkMappedMemoryRange>_mappedRange;	// Metadata of memory mapped objects
		uint8_t*						_pData;			// Host pointer containing the mapped device address which is used to write data into.
	} _uniformData;

	// Structure storing vertex buffer metadata
	struct 
	{
		VkBuffer               _buf;
		VkDeviceMemory         _mem;
		VkDescriptorBufferInfo _bufferInfo;
	} _vertexBuffer;

	// Stores the vertex input rate
	VkVertexInputBindingDescription		_viIpBind;
	// Store metadata helpful in data interpretation
	VkVertexInputAttributeDescription	_viIpAttrb[2];

private:	
	void RecordCommandBuffer(int currentImage, VkCommandBuffer* cmdDraw);

	std::vector<VkCommandBuffer> _vecCmdDraw;			// Command buffer for drawing
	VkViewport                   _viewport;
	VkRect2D                     _scissor;
	VkSemaphore                  _presentCompleteSemaphore;
	VkSemaphore                  _drawingCompleteSemaphore;
	TextureData*                 _textures;

	glm::mat4                    _projectionMatrix;
	glm::mat4                    _viewMatrix;
	glm::mat4                    _modelMatrix;
	glm::mat4                    _mvpMatrix;

	VulkanRenderer*              _rendererObj;
	VkPipeline*		             _pipeline;
};