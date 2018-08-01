#include "VulkanDrawable.h"
#include "VulkanApplication.h"

VulkanDrawable::VulkanDrawable(VkDevice* device,
                               VkCommandPool* commandPool,
                               VkRenderPass* renderPass,
                               VkQueue* queue,
                               std::vector<VkFramebuffer>* framebuffers,
                               VulkanSwapChain* swapChain,
                               int* width,
                               int* height) :
    _device(device),
    _commandPool(commandPool),
    _renderPass(renderPass),
    _queue(queue),
    _framebuffers(framebuffers),
    _swapChain(swapChain),
    _width(width),
    _height(height)
{
    // Note: It's very important to initilize the member with 0 or respective value other wise it will break the system
    memset(&UniformData, 0, sizeof(UniformData));
    memset(&VertexBuffer, 0, sizeof(VertexBuffer));

    VkSemaphoreCreateInfo presentCompleteSemaphoreCreateInfo;
    presentCompleteSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    presentCompleteSemaphoreCreateInfo.pNext = nullptr;
    presentCompleteSemaphoreCreateInfo.flags = 0;

    VkSemaphoreCreateInfo drawingCompleteSemaphoreCreateInfo;
    drawingCompleteSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    drawingCompleteSemaphoreCreateInfo.pNext = nullptr;
    drawingCompleteSemaphoreCreateInfo.flags = 0;

    vkCreateSemaphore(*_device, &presentCompleteSemaphoreCreateInfo, nullptr, &_presentCompleteSemaphore);
    vkCreateSemaphore(*_device, &drawingCompleteSemaphoreCreateInfo, nullptr, &_drawingCompleteSemaphore);
}

VulkanDrawable::~VulkanDrawable()
{
}

void VulkanDrawable::DestroyCommandBuffer()
{
	for (auto& i : _vecCmdDraw)
	{
		vkFreeCommandBuffers(*_device, *_commandPool, 1, &i);
	}
}

void VulkanDrawable::DestroySynchronizationObjects()
{
	vkDestroySemaphore(*_device, _presentCompleteSemaphore, nullptr);
	vkDestroySemaphore(*_device, _drawingCompleteSemaphore, nullptr);
}

void VulkanDrawable::CreateUniformBuffer()
{
    _projection	= glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
	_view		= glm::lookAt(
						glm::vec3(10, 3, 10),	// Camera in World Space
						glm::vec3(0, 0, 0),		// and looks at the origin
						glm::vec3(0, -1, 0)		// Head is up
						);
	_model		= glm::mat4(1.0f);
	_mvp		= _projection * _view * _model;

	// Create buffer resource states using VkBufferCreateInfo
	VkBufferCreateInfo bufInfo;
	bufInfo.sType					= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufInfo.pNext					= nullptr;
	bufInfo.usage					= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufInfo.size					= sizeof(_mvp);
	bufInfo.queueFamilyIndexCount	= 0;
	bufInfo.pQueueFamilyIndices	= nullptr;
	bufInfo.sharingMode			= VK_SHARING_MODE_EXCLUSIVE;
	bufInfo.flags					= 0;

	// Use create buffer info and create the buffer objects
	VkResult result = vkCreateBuffer(*_device, &bufInfo, nullptr, &UniformData._buffer);
	assert(result == VK_SUCCESS);

	// Get the buffer memory requirements
	VkMemoryRequirements memRqrmnt;
	vkGetBufferMemoryRequirements(*_device, UniformData._buffer, &memRqrmnt);

	VkMemoryAllocateInfo memAllocInfo;
	memAllocInfo.sType				= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.pNext				= nullptr;
	memAllocInfo.memoryTypeIndex		= 0;
	memAllocInfo.allocationSize = memRqrmnt.size;

	// Determine the type of memory required 
	// with the help of memory properties
    const bool pass = _deviceObj->MemoryTypeFromProperties(memRqrmnt.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memAllocInfo.memoryTypeIndex);
	assert(pass);

	// Allocate the memory for buffer objects
	result = vkAllocateMemory(*_device, &memAllocInfo, nullptr, &(UniformData._memory));
	assert(result == VK_SUCCESS);

	// Map the GPU memory on to local host
	result = vkMapMemory(*_device, UniformData._memory, 0, memRqrmnt.size, 0, reinterpret_cast<void **>(&UniformData._pData));
	assert(result == VK_SUCCESS);

	// Copy computed data in the mapped buffer
	memcpy(UniformData._pData, &_mvp, sizeof(_mvp));

	// We have only one Uniform buffer object to update
	UniformData._mappedRange.resize(1);

	// Populate the VkMappedMemoryRange data structure
	UniformData._mappedRange[0].sType	= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	UniformData._mappedRange[0].memory	= UniformData._memory;
	UniformData._mappedRange[0].offset	= 0;
	UniformData._mappedRange[0].size		= sizeof(_mvp);

	// Invalidate the range of mapped buffer in order to make it visible to the host.
	// If the memory property is set with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	// then the driver may take care of this, otherwise for non-coherent 
	// mapped memory vkInvalidateMappedMemoryRanges() needs to be called explicitly.
	vkInvalidateMappedMemoryRanges(*_device, 1, &UniformData._mappedRange[0]);

	// Bind the buffer device memory 
	result = vkBindBufferMemory(*_device, UniformData._buffer, UniformData._memory, 0);
	assert(result == VK_SUCCESS);

	// Update the local data structure with uniform buffer for house keeping
	UniformData._bufferInfo.buffer	= UniformData._buffer;
	UniformData._bufferInfo.offset	= 0;
	UniformData._bufferInfo.range	= sizeof(_mvp);
	UniformData._memRqrmnt			= memRqrmnt;
}

void VulkanDrawable::CreateVertexBuffer(const void *vertexData, uint32_t dataSize, uint32_t dataStride, bool useTexture)
{
	VulkanApplication* appObj	= VulkanApplication::GetInstance();
	VulkanDevice* deviceObj		= appObj->_deviceObj;

    // Create the Buffer resourece metadata information
	VkBufferCreateInfo bufInfo;
	bufInfo.sType					= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufInfo.pNext					= nullptr;
	bufInfo.usage					= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufInfo.size					= dataSize;
	bufInfo.queueFamilyIndexCount	= 0;
	bufInfo.pQueueFamilyIndices	= nullptr;
	bufInfo.sharingMode			= VK_SHARING_MODE_EXCLUSIVE;
	bufInfo.flags					= 0;

	// Create the Buffer resource
	VkResult result = vkCreateBuffer(deviceObj->_device, &bufInfo, nullptr, &VertexBuffer._buf);
	assert(result == VK_SUCCESS);

	// Get the Buffer resource requirements
	VkMemoryRequirements memRqrmnt;
	vkGetBufferMemoryRequirements(deviceObj->_device, VertexBuffer._buf, &memRqrmnt);

	// Create memory allocation metadata information
	VkMemoryAllocateInfo allocInfo;
	allocInfo.sType				= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext				= nullptr;
	allocInfo.memoryTypeIndex	= 0;
	allocInfo.allocationSize	= memRqrmnt.size;

	// Get the compatible type of memory
    const bool pass = deviceObj->MemoryTypeFromProperties(memRqrmnt.memoryTypeBits,
	                                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                                                          &allocInfo.memoryTypeIndex);
	assert(pass);

	// Allocate the physical backing for buffer resource
	result = vkAllocateMemory(deviceObj->_device, &allocInfo, nullptr, &(VertexBuffer._mem));
	assert(result == VK_SUCCESS);
	VertexBuffer._bufferInfo.range	= memRqrmnt.size;
	VertexBuffer._bufferInfo.offset	= 0;

	// Map the physical device memory region to the host 
	uint8_t *pData;
	result = vkMapMemory(deviceObj->_device, VertexBuffer._mem, 0, memRqrmnt.size, 0, reinterpret_cast<void **>(&pData));
	assert(result == VK_SUCCESS);

	// Copy the data in the mapped memory
	memcpy(pData, vertexData, dataSize);

	// Unmap the device memory
	vkUnmapMemory(deviceObj->_device, VertexBuffer._mem);

	// Bind the allocated buffer resource to the device memory
	result = vkBindBufferMemory(deviceObj->_device, VertexBuffer._buf, VertexBuffer._mem, 0);
	assert(result == VK_SUCCESS);

	// Once the buffer resource is implemented, its binding points are 
	// stored into the(
	// The VkVertexInputBinding viIpBind, stores the rate at which the information will be
	// injected for vertex input.
	_viIpBind.binding		= 0;
	_viIpBind.inputRate		= VK_VERTEX_INPUT_RATE_VERTEX;
	_viIpBind.stride		= dataStride;

	// The VkVertexInputAttribute - Description) structure, store 
	// the information that helps in interpreting the data.
	_viIpAttrb[0].binding	= 0;
	_viIpAttrb[0].location	= 0;
	_viIpAttrb[0].format	= VK_FORMAT_R32G32B32A32_SFLOAT;
	_viIpAttrb[0].offset	= 0;
	_viIpAttrb[1].binding	= 0;
	_viIpAttrb[1].location	= 1;
	_viIpAttrb[1].format	= useTexture ? VK_FORMAT_R32G32_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT;
	_viIpAttrb[1].offset	= 16; // After, 4 components - RGBA  each of 4 bytes(32bits)
}

// Creates the descriptor pool, this function depends on - 
// createDescriptorSetLayout()
void VulkanDrawable::CreateDescriptorPool(bool useTexture)
{
    // Define the size of descriptor pool based on the
	// type of descriptor set being used.
	std::vector<VkDescriptorPoolSize> descriptorTypePool;

	// The first descriptor pool object is of type Uniform buffer
	descriptorTypePool.push_back(VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 });

	// If texture is supported then define second object with 
	// descriptor type to be Image sampler
	if (useTexture) 
    {
		descriptorTypePool.push_back(VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 });
	}

	// Populate the descriptor pool state information
	// in the create info structure.
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
	descriptorPoolCreateInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext			= nullptr;
	descriptorPoolCreateInfo.maxSets		= 1;
	descriptorPoolCreateInfo.flags			= VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	descriptorPoolCreateInfo.poolSizeCount	= static_cast<uint32_t>(descriptorTypePool.size());
	descriptorPoolCreateInfo.pPoolSizes		= descriptorTypePool.data();
	
	// Create the descriptor pool using the descriptor 
	// pool create info structure
    const VkResult result = vkCreateDescriptorPool(*_device,	&descriptorPoolCreateInfo, nullptr, &_descriptorPool);
	assert(result == VK_SUCCESS);
}

// Create the Uniform resource inside. Create Descriptor set associated resources 
// before creating the descriptor set
void VulkanDrawable::CreateDescriptorResources()
{
	CreateUniformBuffer();
}

// Creates the descriptor sets using descriptor pool.
// This function depend on the createDescriptorPool() and createUniformBuffer().
void VulkanDrawable::CreateDescriptorSet(bool useTexture)
{
    // Create the descriptor allocation structure and specify the descriptor 
	// pool and descriptor layout
	VkDescriptorSetAllocateInfo dsAllocInfo[1];
	dsAllocInfo[0].sType				= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dsAllocInfo[0].pNext				= nullptr;
	dsAllocInfo[0].descriptorPool		= _descriptorPool;
	dsAllocInfo[0].descriptorSetCount	= 1;
	dsAllocInfo[0].pSetLayouts			= _descLayout.data();

	// Allocate the number of descriptor sets needs to be produced
	_descriptorSet.resize(1);

	// Allocate descriptor sets
    const VkResult result = vkAllocateDescriptorSets(*_device, dsAllocInfo, _descriptorSet.data());
	assert(result == VK_SUCCESS);

	// Allocate two write descriptors for - 1. MVP and 2. Texture
	VkWriteDescriptorSet writes[2];
	memset(&writes, 0, sizeof(writes));
	
	// Specify the uniform buffer related 
	// information into first write descriptor
	writes[0]					= {};
	writes[0].sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].pNext				= nullptr;
	writes[0].dstSet			= _descriptorSet[0];
	writes[0].descriptorCount	= 1;
	writes[0].descriptorType	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writes[0].pBufferInfo		= &UniformData._bufferInfo;
	writes[0].dstArrayElement	= 0;
	writes[0].dstBinding		= 0; // DESCRIPTOR_SET_BINDING_INDEX

	// If texture is used then update the second write descriptor structure
	if (useTexture)
	{
		writes[1]					= {};
		writes[1].sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet			= _descriptorSet[0];
		writes[1].dstBinding		= 1; // DESCRIPTOR_SET_BINDING_INDEX
		writes[1].descriptorCount	= 1;
		writes[1].descriptorType	= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].pImageInfo		= &_textures->_descsImgInfo;
		writes[1].dstArrayElement	= 0;
	}

	// Update the uniform buffer into the allocated descriptor set
	vkUpdateDescriptorSets(*_device, useTexture ? 2 : 1, writes, 0, nullptr);
}

void VulkanDrawable::InitViewports(VkCommandBuffer* cmd)
{
	_viewport.height	= static_cast<float>(*_height);
	_viewport.width		= static_cast<float>(*_width);
	_viewport.minDepth	= static_cast<float>(0.0f);
	_viewport.maxDepth	= static_cast<float>(1.0f);
	_viewport.x			= 0;
	_viewport.y			= 0;
	vkCmdSetViewport(*cmd, 0, NUMBER_OF_VIEWPORTS, &_viewport);
}

void VulkanDrawable::InitScissors(VkCommandBuffer* cmd)
{
	_scissor.extent.width	= *_width;
	_scissor.extent.height	= *_height;
	_scissor.offset.x		= 0;
	_scissor.offset.y		= 0;
	vkCmdSetScissor(*cmd, 0, NUMBER_OF_SCISSORS, &_scissor);
}

void VulkanDrawable::DestroyVertexBuffer()
{
	vkDestroyBuffer(*_device, VertexBuffer._buf, nullptr);
	vkFreeMemory(*_device, VertexBuffer._mem, nullptr);
}

void VulkanDrawable::DestroyUniformBuffer()
{
	vkUnmapMemory(*_device, UniformData._memory);
	vkDestroyBuffer(*_device, UniformData._buffer, nullptr);
	vkFreeMemory(*_device, UniformData._memory, nullptr);
}

void VulkanDrawable::SetTextures(TextureData * tex)
{
	_textures = tex;
}

void VulkanDrawable::RecordCommandBuffer(int currentImage, VkCommandBuffer* cmdDraw)
{
	// Specify the clear color value
	VkClearValue clearValues[2];
	clearValues[0].color.float32[0]		= 1.0f;
	clearValues[0].color.float32[1]		= 1.0f;
	clearValues[0].color.float32[2]		= 1.0f;
	clearValues[0].color.float32[3]		= 1.0f;

	// Specify the depth/stencil clear value
	clearValues[1].depthStencil.depth	= 1.0f;
	clearValues[1].depthStencil.stencil	= 0;

	// Define the VkRenderPassBeginInfo control structure
	VkRenderPassBeginInfo renderPassBegin;
	renderPassBegin.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBegin.pNext						= nullptr;
	renderPassBegin.renderPass					= *_renderPass;
	renderPassBegin.framebuffer					= (*_framebuffers)[currentImage];
	renderPassBegin.renderArea.offset.x			= 0;
	renderPassBegin.renderArea.offset.y			= 0;
	renderPassBegin.renderArea.extent.width		= *_width;
	renderPassBegin.renderArea.extent.height	= *_height;
	renderPassBegin.clearValueCount				= 2;
	renderPassBegin.pClearValues				= clearValues;
	
	// Start recording the render pass instance
	vkCmdBeginRenderPass(*cmdDraw, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

	// Bound the command buffer with the graphics pipeline
	vkCmdBindPipeline(*cmdDraw, VK_PIPELINE_BIND_POINT_GRAPHICS, *_pipeline);
	vkCmdBindDescriptorSets(*cmdDraw, 
                            VK_PIPELINE_BIND_POINT_GRAPHICS, 
                            _pipelineLayout,
		                    0, 
                            1,
                            _descriptorSet.data(),
                            0,
                            nullptr);

	// Bound the command buffer with the graphics pipeline
	const VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(*cmdDraw, 0, 1, &VertexBuffer._buf, offsets);

	// Define the dynamic viewport here
	InitViewports(cmdDraw);

	// Define the scissoring 
	InitScissors(cmdDraw);

	// Issue the draw command 6 faces consisting of 2 triangles each with 3 vertices.
	vkCmdDraw(*cmdDraw, 3 * 2 * 6, 1, 0, 0);

	// End of render pass instance recording
	vkCmdEndRenderPass(*cmdDraw);
}

void VulkanDrawable::Prepare()
{
	_vecCmdDraw.resize(_swapChain->_scPublicVars._colorBuffer.size());
	// For each swapbuffer color surface image buffer 
	// allocate the corresponding command buffer
	for (int i = 0; i < _swapChain->_scPublicVars._colorBuffer.size(); i++)
    {
		// Allocate, create and start command buffer recording
		CommandBufferMgr::AllocCommandBuffer(_device, *_commandPool, &_vecCmdDraw[i]);
		CommandBufferMgr::BeginCommandBuffer(_vecCmdDraw[i]);

		// Create the render pass instance 
		RecordCommandBuffer(i, &_vecCmdDraw[i]);

		// Finish the command buffer recording
		CommandBufferMgr::EndCommandBuffer(_vecCmdDraw[i]);
	}
}

void VulkanDrawable::Update()
{
	_projection = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
	_view = glm::lookAt(
		glm::vec3(0, 0, 5),		// Camera is in World Space
		glm::vec3(0, 0, 0),		// and looks at the origin
		glm::vec3(0, 1, 0)		// Head is up
		);
	_model = glm::mat4(1.0f);
	static float rot = 0;
	rot += .0005f;
	_model = glm::rotate(_model, rot, glm::vec3(0.0, 1.0, 0.0))
			* glm::rotate(_model, rot, glm::vec3(1.0, 1.0, 1.0));

	glm::mat4 MVP = _projection * _view * _model;

	// Invalidate the range of mapped buffer in order to make it visible to the host.
	// If the memory property is set with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	// then the driver may take care of this, otherwise for non-coherent 
	// mapped memory vkInvalidateMappedMemoryRanges() needs to be called explicitly.
	VkResult res = vkInvalidateMappedMemoryRanges(*_device, 1, &UniformData._mappedRange[0]);
	assert(res == VK_SUCCESS);

	// Copy updated data into the mapped memory
	memcpy(UniformData._pData, &MVP, sizeof(MVP));

	// Flush the range of mapped buffer in order to make it visible to the device
	// If the memory is coherent (memory property must be beVK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	// then the driver may take care of this, otherwise for non-coherent 
	// mapped memory vkFlushMappedMemoryRanges() needs to be called explicitly to flush out 
	// the pending writes on the host side.
	res = vkFlushMappedMemoryRanges(*_device, 1, &UniformData._mappedRange[0]);
	assert(res == VK_SUCCESS);

}

void VulkanDrawable::Render()
{
	uint32_t& currentColorImage		= _swapChain->_scPublicVars._currentColorBuffer;
	VkSwapchainKHR& swapChain		= _swapChain->_scPublicVars._swapChain;

    VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submitInfo;
	submitInfo.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext				= nullptr;
	submitInfo.waitSemaphoreCount	= 1;
	submitInfo.pWaitSemaphores		= &_presentCompleteSemaphore;
	submitInfo.pWaitDstStageMask	= &submitPipelineStages;
	submitInfo.commandBufferCount	= static_cast<uint32_t>(sizeof(&_vecCmdDraw[currentColorImage])) / sizeof(VkCommandBuffer);
	submitInfo.pCommandBuffers		= &_vecCmdDraw[currentColorImage];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores	= &_drawingCompleteSemaphore;

	// Queue the command buffer for execution
	CommandBufferMgr::SubmitCommandBuffer(*_queue, &_vecCmdDraw[currentColorImage], &submitInfo);

	// Present the image in the window
	VkPresentInfoKHR present;
	present.sType				= VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present.pNext				= nullptr;
	present.swapchainCount		= 1;
	present.pSwapchains			= &swapChain;
	present.pImageIndices		= &currentColorImage;
	present.pWaitSemaphores		= &_drawingCompleteSemaphore;
	present.waitSemaphoreCount	= 1;
	present.pResults			= nullptr;

	// Queue the image for presentation,
    const VkResult result = _swapChain->fpQueuePresentKHR(*_queue, &present);
	assert(result == VK_SUCCESS);
}

void VulkanDrawable::CreateDescriptorSetLayout(bool useTexture)
{
	// Define the layout binding information for the descriptor set(before creating it)
	// Specify binding point, shader type(like vertex shader below), count etc.
	VkDescriptorSetLayoutBinding layoutBindings[2];
	layoutBindings[0].binding				= 0; // DESCRIPTOR_SET_BINDING_INDEX
	layoutBindings[0].descriptorType		= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBindings[0].descriptorCount		= 1;
	layoutBindings[0].stageFlags			= VK_SHADER_STAGE_VERTEX_BIT;
	layoutBindings[0].pImmutableSamplers	= nullptr;

	// If texture is being used then there existing second binding in the fragment shader
	if (useTexture)
	{
		layoutBindings[1].binding				= 1; // DESCRIPTOR_SET_BINDING_INDEX
		layoutBindings[1].descriptorType		= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		layoutBindings[1].descriptorCount		= 1;
		layoutBindings[1].stageFlags			= VK_SHADER_STAGE_FRAGMENT_BIT;
		layoutBindings[1].pImmutableSamplers	= nullptr;
	}

	// Specify the layout bind into the VkDescriptorSetLayoutCreateInfo
	// and use it to create a descriptor set layout
	VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
	descriptorLayout.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorLayout.pNext			= nullptr;
	descriptorLayout.bindingCount	= useTexture ? 2 : 1;
	descriptorLayout.pBindings		= layoutBindings;

    // Allocate required number of descriptor layout objects and  
	// create them using vkCreateDescriptorSetLayout()
	_descLayout.resize(1);
    const VkResult result = vkCreateDescriptorSetLayout(*_device, &descriptorLayout, nullptr, _descLayout.data());
	assert(result == VK_SUCCESS);
}

// createPipelineLayout is a virtual function from 
// VulkanDescriptor and defined in the VulkanDrawable class.
// virtual void VulkanDescriptor::createPipelineLayout() = 0;

// Creates the pipeline layout to inject into the pipeline
void VulkanDrawable::CreatePipelineLayout()
{
	// Create the pipeline layout with the help of descriptor layout.
	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
	pPipelineLayoutCreateInfo.sType						= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pPipelineLayoutCreateInfo.pNext						= nullptr;
	pPipelineLayoutCreateInfo.pushConstantRangeCount	= 0;
	pPipelineLayoutCreateInfo.pPushConstantRanges		= nullptr;
	pPipelineLayoutCreateInfo.setLayoutCount			= static_cast<uint32_t>(_descLayout.size());
	pPipelineLayoutCreateInfo.pSetLayouts				= _descLayout.data();

    const VkResult result = vkCreatePipelineLayout(*_device, &pPipelineLayoutCreateInfo, nullptr, &_pipelineLayout);
	assert(result == VK_SUCCESS);
}
