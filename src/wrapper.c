#include "wrapper.h"

#if defined(VK_USE_PLATFORM_ANDROID_KHR)

void android_main(struct android_app *app) {
	wrapper_main();
}

#elif defined(VK_USE_PLATFORM_WIN32_KHR)

int WINAPI
WinMain(HINSTANCE h_inst, HINSTANCE h_prev_inst, LPSTR cmd, int cmd_show) {
	wrapper_main();
}

#else

int main(int argc, char **argv) {
	wrapper_main();
}

#endif

wr_window_t wr_window(const char* title) {
	wr_window_t window = {
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
#else
		.connection = xcb_connect(NULL, NULL),
		.screen = xcb_setup_roots_iterator(
			xcb_get_setup(window.connection)).data,
		.window = xcb_generate_id(window.connection),
#endif
	};
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
#else
	// Create Window
	uint32_t event_mask = XCB_EVENT_MASK_STRUCTURE_NOTIFY
		|XCB_EVENT_MASK_BUTTON_PRESS|XCB_EVENT_MASK_BUTTON_RELEASE
		|XCB_EVENT_MASK_POINTER_MOTION|XCB_EVENT_MASK_KEY_PRESS
		|XCB_EVENT_MASK_KEY_RELEASE;
	xcb_create_window(window.connection, XCB_COPY_FROM_PARENT, window.window,
		window.screen->root, 0, 0, 800, 600, 0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT, window.screen->root_visual,
		XCB_CW_EVENT_MASK, &event_mask);
	// change window title:
	xcb_change_property(window.connection, XCB_PROP_MODE_REPLACE, window.window,
		XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(title), title);
	// prepare for the close event:
 	xcb_intern_atom_cookie_t cookie = xcb_intern_atom(window.connection, 1, 12, "WM_PROTOCOLS");
  	xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(window.connection, cookie, 0);
  	xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(window.connection, 0, 16, "WM_DELETE_WINDOW");
  	xcb_intern_atom_reply_t* reply2 = xcb_intern_atom_reply(window.connection, cookie2, 0);
	xcb_change_property(window.connection, XCB_PROP_MODE_REPLACE, window.window, (*reply).atom, 4, 32, 1, &(*reply2).atom);
	// Map Window
  	xcb_map_window(window.connection, window.window);
#endif
	return window;
}

int8_t wr_window_event(wr_window_t window, uint32_t* event, float* x, float* y){
#if defined(VK_USE_PLATFORM_ANDROID_KHR)

#elif defined(VK_USE_PLATFORM_WIN32_KHR)

#else
	xcb_generic_event_t* ev = xcb_poll_for_event(window.connection);
	if(ev) {
		uint8_t type = ev->response_type & ~0x80;
		if(type == XCB_CLIENT_MESSAGE) {
			return 0; // Kill it!
		}else if(type == XCB_CONFIGURE_NOTIFY) {
			xcb_configure_notify_event_t *cfg =
				(xcb_configure_notify_event_t *)ev;
			
			*x = cfg->width;
			*y = cfg->height;
			*event = WR_EVENT_RESIZE;
		}else{
			*event = 0;
		}
		free(ev);
	}else{
		*event = 0;
	}
	return 1;
#endif
}

// Vulkan

static inline void wr_vulkan_error(const char *msg, VkResult result) {
	if(result != VK_SUCCESS) {
		puts(msg);
		abort();
	}
}

wr_vulkan_t wr_vulkan(const char* title) {
	wr_vulkan_t vulkan;

	const char *extensions[] = { "VK_KHR_surface", "VK_KHR_xcb_surface" };
	const VkApplicationInfo applicationInfo = { 
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = title,
		.engineVersion = 1,
		.apiVersion = VK_MAKE_VERSION(1, 0, 0),
	};
	const VkInstanceCreateInfo instance_ci = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &applicationInfo,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = NULL,
		.enabledExtensionCount = 2,
		.ppEnabledExtensionNames = extensions,
	};
	wr_vulkan_error("Failed to create vulkan instance.",
		vkCreateInstance(&instance_ci, NULL, &vulkan.instance));
	return vulkan;
}

static inline void wr_vulkan_window(wr_vulkan_t* vulkan, wr_window_t window) {
	VkXcbSurfaceCreateInfoKHR surface_ci = { 
		.sType =  VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
		.connection = window.connection,
		.window = window.window,
	};
	wr_vulkan_error("Could not create surface.", vkCreateXcbSurfaceKHR(
		vulkan->instance, &surface_ci, NULL, &vulkan->surface));
}

static inline void wr_vulkan_gpu(wr_vulkan_t* vulkan) {
	uint32_t num_gpus = 0;
	vkEnumeratePhysicalDevices(vulkan->instance, &num_gpus, NULL);
	VkPhysicalDevice gpus[num_gpus];
	vkEnumeratePhysicalDevices(vulkan->instance, &num_gpus, gpus);
	uint8_t found = 0;
	for(uint32_t i = 0; i < num_gpus; i++) {
		VkPhysicalDeviceProperties device_props;
		vkGetPhysicalDeviceProperties(gpus[i], &device_props);
		uint32_t qfc = 0; // queue family count
		vkGetPhysicalDeviceQueueFamilyProperties(gpus[i], &qfc, NULL);
		VkQueueFamilyProperties qfp[qfc]; // qf properties
		vkGetPhysicalDeviceQueueFamilyProperties(gpus[i], &qfc, qfp);
		for(uint32_t j = 0; j < qfc; j++) {
			VkBool32 supportsPresent;
			vkGetPhysicalDeviceSurfaceSupportKHR(gpus[i], j,
				vulkan->surface, &supportsPresent);
			if(supportsPresent != 0 &&
				(qfp[j].queueFlags &VK_QUEUE_GRAPHICS_BIT) != 0)
			{
				vulkan->gpu = gpus[i];
				vulkan->gpu_props = device_props;
				vulkan->present_queue_index = j;
				found = 1;
				break;
			}
		}
	}
	if(found == 0) {
		puts("Can't find a physical device that can render & present!");
		abort();
	}
	vkGetPhysicalDeviceMemoryProperties(vulkan->gpu,&vulkan->gpu_mem_props);
}

static inline void wr_vulkan_device(wr_vulkan_t* vulkan) {
	float queuePriorities[] = { 1.0f };
	VkDeviceQueueCreateInfo queue_ci = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = vulkan->present_queue_index,
		.queueCount = 1,
		.pQueuePriorities = queuePriorities,
	};
	const char *device_extension = "VK_KHR_swapchain";
	VkPhysicalDeviceFeatures features = {
		.shaderClipDistance = VK_TRUE
	};
	VkDeviceCreateInfo device_ci = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &queue_ci,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = NULL,
		.enabledExtensionCount = 1,
		.ppEnabledExtensionNames = &device_extension,
		.pEnabledFeatures = &features,
	};
	wr_vulkan_error("Failed to create logical device!", vkCreateDevice(
		vulkan->gpu, &device_ci, NULL, &vulkan->device));
}

static inline void wr_vulkan_queue(wr_vulkan_t* vulkan) {
	vkGetDeviceQueue(vulkan->device, vulkan->present_queue_index, 0,
		&vulkan->present_queue);
}

static inline void wr_vulkan_command_buffer(wr_vulkan_t* vulkan) {
	VkCommandPool commandPool;
	VkCommandPoolCreateInfo command_pool_ci = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = vulkan->present_queue_index,
	};
	wr_vulkan_error("Failed to create command pool.", vkCreateCommandPool(
		vulkan->device, &command_pool_ci, NULL, &commandPool));
	VkCommandBufferAllocateInfo command_buffer_ai = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	wr_vulkan_error("Failed to allocate draw command buffer.",
		vkAllocateCommandBuffers(vulkan->device, &command_buffer_ai,
			&vulkan->command_buffer));
}

static inline void wr_vulkan_swapchain(wr_vulkan_t* vulkan) {
	// Find preferred format:
	uint32_t formatCount = 1;
	VkSurfaceFormatKHR surface_format;
	vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan->gpu, vulkan->surface,
		&formatCount, &surface_format);
	vulkan->color_format = surface_format.format == VK_FORMAT_UNDEFINED ?
		VK_FORMAT_B8G8R8_UNORM : surface_format.format;
	VkColorSpaceKHR colorSpace = surface_format.colorSpace;
	VkSurfaceCapabilitiesKHR surface_capables;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan->gpu, vulkan->surface,
		&surface_capables);
	uint32_t desiredImageCount = 2;
	// If double-buffering isn't supported, single-buffering will suffice.
	if(surface_capables.maxImageCount < 2) desiredImageCount = 1;
	// Surface Resolution
	VkExtent2D surfaceResolution = surface_capables.currentExtent;
	if(surfaceResolution.width == -1) {
		surfaceResolution.width = vulkan->width;
		surfaceResolution.height = vulkan->height;
	} else {
		vulkan->width = surfaceResolution.width;
		vulkan->height = surfaceResolution.height;
	}
	// Choose a present mode.
	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan->gpu, vulkan->surface,
		&presentModeCount, NULL);
	VkPresentModeKHR presentModes[presentModeCount];
	vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan->gpu, vulkan->surface,
		&presentModeCount, presentModes);
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // fallback
	for(uint32_t i = 0; i < presentModeCount; i++) {
		if(presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR; // optimal
			break;
		}
	}
	// Create the swapchain.
	VkSwapchainCreateInfoKHR swapChainCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = vulkan->surface,
		.minImageCount = desiredImageCount,
		.imageFormat = vulkan->color_format,
		.imageColorSpace = colorSpace,
		.imageExtent = surfaceResolution,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = (surface_capables.supportedTransforms & 
			VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
			? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
			: surface_capables.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = present_mode,
		.clipped = 1,
		.oldSwapchain = NULL,
	};
	wr_vulkan_error("Failed to create swapchain.", vkCreateSwapchainKHR(
		vulkan->device, &swapChainCreateInfo, NULL, &vulkan->swapchain));
	vkGetSwapchainImagesKHR(vulkan->device, vulkan->swapchain,
		&vulkan->image_count, NULL);
	vkGetSwapchainImagesKHR(vulkan->device, vulkan->swapchain,
		&vulkan->image_count, vulkan->present_images);
}

static inline void wr_vulkan_image_view(wr_vulkan_t* vulkan) {
	VkComponentMapping components = {
		.r = VK_COMPONENT_SWIZZLE_R, .g = VK_COMPONENT_SWIZZLE_G,
		.b = VK_COMPONENT_SWIZZLE_B, .a = VK_COMPONENT_SWIZZLE_A,
	};
	VkImageViewCreateInfo presentImagesViewCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = vulkan->color_format,
		.components = components,
		.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel = 0,
		.subresourceRange.levelCount = 1,
		.subresourceRange.baseArrayLayer = 0,
		.subresourceRange.layerCount = 1,
	};

	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	VkFenceCreateInfo fenceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
	};
	vkCreateFence(vulkan->device, &fenceCreateInfo, NULL,
		&vulkan->submit_fence);

	for(uint32_t i = 0; i < vulkan->image_count; i++) {
		vkBeginCommandBuffer(vulkan->command_buffer, &beginInfo);
		presentImagesViewCreateInfo.image = vulkan->present_images[i];

		VkImageMemoryBarrier layoutTransitionBarrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = vulkan->present_images[i],
		};
		VkImageSubresourceRange resourceRange = {
			VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
		};
		layoutTransitionBarrier.subresourceRange = resourceRange;

		vkCmdPipelineBarrier(vulkan->command_buffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 
			1, &layoutTransitionBarrier);

		vkEndCommandBuffer(vulkan->command_buffer);

		VkPipelineStageFlags waitStageMash =
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo submitInfo = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 0,
			.pWaitSemaphores = NULL,
			.pWaitDstStageMask = &waitStageMash,
			.commandBufferCount = 1,
			.pCommandBuffers = &vulkan->command_buffer,
			.signalSemaphoreCount = 0,
			.pSignalSemaphores = NULL,
		};
		wr_vulkan_error("couldn't submit queue", vkQueueSubmit(
			vulkan->present_queue, 1, &submitInfo,
			vulkan->submit_fence));

		vkWaitForFences(vulkan->device, 1, &vulkan->submit_fence,
			VK_TRUE, UINT64_MAX);
		vkResetFences(vulkan->device, 1, &vulkan->submit_fence);

		vkResetCommandBuffer(vulkan->command_buffer, 0);

		wr_vulkan_error("Could not create ImageView.", vkCreateImageView(
			vulkan->device, &presentImagesViewCreateInfo, NULL,
			&vulkan->present_image_views[i]));
	}
}

static inline void wr_vulkan_depth_buffer(wr_vulkan_t* vulkan) {
	VkImageCreateInfo imageCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_D16_UNORM,
		.extent = {
			.width = vulkan->width,
			.height = vulkan->height,
			.depth = 1,
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	wr_vulkan_error("Failed to create depth image.", vkCreateImage(
		vulkan->device, &imageCreateInfo, NULL, &vulkan->depth_image));

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(vulkan->device, vulkan->depth_image,
		&memoryRequirements);

	VkMemoryAllocateInfo image_ai = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = 0,
	};
	wr_vulkan_error("Failed to allocate device memory.", vkAllocateMemory(
		vulkan->device, &image_ai, NULL, &vulkan->depth_image_memory));
	wr_vulkan_error("Failed to bind image memory.", vkBindImageMemory(
		vulkan->device, vulkan->depth_image, vulkan->depth_image_memory,
		0));

	// before using this depth buffer we must change it's layout:
	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	vkBeginCommandBuffer(vulkan->command_buffer, &beginInfo);

	VkImageSubresourceRange resourceRange = {
		VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1
	};
	VkImageMemoryBarrier layoutTransitionBarrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = vulkan->depth_image,
		.subresourceRange = resourceRange,
	};

	vkCmdPipelineBarrier(vulkan->command_buffer, 
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1,
		&layoutTransitionBarrier);

	vkEndCommandBuffer(vulkan->command_buffer);

	VkPipelineStageFlags waitStageMash = 
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 0,
		.pWaitSemaphores = NULL,
		.pWaitDstStageMask = &waitStageMash,
		.commandBufferCount = 1,
		.pCommandBuffers = &vulkan->command_buffer,
		.signalSemaphoreCount = 0,
		.pSignalSemaphores = NULL,
	};
	wr_vulkan_error("couldn't submit queue", vkQueueSubmit(
		vulkan->present_queue, 1, &submitInfo, vulkan->submit_fence));

	vkWaitForFences(vulkan->device, 1, &vulkan->submit_fence, VK_TRUE,
		UINT64_MAX);
	vkResetFences(vulkan->device, 1, &vulkan->submit_fence);
	vkResetCommandBuffer(vulkan->command_buffer, 0);

	// create the depth image view:
	VkComponentMapping depthComponents = { VK_COMPONENT_SWIZZLE_IDENTITY };
	VkImageViewCreateInfo imageViewCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = vulkan->depth_image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = imageCreateInfo.format,
		.components = depthComponents,
		.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
		.subresourceRange.baseMipLevel = 0,
		.subresourceRange.levelCount = 1,
		.subresourceRange.baseArrayLayer = 0,
		.subresourceRange.layerCount = 1,
	};
	wr_vulkan_error("Failed to create image view.", vkCreateImageView(
		vulkan->device, &imageViewCreateInfo, NULL,
		&vulkan->depth_image_view));
}

static inline void wr_vulkan_render_pass(wr_vulkan_t* vulkan) {
	VkAttachmentDescription passAttachments[2] = {
		{
			.format = vulkan->color_format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		}, {
			.format = VK_FORMAT_D16_UNORM,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout =
			    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.finalLayout =
			    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		}
	};
	VkAttachmentReference colorAttachmentReference = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};
	VkAttachmentReference depthAttachmentReference = {
		.attachment = 1,
		.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	};
	VkSubpassDescription subpass = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentReference,
		.pDepthStencilAttachment = &depthAttachmentReference,
	};
	VkRenderPassCreateInfo render_pass_ci = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 2,
		.pAttachments = passAttachments,
		.subpassCount = 1,
		.pSubpasses = &subpass,
	};
	wr_vulkan_error("Failed to create renderpass!", vkCreateRenderPass(
		vulkan->device, &render_pass_ci, NULL, &vulkan->render_pass));
}

static inline void wr_vulkan_framebuffers(wr_vulkan_t* vulkan) {
	VkImageView frameBufferAttachments[2];
	frameBufferAttachments[1] = vulkan->depth_image_view;

	VkFramebufferCreateInfo frameBufferCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = vulkan->render_pass,
		.attachmentCount = 2,  // must be equal to the attachment count on render pass
		.pAttachments = frameBufferAttachments,
		.width = vulkan->width,
		.height = vulkan->height,
		.layers = 1,
	};

	// create a framebuffer per swap chain imageView:
	for(uint32_t i = 0; i < vulkan->image_count; i++) {
		frameBufferAttachments[0] = vulkan->present_image_views[i];
		wr_vulkan_error("Failed to create framebuffer.",
			vkCreateFramebuffer(vulkan->device,
				&frameBufferCreateInfo, NULL,
				&vulkan->frame_buffers[i]));
	}
}

void wr_vulkan_shape(wr_shape_t* shape, wr_vulkan_t vulkan, const float* v) {
	float vertex[4];

	// Create our vertex buffer:
	VkBufferCreateInfo vertex_buffer_ci = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(vertex) * 3, // size in Bytes
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL,
	};
	wr_vulkan_error("Failed to create vertex input buffer.", vkCreateBuffer(
		vulkan.device, &vertex_buffer_ci, NULL,
		&shape->vertex_input_buffer));
	// Allocate memory for buffer.
	VkMemoryRequirements vertexBufferMemoryRequirements;
	vkGetBufferMemoryRequirements(vulkan.device, shape->vertex_input_buffer,
		&vertexBufferMemoryRequirements);
	VkMemoryAllocateInfo bufferAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = NULL,
		.allocationSize = vertexBufferMemoryRequirements.size,
		.memoryTypeIndex = 0,
	};
	VkDeviceMemory vertexBufferMemory;
	wr_vulkan_error("Failed to allocate buffer memory.", vkAllocateMemory(
		vulkan.device, &bufferAllocateInfo, NULL, &vertexBufferMemory));
	// Copy buffer data.
	void *mapped;
	wr_vulkan_error("Failed to map buffer memory.", vkMapMemory(
		vulkan.device,vertexBufferMemory,0,VK_WHOLE_SIZE,0,&mapped));
	memcpy(mapped, v, sizeof(float) * 12);
	vkUnmapMemory(vulkan.device, vertexBufferMemory);
	wr_vulkan_error("Failed to bind buffer memory.", vkBindBufferMemory(
		vulkan.device,shape->vertex_input_buffer,vertexBufferMemory,0));
}

void wr_vulkan_shader(wr_shader_t* shader, wr_vulkan_t vulkan,
	void* vdata, uint32_t vsize, void* fdata, uint32_t fsize)
{
	// Vertex Shader
	VkShaderModuleCreateInfo vertexShaderCreationInfo = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = vsize,
		.pCode = (void *)vdata,
	};
	wr_vulkan_error("Failed to create vertex shader.", vkCreateShaderModule(
		vulkan.device,&vertexShaderCreationInfo,NULL,&shader->vertex));
	// Fragment Shader
	VkShaderModuleCreateInfo fragmentShaderCreationInfo = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = fsize,
		.pCode = (void *)fdata,
	};
	wr_vulkan_error("Failed to create vertex shader.", vkCreateShaderModule(
		vulkan.device,&fragmentShaderCreationInfo,NULL,&shader->fragment));
}

void wr_vulkan_pipeline(wr_vulkan_t* vulkan, wr_shader_t* shaders, uint32_t ns){
	// empty pipeline layout:
	VkPipelineLayoutCreateInfo layoutCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 0,
		.pSetLayouts = NULL,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = NULL,
	};
	wr_vulkan_error("Failed to create pipeline layout.",
		vkCreatePipelineLayout(vulkan->device, &layoutCreateInfo, NULL,
			&vulkan->pipeline_layout));
	// setup shader stages:
	VkPipelineShaderStageCreateInfo shaderStageCreateInfo[2] = {{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = shaders[0].vertex,
		.pName = "main", // shader main function name
		.pSpecializationInfo = NULL,
	}, {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = shaders[0].fragment,
		.pName = "main", // shader main function name
		.pSpecializationInfo = NULL,
	}};
	// vertex input configuration:
	VkVertexInputBindingDescription vertexBindingDescription = {
		.binding = 0,
		.stride = sizeof(float) * 4,
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};
	VkVertexInputAttributeDescription vertexAttributeDescription = {
		.location = 0,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32A32_SFLOAT,
		.offset = 0,
	};
	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertexBindingDescription,
		.vertexAttributeDescriptionCount = 1,
		.pVertexAttributeDescriptions = &vertexAttributeDescription,
	};
	// vertex topology config:
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};
	// viewport config:
	VkViewport viewport = {
		.x = 0, .y = 0,
		.width = vulkan->width, .height = vulkan->height,
		.minDepth = 0, .maxDepth = 1,
	};
	VkRect2D scissors = {
		.extent.width = vulkan->width, .extent.height = vulkan->height,
	};
	VkPipelineViewportStateCreateInfo viewportState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissors,
	};
	// rasterization config:
	VkPipelineRasterizationStateCreateInfo rasterizationState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_NONE,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.depthBiasConstantFactor = 0,
		.depthBiasClamp = 0,
		.depthBiasSlopeFactor = 0,
		.lineWidth = 1,
	};
	// sampling config:
	VkPipelineMultisampleStateCreateInfo multisampleState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 0,
		.pSampleMask = NULL,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};
	// depth/stencil config:
	VkStencilOpState noOPStencilState = {
		.failOp = VK_STENCIL_OP_KEEP,
		.passOp = VK_STENCIL_OP_KEEP,
		.depthFailOp = VK_STENCIL_OP_KEEP,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.compareMask = 0,
		.writeMask = 0,
		.reference = 0,
	};
	VkPipelineDepthStencilStateCreateInfo depthState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
		.front = noOPStencilState,
		.back = noOPStencilState,
		.minDepthBounds = 0,
		.maxDepthBounds = 0,
	};
	// color blend config: (Actually off) TODO: blend?
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
		.blendEnable = VK_FALSE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = 0xf,
	};
	VkPipelineColorBlendStateCreateInfo colorBlendState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_CLEAR,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachmentState,
		.blendConstants[0] = 0.0,
		.blendConstants[1] = 0.0,
		.blendConstants[2] = 0.0,
		.blendConstants[3] = 0.0,
	};
	VkDynamicState dynamicState[2] = {
		VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dynamicState,
	};
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = shaderStageCreateInfo,
		.pVertexInputState = &vertexInputStateCreateInfo,
		.pInputAssemblyState = &inputAssemblyStateCreateInfo,
		.pTessellationState = NULL,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizationState,
		.pMultisampleState = &multisampleState,
		.pDepthStencilState = &depthState,
		.pColorBlendState = &colorBlendState,
		.pDynamicState = &dynamicStateCreateInfo,
		.layout = vulkan->pipeline_layout,
		.renderPass = vulkan->render_pass,
		.subpass = 0,
		.basePipelineHandle = NULL,
		.basePipelineIndex = 0,
	};

	wr_vulkan_error("Failed to create graphics pipeline.",
		vkCreateGraphicsPipelines(vulkan->device, VK_NULL_HANDLE, 1,
		&pipelineCreateInfo, NULL, &vulkan->pipeline));
}

static inline void
wr_vulkan_draw_on_window(wr_vulkan_t* vulkan,wr_window_t window,uint8_t fps) {
	vkDeviceWaitIdle(vulkan->device);
	usleep(1000000 / fps);
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
#else
	xcb_flush(window.connection);
#endif
}

void wr_vulkan_resize(wr_vulkan_t* vulkan) {
	wr_vulkan_swapchain(vulkan); // Link Swapchain to Vulkan Instance
	wr_vulkan_image_view(vulkan); // Link Image Views for each framebuffer
	wr_vulkan_depth_buffer(vulkan); // Link Depth Buffer to swapchain
	wr_vulkan_render_pass(vulkan); // Link Render Pass to swapchain
	wr_vulkan_framebuffers(vulkan); // Link Framebuffers to swapchain
}

void wr_vulkan_swapchain_delete(wr_vulkan_t* vulkan) {
	// Free framebuffers & image view #1
	for (int i = 0; i < vulkan->image_count; i++) {
		vkDestroyFramebuffer(vulkan->device, vulkan->frame_buffers[i],
			NULL);
		vkDestroyImageView(vulkan->device,
			vulkan->present_image_views[i], NULL);
		vkDestroyImage(vulkan->device, vulkan->present_images[i], NULL);
	}
	// Free render pass
	vkDestroyRenderPass(vulkan->device, vulkan->render_pass, NULL);
	// Free depth buffer
	vkDestroyImageView(vulkan->device, vulkan->depth_image_view, NULL);
	vkDestroyImage(vulkan->device, vulkan->depth_image, NULL);
	vkFreeMemory(vulkan->device, vulkan->depth_image_memory, NULL);
	// Free image view #2
	vkDestroyFence(vulkan->device, vulkan->submit_fence, NULL);
	// Free swapchain
	vkDestroySwapchainKHR(vulkan->device, vulkan->swapchain, NULL);
}

void wr_vulkan_draw_begin(wr_vulkan_t* vulkan, float r, float g, float b) {
	VkSemaphoreCreateInfo semaphore_ci = {
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, 0, 0
	};
	vkCreateSemaphore(vulkan->device, &semaphore_ci, NULL,
		&vulkan->presenting_complete_sem);
	vkCreateSemaphore(vulkan->device, &semaphore_ci, NULL,
		&vulkan->rendering_complete_sem);

	vkAcquireNextImageKHR(vulkan->device, vulkan->swapchain, UINT64_MAX,
		vulkan->presenting_complete_sem, VK_NULL_HANDLE,
		&vulkan->next_image_index);

	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	vkBeginCommandBuffer(vulkan->command_buffer, &beginInfo);

	VkImageMemoryBarrier layoutTransitionBarrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = vulkan->present_images[ vulkan->next_image_index ],
	};

	VkImageSubresourceRange resourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	layoutTransitionBarrier.subresourceRange = resourceRange;

	vkCmdPipelineBarrier(vulkan->command_buffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
		0, 0, NULL, 0, NULL, 1, &layoutTransitionBarrier);

	// activate render pass:
	const float CLEAR_COLOR[] = { r, g, b, 1.0f };
	VkClearValue clearValue[2];
	memcpy(clearValue[0].color.float32, CLEAR_COLOR, sizeof(float) * 4);
	clearValue[1].depthStencil = (VkClearDepthStencilValue) { 1.0, 0 };
	VkRenderPassBeginInfo renderPassBeginInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = vulkan->render_pass,
		.framebuffer = vulkan->frame_buffers[vulkan->next_image_index],
		.renderArea.offset.x = 0,
		.renderArea.offset.y = 0,
		.renderArea.extent.width = vulkan->width,
		.renderArea.extent.height = vulkan->height,
		.clearValueCount = 2,
		.pClearValues = clearValue,
	};
	vkCmdBeginRenderPass(vulkan->command_buffer, &renderPassBeginInfo,
		VK_SUBPASS_CONTENTS_INLINE);

	// bind the graphics pipeline to the command buffer. Any vkDraw command afterwards is affeted by this pipeline!
	vkCmdBindPipeline(vulkan->command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->pipeline);

	// take care of dynamic state:
	VkViewport viewport = { 0, 0, vulkan->width, vulkan->height, 0, 1 };
	vkCmdSetViewport(vulkan->command_buffer, 0, 1, &viewport);

	VkRect2D scissor = {
		.offset = { 0, 0 }, .extent = { vulkan->width, vulkan->height },
	};
	vkCmdSetScissor(vulkan->command_buffer, 0, 1, &scissor);
}

void wr_vulkan_draw_shape(wr_vulkan_t* vulkan, wr_shape_t* shape) {
	vulkan->offsets = 0;
	vkCmdBindVertexBuffers(vulkan->command_buffer, 0, 1,
		&shape->vertex_input_buffer, &vulkan->offsets);
	vkCmdDraw(vulkan->command_buffer, 3, 1, 0, 0);
}

void wr_vulkan_draw_update(wr_vulkan_t* vulkan,wr_window_t window,uint8_t fps) {
	vkCmdEndRenderPass(vulkan->command_buffer);
	// change layout back to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	VkImageMemoryBarrier prePresentBarrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel = 0,
		.subresourceRange.levelCount = 1,
		.subresourceRange.baseArrayLayer = 0,
		.subresourceRange.layerCount = 1,
		.image = vulkan->present_images[vulkan->next_image_index],
	};
	vkCmdPipelineBarrier(vulkan->command_buffer,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1,
		&prePresentBarrier);

	vkEndCommandBuffer(vulkan->command_buffer);
	// present:
	VkFence render_fence;
	VkFenceCreateInfo fenceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
	};
	vkCreateFence(vulkan->device, &fenceCreateInfo, NULL, &render_fence);
	VkPipelineStageFlags waitStageMash=VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &vulkan->presenting_complete_sem,
		.pWaitDstStageMask = &waitStageMash,
		.commandBufferCount = 1,
		.pCommandBuffers = &vulkan->command_buffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &vulkan->rendering_complete_sem,
	};
	vkQueueSubmit(vulkan->present_queue, 1, &submitInfo, render_fence);
	vkWaitForFences(vulkan->device, 1, &render_fence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(vulkan->device, render_fence, NULL);
	VkPresentInfoKHR present_info = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = NULL,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &vulkan->rendering_complete_sem,
		.swapchainCount = 1,
		.pSwapchains = &vulkan->swapchain,
		.pImageIndices = &vulkan->next_image_index,
		.pResults = NULL,
	};
	vkQueuePresentKHR(vulkan->present_queue, &present_info);
	vkDestroySemaphore(vulkan->device, vulkan->presenting_complete_sem, NULL);
	vkDestroySemaphore(vulkan->device, vulkan->rendering_complete_sem, NULL);
	wr_vulkan_draw_on_window(vulkan, window, fps);
}

void wr_close(wr_t wrapper) {
	wr_vulkan_swapchain_delete(&wrapper.vulkan);
	xcb_disconnect(wrapper.window.connection);
}

wr_t wr_open(const char* title) {
	wr_t wrapper;
	wrapper.window = wr_window(title); // Create Window
	wrapper.vulkan = wr_vulkan(title); // Initialize Vulkan
	wr_vulkan_window(&wrapper.vulkan, wrapper.window); // Link window to Vk
	wr_vulkan_gpu(&wrapper.vulkan); // Link GPU to Vulkan Instance
	wr_vulkan_device(&wrapper.vulkan); // Link Logical Device to Vulkan
	wr_vulkan_queue(&wrapper.vulkan); // Link Device Queue to Vulkan
	wr_vulkan_command_buffer(&wrapper.vulkan); // Link CMD Buffer to Vulkan
	wr_vulkan_resize(&wrapper.vulkan); // Resize
	return wrapper;
}
