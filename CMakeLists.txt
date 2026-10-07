cmake_minimum_required(VERSION 3.20)

project(robloxdx11vulkan LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(LUAU_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_WEB OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
	luau
	GIT_REPOSITORY https://github.com/luau-lang/luau.git
	GIT_TAG master
)

FetchContent_MakeAvailable(luau)

add_executable(roblox_runtime
	native/src/main.cpp
	native/src/LuauRuntime.cpp
	native/src/RobloxApi.cpp
	native/src/RobloxObjectModel.cpp
	native/src/RobloxTypes.cpp
)

target_include_directories(roblox_runtime PRIVATE
	native/src
)

target_link_libraries(roblox_runtime PRIVATE
	Luau.Compiler
	Luau.VM
)

if(MSVC)
	target_compile_definitions(roblox_runtime PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
