#include "LuauRuntime.h"
#include "RobloxApi.h"
#include "RobloxScheduler.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "lua.h"
#include "lualib.h"

#include "luacode.h"

LuauRuntime::LuauRuntime()
	: state_(luaL_newstate())
{
	if (!state_)
	{
		throw std::runtime_error(
			"Failed to create Luau state"
		);
	}

	luaL_openlibs(state_);

	RobloxApi::install(state_);

	luaL_sandbox(state_);
}

LuauRuntime::~LuauRuntime()
{
	if (state_)
	{
		RobloxApi::shutdown(state_);
		lua_close(state_);
	}
}

bool LuauRuntime::execute(
	const std::string& source,
	const std::string& chunkName
)
{
	size_t bytecodeSize = 0;

	char* bytecode =
		luau_compile(
			source.c_str(),
			source.size(),
			nullptr,
			&bytecodeSize
		);

	if (!bytecode)
	{
		std::cerr
			<< "[Luau] compiler returned no bytecode\n";

		return false;
	}

	lua_State* thread =
		lua_newthread(state_);

	luaL_sandboxthread(thread);

	const int loadStatus =
		luau_load(
			thread,
			chunkName.c_str(),
			bytecode,
			bytecodeSize,
			0
		);

	std::free(bytecode);

	if (loadStatus != 0)
	{
		const char* message =
			lua_tostring(
				thread,
				-1
			);

		std::cerr
			<< "[Luau load] "
			<< (
				message
					? message
					: "unknown error"
			)
			<< "\n";

		lua_pop(state_, 1);
		return false;
	}

	const int status =
		lua_resume(
			thread,
			nullptr,
			0
		);

	if (status == LUA_YIELD)
	{
		const bool managed =
			RobloxScheduler::isManaged(
				state_,
				thread
			);

		lua_pop(state_, 1);

		if (!managed)
		{
			std::cerr
				<< "[Luau] script yielded outside the runtime scheduler\n";

			return false;
		}

		return true;
	}

	if (status != LUA_OK)
	{
		const char* message =
			lua_tostring(
				thread,
				-1
			);

		std::cerr
			<< "[Luau runtime] "
			<< (
				message
					? message
					: "unknown error"
			)
			<< "\n";

		const char* trace =
			lua_debugtrace(thread);

		if (trace)
			std::cerr << trace << "\n";

		lua_pop(state_, 1);
		return false;
	}

	lua_pop(state_, 1);
	return true;
}

bool LuauRuntime::executeFile(
	const std::string& path
)
{
	std::ifstream input(
		path,
		std::ios::binary
	);

	if (!input)
	{
		std::cerr
			<< "Cannot open script: "
			<< path
			<< "\n";

		return false;
	}

	std::ostringstream stream;
	stream << input.rdbuf();

	return execute(
		stream.str(),
		"=" + path
	);
}

void LuauRuntime::step(double deltaTime)
{
	RobloxApi::step(
		state_,
		deltaTime
	);
}

void LuauRuntime::emitKey(
	const std::string& keyCodeName,
	bool pressed
)
{
	RobloxApi::emitKey(
		state_,
		keyCodeName,
		pressed
	);
}


std::vector<
	RobloxObjectModel::RenderPartSnapshot
> LuauRuntime::getRenderParts() const
{
	return RobloxObjectModel::getRenderParts(
		state_
	);
}

RobloxObjectModel::RenderCameraSnapshot
LuauRuntime::getRenderCamera() const
{
	return RobloxObjectModel::getRenderCamera(
		state_
	);
}
