cmake_minimum_required(VERSION 3.19)
project(Lightrail VERSION 1.0 DESCRIPTION "Lightweight game engine")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
find_package(Eigen3 3.3 REQUIRED NO_MODULE)
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
)
target_link_libraries(lightrail SDL2 vulkan Eigen3::Eigen)

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
add_dependencies(lightrail shaders)
