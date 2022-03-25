cmake_minimum_required(VERSION 3.19)
project(Lightrail
	VERSION 1.0
	DESCRIPTION "3D renderer"
	LANGUAGES C
)

find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)
find_package(cglm REQUIRED)
find_package(assimp REQUIRED)

# Shaders
set(
	SHADER_SOURCES
	shaders/basic.vert
	shaders/basic.frag
	shaders/test.vert
	shaders/test.frag
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
set_property(TARGET lightrail PROPERTY C_STANDARD 17)
target_include_directories(lightrail PUBLIC ./include)
# if(APPLE)
	target_include_directories(lightrail PRIVATE /opt/homebrew/include)
# endif(APPLE)
message(STATUS ${ASSIMP_LIBRARIES})
target_include_directories(lightrail PRIVATE /opt/homebrew/include ${Vulkan_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS} ${cglm_INCLUDE_DIRS} ${ASSIMP_INCLUDE_DIRS})
target_sources(
	lightrail PUBLIC
	src/main.c
	src/renderer.c
	src/alloc.c
	src/camera.c
	src/scene.c
)
add_dependencies(lightrail shaders)
target_link_libraries(lightrail ${Vulkan_LIBRARIES} ${SDL2_LIBRARIES} ${cglm_LIBRARIES} ${ASSIMP_LIBRARIES})

# Testing
add_executable(test)
target_include_directories(test PRIVATE ${Vulkan_INCLUDE_DIRECTORIES} ${SDL2_INCLUDE_DIRECTORIES} ${cglm_INCLUDE_DIRECTORIES} ${ASSIMP_INCLUDE_DIRS})
target_include_directories(test PUBLIC ./include)
target_link_libraries(test ${Vulkan_LIBRARIES} ${SDL2_LIBRARIES} ${cglm_LIBRARIES} ${ASSIMP_LIBRARIES})
target_sources(
	test PUBLIC
	src/test.c
	src/scene.c
)
set_property(TARGET test PROPERTY C_STANDARD 17)

