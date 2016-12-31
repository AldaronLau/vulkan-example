#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(__ANDROID__) // Android
	#define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(_WIN32) // Windows
	#define VK_USE_PLATFORM_WIN32_KHR
#else // Linux / Mac
	#define VK_USE_PLATFORM_XCB_KHR
#endif

#include "vulkan/vulkan.h"

#define WR_EVENT_REDRAW 0
#define WR_EVENT_RESIZE 1

typedef struct {
	#if defined(VK_USE_PLATFORM_ANDROID_KHR)
        
	#elif defined(VK_USE_PLATFORM_WIN32_KHR)
        
	#else
	xcb_connection_t *connection;
	xcb_screen_t *screen;
	xcb_window_t window;
	#endif
} wr_window_t;

typedef struct {
	VkInstance instance; // Vulkan instance
	VkSurfaceKHR surface; // Surface that we render to.
	uint32_t present_queue_index;
	VkQueue present_queue;
	VkPhysicalDevice gpu;
	VkPhysicalDeviceProperties gpu_props;
	VkPhysicalDeviceMemoryProperties gpu_mem_props;
	VkDevice device; // The logical device
	VkCommandBuffer command_buffer;
	VkSwapchainKHR swapchain;
	uint32_t width, height; // Swapchain Dimensions.
	VkImage present_images[2]; // 2 for double-buffering
	VkFramebuffer frame_buffers[2]; // 2 for double-buffering
	VkFormat color_format;
	uint32_t image_count; // 1 (single-buffering) or 2 (double-buffering)
	VkFence submit_fence; // The submit fence
	VkImageView present_image_views[2]; // 2 for double-buffering
	VkImage depth_image;
	VkImageView depth_image_view;
	VkDeviceMemory depth_image_memory;
	VkRenderPass render_pass;
	VkPipeline pipeline;
	VkPipelineLayout pipeline_layout;
	uint32_t next_image_index;
	VkSemaphore presenting_complete_sem, rendering_complete_sem;
	VkDeviceSize offsets;
} wr_vulkan_t;

typedef struct {
	wr_window_t window;
	wr_vulkan_t vulkan;
} wr_t;

typedef struct {
	VkBuffer vertex_input_buffer;
} wr_shape_t;

typedef struct {
	VkShaderModule vertex;
	VkShaderModule fragment;
} wr_shader_t;

void wrapper_main(void);
wr_window_t wr_window(const char*);
int8_t wr_window_event(wr_window_t window, uint32_t* event, float* x, float* y);
wr_vulkan_t wr_vulkan(const char*);
void wr_vulkan_shape(wr_shape_t*, wr_vulkan_t, const float*);
void wr_vulkan_shader(wr_shader_t*,wr_vulkan_t,void*,uint32_t,void*,uint32_t);
void wr_vulkan_pipeline(wr_vulkan_t*, wr_shader_t*, uint32_t);
void wr_vulkan_resize(wr_vulkan_t*);
void wr_vulkan_swapchain_delete(wr_vulkan_t* vulkan);
void wr_vulkan_draw_begin(wr_vulkan_t* vulkan, float r, float g, float b);
void wr_vulkan_draw_shape(wr_vulkan_t*, wr_shape_t*);
void wr_vulkan_draw_update(wr_vulkan_t* vulkan,wr_window_t,uint8_t);
void wr_close(wr_t wrapper);
wr_t wr_open(const char* title);
