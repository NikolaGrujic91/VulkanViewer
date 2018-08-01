#pragma once
#include "Headers.h"
#include "VulkanDescriptor.h"
#include "Wrappers.h"

class VulkanRenderer;
class VulkanDrawable : public VulkanDescriptor
{
public:
	VulkanDrawable(VulkanRenderer* parent = 0);
	~VulkanDrawable();

	void CreateVertexBuffer(const void *vertexData, uint32_t dataSize, uint32_t dataStride, bool useTexture);
	void Prepare();
	void Render();
	void Update();

	void SetPipeline(VkPipeline* vulkanPipeline) { _pipeline = vulkanPipeline; }
	VkPipeline* GetPipeline() { return _pipeline; }

	void CreateUniformBuffer();
    virtual void CreateDescriptorPool(bool useTexture);
    virtual void CreateDescriptorResources();
    virtual void CreateDescriptorSet(bool useTexture);
    virtual void CreateDescriptorSetLayout(bool useTexture);
    virtual void CreatePipelineLayout();

	void InitViewports(VkCommandBuffer* cmd);
	void InitScissors(VkCommandBuffer* cmd);

	void DestroyVertexBuffer();
	void DestroyCommandBuffer();
	void DestroySynchronizationObjects();
	void DestroyUniformBuffer();

	void SetTextures(TextureData* tex);

	struct 
    {
		VkBuffer						_buffer;		// Buffer resource object
		VkDeviceMemory					_memory;		// Buffer resourece object's allocated device memory
		VkDescriptorBufferInfo			_bufferInfo;	// Buffer info that need to supplied into write descriptor set (VkWriteDescriptorSet)
		VkMemoryRequirements			_memRqrmnt;		// Store the queried memory requirement of the uniform buffer
		std::vector<VkMappedMemoryRange>_mappedRange;	// Metadata of memory mapped objects
		uint8_t*						_pData;			// Host pointer containing the mapped device address which is used to write data into.
	} UniformData;

	// Structure storing vertex buffer metadata
	struct 
    {
		VkBuffer _buf;
		VkDeviceMemory _mem;
		VkDescriptorBufferInfo _bufferInfo;
	} VertexBuffer;

	// Stores the vertex input rate
	VkVertexInputBindingDescription		_viIpBind;
	// Store metadata helpful in data interpretation
	VkVertexInputAttributeDescription	_viIpAttrb[2];

private:
	std::vector<VkCommandBuffer> _vecCmdDraw;			// Command buffer for drawing
	void RecordCommandBuffer(int currentImage, VkCommandBuffer* cmdDraw);
	VkViewport _viewport;
	VkRect2D   _scissor;
	VkSemaphore _presentCompleteSemaphore;
	VkSemaphore _drawingCompleteSemaphore;
	TextureData* _textures;

	glm::mat4 _projection;
	glm::mat4 _view;
	glm::mat4 _model;
	glm::mat4 _mvp;

	VulkanRenderer* _rendererObj;
	VkPipeline*		_pipeline;
};