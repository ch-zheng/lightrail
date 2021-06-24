cmake_minimum_required(VERSION 3.19)
project(Lightrail VERSION 1.0 DESCRIPTION "Lightweight game engine")
add_executable(lightrail)
target_include_directories(lightrail PUBLIC ./include)
target_sources(
	lightrail PUBLIC
	./src/main.cpp
	./src/app.cpp
	./src/renderer.cpp
)
target_link_libraries(lightrail SDL2 vulkan)
