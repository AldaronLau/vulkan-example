#include "wrapper.h"

const char* APP_TITLE = "Vulkan Example";

typedef struct {
	wr_shape_t shape;
	wr_shader_t shader;
} context_t;

static void ve_load_shaders(wr_shader_t* shader, wr_vulkan_t vulkan) {
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
	// Make shader from vertex code and fragment code.
	wr_vulkan_shader(shader,vulkan,vertCode,vertSize,fragCode,fragSize);
}

void wrapper_main(void) {
	context_t context;
	wr_t wrapper = wr_open(APP_TITLE, 0);

	float v[] = {
		-.5f, .5f, 0.f, 1.0f,
		.5f, .5f, 0.f, 1.0f,
		0.0f, -.5f, 0.f, 1.0f,
	};
	wr_vulkan_shape(&context.shape, wrapper.vulkan, v);
	ve_load_shaders(&context.shader, wrapper.vulkan);
	// Pass shaders to pipeline
	wr_vulkan_pipeline(&wrapper.vulkan, &context.shader, 1);

	uint32_t event;
	float event_x, event_y;
	while (wr_window_event(wrapper.window, &event, &event_x, &event_y)) {
		if(event == WR_EVENT_REDRAW) {
			wr_vulkan_draw_begin(&wrapper.vulkan, 1.f, 0.f, 0.f);
			wr_vulkan_draw_shape(&wrapper.vulkan, &context.shape);
			wr_vulkan_draw_update(&wrapper.vulkan, wrapper.window, 60);
		}else if(event == WR_EVENT_RESIZE) {
			wrapper.vulkan.width = event_x;
			wrapper.vulkan.height = event_y;
			wr_vulkan_swapchain_delete(&wrapper.vulkan);
			wr_vulkan_resize(&wrapper.vulkan);
		}
	}
	wr_close(wrapper);
}
