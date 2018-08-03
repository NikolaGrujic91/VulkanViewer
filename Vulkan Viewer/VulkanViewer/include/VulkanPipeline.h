#pragma once
#include "Headers.h"
class VulkanShader;
class VulkanDrawable;

// While creating the pipeline the number of viewports and number 
// of scissors.                 
#define NUMBER_OF_VIEWPORTS 1
#define NUMBER_OF_SCISSORS NUMBER_OF_VIEWPORTS

class VulkanPipeline
{
public:
	VulkanPipeline(VkDevice* device, VkRenderPass* renderPass);

	~VulkanPipeline();
	
	// Creates the pipeline cache object and stores pipeline object
	void CreatePipelineCache();
	
	// Returns the created pipeline object, it takes the drawable object which 
	// contains the vertex input rate and data interpretation information, 
	// shader files, boolean flag checking enabled depth, and flag to check
	// if the vertex input are available. 	
	bool CreatePipeline(VulkanDrawable* drawableObj, VkPipeline* pipeline, VulkanShader* shaderObj, VkBool32 includeDepth, VkBool32 includeVi = true);

	// Destruct the pipeline cache object
	void DestroyPipelineCache();

private:
	VkPipelineCache _pipelineCache;
	VkDevice*       _device;
	VkRenderPass*   _renderPass;
};