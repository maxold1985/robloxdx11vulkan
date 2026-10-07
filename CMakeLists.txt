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

FetchContent_GetProperties(luau)

if(NOT luau_POPULATED)
	FetchContent_Populate(luau)

	# Luau cria o target isocline mesmo com LUAU_BUILD_CLI=OFF.
	# EXCLUDE_FROM_ALL impede que ele seja compilado pelo target padrão.
	# roblox_runtime puxa somente Luau.Compiler/Luau.VM e dependências reais.
	add_subdirectory(
		"${luau_SOURCE_DIR}"
		"${luau_BINARY_DIR}"
		EXCLUDE_FROM_ALL
	)
endif()

add_executable(roblox_runtime
	native/src/main.cpp
	native/src/LuauRuntime.cpp
	native/src/RobloxApi.cpp
	native/src/RobloxObjectModel.cpp
	native/src/RobloxScheduler.cpp
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


enable_testing()

add_test(
	NAME roblox_runtime_smoke
	COMMAND roblox_runtime
		${CMAKE_CURRENT_SOURCE_DIR}/native/scripts/test.luau
)


add_test(
	NAME roblox_cube_control_smoke
	COMMAND roblox_runtime
		${CMAKE_CURRENT_SOURCE_DIR}/native/scripts/cube/CubeServer.server.lua
		${CMAKE_CURRENT_SOURCE_DIR}/native/scripts/cube/CubeClient.client.lua
		--press=W
		--press=Space
		--step=2
		${CMAKE_CURRENT_SOURCE_DIR}/native/scripts/cube/CubeVerify.luau
)


if(WIN32)
	target_sources(roblox_runtime PRIVATE
		native/src/Dx11Renderer.cpp
	)

	target_link_libraries(roblox_runtime PRIVATE
		d3d11
		dxgi
		user32
		gdi32
	)

	set(
		ROBLOX_FXC
		""
		CACHE FILEPATH
		"Path to fxc.exe used only at build time"
	)

	if(ROBLOX_FXC)
		set(FXC_EXECUTABLE "${ROBLOX_FXC}")
	else()
		find_program(
			FXC_EXECUTABLE
			NAMES
				fxc.exe
				fxc
			HINTS
				"$ENV{WindowsSdkDir}/bin/x86"
				"C:/Program Files (x86)/Windows Kits/8.0/bin/x86"
				"C:/Program Files (x86)/Windows Kits/8.1/bin/x86"
				"C:/Program Files (x86)/Windows Kits/10/bin/x86"
		)
	endif()

	if(NOT FXC_EXECUTABLE)
		message(
			FATAL_ERROR
			"fxc.exe not found. Set -DROBLOX_FXC=C:/path/to/fxc.exe"
		)
	endif()

	add_custom_command(
		TARGET roblox_runtime
		POST_BUILD

		COMMAND
			${CMAKE_COMMAND}
			-E
			make_directory
			"$<TARGET_FILE_DIR:roblox_runtime>/shaders"

		COMMAND
			"${FXC_EXECUTABLE}"
			/nologo
			/T vs_4_0
			/E main
			/Fo "$<TARGET_FILE_DIR:roblox_runtime>/shaders/CubeVS.cso"
			"${CMAKE_CURRENT_SOURCE_DIR}/native/shaders/CubeVS.hlsl"

		COMMAND
			"${FXC_EXECUTABLE}"
			/nologo
			/T ps_4_0
			/E main
			/Fo "$<TARGET_FILE_DIR:roblox_runtime>/shaders/CubePS.cso"
			"${CMAKE_CURRENT_SOURCE_DIR}/native/shaders/CubePS.hlsl"

		VERBATIM
	)
endif()
