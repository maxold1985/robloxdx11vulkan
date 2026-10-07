#pragma once

#include <string>

struct lua_State;

namespace RobloxApi
{
	void install(lua_State* L);
	void shutdown(lua_State* L);

	void step(lua_State* L, double deltaTime);

	void emitKey(
		lua_State* L,
		const std::string& keyCodeName,
		bool pressed
	);
}
