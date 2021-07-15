cmake_minimum_required(VERSION 3.19)
project(Lightrail VERSION 1.0 DESCRIPTION "Lightweight game engine")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_executable(lightrail)
target_include_directories(lightrail PUBLIC ./include)
target_sources(
	lightrail PUBLIC
	src/main.cpp
	src/app.cpp
	src/renderer.cpp
	src/render-utils.cpp
)
target_link_libraries(lightrail SDL2 vulkan)

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
