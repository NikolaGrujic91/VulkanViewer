#pragma once
#include "Headers.h"
#include "VulkanSwapChain.h"
#include "VulkanDrawable.h"
#include "VulkanShader.h"
#include "VulkanPipeline.h"

// Number of samples needs to be the same at image creation
// Used at renderpass creation (in attachment) and pipeline creation
#define NUM_SAMPLES VK_SAMPLE_COUNT_1_BIT

// The Vulkan Renderer is custom class, it is not a Vulkan specific class.
// It works as a presentation manager.
// It manages the presentation windows and drawing surfaces.
class VulkanRenderer
{
public:
	VulkanRenderer(VulkanApplication* app, VulkanDevice* deviceObject);
	~VulkanRenderer();

	//Simple life cycle
	void Initialize();
	void Prepare();
	void Update();
	bool Render();

	// Create an empty window
	void CreatePresentationWindow(const int& windowWidth = 500, const int& windowHeight = 500);
	void SetImageLayout(VkImage image, 
                        VkImageAspectFlags aspectMask, 
                        VkImageLayout oldImageLayout, 
                        VkImageLayout newImageLayout, 
                        const VkImageSubresourceRange& subresourceRange, 
                        const VkCommandBuffer& cmd);

	//! Windows procedure method for handling events.
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	// Destroy the presentation window
	void DestroyPresentationWindow();

	// Getter functions for member variable specific to classes.
    VulkanApplication*             GetApplication()   { return _application; }
	VulkanDevice*                  GetDevice()		  { return _deviceObj; }
	VulkanSwapChain*               GetSwapChain() 	  { return _swapChainObj; }
	std::vector<VulkanDrawable*>*  GetDrawingItems()  { return &_drawableList; }
	VkCommandPool*                 GetCommandPool()	  { return &_cmdPool; }
	VulkanShader*                  GetShader()		  { return &_shaderObj; }
	VulkanPipeline*	               GetPipelineObject(){ return &_pipelineObj; }

	void CreateCommandPool();							// Create command pool
	void BuildSwapChainAndDepthImage();					// Create swapchain color image and depth image
	void CreateDepthImage();							// Create depth image
	void CreateVertexBuffer();
	void CreateRenderPass(bool includeDepth, bool clear = true);	// Render Pass creation
	void CreateFrameBuffer(bool includeDepth);
	void CreateShaders();
	void CreatePipelineStateManagement();
	void CreateDescriptors();
	void CreateTextureLinear (const char* filename, TextureData *texture, VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);
	void CreateTextureOptimal(const char* filename, TextureData *texture, VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);

	void DestroyCommandBuffer();
	void DestroyCommandPool();
	void DestroyDepthBuffer();
	void DestroyDrawableVertexBuffer();
	void DestroyRenderpass(); // Destroy the render pass object when no more required
	void DestroyFramebuffers();
	void DestroyPipeline();
	void DestroyDrawableCommandBuffer();
	void DestroyDrawableSynchronizationObjects();
	void DestroyDrawableUniformBuffer();
	void DestroyTextureResource();

#ifdef _WIN32
#define APP_NAME_STR_LEN 80
	HINSTANCE					_connection;			 // hInstance - Windows Instance
	char						_name[APP_NAME_STR_LEN]; // name - App name appearing on the window
	HWND						_window;				 // hWnd - the window handle
#else
	xcb_connection_t*			connection;
	xcb_screen_t*				screen;
	xcb_window_t				window;
	xcb_intern_atom_reply_t*	reply;
#endif

	struct
    {
		VkFormat		_format;
		VkImage			_image;
		VkDeviceMemory	_mem;
		VkImageView		_view;
	}_depth;

	VkCommandBuffer		_cmdDepthImage;			// Command buffer for depth image layout
	VkCommandPool		_cmdPool;				// Command pool
	VkCommandBuffer		_cmdVertexBuffer;		// Command buffer for vertex buffer - Triangle geometry
	VkCommandBuffer		_cmdTexture;			// Command buffer for creating the texture

	VkRenderPass		       _renderPass;	    // Render pass created object
	std::vector<VkFramebuffer> _framebuffers;	// Number of frame buffer corresponding to each swap chain
	std::vector<VkPipeline*>   _pipelineList;	// List of pipelines

	int					_width, _height;
	TextureData			_texture;

private:
	VulkanApplication* _application;
	// The device object associated with this Presentation layer.
	VulkanDevice*	             _deviceObj;
	VulkanSwapChain*             _swapChainObj;
	std::vector<VulkanDrawable*> _drawableList;
	VulkanShader 	             _shaderObj;
	VulkanPipeline 	             _pipelineObj;
};