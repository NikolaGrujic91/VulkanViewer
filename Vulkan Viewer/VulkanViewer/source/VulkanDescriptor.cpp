#include "VulkanDescriptor.h"
#include "VulkanApplication.h"
#include "VulkanDevice.h"

VulkanDescriptor::VulkanDescriptor() :
	_pipelineLayout(0),
	_descriptorPool(0)
{
	_deviceObj = VulkanApplication::GetInstance()->_deviceObj;
}

VulkanDescriptor::~VulkanDescriptor()
{
}

void VulkanDescriptor::CreateDescriptor(bool useTexture)
{
	// Create the uniform buffer resource 
	CreateDescriptorResources();
	
	// Create the descriptor pool and 
	// use it for descriptor set allocation
	CreateDescriptorPool(useTexture);

	// Create descriptor set with uniform buffer data in it
	CreateDescriptorSet(useTexture);
}

void VulkanDescriptor::DestroyDescriptor()
{
	DestroyDescriptorLayout();
	DestroyPipelineLayouts();
	DestroyDescriptorSet();
	DestroyDescriptorPool();
}

void VulkanDescriptor::DestroyDescriptorLayout()
{
	for (auto i : _descLayout)
	{
		vkDestroyDescriptorSetLayout(_deviceObj->device, i, nullptr);
	}
	_descLayout.clear();
}

void VulkanDescriptor::DestroyPipelineLayouts() const
{
	vkDestroyPipelineLayout(_deviceObj->device, _pipelineLayout, nullptr);
}

void VulkanDescriptor::DestroyDescriptorPool() const
{
	vkDestroyDescriptorPool(_deviceObj->device, _descriptorPool, nullptr);
}

void VulkanDescriptor::DestroyDescriptorSet()
{
	vkFreeDescriptorSets(_deviceObj->device, _descriptorPool, static_cast<uint32_t>(_descriptorSet.size()), &_descriptorSet[0]);
}
