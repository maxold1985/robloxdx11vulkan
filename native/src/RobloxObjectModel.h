#pragma once

struct lua_State;

namespace RobloxObjectModel
{
	void install(lua_State* L);
	void shutdown(lua_State* L);

	bool isInstance(lua_State* L, int index);
}
