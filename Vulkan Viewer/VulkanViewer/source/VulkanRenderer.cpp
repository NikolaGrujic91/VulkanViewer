#include "VulkanRenderer.h"
#include "VulkanApplication.h"
#include "Wrappers.h"
#include "MeshData.h"

VulkanRenderer::VulkanRenderer(VulkanApplication * app, VulkanDevice* deviceObject)
{
	// Note: It's very important to initilize the member with 0 or respective value other wise it will break the system
	memset(&_depth, 0, sizeof(_depth));
	memset(&_connection, 0, sizeof(HINSTANCE));				// hInstance - Windows Instance

	_application = app;
	_deviceObj	= deviceObject;

	_swapChainObj = new VulkanSwapChain(this);
    auto* drawableObj = new VulkanDrawable(this);
	_drawableList.push_back(drawableObj);
}

VulkanRenderer::~VulkanRenderer()
{
	delete _swapChainObj;
	_swapChainObj = nullptr;
	for (auto d : _drawableList)
	{
		delete d;
	}
	_drawableList.clear();
}

void VulkanRenderer::Initialize()
{

	// We need command buffers, so create a command buffer pool
	CreateCommandPool();
	
	// Let's create the swap chain color images and depth image
	BuildSwapChainAndDepthImage();

	// Build the vertex buffer 	
	CreateVertexBuffer();
	
	const bool includeDepth = true;
	// Create the render pass now..
	CreateRenderPass(includeDepth);
	
	// Use render pass and create frame buffer
	CreateFrameBuffer(includeDepth);

	// Create the vertex and fragment shader
	CreateShaders();

	const char* filename = "LearningVulkan.ktx";
	bool renderOptimalTexture = true;
	if (renderOptimalTexture) 
    {
		CreateTextureOptimal(filename, &_texture, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	}
	else {
		CreateTextureLinear(filename, &_texture, VK_IMAGE_USAGE_SAMPLED_BIT);
	}
	// Set the created texture in the drawable object.
	for (VulkanDrawable* drawableObj : _drawableList)
	{
		drawableObj->SetTextures(&_texture);
	}

	// Create descriptor set layout
	CreateDescriptors();

	// Manage the pipeline state objects
	CreatePipelineStateManagement();
}

void VulkanRenderer::Prepare()
{
	for (VulkanDrawable* drawableObj : _drawableList)
	{
		drawableObj->Prepare();
	}
}

void VulkanRenderer::Update()
{
	for (VulkanDrawable* drawableObj : _drawableList)
	{
		drawableObj->Update();
	}
}

bool VulkanRenderer::Render()
{
	MSG msg;   // message
	PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
	if (msg.message == WM_QUIT) 
    {
		return false;
	}
	TranslateMessage(&msg);
	DispatchMessage(&msg);
	RedrawWindow(_window, nullptr, nullptr, RDW_INTERNALPAINT);
	return true;
}

#ifdef _WIN32

// MS-Windows event handling function:
LRESULT CALLBACK VulkanRenderer::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	VulkanApplication* appObj = VulkanApplication::GetInstance();
	switch (uMsg)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		break;
	case WM_PAINT:
		for (VulkanDrawable* drawableObj : appObj->_rendererObj->_drawableList)
		{
			drawableObj->Render();
		}

		return 0;
	
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED) 
        {
			appObj->_rendererObj->_width = lParam & 0xffff;
			appObj->_rendererObj->_height = (lParam & 0xffff0000) >> 16;
			appObj->_rendererObj->GetSwapChain()->setSwapChainExtent(appObj->_rendererObj->_width, appObj->_rendererObj->_height);
			appObj->Resize();
		}
		break;

	default:
		break;
	}
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}

void VulkanRenderer::CreatePresentationWindow(const int& windowWidth, const int& windowHeight)
{
#ifdef _WIN32
	_width	= windowWidth;
	_height	= windowHeight; 
	assert(_width > 0 || _height > 0);

	WNDCLASSEX  winInfo;

	sprintf(_name, "Texture demo - Optimal Layout");
	memset(&winInfo, 0, sizeof(WNDCLASSEX));
	// Initialize the window class structure:
	winInfo.cbSize			= sizeof(WNDCLASSEX);
	winInfo.style			= CS_HREDRAW | CS_VREDRAW;
	winInfo.lpfnWndProc		= WndProc;
	winInfo.cbClsExtra		= 0;
	winInfo.cbWndExtra		= 0;
	winInfo.hInstance		= _connection; // hInstance
	winInfo.hIcon			= LoadIcon(nullptr, IDI_APPLICATION);
	winInfo.hCursor			= LoadCursor(nullptr, IDC_ARROW);
	winInfo.hbrBackground	= static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
	winInfo.lpszMenuName	= nullptr;
	winInfo.lpszClassName	= _name;
	winInfo.hIconSm			= LoadIcon(nullptr, IDI_WINLOGO);

	// Register window class:
	if (!RegisterClassEx(&winInfo)) 
    {
		// It didn't work, so try to give a useful error:
		printf("Unexpected error trying to start the application!\n");
		fflush(stdout);
		exit(1);
	}
	
	// Create window with the registered class:
	RECT wr = { 0, 0, _width, _height };
	AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
	_window = CreateWindowEx(0,
							_name,					// class name
							_name,					// app name
							WS_OVERLAPPEDWINDOW |	// window style
							WS_VISIBLE |
							WS_SYSMENU,
							100, 100,				// x/y coords
							wr.right - wr.left,     // width
							wr.bottom - wr.top,     // height
							nullptr,				// handle to parent
							nullptr,				// handle to menu
							_connection,			// hInstance
							nullptr);				// no extra parameters

	if (!_window) 
    {
		// It didn't work, so try to give a useful error:
		printf("Cannot create a window in which to draw!\n");
		fflush(stdout);
		exit(1);
	}

	SetWindowLongPtr(_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&_application));
#else
	const xcb_setup_t *setup;
	xcb_screen_iterator_t iter;
	int scr;

	connection = xcb_connect(NULL, &scr);
	if (connection == NULL) {
		std::cout << "Cannot find a compatible Vulkan ICD.\n";
		exit(-1);
	}

	setup = xcb_get_setup(connection);
	iter = xcb_setup_roots_iterator(setup);
	while (scr-- > 0)
		xcb_screen_next(&iter);

	screen = iter.data;
#endif // _WIN32
}

void VulkanRenderer::DestroyPresentationWindow()
{
	DestroyWindow(_window);
}
#else
void VulkanRenderer::createPresentationWindow()
{
	assert(width > 0);
	assert(height > 0);

	uint32_t value_mask, value_list[32];

	window = xcb_generate_id(connection);

	value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	value_list[0] = screen->black_pixel;
	value_list[1] = XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_EXPOSURE;

	xcb_create_window(connection, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, width, height, 0, 
		XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, value_mask, value_list);

	/* Magic code that will send notification when window is destroyed */
	xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 1, 12, "WM_PROTOCOLS");
	xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(connection, cookie, 0);

	xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(connection, 0, 16, "WM_DELETE_WINDOW");
	reply = xcb_intern_atom_reply(connection, cookie2, 0);

	xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, (*reply).atom, 4, 32, 1,	&(*reply).atom);
	free(reply);

	xcb_map_window(connection, window);

	// Force the x/y coordinates to 100,100 results are identical in consecutive runs
	const uint32_t coords[] = { 100,  100 };
	xcb_configure_window(connection, window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, coords);
	xcb_flush(connection);

	xcb_generic_event_t *e;
	while ((e = xcb_wait_for_event(connection))) {
		if ((e->response_type & ~0x80) == XCB_EXPOSE)
			break;
	}
}

void VulkanRenderer::destroyWindow()
{
	xcb_destroy_window(connection, window);
	xcb_disconnect(connection);
}

#endif // _WIN32

void VulkanRenderer::CreateCommandPool()
{
	VulkanDevice* deviceObj		= _application->_deviceObj;

    VkCommandPoolCreateInfo cmdPoolInfo;
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.pNext = nullptr;
	cmdPoolInfo.queueFamilyIndex = deviceObj->_graphicsQueueWithPresentIndex;
	cmdPoolInfo.flags = 0;

    const VkResult res = vkCreateCommandPool(deviceObj->_device, &cmdPoolInfo, nullptr, &_cmdPool);
	assert(res == VK_SUCCESS);
}

void VulkanRenderer::CreateDepthImage()
{
    VkImageCreateInfo imageInfo = {};

	// If the depth format is undefined, use fallback as 16-byte value
	if (_depth._format == VK_FORMAT_UNDEFINED) {
		_depth._format = VK_FORMAT_D16_UNORM;
	}

	const VkFormat depthFormat = _depth._format;

	imageInfo.sType			        = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext			        = nullptr;
	imageInfo.imageType		        = VK_IMAGE_TYPE_2D;
	imageInfo.format		        = depthFormat;
	imageInfo.extent.width	        = _width;
	imageInfo.extent.height         = _height;
	imageInfo.extent.depth	        = 1;
	imageInfo.mipLevels		        = 1;
	imageInfo.arrayLayers	        = 1;
	imageInfo.samples		        = NUM_SAMPLES;
	imageInfo.tiling		        = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.queueFamilyIndexCount = 0;
	imageInfo.pQueueFamilyIndices	= nullptr;
	imageInfo.sharingMode			= VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.usage					= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageInfo.flags					= 0;

	// User create image info and create the image objects
	VkResult result = vkCreateImage(_deviceObj->_device, &imageInfo, nullptr, &_depth._image);
	assert(result == VK_SUCCESS);

	// Get the image memory requirements
	VkMemoryRequirements memRqrmnt;
	vkGetImageMemoryRequirements(_deviceObj->_device,	_depth._image, &memRqrmnt);

	VkMemoryAllocateInfo memAlloc;
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAlloc.pNext = nullptr;
	memAlloc.allocationSize = 0;
	memAlloc.memoryTypeIndex = 0;
	memAlloc.allocationSize = memRqrmnt.size;
	// Determine the type of memory required with the help of memory properties
    const bool pass = _deviceObj->MemoryTypeFromProperties(memRqrmnt.memoryTypeBits, 0, /* No requirements */ &memAlloc.memoryTypeIndex);
	assert(pass);

	// Allocate the memory for image objects
	result = vkAllocateMemory(_deviceObj->_device, &memAlloc, nullptr, &_depth._mem);
	assert(result == VK_SUCCESS);

	// Bind the allocated memeory
	result = vkBindImageMemory(_deviceObj->_device, _depth._image, _depth._mem, 0);
	assert(result == VK_SUCCESS);


	VkImageViewCreateInfo imgViewInfo;
	imgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imgViewInfo.pNext = nullptr;
	imgViewInfo.image = VK_NULL_HANDLE;
	imgViewInfo.format = depthFormat;
	imgViewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY };
	imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	imgViewInfo.subresourceRange.baseMipLevel = 0;
	imgViewInfo.subresourceRange.levelCount = 1;
	imgViewInfo.subresourceRange.baseArrayLayer = 0;
	imgViewInfo.subresourceRange.layerCount = 1;
	imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imgViewInfo.flags = 0;

	if (depthFormat == VK_FORMAT_D16_UNORM_S8_UINT ||
		depthFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
		depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT) 
    {
		imgViewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	// Use command buffer to create the depth image. This includes -
	// Command buffer allocation, recording with begin/end scope and submission.
	CommandBufferMgr::allocCommandBuffer(&_deviceObj->_device, _cmdPool, &_cmdDepthImage);
	CommandBufferMgr::beginCommandBuffer(_cmdDepthImage);
	{
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 1;

		// Set the image layout to depth stencil optimal
		SetImageLayout(_depth._image,
			           imgViewInfo.subresourceRange.aspectMask,
			           VK_IMAGE_LAYOUT_UNDEFINED,
			           VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 
                       subresourceRange, 
                       _cmdDepthImage);
	}
	CommandBufferMgr::endCommandBuffer(_cmdDepthImage);
	CommandBufferMgr::submitCommandBuffer(_deviceObj->_queue, &_cmdDepthImage);

	// Create the image view and allow the application to use the images.
	imgViewInfo.image = _depth._image;
	result = vkCreateImageView(_deviceObj->_device, &imgViewInfo, nullptr, &_depth._view);
	assert(result == VK_SUCCESS);
}

void VulkanRenderer::CreateTextureOptimal(const char* filename, TextureData *texture, VkImageUsageFlags imageUsageFlags, VkFormat format)
{
	// Load the image 
	gli::texture2D image2D(gli::load(filename)); assert(!image2D.empty());

	// Get the image dimensions
	texture->textureWidth	= uint32_t(image2D[0].dimensions().x);
	texture->textureHeight	= uint32_t(image2D[0].dimensions().y);

	// Get number of mip-map levels
	texture->mipMapLevels	= uint32_t(image2D.levels());

	// Create a staging buffer resource states using.
	// Indicate it be the source of the transfer command.
	// .usage	= VK_BUFFER_USAGE_TRANSFER_SRC_BIT
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType	= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size	= image2D.size();
	bufferCreateInfo.usage	= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // Create a buffer resource (host-visible) -
	VkBuffer buffer;
	VkResult error = vkCreateBuffer(_deviceObj->_device, &bufferCreateInfo, nullptr, &buffer);
	assert(!error);
	
	// Get the buffer memory requirements for the staging buffer -
	VkMemoryRequirements memRqrmnt;
	VkDeviceMemory	devMemory;
	vkGetBufferMemoryRequirements(_deviceObj->_device, buffer, &memRqrmnt);

	VkMemoryAllocateInfo memAllocInfo;
	memAllocInfo.sType			= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.pNext			= nullptr;
	memAllocInfo.allocationSize = 0;
	memAllocInfo.memoryTypeIndex= 0;
	memAllocInfo.allocationSize = memRqrmnt.size;

	// Determine the type of memory required for the host-visible buffer  -
	_deviceObj->MemoryTypeFromProperties(memRqrmnt.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memAllocInfo.memoryTypeIndex);
		
	// Allocate the memory for host-visible buffer objects -
	error = vkAllocateMemory(_deviceObj->_device, &memAllocInfo, nullptr, &devMemory);
	assert(!error);

	// Bind the host-visible buffer with allocated device memory -
	error = vkBindBufferMemory(_deviceObj->_device, buffer, devMemory, 0);
	assert(!error);

	// Populate the raw image data into the device memory -
	uint8_t *data;
	error = vkMapMemory(_deviceObj->_device, devMemory, 0, memRqrmnt.size, 0, reinterpret_cast<void **>(&data));
	assert(!error);

	memcpy(data, image2D.data(), image2D.size());
	vkUnmapMemory(_deviceObj->_device, devMemory);

	// Create image info with optimal tiling support (.tiling = VK_IMAGE_TILING_OPTIMAL) -
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType			= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.pNext			= nullptr;
	imageCreateInfo.imageType		= VK_IMAGE_TYPE_2D;
	imageCreateInfo.format			= format;
	imageCreateInfo.mipLevels		= texture->mipMapLevels;
	imageCreateInfo.arrayLayers		= 1;
	imageCreateInfo.samples			= VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling			= VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.sharingMode		= VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.extent			= { texture->textureWidth, texture->textureHeight, 1 };
	imageCreateInfo.usage			= imageUsageFlags;

	// Set image object with VK_IMAGE_USAGE_TRANSFER_DST_BIT if
	// not set already. This allows to copy the source VkBuffer 
	// object (with VK_IMAGE_USAGE_TRANSFER_DST_BIT) contents
	// into this image object memory(destination).
	if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
    {
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	error = vkCreateImage(_deviceObj->_device, &imageCreateInfo, nullptr, &texture->image);
	assert(!error);

	// Get the image memory requirements
	vkGetImageMemoryRequirements(_deviceObj->_device, texture->image, &memRqrmnt);

	// Set the allocation size equal to the buffer allocation
	memAllocInfo.allocationSize = memRqrmnt.size;

	// Determine the type of memory required with the help of memory properties
	_deviceObj->MemoryTypeFromProperties(memRqrmnt.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memAllocInfo.memoryTypeIndex);

	// Allocate the physical memory on the GPU
	error = vkAllocateMemory(_deviceObj->_device, &memAllocInfo, nullptr, &texture->mem);
	assert(!error);

	// Bound the physical memory with the created image object 
	error = vkBindImageMemory(_deviceObj->_device, texture->image, texture->mem, 0);
	assert(!error);

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask				= VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel			= 0;
	subresourceRange.levelCount				= texture->mipMapLevels;
	subresourceRange.layerCount				= 1;

	// Use a separate command buffer for texture loading
	// Start command buffer recording
	CommandBufferMgr::allocCommandBuffer(&_deviceObj->_device, _cmdPool, &_cmdTexture);
	CommandBufferMgr::beginCommandBuffer(_cmdTexture);

	// set the image layout to be 
	// VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	// since it is destination for copying buffer 
	// into image using vkCmdCopyBufferToImage -
	SetImageLayout(texture->image,
                   VK_IMAGE_ASPECT_COLOR_BIT,
                   VK_IMAGE_LAYOUT_UNDEFINED,
		           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                   subresourceRange,
                   _cmdTexture);

	// List contains the buffer image copy for each mipLevel -
	std::vector<VkBufferImageCopy> bufferImgCopyList;


	uint32_t bufferOffset = 0;
	// Iterater through each mip level and set buffer image copy -
	for (uint32_t i = 0; i < texture->mipMapLevels; i++)
	{
		VkBufferImageCopy bufImgCopyItem = {};
		bufImgCopyItem.imageSubresource.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
		bufImgCopyItem.imageSubresource.mipLevel		= i;
		bufImgCopyItem.imageSubresource.layerCount		= 1;
		bufImgCopyItem.imageSubresource.baseArrayLayer	= 0;
		bufImgCopyItem.imageExtent.width				= uint32_t(image2D[i].dimensions().x);
		bufImgCopyItem.imageExtent.height				= uint32_t(image2D[i].dimensions().y);
		bufImgCopyItem.imageExtent.depth				= 1;
		bufImgCopyItem.bufferOffset						= bufferOffset;

		bufferImgCopyList.push_back(bufImgCopyItem);

		// adjust buffer offset
		bufferOffset += uint32_t(image2D[i].size());
	}

	// Copy the staging buffer memory data contain
	// the stage raw data(with mip levels) into image object
	vkCmdCopyBufferToImage(_cmdTexture,
                           buffer,
                           texture->image,	
		                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		                   uint32_t(bufferImgCopyList.size()),
                           bufferImgCopyList.data());

	// Advised to change the image layout to shader read
	// after staged buffer copied into image memory -
	texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	SetImageLayout(texture->image, 
                   VK_IMAGE_ASPECT_COLOR_BIT, 
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		           texture->imageLayout, 
                   subresourceRange,
                   _cmdTexture);

	// Submit command buffer containing copy and image layout commands-
	CommandBufferMgr::endCommandBuffer(_cmdTexture);

	// Create a fence object to ensure that the command buffer is executed,
	// coping our staged raw data from the buffers to image memory with
	// respective image layout and attributes into consideration -
	VkFence fence;
	VkFenceCreateInfo fenceCI	= {};
	fenceCI.sType				= VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCI.flags				= 0;

	error = vkCreateFence(_deviceObj->_device, &fenceCI, nullptr, &fence);
	assert(!error);

	VkSubmitInfo submitInfo			= {};
	submitInfo.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext				= nullptr;
	submitInfo.commandBufferCount	= 1;
	submitInfo.pCommandBuffers		= &_cmdTexture;

	CommandBufferMgr::submitCommandBuffer(_deviceObj->_queue, &_cmdTexture, &submitInfo, fence);

	error = vkWaitForFences(_deviceObj->_device, 1, &fence, VK_TRUE, 10000000000);
	assert(!error);

	vkDestroyFence(_deviceObj->_device, fence, nullptr);

	// destroy the allocated resoureces
	vkFreeMemory(_deviceObj->_device, devMemory, nullptr);
	vkDestroyBuffer(_deviceObj->_device, buffer, nullptr);

	///////////////////////////////////////////////////////////////////////////////////////

	// Create sampler
	VkSamplerCreateInfo samplerCI = {};
	samplerCI.sType						= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCI.pNext						= nullptr;
	samplerCI.magFilter					= VK_FILTER_LINEAR;
	samplerCI.minFilter					= VK_FILTER_LINEAR;
	samplerCI.mipmapMode				= VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCI.addressModeU				= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.addressModeV				= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.addressModeW				= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.mipLodBias				= 0.0f;
	if (_deviceObj->_deviceFeatures.samplerAnisotropy == VK_TRUE)
	{
		samplerCI.anisotropyEnable		= VK_TRUE;
		samplerCI.maxAnisotropy			= 8;
	}
	else
	{
		samplerCI.anisotropyEnable		= VK_FALSE;
		samplerCI.maxAnisotropy			= 1;
	}
	samplerCI.compareOp					= VK_COMPARE_OP_NEVER;
	samplerCI.minLod					= 0.0f;
	samplerCI.maxLod					= static_cast<float>(texture->mipMapLevels);
	samplerCI.borderColor				= VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	samplerCI.unnormalizedCoordinates	= VK_FALSE;

	error = vkCreateSampler(_deviceObj->_device, &samplerCI, nullptr, &texture->sampler);
	assert(!error);

	// Create image view to allow shader to access the texture information -
	VkImageViewCreateInfo viewCI = {};
	viewCI.sType			= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCI.pNext			= nullptr;
	viewCI.viewType			= VK_IMAGE_VIEW_TYPE_2D;
	viewCI.format			= format;
	viewCI.components.r		= VK_COMPONENT_SWIZZLE_R;
	viewCI.components.g		= VK_COMPONENT_SWIZZLE_G;
	viewCI.components.b		= VK_COMPONENT_SWIZZLE_B;
	viewCI.components.a		= VK_COMPONENT_SWIZZLE_A;
	viewCI.subresourceRange	= subresourceRange;
	viewCI.subresourceRange.levelCount	= texture->mipMapLevels; 	// Optimal tiling supports mip map levels very well set it.
	viewCI.image			= texture->image;

	error = vkCreateImageView(_deviceObj->_device, &viewCI, nullptr, &texture->view);
	assert(!error);

	// Fill descriptor image info that can be used for setting up descriptor sets
	texture->descsImgInfo.imageView = texture->view;
	texture->descsImgInfo.sampler = texture->sampler;
	texture->descsImgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
}

void VulkanRenderer::CreateTextureLinear(const char* filename, TextureData *texture, VkImageUsageFlags imageUsageFlags, VkFormat format)
{
	// Load the image 
	gli::texture2D image2D(gli::load(filename)); assert(!image2D.empty());

	// Get the image dimensions
	texture->textureWidth	= uint32_t(image2D[0].dimensions().x);
	texture->textureHeight	= uint32_t(image2D[0].dimensions().y);

	// Get number of mip-map levels
	texture->mipMapLevels	= uint32_t(image2D.levels());

	// Create image resource states using VkImageCreateInfo
	VkImageCreateInfo imageCreateInfo;
	imageCreateInfo.sType				    = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.pNext				    = nullptr;
	imageCreateInfo.imageType			    = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format				    = format;
	imageCreateInfo.extent.width		    = uint32_t(image2D[0].dimensions().x);
	imageCreateInfo.extent.height		    = uint32_t(image2D[0].dimensions().y);
	imageCreateInfo.extent.depth		    = 1;
	imageCreateInfo.mipLevels			    = texture->mipMapLevels;
	imageCreateInfo.arrayLayers			    = 1;
	imageCreateInfo.samples				    = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.queueFamilyIndexCount	= 0;
	imageCreateInfo.pQueueFamilyIndices	    = nullptr;
	imageCreateInfo.sharingMode			    = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.usage				    = imageUsageFlags;
	imageCreateInfo.flags				    = 0;
	imageCreateInfo.initialLayout		    = VK_IMAGE_LAYOUT_PREINITIALIZED,
	imageCreateInfo.tiling				    = VK_IMAGE_TILING_LINEAR;

	VkResult  error;
	// Use create image info and create the image objects
	error = vkCreateImage(_deviceObj->_device, &imageCreateInfo, nullptr, &texture->image);
	assert(!error);

	// Get the buffer memory requirements
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(_deviceObj->_device, texture->image, &memoryRequirements);

	// Create memory allocation metadata information
	VkMemoryAllocateInfo& memAlloc	= texture->memoryAlloc;
	memAlloc.sType					= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAlloc.pNext					= nullptr;
	memAlloc.allocationSize			= memoryRequirements.size;
	memAlloc.memoryTypeIndex		= 0;

	// Determine the type of memory required 
	// with the help of memory properties
	bool pass = _deviceObj->MemoryTypeFromProperties(memoryRequirements.memoryTypeBits,
		                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                                     &texture->memoryAlloc.memoryTypeIndex);
	assert(pass);

	// Allocate the memory for buffer objects
	error = vkAllocateMemory(_deviceObj->_device, &texture->memoryAlloc, nullptr, &(texture->mem));
	assert(!error);

	// Bind the image device memory 
	error = vkBindImageMemory(_deviceObj->_device, texture->image, texture->mem, 0);
	assert(!error);

	VkImageSubresource subresource;
	subresource.aspectMask			= VK_IMAGE_ASPECT_COLOR_BIT;
	subresource.mipLevel			= 0;
	subresource.arrayLayer			= 0;

	VkSubresourceLayout layout;
	uint8_t *data;

	vkGetImageSubresourceLayout(_deviceObj->_device, texture->image, &subresource, &layout);

	// Map the GPU memory on to local host
	error = vkMapMemory(_deviceObj->_device, texture->mem, 0, texture->memoryAlloc.allocationSize, 0, reinterpret_cast<void**>(&data));
	assert(!error);

	// Load image texture data in the mapped buffer
    auto* dataTemp = static_cast<uint8_t*>(image2D.data());
	for (int y = 0; y < image2D[0].dimensions().y; y++)
	{
	    const size_t imageSize = image2D[0].dimensions().y * 4;
		memcpy(data, dataTemp, imageSize);
		dataTemp += imageSize;

		// Advance row by row pitch information
		data += layout.rowPitch;
	}

	// UnMap the host memory to push the changes into the device memory
	vkUnmapMemory(_deviceObj->_device, texture->mem);
	
	// Command buffer allocation and recording begins
	CommandBufferMgr::allocCommandBuffer(&_deviceObj->_device, _cmdPool, &_cmdTexture);
	CommandBufferMgr::beginCommandBuffer(_cmdTexture);

	VkImageSubresourceRange subresourceRange	= {};
	subresourceRange.aspectMask					= VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel				= 0;
	subresourceRange.levelCount					= texture->mipMapLevels;
	subresourceRange.layerCount					= 1;

	texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	SetImageLayout(texture->image, 
                   VK_IMAGE_ASPECT_COLOR_BIT,
		           VK_IMAGE_LAYOUT_PREINITIALIZED, 
                   texture->imageLayout,
		           subresourceRange, 
                   _cmdTexture);

	// Stop command buffer recording
	CommandBufferMgr::endCommandBuffer(_cmdTexture);

	// Ensure that the GPU has finished the submitted job before host takes over again 
	VkFence fence;
	VkFenceCreateInfo fenceCI = {};
	fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCI.flags = 0;

	vkCreateFence(_deviceObj->_device, &fenceCI, nullptr, &fence);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &_cmdTexture;

	CommandBufferMgr::submitCommandBuffer(_deviceObj->_queue, &_cmdTexture, &submitInfo, fence);
	vkWaitForFences(_deviceObj->_device, 1, &fence, VK_TRUE, 10000000000);
	vkDestroyFence(_deviceObj->_device, fence, nullptr);

	// Specify a particular kind of texture using samplers
	VkSamplerCreateInfo samplerCI	= {};
	samplerCI.sType					= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCI.pNext					= nullptr;
	samplerCI.magFilter				= VK_FILTER_LINEAR;
	samplerCI.minFilter				= VK_FILTER_LINEAR;
	samplerCI.mipmapMode			= VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCI.addressModeU			= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.addressModeV			= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.addressModeW			= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.mipLodBias			= 0.0f;

	if (_deviceObj->_deviceFeatures.samplerAnisotropy == VK_TRUE)
	{
		samplerCI.anisotropyEnable	= VK_TRUE;
		samplerCI.maxAnisotropy		= 8;
	}
	else
	{
		samplerCI.anisotropyEnable	= VK_FALSE;
		samplerCI.maxAnisotropy		= 1;
	}

	samplerCI.compareOp				= VK_COMPARE_OP_NEVER;
	samplerCI.minLod				= 0.0f;
	samplerCI.maxLod				= 0.0f; // Set to texture->mipLevels if Optimal tiling, generally linear does not support mip-maping
	samplerCI.borderColor			= VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	samplerCI.unnormalizedCoordinates = VK_FALSE;

	// Create the sampler
	error = vkCreateSampler(_deviceObj->_device, &samplerCI, nullptr, &texture->sampler);
	assert(!error);

	// Create image view to allow shader to access the texture information -
	VkImageViewCreateInfo viewCi	   = {};
	viewCi.sType					   = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCi.pNext					   = nullptr;
	viewCi.viewType					   = VK_IMAGE_VIEW_TYPE_2D;
	viewCi.format					   = format;
	viewCi.components.r				   = VK_COMPONENT_SWIZZLE_R;
	viewCi.components.g				   = VK_COMPONENT_SWIZZLE_G;
	viewCi.components.b				   = VK_COMPONENT_SWIZZLE_B;
	viewCi.components.a				   = VK_COMPONENT_SWIZZLE_A;
	viewCi.subresourceRange			   = subresourceRange;
	viewCi.subresourceRange.levelCount = 1;
	viewCi.flags					   = 0;
	viewCi.image					   = texture->image;

	error = vkCreateImageView(_deviceObj->_device, &viewCi, nullptr, &texture->view);
	assert(!error);

	texture->descsImgInfo.sampler		= texture->sampler;
	texture->descsImgInfo.imageView		= texture->view;
	texture->descsImgInfo.imageLayout	= VK_IMAGE_LAYOUT_GENERAL;

	// Set the created texture in the drawable object.
	for (VulkanDrawable* drawableObj : _drawableList)
	{
		drawableObj->SetTextures(texture);
	}
}

void VulkanRenderer::CreateRenderPass(bool isDepthSupported, bool clear)
{
	// Dependency on VulkanSwapChain::createSwapChain() to 
	// get the color surface image and VulkanRenderer::createDepthBuffer()
	// to get the depth buffer image.

    // Attach the color buffer and depth buffer as an attachment to render pass instance
	VkAttachmentDescription attachments[2];
	attachments[0].format					= _swapChainObj->scPublicVars.format;
	attachments[0].samples					= NUM_SAMPLES;
	attachments[0].loadOp					= clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].storeOp					= VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp			= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp			= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout				= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachments[0].flags					= VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;

	// Is the depth buffer present the define attachment properties for depth buffer attachment.
	if (isDepthSupported)
	{
		attachments[1].format				= _depth._format;
		attachments[1].samples				= NUM_SAMPLES;
		attachments[1].loadOp				= clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].storeOp				= VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp		= VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[1].stencilStoreOp		= VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].initialLayout		= VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout			= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[1].flags				= VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
	}

	// Define the color buffer attachment binding point and layout information
	VkAttachmentReference colorReference;
	colorReference.attachment				= 0;
	colorReference.layout					= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Define the depth buffer attachment binding point and layout information
	VkAttachmentReference depthReference;
	depthReference.attachment				= 1;
	depthReference.layout					= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// Specify the attachments - color, depth, resolve, preserve etc.
	VkSubpassDescription subpass;
	subpass.pipelineBindPoint				= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.flags							= 0;
	subpass.inputAttachmentCount			= 0;
	subpass.pInputAttachments				= nullptr;
	subpass.colorAttachmentCount			= 1;
	subpass.pColorAttachments				= &colorReference;
	subpass.pResolveAttachments				= nullptr;
	subpass.pDepthStencilAttachment			= isDepthSupported ? &depthReference : nullptr;
	subpass.preserveAttachmentCount			= 0;
	subpass.pPreserveAttachments			= nullptr;

	// Specify the attachement and subpass associate with render pass
	VkRenderPassCreateInfo rpInfo			= {};
	rpInfo.sType							= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpInfo.pNext							= nullptr;
	rpInfo.attachmentCount					= isDepthSupported ? 2 : 1;
	rpInfo.pAttachments						= attachments;
	rpInfo.subpassCount						= 1;
	rpInfo.pSubpasses						= &subpass;
	rpInfo.dependencyCount					= 0;
	rpInfo.pDependencies					= nullptr;

	// Create the render pass object
    const VkResult result = vkCreateRenderPass(_deviceObj->_device, &rpInfo, nullptr, &_renderPass);
	assert(result == VK_SUCCESS);
}

void VulkanRenderer::CreateFrameBuffer(bool includeDepth)
{
    VkImageView attachments[2];
	attachments[1] = _depth._view;

	VkFramebufferCreateInfo fbInfo	= {};
	fbInfo.sType					= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbInfo.pNext					= nullptr;
	fbInfo.renderPass				= _renderPass;
	fbInfo.attachmentCount			= includeDepth ? 2 : 1;
	fbInfo.pAttachments				= attachments;
	fbInfo.width					= _width;
	fbInfo.height					= _height;
	fbInfo.layers					= 1;

    _framebuffers.clear();
	_framebuffers.resize(_swapChainObj->scPublicVars.swapchainImageCount);
	for (uint32_t i = 0; i < _swapChainObj->scPublicVars.swapchainImageCount; i++) 
    {
		attachments[0] = _swapChainObj->scPublicVars.colorBuffer[i].view;
        const VkResult result = vkCreateFramebuffer(_deviceObj->_device, &fbInfo, nullptr, &_framebuffers.at(i));
		assert(result == VK_SUCCESS);
	}
}

void VulkanRenderer::DestroyFramebuffers()
{
	for (uint32_t i = 0; i < _swapChainObj->scPublicVars.swapchainImageCount; i++)
    {
		vkDestroyFramebuffer(_deviceObj->_device, _framebuffers.at(i), nullptr);
	}
	_framebuffers.clear();
}

void VulkanRenderer::DestroyRenderpass()
{
	vkDestroyRenderPass(_deviceObj->_device, _renderPass, nullptr);
}

void VulkanRenderer::DestroyDrawableVertexBuffer()
{
	for (VulkanDrawable* drawableObj : _drawableList)
	{
		drawableObj->DestroyVertexBuffer();
	}
}

void VulkanRenderer::DestroyDrawableUniformBuffer()
{
	for (VulkanDrawable* drawableObj : _drawableList)
	{
		drawableObj->DestroyUniformBuffer();
	}
}

void VulkanRenderer::DestroyTextureResource()
{
	vkFreeMemory(_deviceObj->_device, _texture.mem, nullptr);
	vkDestroySampler(_deviceObj->_device, _texture.sampler, nullptr);
	vkDestroyImage(_deviceObj->_device, _texture.image, nullptr);
	vkDestroyImageView(_deviceObj->_device, _texture.view, nullptr);
}

void VulkanRenderer::DestroyDrawableCommandBuffer()
{
	for (VulkanDrawable* drawableObj : _drawableList)
	{
		drawableObj->DestroyCommandBuffer();
	}
}

void VulkanRenderer::DestroyDrawableSynchronizationObjects()
{
	for (VulkanDrawable* drawableObj : _drawableList)
	{
		drawableObj->DestroySynchronizationObjects();
	}
}

void VulkanRenderer::DestroyDepthBuffer()
{
	vkDestroyImageView(_deviceObj->_device, _depth._view, nullptr);
	vkDestroyImage(_deviceObj->_device, _depth._image, nullptr);
	vkFreeMemory(_deviceObj->_device, _depth._mem, nullptr);
}

void VulkanRenderer::DestroyCommandBuffer()
{
	VkCommandBuffer cmdBufs[] = { _cmdDepthImage, _cmdVertexBuffer, _cmdTexture };
	vkFreeCommandBuffers(_deviceObj->_device, _cmdPool, sizeof(cmdBufs)/sizeof(VkCommandBuffer), cmdBufs);
}

void VulkanRenderer::DestroyCommandPool()
{
	VulkanDevice* deviceObj		= _application->_deviceObj;

	vkDestroyCommandPool(deviceObj->_device, _cmdPool, nullptr);
}

void VulkanRenderer::BuildSwapChainAndDepthImage()
{
	// Get the appropriate queue to submit the command into
	_deviceObj->GetDeviceQueue();

	// Create swapchain and get the color image
	_swapChainObj->createSwapChain(_cmdDepthImage);
	
	// Create the depth image
	CreateDepthImage();
}

void VulkanRenderer::CreateVertexBuffer()
{
	CommandBufferMgr::allocCommandBuffer(&_deviceObj->_device, _cmdPool, &_cmdVertexBuffer);
	CommandBufferMgr::beginCommandBuffer(_cmdVertexBuffer);

	for (VulkanDrawable* drawableObj : _drawableList)
	{
		drawableObj->CreateVertexBuffer(geometryData, sizeof(geometryData), sizeof(geometryData[0]), false);
	}
	CommandBufferMgr::endCommandBuffer(_cmdVertexBuffer);
	CommandBufferMgr::submitCommandBuffer(_deviceObj->_queue, &_cmdVertexBuffer);
}

void VulkanRenderer::CreateShaders()
{
    if (_application->_isResizing)
    {
        return;
    }

	void* vertShaderCode, *fragShaderCode;
	size_t sizeVert, sizeFrag;

#ifdef AUTO_COMPILE_GLSL_TO_SPV
	vertShaderCode = readFile("Texture.vert", &sizeVert);
	fragShaderCode = readFile("Texture.frag", &sizeFrag);
	
	shaderObj.buildShader((const char*)vertShaderCode, (const char*)fragShaderCode);
#else
	vertShaderCode = readFile("Texture-vert.spv", &sizeVert);
	fragShaderCode = readFile("Texture-frag.spv", &sizeFrag);

	_shaderObj.buildShaderModuleWithSPV(static_cast<uint32_t*>(vertShaderCode), sizeVert, static_cast<uint32_t*>(fragShaderCode), sizeFrag);
#endif
}

// Create the descriptor set
void VulkanRenderer::CreateDescriptors()
{
	for (VulkanDrawable* drawableObj : _drawableList)
	{
		// It is upto an application how it manages the 
		// creation of descriptor. Descriptors can be cached 
		// and reuse for all similar objects.
		drawableObj->CreateDescriptorSetLayout(true);

		// Create the descriptor set
		drawableObj->CreateDescriptor(true);
	}

}

void VulkanRenderer::CreatePipelineStateManagement()
{
	for (VulkanDrawable* drawableObj : _drawableList)
	{
		// Use the descriptor layout and create the pipeline layout.
		drawableObj->CreatePipelineLayout();
	}

	_pipelineObj.CreatePipelineCache();

	const bool depthPresent = true;
	for (VulkanDrawable* drawableObj : _drawableList)
	{
	    auto* pipeline = static_cast<VkPipeline*>(malloc(sizeof(VkPipeline)));
		if (_pipelineObj.CreatePipeline(drawableObj, pipeline, &_shaderObj, depthPresent))
		{
			_pipelineList.push_back(pipeline);
			drawableObj->SetPipeline(pipeline);
		}
		else
		{
			free(pipeline);
			pipeline = nullptr;
		}
	}
}

void VulkanRenderer::SetImageLayout(VkImage image,
                                    VkImageAspectFlags aspectMask,
                                    VkImageLayout oldImageLayout, 
                                    VkImageLayout newImageLayout,
                                    const VkImageSubresourceRange& subresourceRange, 
                                    const VkCommandBuffer& cmd)
{
	// Dependency on cmd
	assert(cmd != VK_NULL_HANDLE);
	
	// The deviceObj->queue must be initialized
	assert(_deviceObj->_queue != VK_NULL_HANDLE);

	VkImageMemoryBarrier imgMemoryBarrier = {};
	imgMemoryBarrier.sType			  = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imgMemoryBarrier.pNext			  = nullptr;
	imgMemoryBarrier.srcAccessMask	  = 0;
	imgMemoryBarrier.dstAccessMask	  = 0;
	imgMemoryBarrier.oldLayout		  = oldImageLayout;
	imgMemoryBarrier.newLayout		  = newImageLayout;
	imgMemoryBarrier.image			  = image;
	imgMemoryBarrier.subresourceRange = subresourceRange;

	if (oldImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
		imgMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	}

	// Source layouts (old)
	switch (oldImageLayout)
	{
		case VK_IMAGE_LAYOUT_UNDEFINED:
			imgMemoryBarrier.srcAccessMask = 0;
			break;
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			imgMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			imgMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			imgMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	}

	switch (newImageLayout)
	{
	    // Ensure that anything that was copying from this image has completed
	    // An image in this layout can only be used as the destination operand of the commands
	    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
	    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
	    	imgMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	    	break;

	    // Ensure any Copy or CPU writes to image are flushed
	    // An image in this layout can only be used as a read-only shader resource
	    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
	    	imgMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	    	imgMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	    	break;

	    // An image in this layout can only be used as a framebuffer color attachment
	    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
	    	imgMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	    	break;

	    // An image in this layout can only be used as a framebuffer depth/stencil attachment
	    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
	    	imgMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	    	break;
	}

    const VkPipelineStageFlags srcStages	= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    const VkPipelineStageFlags destStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	vkCmdPipelineBarrier(cmd, srcStages, destStages, 0, 0, nullptr, 0, nullptr, 1, &imgMemoryBarrier);
}

// Destroy each pipeline object existing in the renderer
void VulkanRenderer::DestroyPipeline()
{
	for (VkPipeline* pipeline : _pipelineList)
	{
		vkDestroyPipeline(_deviceObj->_device, *pipeline, nullptr);
		free(pipeline);
	}
	_pipelineList.clear();
}
