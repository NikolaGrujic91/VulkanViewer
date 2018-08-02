#pragma once
#include "Headers.h"

class VulkanDevice;
class VulkanApplication;

class VulkanDescriptor
{
public:
	// Constructor
	VulkanDescriptor();
	// Destructor
	virtual ~VulkanDescriptor();

	// Creates descriptor pool and allocate descriptor set from it
	void CreateDescriptor(bool useTexture);
	// Deletes the created descriptor set object
	void DestroyDescriptor();

	// Defines the sescriptor sets layout binding and create descriptor layout
	virtual void CreateDescriptorSetLayout(bool useTexture) = 0;
	// Destroy the valid descriptor layout object
	void DestroyDescriptorLayout();

	// Creates the descriptor pool that is used to allocate descriptor sets
	virtual void CreateDescriptorPool(bool useTexture) = 0;
	// Deletes the descriptor pool
	void DestroyDescriptorPool() const;

	// Create Descriptor set associated resources before creating the descriptor set
	virtual void CreateDescriptorResources() = 0;

	// Create the descriptor set from the descriptor pool allocated 
	// memory and update the descriptor set information into it.
	virtual void CreateDescriptorSet(bool useTexture) = 0;
	void DestroyDescriptorSet();

	// Creates the pipeline layout to inject into the pipeline
	virtual void CreatePipelineLayout() = 0;
	// Destroys the create pipelineLayout
	void DestroyPipelineLayouts() const;
	
	// Pipeline layout object
	VkPipelineLayout _pipelineLayout;

	// List of all the VkDescriptorSetLayouts 
	std::vector<VkDescriptorSetLayout> _descLayout;
	
	// Decriptor pool object that will be used for allocating VkDescriptorSet object
	VkDescriptorPool _descriptorPool;
	
	// List of all created VkDescriptorSet
	std::vector<VkDescriptorSet> _descriptorSet;

	// Logical device used for creating the descriptor pool and descriptor sets
	VulkanDevice* _deviceObj;
};