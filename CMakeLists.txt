cmake_minimum_required(VERSION 3.19)
project(Lightrail VERSION 1.0 DESCRIPTION "Lightweight game engine")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
find_package(Eigen3 3.3 REQUIRED NO_MODULE)

# Shaders
set(
	SHADER_SOURCES
	shaders/basic.vert
	shaders/basic.frag
)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shaders)
foreach(SHADER_SOURCE ${SHADER_SOURCES})
	set(SHADER ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_SOURCE}.spv)
	add_custom_command(
		OUTPUT ${SHADER}
		COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE} -o ${SHADER}
		MAIN_DEPENDENCY ${SHADER_SOURCE}
	)
	list(APPEND SHADER_FILES ${SHADER})
endforeach()
add_custom_target(shaders DEPENDS ${SHADER_FILES})

# Main executable
add_executable(lightrail)
target_include_directories(lightrail PUBLIC ./include)
target_sources(
	lightrail PUBLIC
	src/main.cpp
	src/app.cpp
	src/renderer.cpp
	src/buffer.cpp
	src/image.cpp
	src/texture.cpp
	src/camera.cpp
	src/mesh.cpp
)
add_dependencies(lightrail shaders)
target_link_libraries(lightrail SDL2 vulkan Eigen3::Eigen)
target_precompile_headers(lightrail PRIVATE
	<vulkan/vulkan.hpp>
	[["vk_mem_alloc.h"]]
	<Eigen/Core>
	<Eigen/Geometry>
)

# Test executable
add_executable(test)
target_include_directories(test PUBLIC ./include)
target_sources(
	test PUBLIC
	src/test.cpp
	src/mesh.cpp
)
target_link_libraries(test Eigen3::Eigen)
