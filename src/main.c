#include "wrapper.h"

const char* APP_TITLE = "Vulkan Example";

typedef struct {
	wr_shape_t shape;
	wr_shader_t shader;
} vulkan_context_t;

void ve_main(void) {
	vulkan_context_t context;
	wr_window_t window = wr_window(APP_TITLE); // Create Window
	wr_vulkan_t vulkan = wr_vulkan(APP_TITLE); // Initialize Vulkan

	wr_vulkan_attach(&vulkan, window);
	wr_vulkan_resize(&vulkan); // Resize
	float v[] = {
		-.5f, .5f, 0.f, 1.0f,
		.5f, .5f, 0.f, 1.0f,
		0.0f, -.5f, 0.f, 1.0f,
	};
	wr_vulkan_shape(&context.shape, vulkan, v);
	// Shaders
	FILE *fileHandle = 0;
	// Vertex shader
	if((fileHandle = fopen("build/color-vert.spv", "rb")) == NULL) {
		printf("Could not load vertex shader code.");
		abort();
	}
	fseek(fileHandle, 0L, SEEK_END);
	uint32_t vertSize = (uint32_t) ftell(fileHandle);
	fseek(fileHandle, 0L, SEEK_SET);
	char vertCode[vertSize];
	fread(vertCode, vertSize, 1, fileHandle);
	// Fragment shader
	if((fileHandle = fopen("build/color-frag.spv", "rb")) == NULL) {
		printf("Could not load vertex shader code.");
		abort();
	}
	fseek(fileHandle, 0L, SEEK_END);
	uint32_t fragSize = (uint32_t) ftell(fileHandle);
	fseek(fileHandle, 0L, SEEK_SET);
	char fragCode[fragSize];
	fread(fragCode, fragSize, 1, fileHandle);
	// Put shaders together.
	wr_vulkan_shader(&context.shader, vulkan, vertCode, vertSize,
		fragCode, fragSize);
	// Pass shaders to pipeline.
	wr_vulkan_pipeline(&vulkan, &context.shader, 1);

	uint32_t event;
	float event_x, event_y;
	while (wr_window_event(window, &event, &event_x, &event_y)) {
		if(event) {
			if(event == WR_EVENT_RESIZE) {
				vulkan.width = event_x;
				vulkan.height = event_y;
				wr_vulkan_swapchain_delete(&vulkan);
				wr_vulkan_resize(&vulkan);
			}
		}else{
			wr_vulkan_draw_begin(&vulkan);
			wr_vulkan_draw_shape(&vulkan, &context.shape);
			wr_vulkan_draw_update(&vulkan, window, 60);
		}
	}
	wr_vulkan_delete(&vulkan);
	wr_window_close(window);
}

void wrapper_main(void) {
	ve_main();
}
