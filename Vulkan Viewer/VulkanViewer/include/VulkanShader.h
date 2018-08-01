#pragma once
#include "Headers.h"

// Shader class managing the shader conversion, compilation, linking
class VulkanShader
{
public:
	// Constructor
    VulkanShader(VkDevice* device);
	
	// Destructor
	~VulkanShader() {}

	// Use .spv and build shader module
	void BuildShaderModuleWithSpv(uint32_t *vertShaderText, size_t vertexSPVSize, uint32_t *fragShaderText, size_t fragmentSPVSize);

	// Kill the shader when not required
	void DestroyShaders();

#ifdef AUTO_COMPILE_GLSL_TO_SPV
	// Convert GLSL shader to SPIR-V shader
	bool GLSLtoSPV(const VkShaderStageFlagBits shaderType, const char *pshader, std::vector<unsigned int> &spirv);

	// Entry point to build the shaders
	void buildShader(const char *vertShaderText, const char *fragShaderText);

	// Type of shader language. This could be - EShLangVertex,Tessellation Control, 
	// Tessellation Evaluation, Geometry, Fragment and Compute
	EShLanguage getLanguage(const VkShaderStageFlagBits shader_type);

	// Initialize the TBuitInResource
	void initializeResources(TBuiltInResource &Resources);
#endif

	// Vk structure storing vertex & fragment shader information
	VkPipelineShaderStageCreateInfo _shaderStages[2];
    VkDevice* _device;
};
