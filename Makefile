run: build/vulkan_example
	./build/vulkan_example

build/frag.spv:
	glslangvalidator src/color.frag -V -o build/color-frag.spv

build/vert.spv:
	glslangvalidator src/color.vert -V -o build/color-vert.spv

build/vulkan_example: build/frag.spv build/vert.spv
	gcc src/main.c src/wrapper.c -lvulkan -lxcb -Wall -O3 -o build/vulkan_example
