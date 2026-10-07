#pragma once

struct lua_State;

namespace RobloxApi
{
	void install(lua_State* L);
	void shutdown(lua_State* L);
}
