cmake_minimum_required(VERSION 3.19)
project(Lightrail
	VERSION 1.0
	DESCRIPTION "3D renderer"
	LANGUAGES C CXX
)
set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)
find_package(cglm REQUIRED)
find_package(assimp REQUIRED)

include_directories(/opt/homebrew/include ${Vulkan_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS} ${cglm_INCLUDE_DIRS} ${ASSIMP_INCLUDE_DIRS})

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

# imgui
set(BAKENDS_FOLDER "./external/cimgui/imgui/backends/")
set(TABLES_SOURCE "./external/cimgui/imgui/imgui_tables.cpp")

add_definitions("-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=1")

include_directories(./external/cimgui/)
set(IMGUI_SOURCES ./external/cimgui/cimgui.cpp 
	./external/cimgui/imgui/imgui.cpp 
	./external/cimgui/imgui/imgui_draw.cpp 
	./external/cimgui/imgui/imgui_demo.cpp 
	./external/cimgui/imgui/imgui_widgets.cpp
	${TABLES_SOURCE} 
)

set(IMGUI_SOURCES_sdl)
set(IMGUI_LIBRARIES )

if (WIN32)
    add_definitions("-DIMGUI_IMPL_API=extern \"C\" __declspec\(dllexport\)")
else(WIN32)
    add_definitions("-DIMGUI_IMPL_API=extern \"C\" ")
endif(WIN32)

list(APPEND IMGUI_SOURCES ${BAKENDS_FOLDER}imgui_impl_vulkan.cpp)
list(APPEND IMGUI_LIBRARIES ${Vulkan_LIBRARIES})
list(APPEND IMGUI_SOURCES ${BAKENDS_FOLDER}imgui_impl_sdl.cpp)

if(SDL2_FOUND)
	get_target_property(SDL_INCLUDE SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
	message(STATUS "sdlinclude is " ${SDL_INCLUDE})
	if ("${SDL_INCLUDE}" STREQUAL "" OR "${SDL_INCLUDE}" STREQUAL "SDL_INCLUDE-NOTFOUND") #if not found latest SDL2 cmake config use older
		message(STATUS "sdlinclude2 is " ${SDL2_INCLUDE_DIRS})
		include_directories(${SDL2_INCLUDE_DIRS})
		set(IMGUI_SDL_LIBRARY ${SDL2_LIBRARIES})
		message(STATUS IMGUI_SDL_LIBRARY ${SDL2_LIBRARIES})
	else()#use new one SDL2 config
		include_directories(${SDL_INCLUDE})
		set(IMGUI_SDL_LIBRARY SDL2::SDL2)
		set(SDL_MAIN SDL2::SDL2main)
		message(STATUS ${SDL_MAIN} ${IMGUI_SDL_LIBRARY})
	endif()
else(SDL2_FOUND)
	if(DEFINED SDL_PATH)
		message(FATAL_ERROR "Cannot find SDL at SDL_PATH")
	else(DEFINED SDL_PATH)
		message(FATAL_ERROR "Cannot find SDL. Maybe try specifying SDL_PATH?")
	endif(DEFINED SDL_PATH)
endif(SDL2_FOUND)

add_library(cimgui_sdl STATIC ${IMGUI_SOURCES})
target_link_libraries(cimgui_sdl ${IMGUI_LIBRARIES} ${IMGUI_SDL_LIBRARY} ${Vulkan_LIBRARIES})
target_include_directories(cimgui_sdl PRIVATE ./external/cimgui/imgui/ ./external/cimgui/imgui/backends/)

# Main Executable
add_executable(lightrail)
set_property(TARGET lightrail PROPERTY C_STANDARD 17)
target_include_directories(lightrail PUBLIC ./include)
if(APPLE)
	target_include_directories(lightrail PRIVATE /opt/homebrew/include)
endif(APPLE)
message(STATUS ${ASSIMP_LIBRARIES})
target_include_directories(lightrail PRIVATE /opt/homebrew/include ${Vulkan_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS} ${cglm_INCLUDE_DIRS} ${ASSIMP_INCLUDE_DIRS})
target_sources(
	lightrail PUBLIC
	src/main.c
	src/renderer.c
	src/alloc.c
	src/camera.c
	src/scene.c
	src/vertex_buffer.c
	src/util.c
	src/ui.c
)
add_dependencies(lightrail shaders cimgui_sdl)
target_compile_definitions(lightrail PUBLIC CIMGUI_DEFINE_ENUMS_AND_STRUCTS)
target_link_libraries(lightrail ${Vulkan_LIBRARIES} ${SDL2_LIBRARIES} ${cglm_LIBRARIES} ${ASSIMP_LIBRARIES} cimgui_sdl)